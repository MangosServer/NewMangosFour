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

#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>

#include "WorldSocket.h"
#include "Common.h"
#include "MopWireCodec.h"
#include "MopAuthSession.h"
#include "MopAuthProof.h"
#include "MopAuthResponse.h"

#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "ByteBuffer.h"
#include "Opcodes.h"
#include "Database/DatabaseEnv.h"
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
    m_drainState(MopSock::DrainState::Open),
    m_sendInFlight(false),
    m_Seed(0),
    m_connState(MopHs::CONN_GREETING),
    m_frameReader(),
    m_lastDecodeLog(ACE_Time_Value::zero)
{
    reference_counting_policy().value(ACE_Event_Handler::Reference_Counting_Policy::ENABLED);

    // Canonical raw-40 session key: populated by HandleAuthSession via MopAuth::SessionKeyFromHex.
    memset(m_sessionKey, 0, sizeof(m_sessionKey));

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
        // The opcode does not fit the 13-bit MoP frame -- almost always an SMSG that still carries a
        // pre-MoP placeholder value (> 0x1FFF; the full table is remapped in Phase 1b, e.g.
        // SMSG_TALENT_UPDATE still holds the 4.3.4 value 0x6F26). DROP the packet instead of returning
        // -1: BuildServerHeader fails BEFORE EncryptSend, so the crypt keystream is untouched, and a
        // single un-remapped opcode must not tear down the connection (this was disconnecting the client
        // mid character-create when the new player's init packets fired). `sent` stays false so callers
        // see it was not transmitted. Report each offending opcode ONCE per run -- Phase 1b needs the
        // list, but hot init paths (char-create/login) would otherwise flood the log dozens of times.
        static std::atomic<bool> s_reportedDrop[0x10000];
        if (!s_reportedDrop[pct.GetOpcode()].exchange(true))
        {
            sLog.outError("WorldSocket::SendPacket: opcode 0x%.4X (%s) not framable in MoP -- still on a "
                          "placeholder value; dropping it and further such packets silently.",
                          pct.GetOpcode(), LookupOpcodeName(DIR_SERVER, pct.GetOpcode()));
        }
        return 0;
    }
    if (postCrypt && !m_Crypt.EncryptSend(header, sizeof(header)))
    {
        // The header is UNDEFINED on an EVP failure: in-place processing may have partially written
        // it. Dropping the packet is the only safe answer; the socket closes rather than transmit
        // plaintext or garbage under a post-crypt frame.
        sLog.outError("WorldSocket::SendPacket: header encryption failed (opcode 0x%.4X); closing.",
                      pct.GetOpcode());
        return -1;
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
    DEBUG_LOG("Received MSG_WOW_CONNECTION FROM %s", GetRemoteAddress().c_str());   // socket's own peer IP (set in open(), available pre-session) — was "<unk>" via the not-yet-created m_Session
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

/**
 * @brief Queues the auth error response and enters the drain state.
 *
 * The sole SMSG_AUTH_RESPONSE construction site on the rejection path: every auth rejection
 * routes through here, so the response body and the drain bookkeeping cannot drift apart.
 *
 * Ordering matters. READ interest is dropped BEFORE the response is queued, so a failure to
 * quiesce input is reported while nothing has been committed to the wire.
 *
 * @param code the AUTH_* result to report.
 * @return int 0 with the response queued and the drain armed; -1 if nothing was queued.
 */
int WorldSocket::BeginAuthErrorDrain(uint8 code)
{
    // 1. Idempotent: a second rejection must not re-arm the drain or re-queue a response.
    if (m_drainState.load() == MopSock::DrainState::Flushing)
    {
        return 0;
    }

    // 2. Drop reactor READ interest only; the socket must stay writable to drain.
    //
    // NOTE: cancel_wakeup(), NOT remove_handler(READ_MASK | DONT_CALL). Verified against the
    // vendored ACE (dep/acelite): remove_handler() -> handler_rep_.unbind(), which sets
    // complete_removal when the cleared mask leaves the handle with no wait/suspend mask left,
    // and then calls remove_reference() (Select_Reactor_Base.cpp:395). open() registers
    // READ|WRITE, but cancel_wakeup_output() clears WRITE as soon as the auth challenge has
    // flushed, so by the time a rejection runs READ is the ONLY mask: removing it would fully
    // unbind the handler and drop the reactor's reference, not merely stop reads. The socket
    // would survive (WorldSocketMgr::OnSocketOpen holds a second reference) but the handler
    // would be gone from the reactor, so a later schedule_wakeup_output() on a short write
    // could never be serviced -- m_OutActive would stay true, Update() would early-return 0
    // forever, and the socket would leak, never draining and never closing.
    //
    // cancel_wakeup() -> mask_ops(CLR_MASK) clears the read bit without unbinding, without
    // touching the reference count, and without invoking handle_close() (which would set
    // closing_ and defeat the drain outright, making DONT_CALL moot here). This is the same
    // idiom cancel_wakeup_output() uses for WRITE_MASK a few lines below.
    //
    // What actually stops the reads is a SUSPEND-set operation, not a wait-set one, and that
    // depends on running inside the suspended dispatch window: ACE_TP_Reactor::dispatch_socket_event
    // calls suspend_i() BEFORE dispatching us (TP_Reactor.cpp:395-397), and suspend_i() moves READ
    // out of wait_set_ into suspend_set_ and clears the dispatch mask, dropping a read event
    // already selected for this dispatch (Select_Reactor_T.cpp:949-975). mask_ops() is
    // suspend-aware -- "if (is_suspended_i(handle)) bit_ops(..., suspend_set_, ops)"
    // (Select_Reactor_T.cpp:851-866) -- so our CLR_MASK clears READ from suspend_set_. Afterwards
    // post_process_socket_event() calls resume_i() (TP_Reactor.cpp:596), and resume_i() restores
    // to wait_set_ only bits STILL present in suspend_set_ (Select_Reactor_T.cpp:929-933). READ is
    // gone by then, so it is never restored and no further read event is raised.
    //
    // Therefore this must stay INSIDE the suspended dispatch window (i.e. on the handler's own
    // upcall path, as HandleAuthSession is). Arming the drain from outside it would route the
    // CLR_MASK to wait_set_ instead, where a subsequent resume_i() has nothing to do with it --
    // the reasoning above simply would not apply and reads would continue.
    if (reactor()->cancel_wakeup(this, ACE_Event_Handler::READ_MASK) == -1)
    {
        sLog.outError("WorldSocket::BeginAuthErrorDrain: could not cancel read interest for %s; closing.",
                      GetRemoteAddress().c_str());
        return -1;                                            // nothing queued yet
    }

    // 3. Construct the error response via the canonical serializer (unchanged wire shape).
    WorldPacket packet;
    MopAuth::BuildAuthResponseError(packet, code);

    // 4. Queue it. If it never reached the buffer there is nothing to drain, so close now.
    bool sent = false;
    if (SendPacket(packet, &sent) == -1)
    {
        return -1;
    }
    if (!sent)
    {
        // Eluna's OnPacketSend vetoed the response; draining for it would hang the socket open.
        DEBUG_LOG("WorldSocket::BeginAuthErrorDrain: auth error response suppressed; closing.");
        return -1;
    }

    // 5. Arm the drain only once the bytes are genuinely pending.
    m_drainState.store(MopSock::DrainState::Flushing);
    return 0;
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

namespace
{
    /**
     * @brief Marks a dequeued-but-unaccounted-for block as an in-flight write.
     *
     * RAII rather than hand-placed stores: handle_output_queue() has seven exits once a block is
     * dequeued (dequeue failure, n == 0, EWOULDBLOCK re-enqueue, hard send error, partial-send
     * re-enqueue, partial-send re-enqueue FAILURE, full send). A single missed lower would pin the
     * flag high and the socket would then NEVER close -- trading a dropped auth response for a
     * socket leak. Scope exit cannot miss one.
     */
    class ScopedSendInFlight
    {
        public:
            explicit ScopedSendInFlight(std::atomic<bool>& flag) : m_flag(flag)
            {
                m_flag.store(true);
            }

            ~ScopedSendInFlight()
            {
                m_flag.store(false);
            }

        private:
            ScopedSendInFlight(ScopedSendInFlight const&);
            ScopedSendInFlight& operator=(ScopedSendInFlight const&);

            std::atomic<bool>& m_flag;
    };
}

int WorldSocket::handle_output_queue(GuardType& g)
{
    if (msg_queue()->is_empty())
    {
        return cancel_wakeup_output(g);
    }

    // Raised BEFORE dequeue_head() and lowered on every exit below. The queue is still non-empty
    // here, so from this point until the block is sent, released or re-enqueued there is no instant
    // at which the queue reports empty while no write is recorded as in flight -- which is exactly
    // the window a concurrent Update() would otherwise read as a finished drain (see
    // MopSock::ShouldCloseNow). Lowering happens at scope exit, i.e. AFTER any enqueue_head() has
    // put the block back, so the two never both look absent.
    ScopedSendInFlight inFlight(m_sendInFlight);

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

    // The auth error response has now been written: close for real.
    // This MUST sit above the drained early-return below, and NOT in handle_output(): once the
    // buffers are empty Update() returns 0 without ever calling handle_output(), and the full
    // drain already called cancel_wakeup_output(), so the reactor raises no further write event
    // either. A check inside handle_output() would never run and the socket would leak.
    // Returning -1 makes WorldSocketMgr::svc() do the orderly CloseSocket()/RemoveReference().
    //
    // The four inputs are SNAPSHOTTED UNDER m_OutBufferLock, in a nested scope that releases before
    // handle_output() is reached below. Reading them unlocked is not sufficient here and never was:
    // argument evaluation order is unspecified in C++ (MSVC commonly evaluates right-to-left), so an
    // unlocked call could sample m_sendInFlight BEFORE handle_output_queue() raises it and
    // msg_queue()->is_empty() AFTER dequeue_head() emptied the queue -- concluding "fully drained"
    // in the middle of a send and closing on top of the unsent response. Four independent non-atomic
    // reads cannot be made mutually consistent by making one of them atomic; only a common lock can.
    //
    // Taking the lock here does NOT deadlock, and the earlier claim that it would was wrong. The
    // real rule is: do not HOLD it across the handle_output() call below, which acquires this same
    // non-recursive ACE_Thread_Mutex through its own function-scoped ACE_GUARD_RETURN. Acquiring and
    // RELEASING it first, as the nested scope does, deadlocks nothing. No Update() caller holds it
    // either: WorldSocketMgr::svc() holds nothing, and handle_input() takes no guard.
    //
    // The lock genuinely covers the dequeue_head()->send() window: handle_output_queue() receives
    // handle_output()'s guard by reference and releases it ONLY inside cancel_wakeup_output() and
    // schedule_wakeup_output(), both of which run strictly AFTER send() returns. So a snapshot taken
    // under the lock can never land mid-send. THAT -- not m_sendInFlight -- is what makes this
    // decision correct; see WorldSocket.h for why the flag is redundant rather than load-bearing.
    //
    // The drain state is tested FIRST so m_OutBuffer is not dereferenced unless a drain is actually
    // armed. open() sets m_OutActive = true specifically to keep Update() out while initialising,
    // but m_OutBuffer stays NULL from that assignment until the ACE_NEW_RETURN a few lines later --
    // a window spanning the OnSocketOpen() call that inserts this socket into the set svc() sweeps.
    // Not reachable today (sockets_ is ACE_TSS, so only the opening thread sweeps it, and that
    // thread is inside run_reactor_event_loop() for all of open()) -- but this keeps the safety
    // local and obvious instead of resting on an invariant stated nowhere near here. Drain state is
    // only ever armed long after open() completes, so the test costs nothing.
    if (m_drainState.load() == MopSock::DrainState::Flushing)
    {
        MopSock::DrainState drainState;
        size_t outBufferLen;
        bool queueEmpty;
        bool sendInFlight;

        // Critical section: snapshot only. Released before handle_output() below.
        {
            ACE_GUARD_RETURN(LockType, Guard, m_OutBufferLock, -1);

            drainState = m_drainState.load();
            outBufferLen = m_OutBuffer->length();
            queueEmpty = msg_queue()->is_empty();
            sendInFlight = m_sendInFlight.load();
        }

        if (MopSock::ShouldCloseNow(drainState, outBufferLen, queueEmpty, sendInFlight))
        {
            return -1;
        }
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
        // Structural guard: never dispatch a frame while an auth error is draining, INCLUDING the
        // first frame of a re-entered call. The post-ProcessIncoming check below only suppresses
        // the next iteration of this loop, so on its own it would let any re-entry dispatch one
        // frame from a rejected peer. Unreachable today (legs 1 and 3 stop re-entry happening at
        // all), so this makes the property structural rather than contingent on those two legs.
        if (!MopSock::MayProcessInput(m_drainState.load()))
        {
            break;
        }

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

        const int processed = ProcessIncoming(pct);
        if (processed == -1)
        {
            errno = EINVAL;
            return -1;
        }

        // An auth rejection is now draining. Stop dispatching immediately: MopFrameReader may
        // already have coalesced a further frame into the buffer, and a peer we have decided to
        // reject must not have it acted upon. Removing reactor READ interest stops FUTURE
        // callbacks; only this break stops the frame that is already buffered -- both are needed.
        // Nothing is discarded here: the buffered frames die with the socket once output drains.
        if (!MopSock::MayProcessInput(m_drainState.load()))
        {
            break;
        }
        // Phase 3: after crypt Init in the auth path, the NEXT TryFrame sees IsInitialized()==true (no manual flag).
    }

    // Third leg of the quiesce, and NOT redundant with the break above or the READ-interest
    // removal: while Flushing this reports a partial read so ACE_TP_Reactor cannot re-invoke
    // handle_input() directly off a positive status. The rule itself lives in MopSocketDrain.h
    // (see MopSock::InputStatus) where it is unit-tested; do not re-open-code it here.
    // Otherwise the plain ACE contract applies: full read -> 1, partial -> 2.
    return MopSock::InputStatus(m_drainState.load(), size_t(n) == sizeof(buf));
}

/// MopFrameReader::DecryptFn hook (Phase 2 wire framing): in-place ARC4 on the header only.
bool WorldSocket::DecryptHeaderHook(void* ctx, uint8* header, size_t len)
{
    WorldSocket* const self = static_cast<WorldSocket*>(ctx);

    // Pre-crypt frames are not decrypted at all -- the reader must not treat that as a failure.
    // AuthCrypt::DecryptRecv returns false when not ACTIVE, which is the pre-crypt case, so the
    // question has to be asked HERE rather than inferred from the return value.
    if (!self->m_Crypt.IsInitialized())
    {
        return true;
    }

    // Post-crypt: a false return means the header is UNDECRYPTED, so parsing it would read garbage.
    // Reject the frame (MF_DECRYPT) instead of guessing.
    return self->m_Crypt.DecryptRecv(header, len);
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
                // THE PHASE 3 STAGE 2 CUTOVER. Phase 2 proved framing here and deliberately returned
                // -1 without authenticating; Stage 1 restructured the dormant handler; Stage 2 gave
                // it correct semantics. This line is where all of it becomes reachable.
                //
                // The allowlist above guarantees CONN_CHALLENGED here, so this is the legal
                // transition. HandleAuthSession owns every outcome from here:
                //   0  = EITHER the session committed (crypt -> CONN_AUTHED -> publish), OR a
                //        rejection was queued and the drain armed -- BeginAuthErrorDrain returns 0
                //        on success, and the socket stays open until Update() sees it drained and
                //        closes it. Returning 0 is what lets the error reach the client.
                //   -1 = nothing was queued (cancel_wakeup failed / SendPacket failed / Eluna
                //        vetoed), so there is nothing to drain and the socket closes immediately.
                // DO NOT "fix" this to return -1 on rejection: that closes the socket on top of the
                // unsent response and reintroduces the exact Stage 1 defect (Codex C1) that
                // BeginAuthErrorDrain exists to fix.
                //
                // The 6->4-byte inbound header switch needs no code: the reader derives the header
                // kind fresh per call from m_Crypt.IsInitialized() (see DecryptHeaderHook below), so
                // the first frame after a successful commit is read post-crypt automatically.
                //
                // The Eluna OnPacketReceive hook for this opcode remains deliberately NOT invoked:
                // Phase 2 deferred it to Phase 3 along with the rest of the auth path, and invoking
                // it here would let a script observe the raw auth body, including the proof, which
                // spec 2 forbids. This is a Phase 4 decision, not an oversight.
                m_connState = MopHs::CONN_AUTHENTICATING;
                return HandleAuthSession(*new_pct);
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
 * @brief Tears down a WorldSession allocated but never published to World.
 *
 * The single cleanup path for every post-allocation rejection in HandleAuthSession.
 * AbandonUnpublishedSocket() nulls the session's m_Socket before this deletes it, so
 * ~WorldSession() does NOT CloseSocket() the socket we are still draining an auth error through,
 * and it releases exactly the one extra reference the WorldSession constructor took.
 */
void WorldSocket::DestroyUnpublishedSession() noexcept
{
    if (!m_Session)
    {
        return;
    }

    m_Session->AbandonUnpublishedSocket();
    delete m_Session;
    // Unlocked write is safe ONLY because this runs on the reactor upcall path (HandleAuthSession),
    // which the reactor serializes against handle_close(), and m_Session is not yet published to
    // World -- no other thread can observe or race this store. If this cleanup is ever moved off
    // the upcall path, re-add the m_SessionLock guard used elsewhere in this file.
    m_Session = NULL;
}

/**
 * @brief Infallible auth commit run by World::AddSession under the add-queue lock.
 *
 * Activates the prepared crypt (PREPARED -> ACTIVE, a single state store that cannot fail) and
 * stores CONN_AUTHED. Runs while the queue lock is held and the queued session is not yet visible
 * to the world thread; unlocking after this returns is the publication point. Must stay noexcept.
 *
 * @param context The originating WorldSocket, passed opaque through AddSession.
 */
void WorldSocket::CommitAuthenticatedSession(void* context) noexcept
{
    WorldSocket* socket = static_cast<WorldSocket*>(context);
    socket->m_Crypt.Activate();
    socket->m_connState = MopHs::CONN_AUTHED;
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

    // Read the content of the packet.
    // The read order, the scattered digest[] indices and the missing leading ReadBit() before
    // ReadBits(11) all now live in MopAuth::DecodeAuthSession, which reproduces them bug-for-bug
    // while bounding every read. See MopAuthSession.h.
    // NOTE: named authFields, not fields: a 'Field* fields' DB row local already exists below.
    MopAuth::AuthSessionFields authFields{};
    MopAuth::DecodeResult const decodeResult = MopAuth::DecodeAuthSession(recvPacket, authFields);
    if (decodeResult != MopAuth::DecodeResult::Ok)
    {
        // DEBUG_LOG, not sLog.outError: this fires on every malformed body on an UNAUTHENTICATED
        // path, so at error level an attacker spraying junk auth bodies would drive unbounded
        // error-level log writes (disk/IO amplification). Deliberately NOT rate-limited via
        // m_lastDecodeLog like the sibling decode paths: a rejection now drains and closes the
        // socket, so this path logs at most ONCE per connection and a PER-SOCKET limiter cannot
        // bound it -- volume tracks connection rate, which the limiter never sees. Demoting is
        // what actually removes the amplification at default log levels.
        // Payload-free: the decode result enum only, never the body.
        DEBUG_LOG("WorldSocket::HandleAuthSession: malformed auth session body (result %u).",
                  static_cast<uint32>(decodeResult));
        return BeginAuthErrorDrain(AUTH_FAILED);
    }

    std::memcpy(digest, authFields.digest, sizeof(digest));
    clientSeed = authFields.clientSeed;
    BuiltNumberClient = authFields.builtNumberClient;
    account = authFields.account;

    ByteBuffer addonsData;
    if (!authFields.addonData.empty())
    {
        addonsData.append(authFields.addonData.data(), authFields.addonData.size());
    }

    DEBUG_LOG("WorldSocket::HandleAuthSession: client build %u.", BuiltNumberClient);

    // Check the version of client trying to connect
    if (!IsAcceptableClientBuild(BuiltNumberClient))
    {
        // Claims only the decision, not the delivery: BeginAuthErrorDrain can return -1 having sent
        // nothing, and it reports the actual send outcome itself. Keep payload-free.
        // BASIC_LOG, not sLog.outError: operator-actionable and low volume (a version mismatch
        // means a client/server build skew, not an attacker), so it belongs at default log level
        // rather than error level.
        BASIC_LOG("WorldSocket::HandleAuthSession: rejecting auth session (version mismatch).");
        return BeginAuthErrorDrain(AUTH_VERSION_MISMATCH);
    }

    // Get the account information from the realmd database
    std::string safe_account = account; // Duplicate, else will screw the SHA hash verification below
    LoginDatabase.escape_string(safe_account);
    // No SQL injection, username escaped.

    // ACCEPTED RISK, scoped precisely (spec 2): the account name is a query parameter, so it
    // reaches the DB layer's SQL log whenever SQL logging is enabled. That logging is config-gated,
    // OFF by default, pre-existing, and lives in code SHARED WITH REALMD -- Phase 3 cannot suppress
    // it without violating "never touch realmd". Account name only, SQL layer only, opt-in only.
    // NO exception exists for K, the proof, s or v: none of them is a query parameter, and after
    // this task v and s are not even SELECTed.
    QueryResult* result =
        LoginDatabase.PQuery("SELECT "
                             "`id`, "                      // 0
                             "`gmlevel`, "                 // 1
                             "`sessionkey`, "              // 2
                             "`last_ip`, "                 // 3
                             "`locked`, "                  // 4
                             "`expansion`, "               // 5  (was 7)
                             "`mutetime`, "                // 6  (was 8)
                             "`locale` "                   // 7  (was 9)
                             "FROM `account` "
                             "WHERE `username` = '%s'",
                             safe_account.c_str());

    // Stop if the account is not found
    if (!result)
    {
        // DEBUG_LOG, not sLog.outError: attacker-driven volume on an unauthenticated path -- a
        // typo'd or scanned username is not an operator event and must not drive unbounded
        // error-level disk writes.
        DEBUG_LOG("WorldSocket::HandleAuthSession: rejecting auth session (unknown account).");
        return BeginAuthErrorDrain(AUTH_UNKNOWN_ACCOUNT);
    }

    Field* fields = result->Fetch();

    expansion = ((sWorld.getConfig(CONFIG_UINT32_EXPANSION) > fields[5].GetUInt8()) ? fields[5].GetUInt8() : sWorld.getConfig(CONFIG_UINT32_EXPANSION));

    ///- Re-check ip locking (same check as in realmd).
    if (fields[4].GetUInt8() == 1)  // if ip is locked
    {
        if (strcmp(fields[3].GetString(), GetRemoteAddress().c_str()))
        {
            delete result;                                    // 'fields' dies with it; not used below
            BASIC_LOG("WorldSocket::HandleAuthSession: rejecting auth session (account IP differs).");
            return BeginAuthErrorDrain(AUTH_FAILED);
        }
    }

    id = fields[0].GetUInt32();
    security = fields[1].GetUInt16();
    if (security > SEC_ADMINISTRATOR)                       // prevent invalid security settings in DB
    {
        security = SEC_ADMINISTRATOR;
    }

    // ---- Ladder step 1 (spec 5.1): guard the sessionkey with its own distinct log line. -------
    // Until now K.SetHexStr(fields[2].GetString()) ran UNGUARDED: a NULL or empty value yields a
    // garbage BigNumber and an AUTH_FAILED indistinguishable from a wrong seed, a wrong
    // permutation, or a byte-reversed K. Naming the cause is the whole point.
    //
    // VALIDATE STRUCTURALLY -- NEVER BRANCH ON A SPECIFIC LENGTH. realmd stores K via AsHexStr()
    // (= BN_bn2hex), which DROPS LEADING ZERO BYTES: one leading zero gives 78 hex chars, two give
    // 76, and so on with NO floor. Rule: non-NULL, non-empty, hex-only, EVEN length, <= 80 ->
    // convert -> assert the CONVERTED value is 1..40 bytes. "" is vacuously hex-only, even (0) and
    // <= 80, and would zero-fill into forty 0x00s -- a K of zeros that looks well-formed to every
    // consumer downstream; that confounder is precisely what this guard catches, so empty is a
    // HARD REJECT.
    const char* const sessionKeyHex = fields[2].GetString();

    // ONE call owns the whole chain: validate -> BigNumber -> raw-40 -> re-check. Do NOT
    // re-implement any of it here. SessionKeyFromHex is covered end-to-end by
    // test_sessionkey_from_hex_roundtrip, which drives realmd's own SetBinary/AsHexStr write path.
    // sessionKeyHex points INTO the query result, so this must run BEFORE any `delete result`.
    if (!MopAuth::SessionKeyFromHex(sessionKeyHex, m_sessionKey))
    {
        delete result;                                        // 'fields' dies with it -- see below
        // Payload-free by construction: the account id only, never the key text. `id` is a LOCAL
        // copy taken above, so it survives the delete; fields[] does NOT.
        // BASIC_LOG, not sLog.outError: ladder step 1 (spec 5.1) must stay visible at default log
        // level -- it is the gate's first question, and demoting it below default would hide the
        // most common ordering mistake (HandleAuthSession racing realmd's own auth write).
        BASIC_LOG("WorldSocket::HandleAuthSession: rejecting auth session "
                  "(account %u has a NULL, empty or malformed sessionkey; realmd may not have "
                  "authenticated this account yet).",
                  id);
        return BeginAuthErrorDrain(AUTH_SESSION_EXPIRED);
    }

    time_t mutetime = time_t (fields[6].GetUInt64());

    locale = LocaleConstant(fields[7].GetUInt8());
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
        delete banresult;

        // BASIC_LOG, not sLog.outError: operator-actionable and bounded by the size of the ban
        // list, so it belongs at default log level rather than error level.
        BASIC_LOG("WorldSocket::HandleAuthSession: rejecting auth session (account banned).");
        return BeginAuthErrorDrain(AUTH_BANNED);
    }

    // Check locked state for server
    AccountTypes allowedAccountType = sWorld.GetPlayerSecurityLimit();

    if (allowedAccountType > SEC_PLAYER && AccountTypes(security) < allowedAccountType)
    {
        BASIC_LOG("WorldSocket::HandleAuthSession: User tries to login but his security level is not enough");
        return BeginAuthErrorDrain(AUTH_UNAVAILABLE);
    }

    // Phase 3 defers Warden structurally. Everything needed to re-enable it correctly is stated
    // here; do not re-add the code without addressing all four points:
    //
    // 1. K CONSUMER. Warden is the fourth consumer of the session key K (after the digest check,
    //    m_Crypt.Init and the DB). Any migration to a canonical raw-40 K must account for it.
    //    WorldSession::InitWarden still takes BigNumber* k and currently has ZERO callers, so the
    //    compiler will NOT flag it when the key representation changes. This comment is the only
    //    barrier -- treat it as a live TODO, not history.
    // 2. SHORT-K BUG. WardenWin.cpp / WardenMac.cpp reseed from k->AsByteArray(), k->GetNumBytes().
    //    BigNumber drops leading zero bytes, so whenever K's most-significant byte is zero (~1/256
    //    of logins) that yields 39 bytes, not 40, and Warden desyncs. A raw-40 K fixes this by
    //    construction; a BigNumber-shaped K does not.
    // 3. NO KEY LOGGING. Warden logged its derived encryption keys. That logging was removed in
    //    Stage 1 and must not be reintroduced.
    // 4. ORDERING. InitWarden previously ran BEFORE AddSession, which made SMSG_WARDEN_DATA the
    //    first encrypted server packet -- ahead of SMSG_AUTH_RESPONSE. Re-enabling must decide
    //    that ordering deliberately rather than inherit it.

    // Check that Key and account name are the same on client and server (spec 3.2).
    uint8 serverProof[MopAuth::AUTH_PROOF_LEN];
    MopAuth::ComputeAuthProof(account, clientSeed, m_Seed, m_sessionKey, serverProof);

    // CRYPTO_memcmp, not memcmp: a short-circuiting compare leaks how many leading proof bytes an
    // attacker guessed right, one online attempt at a time.
    if (!MopAuth::ProofEquals(serverProof, digest))
    {
        // DEBUG_LOG, not sLog.outError: this is the credential-spray path -- an attacker driving
        // wrong-password attempts against valid or invalid usernames alike hits this rejection on
        // every try, so at error level that traffic drives unbounded error-level disk writes.
        DEBUG_LOG("WorldSocket::HandleAuthSession: rejecting auth session (authentication failed).");
        return BeginAuthErrorDrain(AUTH_FAILED);
    }

    // ---- Crypt: PREPARE here, ACTIVATE in the commit region. --------------------------------
    // Everything about the crypt that can fail happens HERE: the HMAC-SHA1 key derivation, both
    // ARC4 keyings, and the 1024-byte drop. All of it precedes ACE_NEW_RETURN, so a failure has
    // nothing allocated to clean up and can simply drain an error like every other rejection in
    // this function. Prepare() does NOT publish: IsInitialized() stays false until Activate(), so
    // the crypt is inert and the wire codec still frames PRE-crypt through the fallible loads below.
    if (!m_Crypt.Prepare(m_sessionKey))
    {
        // Payload-free: no K, no digest. Prepare has already logged the specific OpenSSL cause.
        sLog.outError("WorldSocket::HandleAuthSession: rejecting auth session "
                      "(crypt could not be prepared for account %u).", id);
        return BeginAuthErrorDrain(AUTH_SYSTEM_ERROR);
    }

    std::string address = GetRemoteAddress();

    DEBUG_LOG("WorldSocket::HandleAuthSession: account %u authenticated successfully from %s.",
              id,
              address.c_str());

    // Phase 3 performs NO writes to the `account` row on the auth path (spec 2, 6.7). This
    // last_ip UPDATE was the only one, and it is not load-bearing: realmd rewrites last_ip on
    // EVERY authentication (src/realmd/Auth/AuthSocket.cpp:714), so the column stays current
    // without it. It also fed the account NAME to the SQL layer a second time, on a path that had
    // no need to.

    // NOTE ATM the socket is single-threaded, have this in mind ...
    // Allocation stays ahead of its own load calls below; ACE_NEW_RETURN is an explicit early
    // return, so it must remain pre-commit.
    ACE_NEW_RETURN(m_Session, WorldSession(id, this, AccountTypes(security), expansion, mutetime, locale), -1);

    // Every fallible step -- DB loads, addon inflate -- runs BEFORE the commit region. Their
    // internal semantics are unchanged from the pre-Task-4 code; only their position moved above
    // the crypt Prepare()/Activate() split (Prepare() already ran above, before session
    // allocation; Activate() runs inside the AUTH PUBLICATION TRANSACTION below). This is what
    // makes the region atomic BY ORDERING: once the commit region is entered nothing below it can
    // fail, so no failure path can observe an activated crypt on an unauthenticated session.
    // AuthCrypt has no rollback and deliberately does not gain one -- rollback would exist only to
    // paper over the ordering defect this reorder removes.
    //
    // ReadAddonsInfo inflates attacker-controlled zlib and CAN throw ByteBufferException. Without
    // this local catch the outer ProcessIncoming ByteBufferException handler would close the socket
    // while handle_close() only nulls m_Session, leaking the freshly allocated session. Every catch
    // routes through the single DestroyUnpublishedSession() path and drains an error. Never log
    // addon bytes.
    try
    {
        m_Session->LoadGlobalAccountData();
        m_Session->LoadTutorialsData();
        m_Session->ReadAddonsInfo(addonsData);
    }
    catch (ByteBufferException const&)
    {
        sLog.outError("WorldSocket::HandleAuthSession: rejecting auth session "
                      "(malformed addon data for account %u).", id);
        DestroyUnpublishedSession();
        return BeginAuthErrorDrain(AUTH_FAILED);
    }
    catch (std::exception const& e)
    {
        sLog.outError("WorldSocket::HandleAuthSession: rejecting auth session "
                      "(session load failed for account %u: %s).", id, e.what());
        DestroyUnpublishedSession();
        return BeginAuthErrorDrain(AUTH_SYSTEM_ERROR);
    }
    catch (...)
    {
        sLog.outError("WorldSocket::HandleAuthSession: rejecting auth session "
                      "(unknown session-load failure for account %u).", id);
        DestroyUnpublishedSession();
        return BeginAuthErrorDrain(AUTH_SYSTEM_ERROR);
    }

    // In case needed sometime the second arg is in microseconds 1 000 000 = 1 sec
    // Legacy delay, semantics unchanged; hoisted above the commit region because a delay may not
    // appear between the three commit statements.
    ACE_OS::sleep(ACE_Time_Value(0, 10000));

    // ================= AUTH PUBLICATION TRANSACTION =================
    // Everything fallible already ran ABOVE: crypt Prepare() (HMAC, ARC4 keying, drop-1024), the
    // session allocation, the DB loads and the addon inflate. World::AddSession now performs the
    // ONLY remaining fallible operation -- queue insertion -- while its lock is held. Only after
    // that succeeds does it invoke CommitAuthenticatedSession(), still under the lock, so the world
    // thread cannot observe the queued session until crypt and state are committed. Unlock is the
    // publication point:
    //   reserve queue node (locked) -> crypt ACTIVE -> CONN_AUTHED -> unlock/publish.
    // Activate() is the ONLY PREPARED->ACTIVE transition and is infallible -- a single state store.
    //
    // On false the callback did not run: crypt remains PREPARED/inert, state remains
    // CONN_AUTHENTICATING, and the queue is unchanged. DestroyUnpublishedSession releases the
    // session's extra socket reference without closing this socket, allowing the auth error to
    // drain normally.
    if (!sWorld.AddSession(m_Session, &WorldSocket::CommitAuthenticatedSession, this))
    {
        DestroyUnpublishedSession();
        return BeginAuthErrorDrain(AUTH_SYSTEM_ERROR);
    }

    // GATE 3b EVIDENCE. AddSession returned only after unlocking, so the queue entry is now visible
    // and crypt/state were committed before publication. This does NOT claim AddSession_ has already
    // inserted into m_sessions or sent SMSG_AUTH_RESPONSE; those happen on the world thread.
    DEBUG_LOG("WorldSocket::HandleAuthSession: account %u CONN_AUTHED "
              "(crypt activated, session enqueue published).", id);
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
