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

#ifndef MANGOS_MOP_SOCKET_DRAIN_H
#define MANGOS_MOP_SOCKET_DRAIN_H

#include <cstddef>
#include <cstdint>

/**
 * Pure decisions for the auth-error drain state machine.
 *
 * An auth rejection cannot simply close the socket: SendPacket() only appends to the
 * outbound buffer, so closing straight away discards the error response and the peer sees
 * a bare TCP close. The socket must instead stay open until the response has actually been
 * written, while refusing to act on any further input from a peer we have already rejected.
 *
 * Header-only and free of ACE/WorldSocket so the decisions are unit-testable.
 */
namespace MopSock
{
    /// Open   -> normal traffic.
    /// Flushing -> an auth error is queued; accept no further input, close once fully written.
    enum class DrainState : uint8_t { Open, Flushing };

    /// Whether an inbound frame may still be acted upon.
    /// False once Flushing, so a frame already coalesced into the read buffer before the
    /// rejection is never dispatched from a socket we have decided to reject.
    inline bool MayProcessInput(DrainState state)
    {
        return state == DrainState::Open;
    }

    /// Whether the socket has finished draining and may now be closed.
    /// Both the buffer and the queue must be empty: closing earlier is exactly the bug this
    /// state exists to prevent, since the queued error response would be discarded unsent.
    inline bool ShouldCloseNow(DrainState state, size_t outBufferLen, bool queueEmpty)
    {
        return state == DrainState::Flushing && outBufferLen == 0 && queueEmpty;
    }

    /// The receive-status a handle_input() implementation must report (ACE contract: 1 = full
    /// read, 2 = partial read). Normally a full read reports 1 so the reactor keeps draining the
    /// socket. While Flushing it must report 2 instead: ACE_TP_Reactor::dispatch_socket_event
    /// re-invokes the callback DIRECTLY on a positive status
    /// ("while (status > 0) status = (event_handler->*callback)(handle);", TP_Reactor.cpp:541-543)
    /// consulting neither the wait set nor the dispatch mask, so dropping READ interest cannot
    /// stop that loop -- only a non-positive... specifically a non-1 ... status ends it. A peer
    /// can otherwise force continued service simply by sending >= 4096 bytes at once.
    ///
    /// This is the drain's third leg and is called from WorldSocket::handle_input_missing_data();
    /// it lives here, not open-coded at the call site, so the rule is pinned by unit tests.
    inline int InputStatus(DrainState state, bool fullRead)
    {
        if (!MayProcessInput(state))
        {
            return 2;
        }
        return fullRead ? 1 : 2;
    }
}

#endif
