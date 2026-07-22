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

// WorldPacket lives in src/shared/Utilities/, NOT src/game/Server/ -- match MopAuthSession.cpp's
// "Utilities/ByteBuffer.h" idiom. Opcodes.h and SharedDefines.h are siblings of this file.
#include "MopAuthResponse.h"
#include "Utilities/WorldPacket.h"
#include "Opcodes.h"
#include "SharedDefines.h"
#include "Log/Log.h"

namespace
{
    struct ExpansionInfo
    {
        uint8 raceOrClass;
        uint8 expansion;
    };

    // EXPANSION_* (SharedDefines.h:4263-4267): EXPANSION_NONE=0, EXPANSION_TBC=1, EXPANSION_WOTLK=2,
    // EXPANSION_CATA=3, EXPANSION_MOP=4. (The plan's "CLASSIC/CATACLYSM/MISTS" spellings are
    // SkyFire-derived and do not exist in this tree -- the values above are what is used.)
    //
    // GOTCHA: the bit section counts CLASS-then-RACE, but the payload is RACE-then-CLASS (see
    // Build() below). Both loops here use { expansion, id } order -- the (now-deleted) legacy
    // WorldSession::SendAuthResponse class loop wrote { id, expansion }, inconsistent with its
    // own race loop; that inconsistency is the tell it was wrong.
    const ExpansionInfo kClassExpansion[MAX_CLASSES - 1] =
    {
        { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 0 }, { 5, 0 }, { 6, 2 },
        { 7, 0 }, { 8, 0 }, { 9, 0 }, { 10, 4 }, { 11, 0 }
    };

    const ExpansionInfo kRaceExpansion[MAX_PLAYABLE_RACES] =
    {
        { 1, 0 }, { 2, 0 }, { 3, 0 }, { 4, 0 }, { 5, 0 }, { 6, 0 }, { 7, 0 }, { 8, 0 },
        { 9, 3 }, { 10, 1 }, { 11, 1 }, { 22, 3 }, { 24, 4 }, { 25, 4 }, { 26, 4 }
    };
}

namespace
{
    // The ONE serializer. Private to this TU: the three public entry points are the only way in,
    // which is what makes "AUTH_OK always carries the account block" and "27 is never sent"
    // structural rather than merely tested.
    void Build(WorldPacket& out, uint8 code, bool queued, uint32 queuePos,
               uint8 accountExpansion, uint8 serverExpansion)
    {
        const bool hasAccountData = (code == AUTH_OK);

        out.Initialize(SMSG_AUTH_RESPONSE);

        // ---- BIT SECTION (WriteBit packs MSB-first per byte; FlushBits zero-pads) --------------
        out.WriteBit(hasAccountData);
        if (hasAccountData)
        {
            out.WriteBits(0, 21);                   // realmCount -- no per-realm block emitted
            out.WriteBits(MAX_CLASSES - 1, 23);     // classActivationCount
            out.WriteBits(0, 21);                   // characterTemplateCount (none -> no template block)
            out.WriteBit(0);                        // unknown
            out.WriteBit(0);                        // unknown
            out.WriteBit(0);                        // unknown
            out.WriteBit(0);                        // unknown
            out.WriteBits(MAX_PLAYABLE_RACES, 23);  // raceActivationCount
            out.WriteBit(0);                        // unknown
        }

        out.WriteBit(queued);
        if (queued)
        {
            out.WriteBit(0);                        // unknown queue-related bit
        }

        out.FlushBits();

        // ---- BYTE SECTION ----------------------------------------------------------------------
        if (queued)
        {
            out << uint32(queuePos);
        }

        if (hasAccountData)
        {
            // GOTCHA: the bit section counts CLASS-then-RACE, but the payload is RACE-then-CLASS.
            for (uint8 i = 0; i < MAX_PLAYABLE_RACES; ++i)
            {
                out << uint8(kRaceExpansion[i].expansion);
                out << uint8(kRaceExpansion[i].raceOrClass);
            }

            for (uint8 i = 0; i < MAX_CLASSES - 1; ++i)
            {
                out << uint8(kClassExpansion[i].expansion);
                out << uint8(kClassExpansion[i].raceOrClass);
            }

            out << uint32(0);
            out << uint8(accountExpansion);
            out << uint32(0);                       // BillingTimeRemaining
            out << uint32(0);                       // playtime
            out << uint8(serverExpansion);
            out << uint32(0);
            out << uint32(0);
            out << uint32(0);
        }

        out << uint8(code);                         // ALWAYS LAST
    }
}

namespace MopAuth
{
    void BuildAuthResponseAccepted(WorldPacket& out, uint8 accountExpansion, uint8 serverExpansion)
    {
        Build(out, AUTH_OK, false, 0, accountExpansion, serverExpansion);
    }

    void BuildAuthResponseQueued(WorldPacket& out, uint32 queuePos, uint8 accountExpansion,
                                 uint8 serverExpansion)
    {
        // A RELEASE is position 0, and it is BYTE-IDENTICAL to Accepted -- not a bare AUTH_OK.
        // Routing it here rather than making callers remember keeps the rule in one place.
        if (queuePos == 0)
        {
            BuildAuthResponseAccepted(out, accountExpansion, serverExpansion);
            return;
        }

        Build(out, AUTH_OK, true, queuePos, accountExpansion, serverExpansion);
    }

    void BuildAuthResponseError(WorldPacket& out, uint8 code)
    {
        // Refuse the two codes that are wrong BY CONSTRUCTION on this path:
        //   AUTH_OK (12) -- an "error" of 12 with no account block is rewritten by the client to 13
        //     anyway (sub_140A70980), so it is a caller bug that would present as a mystery failure;
        //   AUTH_WAIT_QUEUE (27) -- the client SYNTHESISES 27 from the queued bit. An emitted 27
        //     lands in the != 12 path and delivers 27, i.e. failure by a confusing route. Four did
        //     exactly this at WorldSessionMgr.cpp:248 and WorldSession.cpp:1008 before Stage 2.
        // Coerce + log rather than silently pass: the client gets an honest failure and the operator
        // gets told. This is what makes "never emits 27" a property of the SERIALIZER, testable
        // without trusting any caller.
        if (code == AUTH_OK || code == AUTH_WAIT_QUEUE)
        {
            sLog.outError("MopAuth::BuildAuthResponseError: refusing to emit code %u on the error "
                          "path; sending AUTH_FAILED instead.", uint32(code));
            code = AUTH_FAILED;
        }

        Build(out, code, false, 0, 0, 0);
    }
}
