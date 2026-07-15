/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file WorldSocket.cpp
 * @brief World server network socket implementation
 *
 * This file implements WorldSocket which handles individual client
 * connections to the world server. It manages:
 *
 * - TCP socket communication using ACE
 * - Packet encryption/decryption (SRP6)
 * - Packet fragmentation and reassembly
 * - Ping/pong keepalive mechanism
 * - Session creation and management
 *
 * The socket uses the SRP6 authentication protocol for secure
 * client-server communication.
 *
 * @see WorldSocket for the socket class
 * @see WorldSession for the player session
 * @see WorldSocketMgr for the socket manager
 */

#include <ace/Message_Block.h>
#include <ace/OS_NS_string.h>
#include <ace/OS_NS_unistd.h>
#include <ace/os_include/arpa/os_inet.h>
#include <ace/os_include/netinet/os_tcp.h>
#include <ace/os_include/sys/os_types.h>
#include <ace/os_include/sys/os_socket.h>
#include <ace/OS_NS_string.h>
#include <ace/Reactor.h>
#include <ace/Auto_Ptr.h>

#include <cstdio>

#include "WorldSocket.h"
#include "Common.h"
#include "MopWireCodec.h"
#include "MopAuthSession.h"

#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "ByteBuffer.h"
#include "Opcodes.h"
#include "Database/DatabaseEnv.h"
#include "Auth/Sha1.h"
#include "WorldSession.h"
#include "WorldSocketMgr.h"
#include "Log.h"
#include "DBCStores.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#include <openssl/rand.h>

/**
 * @brief WorldSocket constructor
 *
 * Initializes a new client socket with default values:
 * - Last ping time: zero
 * - Overspeed pings: 0
 * - Session: NULL
 * - Output buffer size: 64KB
 * - Random seed for encryption
 */
WorldSocket::WorldSocket(void) :
    WorldHandler(),
    m_LastPingTime(ACE_Time_Value::zero),
    m_OverSpeedPings(0),
    m_Session(0),
    m_OutBuffer(0),
    m_OutBufferSize(65536),
    m_OutActive(false),
    m_Seed(0),
    m_connState(MopHs::CONN_GREETING),
    m_frameReader(),
    m_lastDecodeLog(ACE_Time_Value::zero)
{
    reference_counting_policy().value(ACE_Event_Handler::Reference_Counting_Policy::ENABLED);

    msg_queue()->high_water_mark(8 * 1024 * 1024);
    msg_queue()->low_water_mark(8 * 1024 * 1024);
}

/**
 * @brief WorldSocket destructor
 *
 * Cleans up the receive packet and any allocated resources.
 */
WorldSocket::~WorldSocket(void)
{
    if (m_OutBuffer)
    {
        m_OutBuffer->release();
    }

    closing_ = true;

    peer().close();
}

/**
 * @brief Checks whether the socket is already marked as closing.
 *
 * @return true if the socket is closed or closing; otherwise false.
 */
bool WorldSocket::IsClosed(void) const
{
    return closing_;
}

/**
 * @brief Begins graceful shutdown of the socket writer side.
 */
void WorldSocket::CloseSocket(void)
{
    ACE_GUARD(LockType, Guard, m_OutBufferLock);

    if (closing_)
    {
        return;
    }

    closing_ = true;
    peer().close_writer();

    m_Session = NULL;
}

/**
 * @brief Gets the cached remote address string for this connection.
 *
 * @return const std::string& The remote address.
 */
const std::string& WorldSocket::GetRemoteAddress(void) const
{
    return m_Address;
}

/**
 * @brief Sends a packet immediately or queues it for later flush.
 *
 * @param pct The packet to send.
 * @return int Zero on success; otherwise -1.
 */
int WorldSocket::SendPacket(const WorldPacket& pct, bool* sent)
{
    if (sent)
    {
        *sent = false;
    }

    ACE_GUARD_RETURN(LockType, Guard, m_OutBufferLock, -1);

    if (closing_)
    {
        return -1;
    }

    // Dump outgoing packet (opt-in via PacketLoggingEnabled; off by default).
    // SMSG_AUTH_RESPONSE is auth-adjacent and must never have its body logged
    // (defensive for Phase 3, when this response starts carrying auth material).
    if (sLog.IsPacketLoggingEnabled())
    {
        if (pct.GetOpcode() == SMSG_AUTH_RESPONSE)
        {
            sLog.outWorldPacketDumpRedacted(uint32(get_handle()), pct.GetOpcode(), LookupOpcodeName(DIR_SERVER, pct.GetOpcode()), pct.size(), false);
        }
        else
        {
            sLog.outWorldPacketDump(uint32(get_handle()), pct.GetOpcode(), LookupOpcodeName(DIR_SERVER, pct.GetOpcode()), &pct, false);
        }
    }

#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        if (!e->OnPacketSend(m_Session, pct))
        {
            if (sent)
            {
                *sent = false;
            }
            return 0;
        }
    }
#endif

    uint8 header[4];
    const bool postCrypt = m_Crypt.IsInitialized();
    if (!MopWire::BuildServerHeader(postCrypt, pct.size(), pct.GetOpcode(), header))
    {
        sLog.outError("WorldSocket::SendPacket: frame out of range (size=%zu opcode=0x%.4X postCrypt=%d)",
                      pct.size(), pct.GetOpcode(), int(postCrypt));
        return -1;
    }
    if (postCrypt)
    {
        m_Crypt.EncryptSend(header, sizeof(header));
    }

    if (m_OutBuffer->space() >= pct.size() + sizeof(header) && msg_queue()->is_empty())
    {
        // Put the packet on the buffer.
        if (m_OutBuffer->copy((char*) header, sizeof(header)) == -1)
        {
            MANGOS_ASSERT(false);
        }

        if (!pct.empty())
        {
            if (m_OutBuffer->copy((char*) pct.contents(), pct.size()) == -1)
            {
                MANGOS_ASSERT(false);
            }
        }
    }
    else
    {
        // Enqueue the packet.
        ACE_Message_Block* mb;

        ACE_NEW_RETURN(mb, ACE_Message_Block(pct.size() + sizeof(header)), -1);

        mb->copy((char*) header, sizeof(header));

        if (!pct.empty())
        {
            mb->copy((const char*)pct.contents(), pct.size());
        }

        if (msg_queue()->enqueue_tail(mb, (ACE_Time_Value*)&ACE_Time_Value::zero) == -1)
        {
            sLog.outError("WorldSocket::SendPacket enqueue_tail");
            mb->release();
            return -1;
        }
    }

    if (sent)
    {
        *sent = true;
    }
    return 0;
}

/**
 * @brief Increments the socket reference count.
 *
 * @return long The new reference count.
 */
long WorldSocket::AddReference(void)
{
    return static_cast<long>(add_reference());
}

/**
 * @brief Decrements the socket reference count.
 *
 * @return long The new reference count.
 */
long WorldSocket::RemoveReference(void)
{
    return static_cast<long>(remove_reference());
}

/**
 * @brief Opens the socket handler and sends the connection greeting; the auth challenge is sent later via HandleWowConnection -> SendAuthChallenge().
 *
 * @param a The ACE open hook parameter.
 * @return int Zero on success; otherwise -1.
 */
int WorldSocket::open(void* a)
{
    ACE_UNUSED_ARG(a);

    // Prevent double call to this func.
    if (m_OutBuffer)
    {
        return -1;
    }

    // This will also prevent the socket from being Updated
    // while we are initializing it.
    m_OutActive = true;

    // Hook for the manager.
    if (sWorldSocketMgr->OnSocketOpen(this) == -1)
    {
        return -1;
    }

    // Allocate the buffer.
    ACE_NEW_RETURN(m_OutBuffer, ACE_Message_Block(m_OutBufferSize), -1);

    // Store peer address.
    ACE_INET_Addr remote_addr;

    if (peer().get_remote_addr(remote_addr) == -1)
    {
        sLog.outError("WorldSocket::open: peer ().get_remote_addr errno = %s", ACE_OS::strerror(errno));
        return -1;
    }

    m_Address = remote_addr.get_host_addr();

    std::string ServerToClient = "RLD OF WARCRAFT CONNECTION - SERVER TO CLIENT";
    WorldPacket data(MSG_WOW_CONNECTION,46);

    data << ServerToClient;

    if (SendPacket(data) == -1)
    {
        return -1;
    }

    // Register with ACE Reactor
    if (reactor()->register_handler(this, ACE_Event_Handler::READ_MASK | ACE_Event_Handler::WRITE_MASK) == -1)
    {
        sLog.outError("WorldSocket::open: unable to register client handler errno = %s", ACE_OS::strerror(errno));
        return -1;
    }

    // reactor takes care of the socket from now on
    remove_reference();

    return 0;
}

int WorldSocket::HandleWowConnection(WorldPacket& recvPacket)
{
    std::string ClientToServerMsg;
    recvPacket >> ClientToServerMsg;
    DEBUG_LOG("Received MSG_WOW_CONNECTION FROM %s", m_Session ? m_Session->GetRemoteAddress().c_str() : "<unk>");
    // Mirror of the greeting we send ("RLD OF WARCRAFT CONNECTION - SERVER TO CLIENT"): the leading
    // "WO" of "WORLD" is carried by the header's cmd field (0x4F57 == "WO" little-endian), so the
    // payload legitimately begins at "RLD".
    // NOTE: this previously expected "D OF WARCRAFT ..." -- that constant was compensating for the
    // Cata-era 6-byte {uint16 size; uint32 cmd} read of the greeting, which swallowed "WORL" into cmd
    // and left the payload starting at 'D' (the garbage cmd then truncated to 0x4F57 and dispatched
    // here by accident). With the greeting parsed at its true 4-byte width, the payload is "RLD...".
    if (strcmp(ClientToServerMsg.c_str(), "RLD OF WARCRAFT CONNECTION - CLIENT TO SERVER") != 0)
    {
        sLog.outError("WorldSocket::ProcessIncoming: received wrong data in MSG_WOW_CONNECTION.");
        return -1;
    }

    return SendAuthChallenge();
}

static bool DefaultRandomBytes(uint8_t* out, size_t len) { return RAND_bytes(out, int(len)) == 1; }

int WorldSocket::SendAuthChallenge()
{
    DEBUG_LOG("Sending SMSG_AUTH_CHALLENGE");
    std::vector<uint8_t> payload; uint32 seed = 0;
    if (!MopHs::BuildAuthChallengePayload(&DefaultRandomBytes, payload, seed))
    {
        sLog.outError("WorldSocket::SendAuthChallenge: CSPRNG failure; closing.");
        return -1;                                            // nothing sent
    }
    WorldPacket packet(SMSG_AUTH_CHALLENGE, uint32(payload.size()));
    packet.append(payload.data(), payload.size());
    bool sent = false;
    if (SendPacket(packet, &sent) == -1)
    {
        return -1;
    }
    if (!sent)
    {
        sLog.outError("WorldSocket::SendAuthChallenge: challenge was suppressed (not sent); closing.");
        return -1;                                            // never advance state on a challenge the client never got
    }
    m_Seed = seed;                                            // only after the challenge is actually on the wire
    m_connState = MopHs::CONN_CHALLENGED;                     // member exists since Task 2.1
    return 0;
}

void WorldSocket::SendAuthResponseError(uint8 code)
{
    WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
    packet.WriteBit(0); // has account info
    packet.WriteBit(0); // has queue info
    packet << uint8(code);
    SendPacket(packet);
}

/**
 * @brief Closes the socket and releases its final ACE reference.
 *
 * @return int Always returns zero.
 */
int WorldSocket::close(int)
{
    shutdown();

    closing_ = true;

    remove_reference();

    return 0;
}

/**
 * @brief Processes readable socket input.
 *
 * @return int Zero on success; otherwise -1.
 */
int WorldSocket::handle_input(ACE_HANDLE)
{
    if (closing_)
    {
        return -1;
    }

    switch (handle_input_missing_data())
    {
        case -1 :
        {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
            {
                return Update();                            // interesting line ,isn't it ?
            }

            DEBUG_LOG("WorldSocket::handle_input: Peer error closing connection errno = %s", ACE_OS::strerror(errno));

            errno = ECONNRESET;
            return -1;
        }
        case 0:
        {
            DEBUG_LOG("WorldSocket::handle_input: Peer has closed connection");

            errno = ECONNRESET;
            return -1;
        }
        case 1:
            return 1;
        default:
            return Update();                                // another interesting line ;)
    }

    ACE_NOTREACHED(return -1);
}

/**
 * @brief Flushes pending outbound socket data.
 *
 * @return int Zero on success; otherwise -1.
 */
int WorldSocket::handle_output(ACE_HANDLE)
{
    ACE_GUARD_RETURN(LockType, Guard, m_OutBufferLock, -1);

    if (closing_)
    {
        return -1;
    }

    const size_t send_len = m_OutBuffer->length();

    if (send_len == 0)
    {
        return handle_output_queue(Guard);
    }

#ifdef MSG_NOSIGNAL
    ssize_t n = peer().send(m_OutBuffer->rd_ptr(), send_len, MSG_NOSIGNAL);
#else
    ssize_t n = peer().send(m_OutBuffer->rd_ptr(), send_len);
#endif // MSG_NOSIGNAL

    if (n == 0)
    {
        return -1;
    }
    else if (n == -1)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            return schedule_wakeup_output(Guard);
        }

        return -1;
    }
    else if (n < (ssize_t)send_len) // now n > 0
    {
        m_OutBuffer->rd_ptr(static_cast<size_t>(n));

        // move the data to the base of the buffer
        m_OutBuffer->crunch();

        return schedule_wakeup_output(Guard);
    }
    else // now n == send_len
    {
        m_OutBuffer->reset();

        return handle_output_queue(Guard);
    }

    ACE_NOTREACHED(return 0);
}

int WorldSocket::handle_output_queue(GuardType& g)
{
    if (msg_queue()->is_empty())
    {
        return cancel_wakeup_output(g);
    }

    ACE_Message_Block* mblk;

    if (msg_queue()->dequeue_head(mblk, (ACE_Time_Value*)&ACE_Time_Value::zero) == -1)
    {
        sLog.outError("WorldSocket::handle_output_queue dequeue_head");
        return -1;
    }

    const size_t send_len = mblk->length();

#ifdef MSG_NOSIGNAL
    ssize_t n = peer().send(mblk->rd_ptr(), send_len, MSG_NOSIGNAL);
#else
    ssize_t n = peer().send(mblk->rd_ptr(), send_len);
#endif // MSG_NOSIGNAL

    if (n == 0)
    {
        mblk->release();

        return -1;
    }
    else if (n == -1)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            msg_queue()->enqueue_head(mblk, (ACE_Time_Value*) &ACE_Time_Value::zero);
            return schedule_wakeup_output(g);
        }

        mblk->release();
        return -1;
    }
    else if (n < (ssize_t)send_len) // now n > 0
    {
        mblk->rd_ptr(static_cast<size_t>(n));

        if (msg_queue()->enqueue_head(mblk, (ACE_Time_Value*) &ACE_Time_Value::zero) == -1)
        {
            sLog.outError("WorldSocket::handle_output_queue enqueue_head");
            mblk->release();
            return -1;
        }

        return schedule_wakeup_output(g);
    }
    else // now n == send_len
    {
        mblk->release();

        return msg_queue()->is_empty() ? cancel_wakeup_output(g) : ACE_Event_Handler::WRITE_MASK;
    }

    ACE_NOTREACHED(return -1);
}

/**
 * @brief Handles socket closure and unregisters the handler from the reactor.
 *
 * @param h The closing handle.
 * @return int Always returns zero.
 */
int WorldSocket::handle_close(ACE_HANDLE h, ACE_Reactor_Mask)
{
    // Critical section
    {
        ACE_GUARD_RETURN(LockType, Guard, m_OutBufferLock, -1);

        closing_ = true;

        if (h == ACE_INVALID_HANDLE)
        {
            peer().close_writer();
        }
    }

    // Critical section
    {
        ACE_GUARD_RETURN(LockType, Guard, m_SessionLock, -1);

        m_Session = NULL;
    }

    reactor()->remove_handler(this, ACE_Event_Handler::DONT_CALL | ACE_Event_Handler::ALL_EVENTS_MASK);
    return 0;
}

int WorldSocket::Update(void)
{
    if (closing_)
    {
        return -1;
    }

    if (m_OutActive || (m_OutBuffer->length() == 0 && msg_queue()->is_empty()))
    {
        return 0;
    }

    int ret;
    do
        ret = handle_output(get_handle());
    while (ret > 0);

    return ret;
}

/**
 * @brief Receives socket data and drains as many complete frames as are buffered.
 *
 * Delegates two-phase header + payload reassembly to MopFrameReader (consume-one-frame
 * reader). postCrypt is passed per call as m_Crypt.IsInitialized() - the sole codec
 * authority - so a crypt Init() mid-stream (Phase 3 auth) is picked up automatically on
 * the very next TryFrame() with no separate flag to keep in sync.
 *
 * @return int A receive state code (ACE contract: 0 peer-closed, -1 error, 1 full read,
 *             2 partial read), or -1 on error.
 */
int WorldSocket::handle_input_missing_data(void)
{
    char buf[4096];
    const ssize_t n = peer().recv(buf, sizeof(buf));
    if (n <= 0)
    {
        return (int)n;
    }

    m_frameReader.Push((const uint8*)buf, size_t(n));

    MopFrameReader::Frame frame;
    for (;;)
    {
        // Header width is state-driven, derived fresh every frame (no stored codec flag):
        //   crypt live            -> 4-byte packed
        //   still awaiting greet  -> 4-byte {size, cmd16}; the client's MSG_WOW_CONNECTION reply is
        //                            symmetric with the greeting we sent (size = payload + 2)
        //   otherwise (pre-crypt) -> 6-byte {size, cmd32}; CMSG_AUTH_SESSION onward (size = payload + 4)
        const MopFrameReader::HeaderKind kind =
            m_Crypt.IsInitialized()             ? MopFrameReader::HDR_POSTCRYPT :
            (m_connState == MopHs::CONN_GREETING) ? MopFrameReader::HDR_GREETING :
                                                    MopFrameReader::HDR_PRECRYPT;
        const MopFrameReader::Status s =
            m_frameReader.TryFrame(frame, kind, this, &DecryptHeaderHook, &CmdValidHook);

        if (s == MopFrameReader::NEED_MORE)
        {
            break;
        }

        if (s == MopFrameReader::MALFORMED)
        {
            const ACE_Time_Value now = ACE_OS::gettimeofday();
            if (MopHs::RateLimitElapsed(m_lastDecodeLog.sec(), now.sec()))
            {
                m_lastDecodeLog = now;
                size_t hl = 0;
                bool preCrypt = false;
                const uint8* hb = m_frameReader.LastHeader(hl, preCrypt);   // <=6 HEADER bytes; never payload
                char hex[6 * 3 + 1];
                for (size_t i = 0; i < hl; ++i)
                {
                    snprintf(hex + i * 3, 4, "%02X ", hb[i]);
                }
                hex[hl ? hl * 3 - 1 : 0] = '\0';
                sLog.outError("WorldSocket: malformed frame from %s [%s reason=%s header=%s]; closing.",
                              GetRemoteAddress().c_str(), preCrypt ? "pre-crypt" : "post-crypt",
                              MalformedReasonName(m_frameReader.LastReason()), hex);
            }
            errno = EINVAL;
            return -1;
        }

        WorldPacket* pct = NULL;
        ACE_NEW_RETURN(pct, WorldPacket(OpcodesList(uint16(frame.cmd)), uint32(frame.payload.size())), -1);
        if (!frame.payload.empty())
        {
            pct->append(frame.payload.data(), frame.payload.size());
        }

        if (ProcessIncoming(pct) == -1)
        {
            errno = EINVAL;
            return -1;
        }
        // Phase 3: after crypt Init in the auth path, the NEXT TryFrame sees IsInitialized()==true (no manual flag).
    }

    return (size_t(n) == sizeof(buf)) ? 1 : 2;   // preserve ACE contract: full read -> 1, partial -> 2
}

/// MopFrameReader::DecryptFn hook (Phase 2 wire framing): in-place ARC4 on the header only.
bool WorldSocket::DecryptHeaderHook(void* ctx, uint8* header, size_t len)
{
    static_cast<WorldSocket*>(ctx)->m_Crypt.DecryptRecv(header, len);
    return true;   // no-op pre-crypt (m_Crypt.DecryptRecv() is itself a no-op before Init())
}

/// MopFrameReader::CmdValidFn hook (Phase 2 wire framing): game rule; false rejects the frame.
bool WorldSocket::CmdValidHook(void* ctx, uint32 cmd, bool preCrypt)
{
    if (!preCrypt)
    {
        return true;
    }
    return cmd == MSG_WOW_CONNECTION || cmd < OPCODE_TABLE_SIZE;
}

int WorldSocket::cancel_wakeup_output(GuardType& g)
{
    if (!m_OutActive)
    {
        return 0;
    }

    m_OutActive = false;

    g.release();

    if (reactor()->cancel_wakeup
        (this, ACE_Event_Handler::WRITE_MASK) == -1)
    {
        // would be good to store errno from reactor with errno guard
        sLog.outError("WorldSocket::cancel_wakeup_output");
        return -1;
    }

    return 0;
}

int WorldSocket::schedule_wakeup_output(GuardType& g)
{
    if (m_OutActive)
    {
        return 0;
    }

    m_OutActive = true;

    g.release();

    if (reactor()->schedule_wakeup
        (this, ACE_Event_Handler::WRITE_MASK) == -1)
    {
        sLog.outError("WorldSocket::schedule_wakeup_output");
        return -1;
    }

    return 0;
}

/**
 * @brief Dispatches a fully assembled incoming packet.
 *
 * @param new_pct The packet to process.
 * @return int Zero on success; otherwise -1.
 */
int WorldSocket::ProcessIncoming(WorldPacket* new_pct)
{
    MANGOS_ASSERT(new_pct);

    // manage memory ;)
    ACE_Auto_Ptr<WorldPacket> aptr(new_pct);

    const uint16 opcode = new_pct->GetOpcode();

    if (closing_)
    {
        return -1;
    }

    // Dump received packet (opt-in via PacketLoggingEnabled; off by default).
    // CMSG_AUTH_SESSION carries the client's auth proof and must never have its body logged.
    if (sLog.IsPacketLoggingEnabled())
    {
        if (opcode == CMSG_AUTH_SESSION)
        {
            sLog.outWorldPacketDumpRedacted(uint32(get_handle()), opcode, LookupOpcodeName(DIR_CLIENT, opcode), new_pct->size(), true);
        }
        else
        {
            sLog.outWorldPacketDump(uint32(get_handle()), opcode, LookupOpcodeName(DIR_CLIENT, opcode), new_pct, true);
        }
    }

    // Handshake state legality allowlist. MUST run before the MSG_WOW_CONNECTION early-return below,
    // otherwise a client could resend the greeting after already being CONN_CHALLENGED (duplicate-greeting
    // bypass). OPC_NORMAL is AUTHED-only; Phase 2 never reaches CONN_AUTHED, so all non-handshake opcodes
    // are rejected pre-auth (prevents pre-auth socket pinning). Phase 3 sets CONN_AUTHED and opens that gate.
    const MopHs::OpcodeClass cls =
        (opcode == MSG_WOW_CONNECTION) ? MopHs::OPC_GREETING :
        (opcode == CMSG_AUTH_SESSION)  ? MopHs::OPC_AUTH_SESSION : MopHs::OPC_NORMAL;
    if (!MopHs::IsHandshakeOpcodeLegal(m_connState, cls))
    {
        sLog.outError("WorldSocket::ProcessIncoming: opcode 0x%.4X illegal in state %d from %s; closing.",
                      opcode, int(m_connState), GetRemoteAddress().c_str());
        return -1;
    }

    // Greeting is out-of-band: 0x4F57 is outside the 13-bit table, resolved by name only.
    if (opcode == MSG_WOW_CONNECTION)
    {
        return HandleWowConnection(*new_pct);
    }

    // Phase 1a registers only the login closure in clientOpcodeTable, so a closure-based reject here
    // would hard-close the socket for every non-closure client opcode (movement, char management, ...).
    // Do NOT gate on the client table: hand non-closure opcodes to the switch below, whose default case
    // queues them to WorldSession, where the bounded LookupClientOpcode() drops unregistered opcodes
    // gracefully (no disconnect, no out-of-range index). Phase 1b reinstates a table-based inbound guard
    // once all client opcodes live within OPCODE_TABLE_SIZE. Mirrors the outbound SendPacket fix: no
    // faithful table-based guard exists in Phase 1a.

    try
    {
        switch (opcode)
        {
            case CMSG_PING:
                return HandlePing(*new_pct);
            case CMSG_AUTH_SESSION:
                // Phase 2: framing proven. Do NOT enter the legacy auth path (HandleAuthSession -> m_Crypt.Init).
                // The allowlist above guarantees we are in CONN_CHALLENGED here, so this is the LEGAL transition.
                // The ENABLE_ELUNA OnPacketReceive hook for this opcode is intentionally NOT invoked here;
                // it is deferred to Phase 3 along with the rest of the auth path.
                m_connState = MopHs::CONN_AUTHENTICATING;
                DEBUG_LOG("WorldSocket: CMSG_AUTH_SESSION accepted; CONN_CHALLENGED -> CONN_AUTHENTICATING; auth deferred to Phase 3 (framing OK, len %zu, body redacted); closing.", new_pct->size());
                return -1;
            case CMSG_KEEP_ALIVE:
                DEBUG_LOG("CMSG_KEEP_ALIVE ,size: %zu ", new_pct->size());

#ifdef ENABLE_ELUNA
                if (Eluna* e = sWorld.GetEluna())
                {
                    e->OnPacketReceive(m_Session, *new_pct);
                }
#endif /* ENABLE_ELUNA */
                return 0;
            case CMSG_LOG_DISCONNECT:
                new_pct->rfinish();                                  // uint32 disconnect reason; socket notification, not logout
#ifdef ENABLE_ELUNA
                if (Eluna* e = sWorld.GetEluna())
                {
                    e->OnPacketReceive(m_Session, *new_pct);
                }
#endif
                return 0;
            default:
            {
                ACE_GUARD_RETURN(LockType, Guard, m_SessionLock, -1);

                if (m_Session != NULL)
                {
                    // OK ,give the packet to WorldSession
                    aptr.release();
                    // WARNING here we call it with locks held.
                    // Its possible to cause deadlock if QueuePacket calls back
                    m_Session->QueuePacket(new_pct);
                    return 0;
                }
                else
                {
                    sLog.outError("WorldSocket::ProcessIncoming: Client not authed opcode = %u", uint32(opcode));
                    return -1;
                }
            }
        }
    }
    catch (ByteBufferException&)
    {
        // Bounded + rate-limited: shares m_lastDecodeLog with the Task-2.5 malformed-frame
        // path so every decode-failure path is rate-limited together. CMSG_AUTH_SESSION never
        // has its body logged, even on error; other opcodes log at most the first 64 bytes.
        const ACE_Time_Value now = ACE_OS::gettimeofday();
        if (MopHs::RateLimitElapsed(m_lastDecodeLog.sec(), now.sec()))
        {
            m_lastDecodeLog = now;
            if (opcode == CMSG_AUTH_SESSION)
            {
                sLog.outError("WorldSocket: CMSG_AUTH_SESSION decode failed from %s (body redacted).", GetRemoteAddress().c_str());
            }
            else
            {
                const size_t cap = 64;
                const size_t dumpLen = new_pct->size() < cap ? new_pct->size() : cap;
                if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
                {
                    char hex[cap * 3 + 1];
                    for (size_t i = 0; i < dumpLen; ++i)
                    {
                        snprintf(hex + i * 3, 4, "%02X ", new_pct->contents()[i]);
                    }
                    hex[dumpLen ? dumpLen * 3 - 1 : 0] = '\0';
                    sLog.outError("WorldSocket: decode failure opcode 0x%.4X (%s) from %s; first %zu bytes: %s",
                                  opcode, LookupOpcodeName(DIR_CLIENT, opcode), GetRemoteAddress().c_str(), dumpLen, hex);
                }
                else
                {
                    sLog.outError("WorldSocket: decode failure opcode 0x%.4X (%s) from %s.",
                                  opcode, LookupOpcodeName(DIR_CLIENT, opcode), GetRemoteAddress().c_str());
                }
            }
        }

        if (sWorld.getConfig(CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET))
        {
            DETAIL_LOG("Disconnecting session [account id %i / address %s] for badly formatted packet.",
                       m_Session ? m_Session->GetAccountId() : -1, GetRemoteAddress().c_str());

            return -1;
        }
        else
        {
            return 0;
        }
    }

    ACE_NOTREACHED(return 0);
}

/**
 * @brief Authenticates a new client session and creates the world session.
 *
 * @param recvPacket The authentication packet.
 * @return int Zero on success; otherwise -1.
 */
int WorldSocket::HandleAuthSession(WorldPacket& recvPacket)
{
    // NOTE: ATM the socket is singlethread, have this in mind ...
    uint8 digest[20];
    uint32 clientSeed, id, security;
    uint16 BuiltNumberClient;
    uint8 expansion = 0;
    LocaleConstant locale;
    std::string account;
    Sha1Hash sha1;
    BigNumber v, s, g, N, K;

    // Read the content of the packet.
    // The read order, the scattered digest[] indices and the missing leading ReadBit() before
    // ReadBits(11) all now live in MopAuth::DecodeAuthSession, which reproduces them bug-for-bug
    // while bounding every read. See MopAuthSession.h.
    // NOTE: named authFields, not fields: a 'Field* fields' DB row local already exists below.
    MopAuth::AuthSessionFields authFields{};
    MopAuth::DecodeResult const decodeResult = MopAuth::DecodeAuthSession(recvPacket, authFields);
    if (decodeResult != MopAuth::DecodeResult::Ok)
    {
        // TODO(Task 3): replace with the drain helper + AUTH_FAILED response.
        sLog.outError("WorldSocket::HandleAuthSession: malformed auth session body (result %u).",
                      static_cast<uint32>(decodeResult));
        return -1;
    }

    memcpy(digest, authFields.digest, sizeof(digest));
    clientSeed = authFields.clientSeed;
    BuiltNumberClient = authFields.builtNumberClient;
    account = authFields.account;

    ByteBuffer addonsData;
    if (!authFields.addonData.empty())
    {
        addonsData.append(authFields.addonData.data(), authFields.addonData.size());
    }

    DEBUG_LOG("WorldSocket::HandleAuthSession: client build %u, account %s, clientseed %X",
              BuiltNumberClient,
              account.c_str(),
              clientSeed);

    // Check the version of client trying to connect
    if (!IsAcceptableClientBuild(BuiltNumberClient))
    {
        SendAuthResponseError(AUTH_VERSION_MISMATCH);

        sLog.outError("WorldSocket::HandleAuthSession: Sent Auth Response (version mismatch).");
        return -1;
    }

    // Get the account information from the realmd database
    std::string safe_account = account; // Duplicate, else will screw the SHA hash verification below
    LoginDatabase.escape_string(safe_account);
    // No SQL injection, username escaped.

    QueryResult* result =
        LoginDatabase.PQuery("SELECT "
                             "`id`, "                      // 0
                             "`gmlevel`, "                 // 1
                             "`sessionkey`, "              // 2
                             "`last_ip`, "                 // 3
                             "`locked`, "                  // 4
                             "`v`, "                       // 5
                             "`s`, "                       // 6
                             "`expansion`, "               // 7
                             "`mutetime`, "                // 8
                             "`locale` "                   // 9
                             "FROM `account` "
                             "WHERE `username` = '%s'",
                             safe_account.c_str());

    // Stop if the account is not found
    if (!result)
    {
        SendAuthResponseError(AUTH_UNKNOWN_ACCOUNT);

        sLog.outError("WorldSocket::HandleAuthSession: Sent Auth Response (unknown account).");
        return -1;
    }

    Field* fields = result->Fetch();

    expansion = ((sWorld.getConfig(CONFIG_UINT32_EXPANSION) > fields[7].GetUInt8()) ? fields[7].GetUInt8() : sWorld.getConfig(CONFIG_UINT32_EXPANSION));

    N.SetHexStr("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
    g.SetDword(7);

    v.SetHexStr(fields[5].GetString());
    s.SetHexStr(fields[6].GetString());
    m_s = s;

    const char* sStr = s.AsHexStr();                        // Must be freed by OPENSSL_free()
    const char* vStr = v.AsHexStr();                        // Must be freed by OPENSSL_free()

    DEBUG_LOG("WorldSocket::HandleAuthSession: (s,v) check s: %s v: %s",
              sStr,
              vStr);

    OPENSSL_free((void*) sStr);
    OPENSSL_free((void*) vStr);

    ///- Re-check ip locking (same check as in realmd).
    if (fields[4].GetUInt8() == 1)  // if ip is locked
    {
        if (strcmp(fields[3].GetString(), GetRemoteAddress().c_str()))
        {
            SendAuthResponseError(AUTH_FAILED);

            delete result;
            BASIC_LOG("WorldSocket::HandleAuthSession: Sent Auth Response (Account IP differs).");
            return -1;
        }
    }

    id = fields[0].GetUInt32();
    security = fields[1].GetUInt16();
    if (security > SEC_ADMINISTRATOR)                       // prevent invalid security settings in DB
    {
        security = SEC_ADMINISTRATOR;
    }

    K.SetHexStr(fields[2].GetString());

    time_t mutetime = time_t (fields[8].GetUInt64());

    locale = LocaleConstant(fields[9].GetUInt8());
    if (locale >= MAX_LOCALE)
    {
        locale = LOCALE_enUS;
    }

    delete result;

    // Re-check account ban (same check as in realmd)
    QueryResult* banresult =
        LoginDatabase.PQuery("SELECT 1 FROM `account_banned` WHERE `id` = %u AND `active` = 1 AND (`unbandate` > UNIX_TIMESTAMP() OR `unbandate` = `bandate`)"
                             "UNION "
                             "SELECT 1 FROM `ip_banned` WHERE (`unbandate` = `bandate` OR `unbandate` > UNIX_TIMESTAMP()) AND `ip` = '%s'",
                             id, GetRemoteAddress().c_str());

    if (banresult) // if account banned
    {
        SendAuthResponseError(AUTH_BANNED);

        delete banresult;

        sLog.outError("WorldSocket::HandleAuthSession: Sent Auth Response (Account banned).");
        return -1;
    }

    // Check locked state for server
    AccountTypes allowedAccountType = sWorld.GetPlayerSecurityLimit();

    if (allowedAccountType > SEC_PLAYER && AccountTypes(security) < allowedAccountType)
    {
        SendAuthResponseError(AUTH_UNAVAILABLE);

        BASIC_LOG("WorldSocket::HandleAuthSession: User tries to login but his security level is not enough");
        return -1;
    }

    // Phase 3 defers Warden structurally; see auth design §6.11 before re-enabling.

    // Check that Key and account name are the same on client and server
    Sha1Hash sha;

    uint32 t = 0;
    uint32 seed = m_Seed;

    sha.UpdateData(account);
    sha.UpdateData((uint8*) & t, 4);
    sha.UpdateData((uint8*) & clientSeed, 4);
    sha.UpdateData((uint8*) & seed, 4);
    sha.UpdateBigNumbers(&K, NULL);
    sha.Finalize();

    if (memcmp(sha.GetDigest(), digest, 20))
    {
        SendAuthResponseError(AUTH_FAILED);

        sLog.outError("WorldSocket::HandleAuthSession: Sent Auth Response (authentification failed).");
        return -1;
    }

    std::string address = GetRemoteAddress();

    DEBUG_LOG("WorldSocket::HandleAuthSession: Client '%s' authenticated successfully from %s.",
              account.c_str(),
              address.c_str());

    // Update the last_ip in the database
    // No SQL injection, username escaped.
    static SqlStatementID updAccount;

    SqlStatement stmt = LoginDatabase.CreateStatement(updAccount, "UPDATE `account` SET `last_ip` = ? WHERE `username` = ?");
    stmt.PExecute(address.c_str(), account.c_str());

    // NOTE ATM the socket is single-threaded, have this in mind ...
    ACE_NEW_RETURN(m_Session, WorldSession(id, this, AccountTypes(security), expansion, mutetime, locale), -1);

    m_Crypt.Init(&K);

    m_Session->LoadGlobalAccountData();
    m_Session->LoadTutorialsData();
    m_Session->ReadAddonsInfo(addonsData);

    // In case needed sometime the second arg is in microseconds 1 000 000 = 1 sec
    ACE_OS::sleep(ACE_Time_Value(0, 10000));

    sWorld.AddSession(m_Session);

    return 0;
}

/**
 * @brief Handles a client ping and replies with a pong.
 *
 * @param recvPacket The ping packet.
 * @return int Zero on success; otherwise -1.
 */
int WorldSocket::HandlePing(WorldPacket& recvPacket)
{
    uint32 ping;
    uint32 latency;

    // Get the ping packet content
    recvPacket >> ping;
    recvPacket >> latency;

    if (m_LastPingTime == ACE_Time_Value::zero)
    {
        m_LastPingTime = ACE_OS::gettimeofday();             // for 1st ping
    }
    else
    {
        ACE_Time_Value cur_time = ACE_OS::gettimeofday();
        ACE_Time_Value diff_time(cur_time);
        diff_time -= m_LastPingTime;
        m_LastPingTime = cur_time;

        if (diff_time < ACE_Time_Value(27))
        {
            ++m_OverSpeedPings;

            uint32 max_count = sWorld.getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS);

            if (max_count && m_OverSpeedPings > max_count)
            {
                ACE_GUARD_RETURN(LockType, Guard, m_SessionLock, -1);

                if (m_Session && m_Session->GetSecurity() == SEC_PLAYER)
                {
                    sLog.outError("WorldSocket::HandlePing: Player kicked for "
                                  "overspeeded pings address = %s",
                                  GetRemoteAddress().c_str());

                    return -1;
                }
            }
        }
        else
        {
            m_OverSpeedPings = 0;
        }
    }

    // critical section
    {
        if (m_Session)
        {
            m_Session->SetLatency(latency);
            m_Session->ResetClientTimeDelay();
        }
        else
        {
            sLog.outError("WorldSocket::HandlePing: peer sent CMSG_PING, "
                          "but is not authenticated or got recently kicked,"
                          " address = %s",
                          GetRemoteAddress().c_str());
            return false;
        }
    }

    WorldPacket packet(SMSG_PONG, 4);
    packet << ping;
    return SendPacket(packet);
}
