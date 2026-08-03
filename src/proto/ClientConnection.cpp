/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "ClientConnection.h"

#include "MopAuthProof.h"
#include "MopWireCodec.h"

#include "Log/Log.h"
#include "Utilities/ByteBuffer.h"

#include <atomic>
#include <cstring>
#include <openssl/rand.h>

namespace proto
{
    std::atomic<uint32> ClientConnection::s_openConnections{0};

    namespace
    {
        // The handful of transport opcodes this file speaks. Re-derived from (grepped
        // out of, never copied by memory from) src/game/Server/Opcodes.h -- proto does
        // not link game, so these are proto-local constants. Values confirmed against
        // that header on 2026-07-26:
        //   MSG_WOW_CONNECTION   Opcodes.h:71  (0x4F57)
        //   SMSG_AUTH_CHALLENGE  Opcodes.h:72  (0x0949)
        //   CMSG_AUTH_SESSION    Opcodes.h:73  (0x00B2)
        //   CMSG_PING            Opcodes.h:515 (0x0012)
        //   SMSG_PONG            Opcodes.h:516 (0x1969)
        //   CMSG_KEEP_ALIVE      Opcodes.h:985 (0x1A87)
        //   CMSG_LOG_DISCONNECT  Opcodes.h:1246 (0x10B3)
        const uint16 MSG_WOW_CONNECTION  = 0x4F57;
        const uint16 SMSG_AUTH_CHALLENGE = 0x0949;
        const uint16 CMSG_AUTH_SESSION   = 0x00B2;
        const uint16 CMSG_PING           = 0x0012;
        const uint16 SMSG_PONG           = 0x1969;
        const uint16 CMSG_KEEP_ALIVE     = 0x1A87;
        const uint16 CMSG_LOG_DISCONNECT = 0x10B3;

        // Opcodes.h:1445 -- `#define OPCODE_TABLE_SIZE 0x2000`, the 13-bit wire space
        // the post-crypt frame packs into. Duplicated here for the same reason the
        // opcode constants above are: proto cannot include Opcodes.h.
        const uint32_t OPCODE_TABLE_SIZE = 0x2000;

        bool DefaultRandomBytes(uint8_t* out, size_t len)
        {
            return RAND_bytes(out, int(len)) == 1;
        }
    }

    ClientConnection::ClientConnection(IWorldGateway& gateway)
        : m_gateway(gateway),
          m_closed(false),
          m_connState(MopHs::CONN_GREETING),
          m_seed(0),
          m_session(INVALID_SESSION_ID),
          m_hadPing(false),
          m_fastPingRun(0),
          m_lastDecodeLogSec(0)
    {
        s_openConnections.fetch_add(1, std::memory_order_relaxed);
    }

    ClientConnection::~ClientConnection()
    {
        s_openConnections.fetch_sub(1, std::memory_order_relaxed);
    }

    std::vector<uint8_t> ClientConnection::EncodeForSend(const WorldPacket& packet, bool& fatal)
    {
        fatal = false;

        uint8 header[4];
        const bool postCrypt = m_crypt.IsInitialized();
        if (!MopWire::BuildServerHeader(postCrypt, packet.size(), packet.GetOpcode(), header))
        {
            // Hazard H2: an opcode still on its pre-Phase-1b placeholder value (> 0x1FFF
            // post-crypt) does not fit the 13-bit frame. DROP the packet, exactly as
            // WorldSocket::SendPacket does (WorldSocket.cpp:226-244) -- disconnecting here
            // was the historical bug (mid character-create teleport drops). Report each
            // offending opcode ONCE per run; proto has no LookupOpcodeName (no Opcodes.h
            // access), so only the raw value is logged.
            static std::atomic<bool> s_reportedDrop[0x10000];
            if (!s_reportedDrop[packet.GetOpcode()].exchange(true))
            {
                sLog.outError("proto: opcode 0x%.4X not framable in MoP -- still on a "
                              "placeholder value; dropping it and further such packets "
                              "silently.", packet.GetOpcode());
            }
            return std::vector<uint8_t>();
        }

        if (postCrypt)
        {
            // The cipher is a stream: two threads encrypting headers concurrently would
            // interleave the keystream and desynchronise the client for good.
            std::lock_guard<std::mutex> lock(m_cryptSendLock);
            if (!m_crypt.EncryptSend(header, sizeof(header)))
            {
                // The header is UNDEFINED on an EVP failure (may be partially written).
                // Dropping is not safe here -- the caller must close the connection.
                sLog.outError("proto: header encryption failed (opcode 0x%.4X) for %s; "
                              "closing.", packet.GetOpcode(), m_address.c_str());
                fatal = true;
                return std::vector<uint8_t>();
            }
        }

        std::vector<uint8_t> wire;
        wire.reserve(sizeof(header) + packet.size());
        wire.insert(wire.end(), header, header + sizeof(header));
        if (!packet.empty())
        {
            wire.insert(wire.end(), packet.contents(), packet.contents() + packet.size());
        }
        return wire;
    }

    std::vector<uint8_t> ClientConnection::onConnect()
    {
        // WorldSocket::open() (WorldSocket.cpp:326-381) sends ONLY the server's own
        // greeting on connect. M4's FSM validates the client's greeting BEFORE sending
        // the auth challenge (hazard H5) -- this is NOT Cata's fire-and-continue FSM,
        // where both packets go out back to back. HandleWowConnection() sends the
        // challenge later, once the client's own greeting string has checked out.
        WorldPacket greeting(MSG_WOW_CONNECTION, 46);
        greeting << std::string("RLD OF WARCRAFT CONNECTION - SERVER TO CLIENT");

        bool fatal = false;
        std::vector<uint8_t> wire = EncodeForSend(greeting, fatal);
        if (fatal || wire.empty())
        {
            Close();
            return std::vector<uint8_t>();
        }
        return wire;
    }

    std::vector<uint8_t> ClientConnection::onData(const uint8_t* data, size_t len)
    {
        // Hazard H4 input guard. Both net:: backends re-arm reads once a session is
        // closed() but its outbound buffer is still draining (ReactorServer checks
        // channel->out.empty() before tearing down; IocpServer's handleRecv()
        // re-posts a receive for exactly the same reason) -- deliberately, so the
        // teardown has a completion to hang off even if the peer goes quiet. That
        // means onData() CAN be re-entered after Close() was already called (e.g.
        // an auth rejection still flushing its SMSG_AUTH_RESPONSE). A peer already
        // rejected must not have anything it sends acted upon, so refuse outright.
        // This replaces WorldSocket-era MopSock::MayProcessInput/DrainState: those
        // encoded ACE_TP_Reactor's suspend-window mechanics (cancel_wakeup, a
        // dispatch-status contract requiring a non-1 return to stop re-entry), which
        // have no counterpart in either net:: backend and would not mean anything
        // here. The PROPERTY those mechanics existed to guarantee is what survives.
        if (closed())
        {
            return std::vector<uint8_t>();
        }

        m_frameReader.Push(data, len);

        MopFrameReader::Frame frame;
        for (;;)
        {
            // Header width is state-driven, derived fresh every frame -- mirrors
            // WorldSocket::handle_input_missing_data (WorldSocket.cpp:879-887).
            const MopFrameReader::HeaderKind kind =
                m_crypt.IsInitialized()               ? MopFrameReader::HDR_POSTCRYPT :
                (m_connState == MopHs::CONN_GREETING) ? MopFrameReader::HDR_GREETING :
                                                         MopFrameReader::HDR_PRECRYPT;

            const MopFrameReader::Status status =
                m_frameReader.TryFrame(frame, kind, this, &DecryptHeaderHook, &CmdValidHook);

            if (status == MopFrameReader::NEED_MORE)
            {
                break;
            }

            if (status == MopFrameReader::MALFORMED)
            {
                const int64_t nowSec =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                const int64_t lastSec = m_lastDecodeLogSec.load();
                if (MopHs::RateLimitElapsed(lastSec, nowSec))
                {
                    m_lastDecodeLogSec.store(nowSec);
                    sLog.outError("proto: malformed frame from %s (reason=%s); closing.",
                                  m_address.c_str(),
                                  MalformedReasonName(m_frameReader.LastReason()));
                }
                Close();
                break;
            }

            if (!DispatchFrame(frame.cmd, std::move(frame.payload)))
            {
                Close();
                break;
            }
        }

        // Every reply goes through SendPacket()/net::Sender: a reply may be produced
        // long after this call returns (e.g. SendAuthWaitQue, from the world
        // thread), so nothing is ever returned inline here.
        return std::vector<uint8_t>();
    }

    bool ClientConnection::DispatchFrame(uint32_t cmd, std::vector<uint8_t>&& payload)
    {
        WorldPacket packet(uint16(cmd), uint32(payload.size()));
        if (!payload.empty())
        {
            packet.append(payload.data(), payload.size());
        }

        // Handshake-state legality allowlist -- mirrors WorldSocket::ProcessIncoming
        // (WorldSocket.cpp:1058-1070) exactly, including running BEFORE the greeting
        // dispatch below (a duplicate-greeting bypass otherwise).
        const MopHs::OpcodeClass cls =
            (cmd == MSG_WOW_CONNECTION) ? MopHs::OPC_GREETING :
            (cmd == CMSG_AUTH_SESSION)  ? MopHs::OPC_AUTH_SESSION : MopHs::OPC_NORMAL;
        if (!MopHs::IsHandshakeOpcodeLegal(m_connState, cls))
        {
            sLog.outError("proto: opcode 0x%.4X illegal in state %d from %s; closing.",
                          cmd, int(m_connState), m_address.c_str());
            return false;
        }

        if (cmd == MSG_WOW_CONNECTION)
        {
            return HandleWowConnection(packet);
        }

        try
        {
            switch (cmd)
            {
                case CMSG_PING:
                    return HandlePing(packet);

                case CMSG_AUTH_SESSION:
                    // WorldSocket.cpp:1113-1116: the Eluna OnPacketReceive hook for this
                    // opcode is DELIBERATELY not invoked -- a script must never observe
                    // the raw auth body (digest included) before it is parsed. This is a
                    // Phase 4 decision, not an oversight; CP3 carries the absence intact
                    // rather than "restoring" a hook M4 never had (contrast MangosThree,
                    // whose WorldSocket did call it and whose IWorldGateway therefore
                    // grew an OnAuthPacketReceived veto point -- not applicable here).
                    m_connState = MopHs::CONN_AUTHENTICATING;
                    return HandleAuthSession(packet);

                case CMSG_KEEP_ALIVE:
                    // WorldSocket.cpp:1119-1128: fire-and-forget Eluna notification, no veto.
                    DEBUG_LOG("proto: CMSG_KEEP_ALIVE from %s.", m_address.c_str());
                    m_gateway.OnPacketReceived(packet, m_session);
                    return true;

                case CMSG_LOG_DISCONNECT:
                    packet.rfinish();                     // uint32 disconnect reason; socket notification only
                    // WorldSocket.cpp:1129-1137: fire-and-forget Eluna notification, no veto.
                    m_gateway.OnPacketReceived(packet, m_session);
                    return true;

                default:
                    if (m_session == INVALID_SESSION_ID)
                    {
                        sLog.outError("proto: opcode 0x%.4X from unauthenticated peer %s; "
                                      "closing.", cmd, m_address.c_str());
                        return false;
                    }
                    m_gateway.Deliver(m_session, std::move(packet));
                    return true;
            }
        }
        catch (ByteBufferException&)
        {
            sLog.outError("proto: decode failure opcode 0x%.4X from %s; closing.",
                          cmd, m_address.c_str());
            return false;
        }
    }

    bool ClientConnection::HandleWowConnection(WorldPacket& packet)
    {
        // WorldSocket::HandleWowConnection (WorldSocket.cpp:383-402). "RLD..." not
        // "WORLD...": the leading "WO" is carried by the frame's cmd field
        // (0x4F57 == "WO" little-endian), so the payload legitimately begins at "RLD".
        std::string clientToServerMsg;
        packet >> clientToServerMsg;

        if (strcmp(clientToServerMsg.c_str(), "RLD OF WARCRAFT CONNECTION - CLIENT TO SERVER") != 0)
        {
            sLog.outError("proto: wrong data in MSG_WOW_CONNECTION from %s; closing.",
                          m_address.c_str());
            return false;
        }

        return SendAuthChallenge();
    }

    bool ClientConnection::SendAuthChallenge()
    {
        std::vector<uint8_t> payload;
        uint32 seed = 0;
        if (!MopHs::BuildAuthChallengePayload(&DefaultRandomBytes, payload, seed))
        {
            sLog.outError("proto: SendAuthChallenge: CSPRNG failure for %s; closing.",
                          m_address.c_str());
            return false;
        }

        WorldPacket packet(SMSG_AUTH_CHALLENGE, uint32(payload.size()));
        packet.append(payload.data(), payload.size());
        SendPacket(packet);

        if (closed())
        {
            // SendPacket() no-ops once closed, so nothing reached the peer -- mirrors
            // WorldSocket::SendAuthChallenge's `sent` check: never advance the FSM on a
            // challenge the client never got.
            return false;
        }

        m_seed = seed;
        m_connState = MopHs::CONN_CHALLENGED;
        return true;
    }

    bool ClientConnection::HandleAuthSession(WorldPacket& packet)
    {
        AuthRequest request;
        request.peerAddress = m_address;

        MopAuth::DecodeResult const decodeResult =
            MopAuth::DecodeAuthSession(packet, request.fields);
        if (decodeResult != MopAuth::DecodeResult::Ok)
        {
            // DEBUG_LOG, not sLog.outError: fires on every malformed body on an
            // UNAUTHENTICATED path (WorldSocket::HandleAuthSession's own rationale).
            // Deliberately NOT rate-limited: this closes the connection immediately,
            // so it logs at most once per connection regardless.
            DEBUG_LOG("proto: malformed CMSG_AUTH_SESSION from %s (result %u).",
                      m_address.c_str(), static_cast<uint32>(decodeResult));
            RejectAuth(AuthStatus::Failed);
            return false;
        }

        const AuthLookup lookup = m_gateway.LookupAccount(request);
        if (lookup.status != AuthStatus::Ok)
        {
            RejectAuth(lookup.status);
            return false;
        }

        uint8 serverProof[MopAuth::AUTH_PROOF_LEN];
        MopAuth::ComputeAuthProof(request.fields.account, request.fields.clientSeed, m_seed,
                                  lookup.sessionKey, serverProof);

        // CRYPTO_memcmp inside ProofEquals, not memcmp: a short-circuiting compare
        // leaks how many leading proof bytes an attacker guessed right.
        if (!MopAuth::ProofEquals(serverProof, request.fields.digest))
        {
            DEBUG_LOG("proto: bad login proof for account '%s' from %s.",
                      request.fields.account.c_str(), m_address.c_str());
            RejectAuth(AuthStatus::Failed);
            return false;
        }

        // Everything fallible about the crypt (HMAC, ARC4 keying, drop-1024) happens
        // in Prepare(), BEFORE the world allocates anything -- mirrors
        // WorldSocket::HandleAuthSession's Prepare()/Activate() split exactly.
        if (!m_crypt.Prepare(lookup.sessionKey))
        {
            sLog.outError("proto: crypt could not be prepared for account '%s' from %s.",
                          request.fields.account.c_str(), m_address.c_str());
            RejectAuth(AuthStatus::SystemError);
            return false;
        }

        std::shared_ptr<IClientLink> link =
            std::static_pointer_cast<ClientConnection>(shared_from_this());

        // Hazard H3, resolved: Activate() must NOT run here. It runs only from
        // CommitCrypt, which the gateway is contractually required to forward into
        // World::AddSession(session, commit, context) verbatim (see
        // IWorldGateway::Attach()'s doc comment) -- so it executes while the
        // add-queue lock is held, exactly where WorldSocket::CommitAuthenticatedSession
        // used to run, and only once the session is genuinely about to be published.
        // Every fallible step (the DB loads inside Attach(), the queue insertion
        // inside AddSession) has already happened by the time commit is reachable,
        // so no failure path -- including this one, on INVALID_SESSION_ID -- can
        // observe an activated crypt.
        const SessionId session = m_gateway.Attach(request, link, lookup.context,
                                                   &ClientConnection::CommitCrypt, this);
        if (session == INVALID_SESSION_ID)
        {
            RejectAuth(AuthStatus::SystemError);
            return false;
        }

        m_session = session;

        DEBUG_LOG("proto: account '%s' authenticated from %s.",
                  request.fields.account.c_str(), m_address.c_str());
        return true;
    }

    void ClientConnection::CommitCrypt(void* ctx) noexcept
    {
        ClientConnection* const self = static_cast<ClientConnection*>(ctx);
        self->m_crypt.Activate();
        self->m_connState = MopHs::CONN_AUTHED;
    }

    bool ClientConnection::HandlePing(WorldPacket& packet)
    {
        // 5.4.8 reverses the 3.3.5 field order: latency is on the wire FIRST,
        // then the sequence. Read from Wow.exe 18414, not assumed:
        //
        //   sub_798727  ping sender. The per-stream struct is
        //               connection+0x4560 + index*0x58. It stamps the send
        //               time at struct+8 and takes the sequence from
        //               ++struct+0, storing it in the message at +0x14, with
        //               the latency sample at +0x18.
        //   sub_66F4D7  writes opcode 18 (0x12, CMSG_PING), then calls
        //   sub_66F403  which writes *(this+6) BEFORE *(this+5) - byte
        //               offsets 0x18 then 0x14, so latency precedes sequence.
        //
        // Reading these the 3.3.5 way echoes the latency back as the sequence.
        // The client compares it against its current sequence at 0x79976C and
        // logs "Received pong with old sequence" at 0x799770 - "old" rather
        // than "bad" because a millisecond latency is a small number, so it
        // reads as an earlier sequence rather than as garbage.
        //
        // It also reported the sequence to the gateway as if it were a latency
        // measurement, which feeds movement timestamp adjustment
        // (MovementHandler.cpp) as well as the exposed latency figure.
        uint32 latency = 0;
        uint32 ping = 0;
        packet >> latency;
        packet >> ping;

        // WorldSocket.cpp:1594-1628's 27-second overspeed-ping window.
        static const std::chrono::seconds MIN_PING_INTERVAL(27);
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

        if (m_hadPing)
        {
            if (now - m_lastPing < MIN_PING_INTERVAL)
            {
                ++m_fastPingRun;
            }
            else
            {
                m_fastPingRun = 0;
            }
        }
        m_lastPing = now;
        m_hadPing = true;

        if (!m_gateway.OnPing(m_session, latency, m_fastPingRun))
        {
            return false;
        }

        WorldPacket pong(SMSG_PONG, 4);
        pong << ping;
        SendPacket(pong);
        return true;
    }

    void ClientConnection::RejectAuth(AuthStatus status)
    {
        // The sole SMSG_AUTH_RESPONSE construction site on the rejection path, via
        // the gateway's game-side MopAuth::BuildAuthResponseError() serializer.
        //
        // Hazard H4, RESOLVED (read from the engine source, not assumed): a bare
        // close must never race ahead of this response reaching the wire, and both
        // net:: backends already guarantee that on their own --
        //   - ReactorServer: onData()'s caller checks `session->closed()`, and if
        //     `channel->out` is non-empty (this packet, already queued below) sets
        //     closeAfterDrain instead of tearing the connection down immediately;
        //     drainSendRequests()/the EvWrite path only call closeConn() once the
        //     buffer has actually emptied via a real send().
        //   - IocpServer: handleRecv()/handleSend() run the identical
        //     `closed() && channel->out.empty()` check before markDead(); while
        //     false they keep the connection alive (re-posting a receive, in
        //     handleRecv()'s case) so a completion exists to carry the eventual
        //     teardown.
        // That is WorldSocket-era BeginAuthErrorDrain's entire job -- cancel_wakeup,
        // MopSocketDrain's Flushing state, ScopedSendInFlight, the Update() snapshot
        // -- done by the transport itself. None of those ACE_TP_Reactor-specific
        // mechanics have a counterpart here (there is no suspend/resume dispatch
        // window to race), so they are not ported; MopSocketDrain.h and its unit
        // tests are retired in this same commit rather than kept as dead code
        // pretending to guard a property the engine now guards on its own.
        //
        // What does NOT come for free, and IS still this class's job: refusing to
        // act on further input from a peer already rejected while its response
        // drains (onData()'s closed() guard) -- the property MopSock::MayProcessInput
        // used to express, now checked directly rather than through a dedicated
        // state enum.
        WorldPacket packet;
        m_gateway.BuildAuthErrorResponse(status, packet);
        SendPacket(packet);
    }

    bool ClientConnection::DecryptHeaderHook(void* ctx, uint8_t* header, size_t len)
    {
        ClientConnection* const self = static_cast<ClientConnection*>(ctx);

        // Pre-crypt frames are not decrypted at all -- the reader must not treat that
        // as a failure (mirrors WorldSocket::DecryptHeaderHook exactly).
        if (!self->m_crypt.IsInitialized())
        {
            return true;
        }

        return self->m_crypt.DecryptRecv(header, len);
    }

    bool ClientConnection::CmdValidHook(void* /*ctx*/, uint32_t cmd, bool preCrypt)
    {
        if (!preCrypt)
        {
            return true;
        }
        return cmd == MSG_WOW_CONNECTION || cmd < OPCODE_TABLE_SIZE;
    }

    void ClientConnection::SendPacket(const WorldPacket& packet)
    {
        if (m_closed.load(std::memory_order_acquire) || !m_sender)
        {
            return;
        }

        bool fatal = false;
        std::vector<uint8_t> wire = EncodeForSend(packet, fatal);
        if (fatal)
        {
            Close();
            return;
        }
        if (wire.empty())
        {
            return;   // dropped: opcode not framable in MoP (hazard H2)
        }

        m_sender(wire.data(), wire.size());
    }

    void ClientConnection::Close()
    {
        m_closed.store(true, std::memory_order_release);
        if (m_closer)
        {
            m_closer();
        }
    }

    void ClientConnection::onClose()
    {
        m_closed.store(true, std::memory_order_release);

        if (m_session != INVALID_SESSION_ID)
        {
            m_gateway.Detach(m_session);
            m_session = INVALID_SESSION_ID;
        }
    }
}
