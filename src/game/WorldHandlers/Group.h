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

/**
 * @file Group.h
 * @brief Player group/raid management and coordination.
 *
 * This file defines the Group class which manages collections of players in groups
 * (up to 5 members) or raids (up to 40 members). Groups provide coordinated features including:
 *
 * - Member management (joining, leaving, kicking)
 * - Loot distribution and roll systems
 * - Experience and reputation sharing
 * - Group-wide spell effects and auras
 * - Raid target markers
 * - Dungeon and raid lockout management
 * - Battleground and arena participation
 * - Group chat and communication
 * - Summoning mechanics
 * - Offline player tracking
 * - Group role assignment (tank, healer, damage)
 *
 * Groups can be of different types: normal group, raid, raid sub-group, or battleground group.
 * Various loot methods are supported: free-for-all, round-robin, master loot, group loot, and need-before-greed.
 *
 * @see Group for the main group implementation
 * @see GroupReference for member references
 * @see LootMgr for loot management
 */

#ifndef MANGOSSERVER_GROUP_H
#define MANGOSSERVER_GROUP_H

#include "Common.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "ObjectGuid.h"
#include "GroupReference.h"
#include "GroupRefManager.h"
#include "BattleGround.h"
#include "LootMgr.h"
#include "DBCEnums.h"
#include "SharedDefines.h"

#include <map>
#include <string>
#include <vector>

struct ItemPrototype;

class WorldSession;
class Map;
class BattleGround;
class DungeonPersistentState;
class Field;
class Unit;
class ByteBuffer;
class WorldPacket;

namespace MopPartyStatsPackets
{
    struct Request
    {
        uint8 mode = 0;
        uint64 memberGuid = 0;
    };

    Request ReadRequest(WorldPacket& in);
    void BuildResponse(WorldPacket& out, uint64 memberGuid, uint32 updateMask,
        bool resetState, bool associateGuid, ByteBuffer const& payload);
}

namespace MopPartyUpdatePackets
{
    struct Member
    {
        uint64 guid = 0;
        std::string name;
        uint8 roles = 0;
        uint8 status = 0;
        uint8 subgroup = 0;
        uint8 flags = 0;
    };

    struct PartyUpdate
    {
        uint64 groupGuid = 0;
        uint64 leaderGuid = 0;
        uint64 looterGuid = 0;
        std::vector<Member> members;

        bool hasInstanceDifficulty = false;
        uint32 raidDifficulty = 0;
        uint32 dungeonDifficulty = 0;

        bool hasLootMode = false;
        uint8 lootMethod = 0;
        uint8 lootThreshold = 0;

        bool isLfg = false;
        bool lfgUnknownBit0 = false;
        bool lfgUnknownBit1 = false;
        float lfgFloat = 1.0f;
        uint8 lfgUnknownByte0 = 0;
        uint8 lfgUnknownByte1 = 0;
        uint32 lfgDungeonEntry = 0;
        uint8 lfgUnknownByte2 = 0;
        uint8 lfgUnknownByte3 = 0;
        uint8 lfgUnknownByte4 = 0;
        uint32 lfgTail = 0;

        uint8 groupType = 0;
        uint8 partyIndex = 0;
        int32 groupPosition = 0;
        uint32 sequence = 0;
        uint8 groupRole = 0;
    };

    uint8 ReadRequest(WorldPacket& in);
    bool BuildPartyUpdate(WorldPacket& out, PartyUpdate const& update);
    bool BuildRemovedPartyUpdate(WorldPacket& out, PartyUpdate const& update);
}

namespace MopReadyCheckPackets
{
    struct ResponseRequest
    {
        uint8 partyIndex = 0;
        uint64 reservedGuid = 0;
        bool ready = false;
    };

    uint8 ReadStartRequest(WorldPacket& in);
    ResponseRequest ReadResponseRequest(WorldPacket& in);

    void BuildStarted(WorldPacket& out, uint64 groupGuid, uint64 initiatorGuid,
        uint32 duration, uint8 partyIndex);
    void BuildResponse(WorldPacket& out, uint64 groupGuid, uint64 playerGuid,
        bool ready);
    void BuildCompleted(WorldPacket& out, uint64 groupGuid, uint8 partyIndex);
}

namespace MopGroupMarkerPackets
{
    struct MinimapPingRequest
    {
        float x = 0.0f;
        float y = 0.0f;
        uint8 context = 0;
    };

    struct RaidTargetRequest
    {
        uint8 context = 0;
        uint8 icon = 0;
        uint64 targetGuid = 0;
    };

    struct TargetIcon
    {
        uint8 icon = 0;
        uint64 targetGuid = 0;
    };

    MinimapPingRequest ReadMinimapPingRequest(WorldPacket& in);
    void BuildMinimapPing(WorldPacket& out, uint64 authorGuid, float x, float y);
    RaidTargetRequest ReadRaidTargetRequest(WorldPacket& in);
    void BuildRaidTargetSingle(WorldPacket& out, uint64 authorGuid,
        uint64 targetGuid, uint8 icon, uint8 context);
    bool BuildRaidTargetAll(WorldPacket& out,
        std::vector<TargetIcon> const& targets, uint8 context);
}

namespace MopGroupPacketDetail
{
    inline uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (index * 8));
    }

    inline uint64 AssembleGuid(uint8 const bytes[8])
    {
        uint64 guid = 0;
        for (uint8 index = 0; index < 8; ++index)
            guid |= uint64(bytes[index]) << (index * 8);
        return guid;
    }

    inline bool Validate(MopPartyUpdatePackets::PartyUpdate const& update)
    {
        if (update.members.size() >= (size_t(1) << 21))
            return false;

        for (MopPartyUpdatePackets::Member const& member : update.members)
            if (member.name.size() >= (size_t(1) << 6))
                return false;

        return true;
    }
}

inline MopGroupMarkerPackets::MinimapPingRequest
MopGroupMarkerPackets::ReadMinimapPingRequest(WorldPacket& in)
{
    MinimapPingRequest request;
    in >> request.y;
    in >> request.x;
    in >> request.context;
    return request;
}

inline void MopGroupMarkerPackets::BuildMinimapPing(WorldPacket& out,
    uint64 authorGuid, float x, float y)
{
    uint8 const maskOrder[] = { 0, 5, 2, 7, 1, 3, 6, 4 };
    uint8 const byteOrder[] = { 6, 5, 7, 2, 0, 3, 1, 4 };

    out.Initialize(SMSG_MINIMAP_PING, 17);
    out << y;
    out << x;
    for (uint8 index : maskOrder)
        out.WriteBit(MopGroupPacketDetail::GuidByte(authorGuid, index) != 0);
    out.FlushBits();
    for (uint8 index : byteOrder)
        out.WriteByteSeq(MopGroupPacketDetail::GuidByte(authorGuid, index));
}

inline MopGroupMarkerPackets::RaidTargetRequest
MopGroupMarkerPackets::ReadRaidTargetRequest(WorldPacket& in)
{
    RaidTargetRequest request;
    uint8 guidBytes[8] = {};
    uint8 const maskOrder[] = { 3, 2, 1, 5, 0, 6, 7, 4 };
    uint8 const byteOrder[] = { 2, 3, 0, 7, 5, 1, 6, 4 };

    in >> request.context;
    in >> request.icon;
    for (uint8 index : maskOrder)
        guidBytes[index] = in.ReadBit();
    for (uint8 index : byteOrder)
        in.ReadByteSeq(guidBytes[index]);
    request.targetGuid = MopGroupPacketDetail::AssembleGuid(guidBytes);
    return request;
}

inline void MopGroupMarkerPackets::BuildRaidTargetSingle(WorldPacket& out,
    uint64 authorGuid, uint64 targetGuid, uint8 icon, uint8 context)
{
    out.Initialize(SMSG_RAID_TARGET_UPDATE_SINGLE, 20);
    out.WriteBit(MopGroupPacketDetail::GuidByte(authorGuid, 6) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(targetGuid, 4) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(authorGuid, 0) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(authorGuid, 7) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(targetGuid, 6) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(authorGuid, 5) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(authorGuid, 3) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(authorGuid, 4) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(targetGuid, 7) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(targetGuid, 2) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(targetGuid, 5) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(targetGuid, 1) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(authorGuid, 2) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(authorGuid, 1) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(targetGuid, 0) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(targetGuid, 3) != 0);
    out.FlushBits();

    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(targetGuid, 1));
    out << context;
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(authorGuid, 0));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(authorGuid, 5));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(authorGuid, 3));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(targetGuid, 7));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(targetGuid, 6));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(authorGuid, 1));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(targetGuid, 2));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(targetGuid, 4));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(targetGuid, 0));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(targetGuid, 3));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(targetGuid, 5));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(authorGuid, 6));
    out << icon;
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(authorGuid, 4));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(authorGuid, 2));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(authorGuid, 7));
}

inline bool MopGroupMarkerPackets::BuildRaidTargetAll(WorldPacket& out,
    std::vector<TargetIcon> const& targets, uint8 context)
{
    if (targets.size() >= (size_t(1) << 17))
        return false;

    uint8 const maskOrder[] = { 2, 1, 3, 7, 6, 4, 0, 5 };
    uint8 const firstBytes[] = { 4, 7, 1, 0, 6, 5, 3 };

    out.Initialize(SMSG_RAID_TARGET_UPDATE_ALL, 4 + targets.size() * 10);
    out.WriteBits(uint32(targets.size()), 17);
    for (TargetIcon const& target : targets)
        for (uint8 index : maskOrder)
            out.WriteBit(MopGroupPacketDetail::GuidByte(target.targetGuid, index) != 0);
    out.FlushBits();

    for (TargetIcon const& target : targets)
    {
        for (uint8 index : firstBytes)
            out.WriteByteSeq(MopGroupPacketDetail::GuidByte(target.targetGuid, index));
        out << target.icon;
        out.WriteByteSeq(MopGroupPacketDetail::GuidByte(target.targetGuid, 2));
    }
    out << context;
    return true;
}

inline MopPartyStatsPackets::Request MopPartyStatsPackets::ReadRequest(WorldPacket& in)
{
    uint8 const maskOrder[] = { 7, 4, 0, 1, 3, 6, 2, 5 };
    uint8 const byteOrder[] = { 3, 6, 5, 2, 1, 4, 0, 7 };
    uint8 guidBytes[8] = {};
    Request request;

    in >> request.mode;
    for (uint8 index : maskOrder)
        guidBytes[index] = in.ReadBit();
    for (uint8 index : byteOrder)
        in.ReadByteSeq(guidBytes[index]);

    for (uint8 index = 0; index < 8; ++index)
        request.memberGuid |= uint64(guidBytes[index]) << (index * 8);
    return request;
}

inline void MopPartyStatsPackets::BuildResponse(WorldPacket& out, uint64 memberGuid,
    uint32 updateMask, bool resetState, bool associateGuid,
    ByteBuffer const& payload)
{
    uint8 const firstBytes[] = { 3, 2, 6, 7, 5 };
    uint8 const trailingBytes[] = { 1, 4, 0 };

    out.Initialize(SMSG_PARTY_MEMBER_STATS,
        2 + 8 + sizeof(updateMask) + sizeof(uint32) + payload.size());
    out.WriteBit(MopGroupPacketDetail::GuidByte(memberGuid, 0) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(memberGuid, 5) != 0);
    out.WriteBit(resetState);
    out.WriteBit(MopGroupPacketDetail::GuidByte(memberGuid, 1) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(memberGuid, 4) != 0);
    out.WriteBit(associateGuid);
    out.WriteBit(MopGroupPacketDetail::GuidByte(memberGuid, 6) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(memberGuid, 2) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(memberGuid, 7) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(memberGuid, 3) != 0);
    out.FlushBits();

    for (uint8 index : firstBytes)
        out.WriteByteSeq(MopGroupPacketDetail::GuidByte(memberGuid, index));
    out << updateMask;
    for (uint8 index : trailingBytes)
        out.WriteByteSeq(MopGroupPacketDetail::GuidByte(memberGuid, index));
    out << uint32(payload.size());
    if (!payload.empty())
        out.append(payload.contents(), payload.size());
}

inline uint8 MopPartyUpdatePackets::ReadRequest(WorldPacket& in)
{
    uint8 partyIndex = 0;
    in >> partyIndex;
    return partyIndex;
}

inline bool MopPartyUpdatePackets::BuildPartyUpdate(WorldPacket& out,
    PartyUpdate const& update)
{
    if (!MopGroupPacketDetail::Validate(update))
        return false;

    uint64 const groupGuid = update.groupGuid;
    uint64 const leaderGuid = update.leaderGuid;
    uint64 const looterGuid = update.looterGuid;
    ByteBuffer memberData;

    out.Initialize(SMSG_GROUP_LIST, 64 + update.members.size() * 24);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 0) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(leaderGuid, 7) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(leaderGuid, 1) != 0);
    out.WriteBit(update.hasInstanceDifficulty);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 7) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(leaderGuid, 6) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(leaderGuid, 5) != 0);
    out.WriteBits(uint32(update.members.size()), 21);

    for (Member const& member : update.members)
    {
        uint64 const memberGuid = member.guid;
        uint8 const maskOrder[] = { 1, 2, 5, 6 };
        for (uint8 index : maskOrder)
            out.WriteBit(MopGroupPacketDetail::GuidByte(memberGuid, index) != 0);
        out.WriteBits(uint32(member.name.size()), 6);
        uint8 const finalMaskOrder[] = { 7, 3, 0, 4 };
        for (uint8 index : finalMaskOrder)
            out.WriteBit(MopGroupPacketDetail::GuidByte(memberGuid, index) != 0);

        memberData.WriteByteSeq(MopGroupPacketDetail::GuidByte(memberGuid, 6));
        memberData.WriteByteSeq(MopGroupPacketDetail::GuidByte(memberGuid, 3));
        memberData << member.roles << member.status;
        memberData.WriteByteSeq(MopGroupPacketDetail::GuidByte(memberGuid, 7));
        memberData.WriteByteSeq(MopGroupPacketDetail::GuidByte(memberGuid, 4));
        memberData.WriteByteSeq(MopGroupPacketDetail::GuidByte(memberGuid, 1));
        memberData.append(member.name.data(), member.name.size());
        memberData.WriteByteSeq(MopGroupPacketDetail::GuidByte(memberGuid, 5));
        memberData.WriteByteSeq(MopGroupPacketDetail::GuidByte(memberGuid, 2));
        memberData << member.subgroup;
        memberData.WriteByteSeq(MopGroupPacketDetail::GuidByte(memberGuid, 0));
        memberData << member.flags;
    }

    out.WriteBit(MopGroupPacketDetail::GuidByte(leaderGuid, 3) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(leaderGuid, 0) != 0);
    out.WriteBit(update.hasLootMode);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 5) != 0);
    if (update.hasLootMode)
    {
        uint8 const looterMaskOrder[] = { 6, 4, 5, 2, 1, 0, 7, 3 };
        for (uint8 index : looterMaskOrder)
            out.WriteBit(MopGroupPacketDetail::GuidByte(looterGuid, index) != 0);
    }
    uint8 const groupMaskOrder[] = { 2, 4, 1 };
    for (uint8 index : groupMaskOrder)
        out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, index) != 0);
    out.WriteBit(update.isLfg);
    out.WriteBit(MopGroupPacketDetail::GuidByte(leaderGuid, 2) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 6) != 0);
    if (update.isLfg)
    {
        out.WriteBit(update.lfgUnknownBit0);
        out.WriteBit(update.lfgUnknownBit1);
    }
    out.WriteBit(MopGroupPacketDetail::GuidByte(leaderGuid, 4) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 3) != 0);
    out.FlushBits();

    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(leaderGuid, 0));
    if (update.hasInstanceDifficulty)
    {
        out << update.raidDifficulty;
        out << update.dungeonDifficulty;
    }
    out.append(memberData);
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 1));

    if (update.isLfg)
    {
        out << update.lfgFloat;
        out << update.lfgUnknownByte0 << update.lfgUnknownByte1;
        out << update.lfgDungeonEntry;
        out << update.lfgUnknownByte2 << update.lfgUnknownByte3 << update.lfgUnknownByte4;
        out << update.lfgTail;
    }

    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(leaderGuid, 4));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(leaderGuid, 2));
    if (update.hasLootMode)
    {
        out << update.lootMethod;
        uint8 const looterBytesFirst[] = { 0, 5, 4, 3, 2 };
        for (uint8 index : looterBytesFirst)
            out.WriteByteSeq(MopGroupPacketDetail::GuidByte(looterGuid, index));
        out << update.lootThreshold;
        uint8 const looterBytesLast[] = { 7, 1, 6 };
        for (uint8 index : looterBytesLast)
            out.WriteByteSeq(MopGroupPacketDetail::GuidByte(looterGuid, index));
    }

    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 6));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 4));
    out << update.groupType;
    out << update.partyIndex;
    out << update.groupPosition;
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 7));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(leaderGuid, 3));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(leaderGuid, 1));
    out << update.sequence;
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 0));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 2));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 5));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 3));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(leaderGuid, 7));
    out << update.groupRole;
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(leaderGuid, 5));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(leaderGuid, 6));
    return true;
}

inline bool MopPartyUpdatePackets::BuildRemovedPartyUpdate(WorldPacket& out,
    PartyUpdate const& update)
{
    if (!update.members.empty() || update.hasInstanceDifficulty ||
        update.hasLootMode || update.isLfg)
        return false;
    return BuildPartyUpdate(out, update);
}

inline uint8 MopReadyCheckPackets::ReadStartRequest(WorldPacket& in)
{
    uint8 partyIndex = 0;
    in >> partyIndex;
    return partyIndex;
}

inline MopReadyCheckPackets::ResponseRequest
MopReadyCheckPackets::ReadResponseRequest(WorldPacket& in)
{
    ResponseRequest request;
    uint8 guidBytes[8] = {};

    in >> request.partyIndex;
    uint8 const maskOrder[] = { 2, 1, 0, 3, 6 };
    for (uint8 index : maskOrder)
        guidBytes[index] = in.ReadBit();
    request.ready = in.ReadBit();
    uint8 const finalMaskOrder[] = { 7, 4, 5 };
    for (uint8 index : finalMaskOrder)
        guidBytes[index] = in.ReadBit();

    uint8 const byteOrder[] = { 1, 0, 3, 2, 4, 5, 7, 6 };
    for (uint8 index : byteOrder)
        in.ReadByteSeq(guidBytes[index]);

    request.reservedGuid = MopGroupPacketDetail::AssembleGuid(guidBytes);
    return request;
}

inline void MopReadyCheckPackets::BuildStarted(WorldPacket& out, uint64 groupGuid,
    uint64 initiatorGuid, uint32 duration, uint8 partyIndex)
{
    uint8 const maskOwner[] = { 4, 2 };
    uint8 const maskInitiatorFirst[] = { 4 };
    uint8 const maskOwnerMiddle[] = { 3, 7, 1, 0 };
    uint8 const maskInitiatorMiddle[] = { 6, 5 };
    uint8 const maskOwnerLast[] = { 6, 5 };
    uint8 const maskInitiatorLast[] = { 0, 1, 2, 7, 3 };

    out.Initialize(SMSG_RAID_READY_CHECK, 32);
    for (uint8 index : maskOwner)
        out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, index) != 0);
    for (uint8 index : maskInitiatorFirst)
        out.WriteBit(MopGroupPacketDetail::GuidByte(initiatorGuid, index) != 0);
    for (uint8 index : maskOwnerMiddle)
        out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, index) != 0);
    for (uint8 index : maskInitiatorMiddle)
        out.WriteBit(MopGroupPacketDetail::GuidByte(initiatorGuid, index) != 0);
    for (uint8 index : maskOwnerLast)
        out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, index) != 0);
    for (uint8 index : maskInitiatorLast)
        out.WriteBit(MopGroupPacketDetail::GuidByte(initiatorGuid, index) != 0);
    out.FlushBits();

    out << duration;
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 2));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 7));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 3));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(initiatorGuid, 4));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 1));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 0));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(initiatorGuid, 1));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(initiatorGuid, 2));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(initiatorGuid, 6));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(initiatorGuid, 5));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 6));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(initiatorGuid, 0));
    out << partyIndex;
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(initiatorGuid, 7));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 4));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(initiatorGuid, 3));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 5));
}

inline void MopReadyCheckPackets::BuildResponse(WorldPacket& out, uint64 groupGuid,
    uint64 playerGuid, bool ready)
{
    out.Initialize(SMSG_RAID_READY_CHECK_CONFIRM, 24);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 4) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(playerGuid, 5) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(playerGuid, 3) != 0);
    out.WriteBit(ready);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 2) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(playerGuid, 6) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 3) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(playerGuid, 0) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(playerGuid, 1) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 1) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 5) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(playerGuid, 7) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(playerGuid, 4) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 6) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(playerGuid, 2) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 0) != 0);
    out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, 7) != 0);
    out.FlushBits();

    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(playerGuid, 4));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(playerGuid, 2));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(playerGuid, 1));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 4));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 2));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(playerGuid, 0));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 5));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 3));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(playerGuid, 7));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 6));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 1));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(playerGuid, 6));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(playerGuid, 3));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(playerGuid, 5));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 0));
    out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, 7));
}

inline void MopReadyCheckPackets::BuildCompleted(WorldPacket& out, uint64 groupGuid,
    uint8 partyIndex)
{
    uint8 const maskOrder[] = { 4, 2, 5, 7, 1, 0, 3, 6 };
    uint8 const firstBytes[] = { 6, 0, 3, 1, 5 };
    uint8 const finalBytes[] = { 7, 2, 4 };

    out.Initialize(SMSG_RAID_READY_CHECK_COMPLETED, 12);
    for (uint8 index : maskOrder)
        out.WriteBit(MopGroupPacketDetail::GuidByte(groupGuid, index) != 0);
    out.FlushBits();
    for (uint8 index : firstBytes)
        out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, index));
    out << partyIndex;
    for (uint8 index : finalBytes)
        out.WriteByteSeq(MopGroupPacketDetail::GuidByte(groupGuid, index));
}


#define MAX_GROUP_SIZE 5
#define MAX_RAID_SIZE 40
#define MAX_RAID_SUBGROUPS (MAX_RAID_SIZE / MAX_GROUP_SIZE)
#define TARGET_ICON_COUNT 8

/// @brief Loot distribution method enumeration.
///
/// Determines how loot is distributed among group members.
enum LootMethod
{
    FREE_FOR_ALL      = 0,
    ROUND_ROBIN       = 1,
    MASTER_LOOT       = 2,
    GROUP_LOOT        = 3,
    NEED_BEFORE_GREED = 4,

    NOT_GROUP_TYPE_LOOT = 5                                 // internal use only
};

/// @brief Loot roll vote enumeration.
///
/// Represents a player's vote during loot distribution rolls.
enum RollVote
{
    ROLL_PASS              = 0,
    ROLL_NEED              = 1,
    ROLL_GREED             = 2,
    ROLL_DISENCHANT        = 3,

    // other not send by client
    MAX_ROLL_FROM_CLIENT   = 4,

    ROLL_NOT_EMITED_YET    = 4,                             // send to client
    ROLL_NOT_VALID         = 5                              // not send to client
};

// set what votes allowed
enum RollVoteMask
{
    ROLL_VOTE_MASK_PASS       = 0x01,
    ROLL_VOTE_MASK_NEED       = 0x02,
    ROLL_VOTE_MASK_GREED      = 0x04,
    ROLL_VOTE_MASK_DISENCHANT = 0x08,

    ROLL_VOTE_MASK_ALL        = 0x0F,
};

/// @brief Group member online status flags.
///
/// Bit flags indicating the online status and state of group members.
enum GroupMemberFlags
{
    MEMBER_STATUS_OFFLINE   = 0x0000,
    MEMBER_STATUS_ONLINE    = 0x0001,                       // Lua_UnitIsConnected
    MEMBER_STATUS_PVP       = 0x0002,                       // Lua_UnitIsPVP
    MEMBER_STATUS_DEAD      = 0x0004,                       // Lua_UnitIsDead
    MEMBER_STATUS_GHOST     = 0x0008,                       // Lua_UnitIsGhost
    MEMBER_STATUS_PVP_FFA   = 0x0010,                       // Lua_UnitIsPVPFreeForAll
    MEMBER_STATUS_UNK3      = 0x0020,                       // used in calls from Lua_GetPlayerMapPosition/Lua_GetBattlefieldFlagPosition
    MEMBER_STATUS_AFK       = 0x0040,                       // Lua_UnitIsAFK
    MEMBER_STATUS_DND       = 0x0080,                       // Lua_UnitIsDND
};

/// @brief Group type enumeration.
///
/// Defines the type and size category of a group.
enum GroupType
{
    GROUPTYPE_NORMAL = 0x00,
    GROUPTYPE_BG     = 0x01,
    GROUPTYPE_RAID   = 0x02,
    GROUPTYPE_BGRAID = GROUPTYPE_BG | GROUPTYPE_RAID,       // mask
    // 0x04?
    GROUPTYPE_LFD    = 0x08,
    // 0x10, leave/change group?, I saw this flag when leaving group and after leaving BG while in group
};

enum GroupFlagMask
{
    GROUP_ASSISTANT      = 0x01,
    GROUP_MAIN_ASSISTANT = 0x02,
    GROUP_MAIN_TANK      = 0x04,
};

enum GroupUpdateFlags
{
    GROUP_UPDATE_FLAG_NONE              = 0x00000000,       // nothing
    GROUP_UPDATE_FLAG_STATUS            = 0x00000001,       // uint16, flags
    GROUP_UPDATE_FLAG_MOP_EXTRA         = 0x00000002,       // uint16, binary-proven extra state
    GROUP_UPDATE_FLAG_CUR_HP            = 0x00000004,       // uint32
    GROUP_UPDATE_FLAG_MAX_HP            = 0x00000008,       // uint32
    GROUP_UPDATE_FLAG_POWER_TYPE        = 0x00000010,       // uint8
    GROUP_UPDATE_FLAG_POWER_EXTRA       = 0x00000020,       // uint16, binary-proven extra power state
    GROUP_UPDATE_FLAG_CUR_POWER         = 0x00000040,       // uint16
    GROUP_UPDATE_FLAG_MAX_POWER         = 0x00000080,       // uint16
    GROUP_UPDATE_FLAG_LEVEL             = 0x00000100,       // uint16
    GROUP_UPDATE_FLAG_ZONE              = 0x00000200,       // uint16
    GROUP_UPDATE_FLAG_UNK               = 0x00000400,       // uint16, area/extra location
    GROUP_UPDATE_FLAG_POSITION          = 0x00000800,       // uint16, uint16, uint16
    GROUP_UPDATE_FLAG_AURAS             = 0x00001000,       // MoP aura block
    GROUP_UPDATE_FLAG_PET_GUID          = 0x00002000,       // uint64 pet guid
    GROUP_UPDATE_FLAG_PET_NAME          = 0x00004000,       // pet name, NULL terminated string
    GROUP_UPDATE_FLAG_PET_MODEL_ID      = 0x00008000,       // uint16, model id
    GROUP_UPDATE_FLAG_PET_CUR_HP        = 0x00010000,       // uint32 pet cur health
    GROUP_UPDATE_FLAG_PET_MAX_HP        = 0x00020000,       // uint32 pet max health
    GROUP_UPDATE_FLAG_PET_POWER_TYPE    = 0x00040000,       // uint8 pet power type
    GROUP_UPDATE_FLAG_PET_EXTRA         = 0x00080000,       // uint16, binary-proven extra pet state
    GROUP_UPDATE_FLAG_PET_CUR_POWER     = 0x00100000,       // uint16 pet cur power
    GROUP_UPDATE_FLAG_PET_MAX_POWER     = 0x00200000,       // uint16 pet max power
    GROUP_UPDATE_FLAG_PET_AURAS         = 0x00400000,       // MoP pet aura block
    GROUP_UPDATE_FLAG_VEHICLE_SEAT      = 0x00800000,       // uint32 vehicle_seat_id
    GROUP_UPDATE_FLAG_PHASE             = 0x01000000,       // uint32 flags, 23-bit phase count, uint16 phases
    GROUP_UPDATE_FLAG_EXTRA_16          = 0x02000000,       // uint16, semantics unknown
    GROUP_UPDATE_FLAG_EXTRA_32          = 0x04000000,       // uint32, semantics unknown

    GROUP_UPDATE_PET = GROUP_UPDATE_FLAG_PET_GUID |
        GROUP_UPDATE_FLAG_PET_NAME |
        GROUP_UPDATE_FLAG_PET_MODEL_ID |
        GROUP_UPDATE_FLAG_PET_CUR_HP |
        GROUP_UPDATE_FLAG_PET_MAX_HP |
        GROUP_UPDATE_FLAG_PET_POWER_TYPE |
        GROUP_UPDATE_FLAG_PET_CUR_POWER |
        GROUP_UPDATE_FLAG_PET_MAX_POWER |
        GROUP_UPDATE_FLAG_PET_AURAS,

    GROUP_UPDATE_FULL = GROUP_UPDATE_PET |
        GROUP_UPDATE_FLAG_STATUS |
        GROUP_UPDATE_FLAG_CUR_HP |
        GROUP_UPDATE_FLAG_MAX_HP |
        GROUP_UPDATE_FLAG_POWER_TYPE |
        GROUP_UPDATE_FLAG_CUR_POWER |
        GROUP_UPDATE_FLAG_MAX_POWER |
        GROUP_UPDATE_FLAG_LEVEL |
        GROUP_UPDATE_FLAG_ZONE |
        GROUP_UPDATE_FLAG_POSITION |
        GROUP_UPDATE_FLAG_AURAS |
        GROUP_UPDATE_FLAG_VEHICLE_SEAT |
        GROUP_UPDATE_FLAG_PHASE,
};

class Roll : public LootValidatorRef
{
    public:
        Roll(ObjectGuid _lootedTragetGuid, LootMethod method, LootItem const& li)
            : lootedTargetGUID(_lootedTragetGuid), itemid(li.itemid), itemRandomPropId(li.randomPropertyId), itemRandomSuffix(li.randomSuffix),
              itemCount(li.count), totalPlayersRolling(0), totalNeed(0), totalGreed(0), totalPass(0), itemSlot(0),
              m_method(method), m_commonVoteMask(ROLL_VOTE_MASK_ALL) {}
        ~Roll() { }
        void setLoot(Loot* pLoot)
        {
            link(pLoot, this);
        }
        Loot* getLoot()
        {
            return getTarget();
        }
        void targetObjectBuildLink() override;

        void CalculateCommonVoteMask(uint32 max_enchanting_skill);
        RollVoteMask GetVoteMaskFor(Player* player) const;

        ObjectGuid lootedTargetGUID;
        uint32 itemid;
        int32  itemRandomPropId;
        uint32 itemRandomSuffix;
        uint8 itemCount;
        typedef std::unordered_map<ObjectGuid, RollVote> PlayerVote;
        PlayerVote playerVote;                              // vote position correspond with player position (in group)
        uint8 totalPlayersRolling;
        uint8 totalNeed;
        uint8 totalGreed;
        uint8 totalPass;
        uint8 itemSlot;

    private:
        LootMethod m_method;
        RollVoteMask m_commonVoteMask;
};

struct InstanceGroupBind
{
    DungeonPersistentState* state;
    bool perm;
    /* permanent InstanceGroupBinds exist iff the leader has a permanent
       PlayerInstanceBind for the same instance. */
    InstanceGroupBind() : state(NULL), perm(false) {}
};

/** request member stats checken **/
/** todo: uninvite people that not accepted invite **/
class Group
{
    public:
        /**
        * Struct MemberSlot
        * Represent a member of a group with some of its caracteristics
        */
        struct MemberSlot
        {
            /* GUID of the player. */
            ObjectGuid  guid;
            /* Name of the player. */
            std::string name;
            /* Group of the player. */
            uint8       group;
            /* Indicates whether the player is assistant. */
            bool        assistant;
            uint32      lastMap;
            bool        readyCheckHasResponded;
        };
        typedef std::list<MemberSlot> MemberSlotList;
        typedef MemberSlotList::const_iterator member_citerator;

        typedef std::unordered_map < uint32 /*mapId*/, InstanceGroupBind > BoundInstancesMap;
    protected:
        typedef MemberSlotList::iterator member_witerator;
        typedef std::set<Player*> InvitesList;
        typedef std::vector<Roll*> Rolls;

    public:
        Group();
        ~Group();

        // group manipulation methods
        bool   Create(ObjectGuid guid, const char* name);
        bool   LoadGroupFromDB(Field* fields);
        bool   LoadMemberFromDB(uint32 guidLow, uint8 subgroup, bool assistant);
        bool   AddInvite(Player* player);
        uint32 RemoveInvite(Player* player);
        void   RemoveAllInvites();
        bool   AddLeaderInvite(Player* player);
        bool   AddMember(ObjectGuid guid, const char* name);
        uint32 RemoveMember(ObjectGuid guid, uint8 method); // method: 0=just remove, 1=kick
        void   ChangeLeader(ObjectGuid guid);
        void   SetLootMethod(LootMethod method)
        {
            m_lootMethod = method;
        }
        void   SetLooterGuid(ObjectGuid guid)
        {
            m_looterGuid = guid;
        }
        void   UpdateLooterGuid(WorldObject* pSource, bool ifneed = false);
        void   SetLootThreshold(ItemQualities threshold)
        {
            m_lootThreshold = threshold;
        }
        void   Disband(bool hideDestroy = false);

        // properties accessories
        uint32 GetId() const
        {
            return m_Id;
        }
        ObjectGuid GetObjectGuid() const { return ObjectGuid(HIGHGUID_GROUP, GetId()); }
        bool IsFull() const
        {
            return (m_groupType == GROUPTYPE_NORMAL) ? (m_memberSlots.size() >= MAX_GROUP_SIZE) : (m_memberSlots.size() >= MAX_RAID_SIZE);
        }
        GroupType GetGroupType() const { return m_groupType; }
        bool isRaidGroup() const
        {
            return m_groupType == GROUPTYPE_RAID;
        }
        bool isBGGroup()   const
        {
            return m_bgGroup != NULL;
        }
        bool isLFGGroup() const { return m_groupType & GROUPTYPE_LFD; }
        bool IsCreated()   const
        {
            return GetMembersCount() > 0;
        }
        ObjectGuid GetLeaderGuid() const
        {
            return m_leaderGuid;
        }
        const char* GetLeaderName() const
        {
            return m_leaderName.c_str();
        }
        LootMethod    GetLootMethod() const
        {
            return m_lootMethod;
        }
        ObjectGuid GetLooterGuid() const
        {
            return m_looterGuid;
        }
        ItemQualities GetLootThreshold() const
        {
            return m_lootThreshold;
        }

        // member manipulation methods
        void SetAsLfgGroup() { m_groupType = GroupType(m_groupType | GROUPTYPE_LFD); }
        bool IsMember(ObjectGuid guid) const
        {
            return _getMemberCSlot(guid) != m_memberSlots.end();
        }
        bool IsLeader(ObjectGuid guid) const
        {
            return GetLeaderGuid() == guid;
        }
        ObjectGuid GetMemberGuid(const std::string& name)
        {
            for (member_citerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
                if (itr->name == name)
                {
                    return itr->guid;
                }

            return ObjectGuid();
        }
        bool IsAssistant(ObjectGuid guid) const
        {
            member_citerator mslot = _getMemberCSlot(guid);
            if (mslot == m_memberSlots.end())
            {
                return false;
            }

            return mslot->assistant;
        }
        Player* GetInvited(ObjectGuid guid) const;
        Player* GetInvited(const std::string& name) const;

        bool HasFreeSlotSubGroup(uint8 subgroup) const
        {
            return (m_subGroupsCounts && m_subGroupsCounts[subgroup] < MAX_GROUP_SIZE);
        }

        bool SameSubGroup(Player const* member1, Player const* member2) const;

        MemberSlotList const& GetMemberSlots() const { return m_memberSlots; }
        GroupReference* GetFirstMember() { return m_memberMgr.getFirst(); }
        GroupReference const* GetFirstMember() const { return m_memberMgr.getFirst(); }
        uint32 GetMembersCount() const { return m_memberSlots.size(); }
        void GetDataForXPAtKill(Unit const* victim, uint32& count, uint32& sum_level, Player*& member_with_max_level, Player*& not_gray_member_with_max_level, Player* additional = NULL);
        uint8 GetMemberGroup(ObjectGuid guid) const
        {
            member_citerator mslot = _getMemberCSlot(guid);
            if (mslot == m_memberSlots.end())
            {
                return MAX_RAID_SUBGROUPS + 1;
            }

            return mslot->group;
        }

        // some additional raid methods
        void ConvertToRaid();

        void SetBattlegroundGroup(BattleGround* bg)
        {
            m_bgGroup = bg;
        }
        GroupJoinBattlegroundResult CanJoinBattleGroundQueue(BattleGround const* bgOrTemplate, BattleGroundQueueTypeId bgQueueTypeId, uint32 MinPlayerCount, uint32 MaxPlayerCount);

        void ChangeMembersGroup(ObjectGuid guid, uint8 group);
        void ChangeMembersGroup(Player* player, uint8 group);

        ObjectGuid GetMainTankGuid() const
        {
            return m_mainTankGuid;
        }
        ObjectGuid GetMainAssistantGuid() const
        {
            return m_mainAssistantGuid;
        }

        void SetAssistant(ObjectGuid guid, bool state)
        {
            if (!isRaidGroup())
            {
                return;
            }
            if (_setAssistantFlag(guid, state))
            {
                SendUpdate();
            }
        }
        void SetMainTank(ObjectGuid guid)
        {
            if (!isRaidGroup())
            {
                return;
            }

            if (_setMainTank(guid))
            {
                SendUpdate();
            }
        }
        void SetMainAssistant(ObjectGuid guid)
        {
            if (!isRaidGroup())
            {
                return;
            }

            if (_setMainAssistant(guid))
            {
                SendUpdate();
            }
        }

        // context defaults to 0 (a normal target-icon set) so pre-MoP 3-argument callers --
        // notably Eluna's shared Group:SetTargetIcon binding -- stay source-compatible.
        void SetTargetIcon(uint8 id, ObjectGuid whoGuid, ObjectGuid targetGuid,
            uint8 context = 0);

        Difficulty GetDifficulty(bool isRaid) const { return isRaid ? m_raidDifficulty : m_dungeonDifficulty; }
        Difficulty GetDungeonDifficulty() const { return m_dungeonDifficulty; }
        Difficulty GetRaidDifficulty() const { return m_raidDifficulty; }
        void SetDungeonDifficulty(Difficulty difficulty);
        void SetRaidDifficulty(Difficulty difficulty);
        uint16 InInstance();
        bool InCombatToInstance(uint32 instanceId);
        void ResetInstances(InstanceResetMethod method, bool isRaid, Player* SendMsgTo);

        void SendTargetIconList(WorldSession* session);
        void SendUpdate();
        void SendUpdateToPlayer(ObjectGuid guid);
        void UpdatePlayerOutOfRange(Player* pPlayer);
        // ignore: GUID of player that will be ignored
        void BroadcastPacket(WorldPacket* packet, bool ignorePlayersInBGRaid, int group = -1, ObjectGuid ignore = ObjectGuid());
        void BroadcastReadyCheck(WorldPacket* packet);
        void OfflineReadyCheck();
        bool StartReadyCheck(uint8 partyIndex, ObjectGuid initiator);
        bool ReadyCheckMemberHasResponded(ObjectGuid guid);
        bool ReadyCheckAllResponded() const;
        bool ReadyCheckInProgress() const { return m_readyCheckActive; }
        bool IsReadyCheckInitiator(ObjectGuid guid) const
        {
            return m_readyCheckActive && m_readyCheckInitiator == guid;
        }
        uint8 GetReadyCheckPartyIndex() const { return m_readyCheckPartyIndex; }
        void CompleteReadyCheck();

        void RewardGroupAtKill(Unit* pVictim, Player* player_tap);

        bool SetPlayerMap(ObjectGuid guid, uint32 mapid);

        /*********************************************************/
        /***                   LOOT SYSTEM                     ***/
        /*********************************************************/

        void SendLootStartRoll(uint32 CountDown, uint32 mapid, const Roll& r);
        void SendLootRoll(ObjectGuid const& targetGuid, uint32 rollNumber, uint8 rollType, const Roll& r);
        void SendLootRollWon(ObjectGuid const& targetGuid, uint32 rollNumber, RollVote rollType, const Roll& r);
        void SendLootAllPassed(const Roll& r);
        void GroupLoot(WorldObject* pSource, Loot* loot);
        void NeedBeforeGreed(WorldObject* pSource, Loot* loot);
        void MasterLoot(WorldObject* pSource, Loot* loot);
        bool CountRollVote(Player* player, ObjectGuid const& lootedTarget, uint32 itemSlot, RollVote vote);
        void StartLootRoll(WorldObject* lootTarget, LootMethod method, Loot* loot, uint8 itemSlot, uint32 maxEnchantingSkill);
        void EndRoll();

        void LinkMember(GroupReference* pRef)
        {
            m_memberMgr.insertFirst(pRef);
        }
        void DelinkMember(GroupReference* /*pRef*/) { }

        InstanceGroupBind* BindToInstance(DungeonPersistentState* save, bool permanent, bool load = false);
        void UnbindInstance(uint32 mapid, uint8 difficulty, bool unload = false);
        InstanceGroupBind* GetBoundInstance(uint32 mapId, Player* player);
        InstanceGroupBind* GetBoundInstance(Map* aMap, Difficulty difficulty);
        BoundInstancesMap& GetBoundInstances(Difficulty difficulty) { return m_boundInstances[difficulty]; }

    protected:
        bool _addMember(ObjectGuid guid, const char* name, bool isAssistant = false);
        bool _addMember(ObjectGuid guid, const char* name, bool isAssistant, uint8 group);
        bool _removeMember(ObjectGuid guid);                // returns true if leader has changed
        void _setLeader(ObjectGuid guid);

        void _removeRolls(ObjectGuid guid);

        void _updateLeaderFlag(const bool remove = false);

        bool _setMembersGroup(ObjectGuid guid, uint8 group);
        bool _setAssistantFlag(ObjectGuid guid, const bool& state);
        bool _setMainTank(ObjectGuid guid);
        bool _setMainAssistant(ObjectGuid guid);

        void _homebindIfInstance(Player* player);
        void SendRemovedUpdate(Player* player);

        void _initRaidSubGroupsCounter()
        {
            // Sub group counters initialization
            if (!m_subGroupsCounts)
            {
                m_subGroupsCounts = new uint8[MAX_RAID_SUBGROUPS];
            }

            memset((void*)m_subGroupsCounts, 0, MAX_RAID_SUBGROUPS * sizeof(uint8));

            for (member_citerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
            {
                ++m_subGroupsCounts[itr->group];
            }
        }

        member_citerator _getMemberCSlot(ObjectGuid guid) const
        {
            for (member_citerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
                if (itr->guid == guid)
                {
                    return itr;
                }

            return m_memberSlots.end();
        }

        member_witerator _getMemberWSlot(ObjectGuid guid)
        {
            for (member_witerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
                if (itr->guid == guid)
                {
                    return itr;
                }

            return m_memberSlots.end();
        }

        void SubGroupCounterIncrease(uint8 subgroup)
        {
            if (m_subGroupsCounts)
            {
                ++m_subGroupsCounts[subgroup];
            }
        }

        void SubGroupCounterDecrease(uint8 subgroup)
        {
            if (m_subGroupsCounts)
            {
                --m_subGroupsCounts[subgroup];
            }
        }

        uint32 GetMaxSkillValueForGroup(SkillType skill);

        void CountTheRoll(Rolls::iterator& roll);           // iterator update to next, in CountRollVote if true
        bool CountRollVote(ObjectGuid const& playerGUID, Rolls::iterator& roll, RollVote vote);

        GroupFlagMask GetFlags(MemberSlot const& slot) const
        {
            uint8 flags = 0;
            if (slot.assistant)
            {
                flags |= GROUP_ASSISTANT;
            }
            if (slot.guid == m_mainAssistantGuid)
            {
                flags |= GROUP_MAIN_ASSISTANT;
            }
            if (slot.guid == m_mainTankGuid)
            {
                flags |= GROUP_MAIN_TANK;
            }
            return GroupFlagMask(flags);
        }

        uint32              m_Id;                           // 0 for not created or BG groups
        MemberSlotList      m_memberSlots;
        GroupRefManager     m_memberMgr;
        InvitesList         m_invitees;
        ObjectGuid          m_leaderGuid;
        std::string         m_leaderName;
        ObjectGuid          m_mainTankGuid;
        ObjectGuid          m_mainAssistantGuid;
        GroupType           m_groupType;
        Difficulty          m_dungeonDifficulty;
        Difficulty          m_raidDifficulty;
        BattleGround*       m_bgGroup;
        ObjectGuid          m_targetIcons[TARGET_ICON_COUNT];
        LootMethod          m_lootMethod;
        ItemQualities       m_lootThreshold;
        ObjectGuid          m_looterGuid;
        ObjectGuid          m_masterLooterGuid;
        ObjectGuid          m_currentLooterGuid;
        Rolls               RollId;
        BoundInstancesMap   m_boundInstances[MAX_DIFFICULTY];
        uint8*              m_subGroupsCounts;
        uint32              m_groupUpdateCounter;
        bool                m_readyCheckActive;
        uint8               m_readyCheckPartyIndex;
        ObjectGuid          m_readyCheckInitiator;
};
#endif
