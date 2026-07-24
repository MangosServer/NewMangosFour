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

#ifndef MANGOS_H_NPCHANDLER
#define MANGOS_H_NPCHANDLER

#include "ObjectGuid.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <array>

namespace MopNpcTextPackets
{
    struct Request
    {
        uint32 textId = 0;
        ObjectGuid sourceGuid;
    };

    struct Response
    {
        uint32 textId = 0;
        bool found = false;
        std::array<float, 8> probabilities = {};
        std::array<uint32, 8> broadcastTextIds = {};
    };

    inline bool RejectRequest(WorldPacket& in)
    {
        in.rfinish();
        return false;
    }

    inline size_t PresentByteCount(uint8 mask)
    {
        size_t count = 0;
        for (; mask != 0; mask >>= 1)
        {
            count += mask & 1;
        }
        return count;
    }

    inline bool ParseRequest(WorldPacket& in, Request& request)
    {
        if (in.size() - in.rpos() < 5)
        {
            return RejectRequest(in);
        }

        uint8 const mask = in[in.rpos() + 4];
        if (in.size() - in.rpos() != 5 + PresentByteCount(mask))
        {
            return RejectRequest(in);
        }

        Request parsed;
        in >> parsed.textId;
        in.ReadGuidMask<4, 5, 1, 7, 0, 2, 6, 3>(parsed.sourceGuid);
        in.ReadGuidBytes<4, 0, 2, 5, 1, 7, 3, 6>(parsed.sourceGuid);
        if (in.rpos() != in.size())
        {
            return RejectRequest(in);
        }

        request = parsed;
        return true;
    }

    inline void BuildResponse(WorldPacket& out, Response const& response)
    {
        uint32 const recordSize = response.found ? 64 : 0;
        out.Initialize(SMSG_NPC_TEXT_UPDATE, 9 + recordSize);
        out << response.textId << recordSize;
        if (response.found)
        {
            for (float probability : response.probabilities)
            {
                out << probability;
            }
            for (uint32 broadcastTextId : response.broadcastTextIds)
            {
                out << broadcastTextId;
            }
        }

        // The final bit controls insertion into the client's WNPC cache.
        out.WriteBit(response.found);
        out.FlushBits();
    }
}

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct PageText
{
    uint32 Page_ID;
    char* Text;

    uint32 Next_Page;
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

struct PageTextLocale
{
    std::vector<std::string> Text;
};

struct NpcTextLocale
{
    NpcTextLocale() { Text_0.resize(8); Text_1.resize(8); }

    std::vector<std::vector<std::string> > Text_0;
    std::vector<std::vector<std::string> > Text_1;
};

struct QEmote
{
    uint32 _Emote;
    uint32 _Delay;
};

struct GossipTextOption
{
    std::string Text_0;
    std::string Text_1;
    // Build 18414 resolves NPC text client-side through BroadcastText.db2.
    uint32 BroadcastTextId = 0;
    uint32 Language;
    float Probability;
    QEmote Emotes[3];
};

#define MAX_GOSSIP_TEXT_OPTIONS 8

struct GossipText
{
    GossipTextOption Options[MAX_GOSSIP_TEXT_OPTIONS];
};

namespace MopNpcTextPackets
{
    inline Response MakeResponse(uint32 textId, GossipText const* gossip)
    {
        Response response;
        response.textId = textId;
        if (!gossip)
        {
            return response;
        }

        for (size_t index = 0; index < MAX_GOSSIP_TEXT_OPTIONS; ++index)
        {
            uint32 const broadcastTextId =
                gossip->Options[index].BroadcastTextId;
            response.broadcastTextIds[index] = broadcastTextId;

            // A probability without a resolvable ID lets the client select a
            // blank record, so unmapped alternatives must not participate.
            response.probabilities[index] = broadcastTextId
                ? gossip->Options[index].Probability
                : 0.0f;
            response.found = response.found || broadcastTextId != 0;
        }

        return response;
    }
}

#endif
