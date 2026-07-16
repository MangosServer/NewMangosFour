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

#ifndef MANGOS_H_MOPAUTHRESPONSE
#define MANGOS_H_MOPAUTHRESPONSE

#include "Common/Common.h"

class WorldPacket;

namespace MopAuth
{
    /**
     * @brief Builds SMSG_AUTH_RESPONSE -- the ONE canonical serializer for all five variants.
     *
     * | Variant       | code        | hasAccountData | queued | extra            |
     * |---------------|-------------|----------------|--------|------------------|
     * | ACCEPTED      | 12 AUTH_OK  | 1 + full block | 0      | --               |
     * | INITIAL QUEUE | 12 AUTH_OK  | 1 + full block | 1      | queuePosition u32|
     * | QUEUE UPDATE  | 12 AUTH_OK  | 1 + full block | 1      | queuePosition u32|
     * | RELEASE (0)   | 12 AUTH_OK  | 1 + full block | 0      | --  (== ACCEPTED)|
     * | ERROR         | != 12       | 0              | 0      | --               |
     *
     * THE RULE THAT MAKES THIS NON-OBVIOUS -- BINARY-CONFIRMED (sub_140A70980):
     *   Every response with queued=0 MUST carry hasAccountData=1, or the client rewrites AUTH_OK
     *   (12) into AUTH_FAILED (13) and delivers failure while the server believes it succeeded.
     *   With queued=1 the queue branch overwrites that 13 with 27, MASKING the defect on every
     *   update packet so it only bites on the RELEASE -- which is why SkyFire (PlayerLimit=100 by
     *   default) never caught it. This builder always sets hasAccountData for AUTH_OK, so the rule
     *   cannot be violated by a caller.
     *
     *   NEVER send code 27 (AUTH_WAIT_QUEUE) on the wire: the client SYNTHESISES it from the queued
     *   bit. Sending 27 lands in the != 12 path and delivers 27 -- effectively a failure. Four did
     *   this at WorldSessionMgr.cpp:248 and WorldSession.cpp:1008 before this serializer landed.
     *   Callers pass AUTH_OK and queued=true; they never pass 27.
     *
     * [HYPOTHESIS -- SkyFire-derived; the SHAPE is corroborated by the client struct, the BIT ORDER
     * IS NOT. The deserializer was not located; two independent tracks failed to find it. Settled
     * at Gate 3b -- the client accepting this is what confirms it.]
     *
     * THREE ENTRY POINTS, DELIBERATELY. A single Build(code, queued, ...) leaves both binary-
     * confirmed rules expressible-but-wrong: a caller can pass AUTH_OK with queued=0 and no account
     * block (-> client rewrites 12 to 13), or pass 27 directly (-> client delivers failure). A test
     * can only ever prove that THIS caller did not; it cannot prove the serializer refuses. So the
     * shapes the protocol allows are the only shapes the API offers:
     *   - Accepted / Queued force code = AUTH_OK and hasAccountData = 1. 27 is UNREPRESENTABLE.
     *   - Error takes a code, and REJECTS both AUTH_OK and AUTH_WAIT_QUEUE at runtime.
     *
     * @param out              receives the packet; cleared first.
     * @param accountExpansion the account's entitlement, already clamped to the realm
     *                         (WorldSession::Expansion(); EXPANSION_NONE=0 .. EXPANSION_MOP=4).
     * @param serverExpansion  the realm's configured cap (CONFIG_UINT32_EXPANSION).
     */
    void BuildAuthResponseAccepted(WorldPacket& out, uint8 accountExpansion, uint8 serverExpansion);

    /// As Accepted, plus the queued bit and a position. code is AUTH_OK -- the client SYNTHESISES
    /// its own 27 from the bit. @param queuePos 1-based; position 0 is a RELEASE, i.e. Accepted.
    void BuildAuthResponseQueued(WorldPacket& out, uint32 queuePos, uint8 accountExpansion,
                                 uint8 serverExpansion);

    /// An AUTH_* failure. hasAccountData=0 is safe here: code != 12 never enters the client's
    /// forcing branch. AUTH_OK and AUTH_WAIT_QUEUE are REJECTED (coerced to AUTH_FAILED + logged).
    void BuildAuthResponseError(WorldPacket& out, uint8 code);
}

#endif
