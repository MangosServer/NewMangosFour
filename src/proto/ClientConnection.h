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

#ifndef MANGOS_PROTO_CLIENTCONNECTION_H
#define MANGOS_PROTO_CLIENTCONNECTION_H

#include "IClientLink.h"
#include "IWorldGateway.h"
#include "MopFrameReader.h"
#include "MopHandshake.h"

#include "Auth/AuthCrypt.h"
#include "net/ISession.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace proto
{
    /**
     * @brief One client connection, speaking the 5.4.8 world protocol.
     *
     * Live since Stage 2 CP3: this class plus Listener replace the deleted
     * WorldSocket/WorldSocketMgr and now own every real world connection.
     *
     * Reuses M4's own Mop* codec verbatim (MopFrameReader/MopWireCodec/
     * MopHandshake/MopAuth*) -- nothing about the wire bytes, the handshake FSM
     * or the auth decode/proof is reinvented here. What this class supplies is
     * only the glue WorldSocket used to provide via ACE: the net::ISession
     * lifecycle, the per-connection state variable, and the IWorldGateway calls
     * that replace direct WorldSession/database access.
     *
     * M4's pre-auth handshake is NOT Cata's fire-and-continue FSM: onConnect()
     * sends only the server's own MSG_WOW_CONNECTION greeting; the auth challenge
     * is sent later, from HandleWowConnection(), and only after the client's own
     * greeting string has been validated (WorldSocket.cpp:383-402, hazard H5).
     *
     * Threading: the transport calls onConnect()/onData()/onClose() on a network
     * thread, one connection at a time. SendPacket() may be called from any
     * thread (the world thread does, once wired), so header encryption is
     * serialised under m_cryptSendLock.
     */
    class ClientConnection : public net::ISession, public IClientLink
    {
        public:

            explicit ClientConnection(IWorldGateway& gateway);
            ~ClientConnection() override;

            // --- net::ISession ------------------------------------------------

            void setPeerAddress(const std::string& address) override
            {
                m_address = address;
            }

            void setSender(net::Sender sender) override
            {
                m_sender = std::move(sender);
            }

            void setCloser(net::Closer closer) override
            {
                m_closer = std::move(closer);
            }

            std::vector<uint8_t> onConnect() override;
            std::vector<uint8_t> onData(const uint8_t* data, size_t len) override;
            void onClose() override;

            bool closed() const override
            {
                return m_closed.load(std::memory_order_acquire);
            }

            // --- IClientLink (what the world may do to us) --------------------

            /// Encode, encrypt and queue a packet. Safe from any thread, and a
            /// no-op once the peer is gone. Carries WorldSocket::SendPacket's
            /// drop-don't-disconnect policy for opcodes that do not fit the
            /// 13-bit post-crypt frame (hazard H2) -- an un-remapped SMSG must
            /// never tear down the connection.
            void SendPacket(const WorldPacket& packet) override;

            /// Mark the connection dead and ask the transport to tear it down.
            void Close() override;

            const std::string& GetRemoteAddress() const override { return m_address; }

            bool IsClosed() const override { return closed(); }

            static uint32 GetOpenConnectionCount()
            {
                return s_openConnections.load(std::memory_order_relaxed);
            }

        private:

            /// Encode one packet to wire bytes: MopWire::BuildServerHeader, then
            /// the header cipher if the crypt is active. Returns empty on both of
            /// its failure paths, distinguished by `fatal`: an unframable opcode
            /// (H2) is a silent per-packet drop, but a header-encryption failure
            /// must close the connection (the header may be partially written).
            std::vector<uint8_t> EncodeForSend(const WorldPacket& packet, bool& fatal);

            /// Dispatch one fully decoded frame. false => caller must Close().
            bool DispatchFrame(uint32_t cmd, std::vector<uint8_t>&& payload);

            /// The client's own MSG_WOW_CONNECTION reply (WorldSocket.cpp's
            /// HandleWowConnection): validated by strcmp, then triggers the
            /// challenge. Unlike WorldSocket, a validation failure here just
            /// returns false; ClientConnection has no ACE handle_close to call.
            bool HandleWowConnection(WorldPacket& packet);

            /// Builds and sends SMSG_AUTH_CHALLENGE via MopHs::BuildAuthChallengePayload.
            /// Advances m_connState to CONN_CHALLENGED only once the challenge is
            /// actually queued (mirrors WorldSocket::SendAuthChallenge).
            bool SendAuthChallenge();

            bool HandleAuthSession(WorldPacket& packet);
            bool HandlePing(WorldPacket& packet);

            /// IWorldGateway::AuthCommit callback: the infallible PREPARED->ACTIVE
            /// crypt transition, run by World::AddSession while its add-queue lock
            /// is held (hazard H3). ctx is always the ClientConnection that prepared
            /// the crypt and called Attach() -- see HandleAuthSession(). Must stay
            /// noexcept: the lock is held across it and IWorldGateway::AuthCommit's
            /// type statically enforces this.
            static void CommitCrypt(void* ctx) noexcept;

            /// Queue the canonical SMSG_AUTH_RESPONSE error variant via the
            /// gateway's game-side serializer (IWorldGateway::BuildAuthErrorResponse)
            /// and send it. Deliberately does NOT reimplement WorldSocket's ACE
            /// drain machinery (cancel_wakeup/ScopedSendInFlight/Update() snapshot)
            /// -- hazard H4, resolved by reading both net:: backends' source: they
            /// already guarantee flush-before-close on their own. See this method's
            /// definition for the full resolution.
            void RejectAuth(AuthStatus status);

            /// MopFrameReader::DecryptFn hook: in-place ARC4 on the header only.
            static bool DecryptHeaderHook(void* ctx, uint8_t* header, size_t len);
            /// MopFrameReader::CmdValidFn hook: pre-crypt legality (mirrors
            /// WorldSocket::CmdValidHook -- MSG_WOW_CONNECTION or < OPCODE_TABLE_SIZE).
            static bool CmdValidHook(void* ctx, uint32_t cmd, bool preCrypt);

            IWorldGateway& m_gateway;

            std::string m_address;
            net::Sender m_sender;
            net::Closer m_closer;
            std::atomic<bool> m_closed;

            MopFrameReader m_frameReader;
            MopHs::ConnectionState m_connState;

            AuthCrypt  m_crypt;
            std::mutex m_cryptSendLock; ///< serialises header encryption on send

            /// Server half of the authentication nonce (WorldSocket's m_Seed),
            /// drawn from the OpenSSL RNG via MopHs::BuildAuthChallengePayload.
            uint32 m_seed;

            SessionId m_session;

            /// When the previous CMSG_PING arrived; valid once m_hadPing is set.
            std::chrono::steady_clock::time_point m_lastPing;
            bool m_hadPing;

            /// Consecutive pings that arrived faster than a real client sends
            /// them (WorldSocket's m_OverSpeedPings). Reset the moment the
            /// cadence returns to normal.
            uint32 m_fastPingRun;

            /// Rate-limits malformed-frame diagnostic logging (WorldSocket's
            /// m_lastDecodeLog), shared across the frame-level and dispatch-level
            /// decode-failure paths. Seconds since epoch; 0 means "never logged".
            std::atomic<int64_t> m_lastDecodeLogSec;

            static std::atomic<uint32> s_openConnections;
    };
}

#endif
