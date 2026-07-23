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

/// \addtogroup u2w
/// @{
/// \file

#ifndef MANGOS_H_WORLDSESSION
#define MANGOS_H_WORLDSESSION

#include "Common.h"
#include "Auth/BigNumber.h"
#include "SharedDefines.h"
#include "ObjectGuid.h"
#include "AuctionHouseMgr.h"
#include "Item.h"
#include "LFGMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <array>
#include <mutex>
#include <string>
#include <vector>

struct ItemPrototype;
struct AuctionEntry;
struct AuctionHouseEntry;
struct DeclinedName;

class ObjectGuid;
class Creature;
class Item;
class Object;
class Player;
class Unit;
class Warden;
class WorldPacket;
class WorldSocket;
class QueryResult;
class LoginQueryHolder;
class CharacterHandler;
class GMTicket;
class MovementInfo;
class WorldSession;

namespace MopHotfixPackets
{
    struct HotfixRecord
    {
        uint64 guid = 0;
        uint32 entry = 0;
    };

    struct HotfixRequest
    {
        uint32 type = 0;
        std::vector<HotfixRecord> records;
    };

    inline bool ReadHotfixRequest(WorldPacket& in, HotfixRequest& request)
    {
        request.records.clear();
        in >> request.type;
        uint32 const count = in.ReadBits(21);
        size_t const remaining = in.size() - in.rpos();
        if (count > remaining / 5)
            return false;

        std::vector<ObjectGuid> guids(count);
        request.records.resize(count);
        for (ObjectGuid& guid : guids)
            in.ReadGuidMask<6, 3, 0, 1, 4, 5, 7, 2>(guid);

        for (uint32 i = 0; i < count; ++i)
        {
            in.ReadGuidBytes<1>(guids[i]);
            in >> request.records[i].entry;
            in.ReadGuidBytes<0, 5, 6, 4, 7, 2, 3>(guids[i]);
            request.records[i].guid = guids[i].GetRawValue();
        }
        return true;
    }

    inline void BuildDbReply(WorldPacket& out, uint32 entry, uint32 hotfixDate,
        uint32 type, ByteBuffer const& record)
    {
        out << entry;
        out << hotfixDate;
        out << type;
        out << uint32(record.size());
        out.append(record);
    }
}

namespace MopClientRequestPackets
{
    struct LoadScreenRequest
    {
        uint32 mapId = 0;
        bool loading = false;
    };

    inline LoadScreenRequest ReadLoadScreenRequest(WorldPacket& in)
    {
        LoadScreenRequest request;
        in >> request.mapId;
        request.loading = in.ReadBit();
        return request;
    }
}

namespace MopQueryPackets
{
    struct NameQueryRequest
    {
        uint64 guid = 0;
        bool hasRealmId2 = false;
        uint32 realmId2 = 0;
        bool hasRealmId1 = false;
        uint32 realmId1 = 0;
    };

    struct NameQueryResponse
    {
        uint64 guid = 0;
        uint8 result = 1;
        uint32 realmId = 0;
        uint32 accountId = 0;
        uint8 classId = 0;
        uint8 race = 0;
        uint8 level = 0;
        uint8 gender = 0;
        uint64 auxiliaryGuid = 0;
        uint64 displayGuid = 0;
        bool isDeleted = false;
        std::string name;
        std::array<std::string, 5> declinedNames;
    };

    NameQueryRequest ReadNameQueryRequest(WorldPacket& in);
    void BuildNameQueryResponse(WorldPacket& out,
        NameQueryResponse const& record);

    struct RealmNameQueryResponse
    {
        uint32 realmId = 0;
        uint8 status = 0;
        bool isHomeRealm = false;
        std::string name;
        std::string normalizedName;
    };

    uint32 ReadRealmNameQueryRequest(WorldPacket& in);
    void BuildRealmNameQueryResponse(WorldPacket& out,
        RealmNameQueryResponse const& record);

    void BuildQueryTimeResponse(WorldPacket& out, uint32 serverTime,
        uint32 secondsUntilReset);
    bool ReadPlayedTimeRequest(WorldPacket& in);
    void BuildPlayedTimeResponse(WorldPacket& out, uint32 totalPlayed,
        uint32 levelPlayed, bool displayEvent);

    struct MailNextTimeEntry
    {
        uint64 senderGuid = 0;
        uint32 nonPlayerSender = 0;
        uint8 messageType = 0;
        float deliveryTime = 0.0f;
        bool hasNativeRealmAddress = false;
        uint32 nativeRealmAddress = 0;
        uint32 stationery = 0;
        bool hasVirtualRealmAddress = false;
        uint32 virtualRealmAddress = 0;
    };

    bool BuildMailQueryNextTimeResult(WorldPacket& out,
        std::vector<MailNextTimeEntry> const& records, float nextMailTime);

    struct CreatureQueryResponse
    {
        uint32 entry = 0;
        bool hasData = false;
        std::string name;
        std::string subName;
        std::string iconName;
        uint32 creatureType = 0;
        uint32 family = 0;
        uint32 rank = 0;
        uint32 expansion = 0;
        uint32 movementTemplateId = 0;
        uint32 creatureTypeFlags = 0;
        uint32 creatureTypeFlags2 = 0;
        std::array<uint32, 4> modelIds{};
        std::array<uint32, 2> killCredits{};
        float healthMultiplier = 0.0f;
        float powerMultiplier = 0.0f;
        bool racialLeader = false;
        std::array<uint32, 6> questItems{};
    };

    void BuildCreatureQueryResponse(WorldPacket& out,
        CreatureQueryResponse const& record);

    struct GameObjectQueryRequest
    {
        uint32 entry = 0;
        uint64 guid = 0;
    };

    struct GameObjectQueryResponse
    {
        uint32 entry = 0;
        bool hasData = false;
        uint32 type = 0;
        uint32 displayId = 0;
        std::array<std::string, 4> names;
        std::string iconName;
        std::string castBarCaption;
        std::string unknownString;
        std::array<uint32, 32> data{};
        float size = 0.0f;
        std::vector<uint32> questItems;
        uint32 trailingUnknown = 0;
    };

    GameObjectQueryRequest ReadGameObjectQueryRequest(WorldPacket& in);
    void BuildGameObjectQueryResponse(WorldPacket& out,
        GameObjectQueryResponse const& record);

    struct CorpseQueryResponse
    {
        bool found = false;
        int32 displayMapId = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        uint32 corpseMapId = 0;
        uint64 transportGuid = 0;
    };

    uint64 ReadCorpseMapPositionQuery(WorldPacket& in);
    void BuildCorpseQueryResponse(WorldPacket& out,
        CorpseQueryResponse const& response);
    void BuildCorpseMapPositionQueryResponse(WorldPacket& out,
        float x, float y, float z, float orientation);
}

namespace MopStablePackets
{
    struct StablePetRecord
    {
        uint32 entry = 0;
        uint32 level = 0;
        uint8 state = 0;
        uint32 modelId = 0;
        std::string name;
        uint32 petNumber = 0;
        uint32 slot = 0;
    };

    uint64 ReadStableListRequest(WorldPacket& in);
    bool BuildPetStableList(WorldPacket& out, uint64 stableMasterGuid,
        std::vector<StablePetRecord> const& records);
    void BuildStableResult(WorldPacket& out, uint8 result);
}

namespace MopTrainerBuyFailed
{
    enum Reason
    {
        REASON_UNAVAILABLE = 0,
        REASON_NOT_ENOUGH_MONEY = 1
    };

    void Build(WorldPacket& out, uint64 trainerGuid, uint32 reason, uint32 serviceId);
}

namespace MopQueryPacketDetail
{
    inline uint8 GuidByte(uint64 guid, size_t index)
    {
        return uint8(guid >> (index * 8));
    }

    inline size_t OptionalCStringLength(std::string const& text, size_t bitCount)
    {
        size_t const encoded = text.empty() ? 0 : text.size() + 1;
        MANGOS_ASSERT(encoded < (size_t(1) << bitCount));
        return encoded;
    }
}

namespace MopStablePacketDetail
{
    inline uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (index * 8));
    }
}

namespace MopTrainerPacketDetail
{
    inline uint8 GuidByte(uint64 guid, int index)
    {
        return uint8(guid >> (8 * index));
    }

    inline constexpr int MaskOrder[8] = { 3, 0, 4, 7, 6, 1, 5, 2 };
    inline constexpr int BytesPre[5] = { 1, 2, 0, 3, 4 };
    inline constexpr int BytesPost[3] = { 5, 6, 7 };
}

inline MopQueryPackets::NameQueryRequest MopQueryPackets::ReadNameQueryRequest(
    WorldPacket& in)
{
    NameQueryRequest request;
    std::array<uint8, 8> guidBytes{};

    guidBytes[4] = in.ReadBit();
    request.hasRealmId2 = in.ReadBit();
    guidBytes[6] = in.ReadBit();
    guidBytes[0] = in.ReadBit();
    guidBytes[7] = in.ReadBit();
    guidBytes[1] = in.ReadBit();
    request.hasRealmId1 = in.ReadBit();
    guidBytes[5] = in.ReadBit();
    guidBytes[2] = in.ReadBit();
    guidBytes[3] = in.ReadBit();

    in.ReadByteSeq(guidBytes[7]);
    in.ReadByteSeq(guidBytes[5]);
    in.ReadByteSeq(guidBytes[1]);
    in.ReadByteSeq(guidBytes[2]);
    in.ReadByteSeq(guidBytes[6]);
    in.ReadByteSeq(guidBytes[3]);
    in.ReadByteSeq(guidBytes[0]);
    in.ReadByteSeq(guidBytes[4]);

    if (request.hasRealmId2)
        in >> request.realmId2;
    if (request.hasRealmId1)
        in >> request.realmId1;

    for (size_t i = 0; i < guidBytes.size(); ++i)
        request.guid |= uint64(guidBytes[i]) << (i * 8);
    return request;
}

inline void MopQueryPackets::BuildNameQueryResponse(WorldPacket& out,
    NameQueryResponse const& record)
{
    for (size_t index : { 3u, 6u, 7u, 2u, 5u, 4u, 0u, 1u })
        out.WriteBit(MopQueryPacketDetail::GuidByte(record.guid, index) != 0);
    out.FlushBits();

    for (size_t index : { 5u, 4u, 7u, 6u, 1u, 2u })
        out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.guid, index));
    out << record.result;

    if (record.result == 0)
    {
        out << record.realmId;
        out << record.accountId;
        out << record.classId;
        out << record.race;
        out << record.level;
        out << record.gender;
    }

    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.guid, 0));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.guid, 3));
    if (record.result != 0)
        return;

    MANGOS_ASSERT(record.name.size() <= 48);
    for (std::string const& name : record.declinedNames)
        MANGOS_ASSERT(name.size() <= 64);

    out.WriteBit(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 2) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 7) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.displayGuid, 7) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.displayGuid, 2) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.displayGuid, 0) != 0);
    out.WriteBit(record.isDeleted);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 4) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.displayGuid, 5) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 1) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 3) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 0) != 0);
    for (std::string const& name : record.declinedNames)
        out.WriteBits(uint32(name.size()), 7);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.displayGuid, 6) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.displayGuid, 3) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 5) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.displayGuid, 1) != 0);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.displayGuid, 4) != 0);
    out.WriteBits(uint32(record.name.size()), 6);
    out.WriteBit(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 6) != 0);
    out.FlushBits();

    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.displayGuid, 6));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.displayGuid, 0));
    if (!record.name.empty())
        out.append(record.name.c_str(), record.name.size());
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 5));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 2));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.displayGuid, 3));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 4));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 3));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.displayGuid, 4));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.displayGuid, 2));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 7));
    for (std::string const& name : record.declinedNames)
        if (!name.empty())
            out.append(name.c_str(), name.size());
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 6));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.displayGuid, 7));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.displayGuid, 1));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 1));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.displayGuid, 5));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.auxiliaryGuid, 0));
}

inline uint32 MopQueryPackets::ReadRealmNameQueryRequest(WorldPacket& in)
{
    uint32 realmId = 0;
    in >> realmId;
    return realmId;
}

inline void MopQueryPackets::BuildRealmNameQueryResponse(WorldPacket& out,
    RealmNameQueryResponse const& record)
{
    // 18414 wire layout for the client handler sub_1403073A0 (fills the RealmCache
    // keyed by realmId and, on status==0, sets the ready-flag that un-gates the parked
    // name-query result). The status byte leads, then realmId; the name/normalizedName
    // tail follows only when the realm is reported found. (Live-confirmed: with realmId
    // leading, the client read realmId's low byte as the status and skipped the store.)
    out << uint8(record.status);                          // 0 = found -> commits the parked player name
    out << uint32(record.realmId);
    if (record.status == 0)
    {
        out.WriteBits(uint32(record.name.size()), 8);
        out.WriteBit(record.isHomeRealm);                 // 1 = home realm -> no cross-realm suffix
        out.WriteBits(uint32(record.normalizedName.size()), 8);
        out.FlushBits();
        if (!record.name.empty())
            out.append(record.name.c_str(), record.name.size());       // no trailing NUL
        if (!record.normalizedName.empty())
            out.append(record.normalizedName.c_str(), record.normalizedName.size());
    }
}

inline void MopQueryPackets::BuildQueryTimeResponse(WorldPacket& out, uint32 serverTime,
    uint32 secondsUntilReset)
{
    out << serverTime;
    out << secondsUntilReset;
}

inline bool MopQueryPackets::ReadPlayedTimeRequest(WorldPacket& in)
{
    return in.ReadBit();
}

inline void MopQueryPackets::BuildPlayedTimeResponse(WorldPacket& out, uint32 totalPlayed,
    uint32 levelPlayed, bool displayEvent)
{
    out << totalPlayed;
    out << levelPlayed;
    out.WriteBit(displayEvent);
    out.FlushBits();
}

inline bool MopQueryPackets::BuildMailQueryNextTimeResult(WorldPacket& out,
    std::vector<MailNextTimeEntry> const& records, float nextMailTime)
{
    // The 18414 client retains only three records, and the reference server
    // stops producing records at that same bound.
    if (records.size() > 3)
        return false;

    out.WriteBits(records.size(), 20);
    for (MailNextTimeEntry const& record : records)
    {
        out.WriteBit(MopQueryPacketDetail::GuidByte(record.senderGuid, 3) != 0);
        out.WriteBit(record.hasVirtualRealmAddress);
        out.WriteBit(MopQueryPacketDetail::GuidByte(record.senderGuid, 2) != 0);
        out.WriteBit(record.hasNativeRealmAddress);
        out.WriteBit(MopQueryPacketDetail::GuidByte(record.senderGuid, 6) != 0);
        out.WriteBit(MopQueryPacketDetail::GuidByte(record.senderGuid, 1) != 0);
        out.WriteBit(MopQueryPacketDetail::GuidByte(record.senderGuid, 4) != 0);
        out.WriteBit(MopQueryPacketDetail::GuidByte(record.senderGuid, 0) != 0);
        out.WriteBit(MopQueryPacketDetail::GuidByte(record.senderGuid, 5) != 0);
        out.WriteBit(MopQueryPacketDetail::GuidByte(record.senderGuid, 7) != 0);
    }
    out.FlushBits();

    for (MailNextTimeEntry const& record : records)
    {
        out << record.nonPlayerSender;
        out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.senderGuid, 5));
        out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.senderGuid, 4));
        out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.senderGuid, 6));
        out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.senderGuid, 1));
        out << record.messageType;
        out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.senderGuid, 0));
        out << record.deliveryTime;
        if (record.hasNativeRealmAddress)
            out << record.nativeRealmAddress;
        out << record.stationery;
        out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.senderGuid, 3));
        out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.senderGuid, 2));
        if (record.hasVirtualRealmAddress)
            out << record.virtualRealmAddress;
        out.WriteByteSeq(MopQueryPacketDetail::GuidByte(record.senderGuid, 7));
    }

    out << nextMailTime;
    return true;
}

inline void MopQueryPackets::BuildCreatureQueryResponse(WorldPacket& out,
    CreatureQueryResponse const& record)
{
    out << record.entry;
    out.WriteBit(record.hasData);
    if (!record.hasData)
    {
        out.FlushBits();
        return;
    }

    size_t const subNameLength = MopQueryPacketDetail::OptionalCStringLength(record.subName, 11);
    size_t const nameLength = MopQueryPacketDetail::OptionalCStringLength(record.name, 11);
    size_t const iconLength = MopQueryPacketDetail::OptionalCStringLength(record.iconName, 6);
    MANGOS_ASSERT(record.questItems.size() < (size_t(1) << 22));

    out.WriteBits(uint32(subNameLength), 11);
    out.WriteBits(uint32(record.questItems.size()), 22);
    out.WriteBits(0, 11);
    out.WriteBits(uint32(nameLength), 11);
    for (int i = 0; i < 7; ++i)
        out.WriteBits(0, 11);
    out.WriteBit(record.racialLeader);
    out.WriteBits(uint32(iconLength), 6);
    out.FlushBits();

    out << record.killCredits[0];
    out << record.modelIds[3];
    out << record.modelIds[1];
    out << record.expansion;
    out << record.creatureType;
    out << record.healthMultiplier;
    out << record.creatureTypeFlags;
    out << record.creatureTypeFlags2;
    out << record.rank;
    out << record.movementTemplateId;
    if (nameLength)
        out << record.name;
    if (subNameLength)
        out << record.subName;
    out << record.modelIds[0];
    out << record.modelIds[2];
    if (iconLength)
        out << record.iconName;
    for (uint32 questItem : record.questItems)
        out << questItem;
    out << record.killCredits[1];
    out << record.powerMultiplier;
    out << record.family;
}

inline MopQueryPackets::GameObjectQueryRequest MopQueryPackets::ReadGameObjectQueryRequest(
    WorldPacket& in)
{
    GameObjectQueryRequest request;
    in >> request.entry;

    std::array<uint8, 8> guidBytes{};
    guidBytes[5] = in.ReadBit();
    guidBytes[3] = in.ReadBit();
    guidBytes[6] = in.ReadBit();
    guidBytes[2] = in.ReadBit();
    guidBytes[7] = in.ReadBit();
    guidBytes[1] = in.ReadBit();
    guidBytes[0] = in.ReadBit();
    guidBytes[4] = in.ReadBit();

    in.ReadByteSeq(guidBytes[1]);
    in.ReadByteSeq(guidBytes[5]);
    in.ReadByteSeq(guidBytes[3]);
    in.ReadByteSeq(guidBytes[4]);
    in.ReadByteSeq(guidBytes[6]);
    in.ReadByteSeq(guidBytes[2]);
    in.ReadByteSeq(guidBytes[7]);
    in.ReadByteSeq(guidBytes[0]);

    for (size_t i = 0; i < guidBytes.size(); ++i)
        request.guid |= uint64(guidBytes[i]) << (i * 8);
    return request;
}

inline void MopQueryPackets::BuildGameObjectQueryResponse(WorldPacket& out,
    GameObjectQueryResponse const& record)
{
    ByteBuffer blob(160);
    if (record.hasData)
    {
        for (std::string const& name : record.names)
            MANGOS_ASSERT(name.size() < 0x400);
        MANGOS_ASSERT(record.iconName.size() < 0x400);
        MANGOS_ASSERT(record.castBarCaption.size() < 0x400);
        MANGOS_ASSERT(record.unknownString.size() < 0x400);
        MANGOS_ASSERT(record.questItems.size() <= 0xFF);

        blob << record.type;
        blob << record.displayId;
        for (std::string const& name : record.names)
            blob << name;
        blob << record.iconName;
        blob << record.castBarCaption;
        blob << record.unknownString;
        for (uint32 value : record.data)
            blob << value;
        blob << record.size;
        blob << uint8(record.questItems.size());
        for (uint32 questItem : record.questItems)
            blob << questItem;
        blob << record.trailingUnknown;
    }

    out.WriteBit(record.hasData);
    out.FlushBits();
    out << record.entry;
    out << uint32(blob.size());
    out.append(blob);
}

inline uint64 MopQueryPackets::ReadCorpseMapPositionQuery(WorldPacket& in)
{
    uint8 const maskOrder[] = { 7, 6, 3, 0, 4, 1, 5, 2 };
    uint8 const byteOrder[] = { 1, 6, 0, 5, 3, 2, 4, 7 };
    uint8 guidBytes[8] = {};

    for (uint8 index : maskOrder)
        guidBytes[index] = in.ReadBit();
    for (uint8 index : byteOrder)
        in.ReadByteSeq(guidBytes[index]);

    uint64 guid = 0;
    for (uint8 index = 0; index < 8; ++index)
        guid |= uint64(guidBytes[index]) << (index * 8);
    return guid;
}

inline void MopQueryPackets::BuildCorpseQueryResponse(WorldPacket& out,
    CorpseQueryResponse const& response)
{
    uint64 const guid = response.transportGuid;

    for (uint8 index : { uint8(0), uint8(3), uint8(2) })
        out.WriteBit(MopQueryPacketDetail::GuidByte(guid, index) != 0);
    out.WriteBit(response.found);
    for (uint8 index : { uint8(5), uint8(4), uint8(1), uint8(7), uint8(6) })
        out.WriteBit(MopQueryPacketDetail::GuidByte(guid, index) != 0);
    out.FlushBits();

    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 5));
    out << response.z;
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 1));
    out << response.displayMapId;
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 6));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 4));
    out << response.x;
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 3));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 7));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 2));
    out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 0));
    out << response.corpseMapId;
    out << response.y;
}

inline void MopQueryPackets::BuildCorpseMapPositionQueryResponse(
    WorldPacket& out, float x, float y, float z, float orientation)
{
    out << x << orientation << z << y;
}

inline uint64 MopStablePackets::ReadStableListRequest(WorldPacket& in)
{
    uint8 const maskOrder[] = { 0, 5, 1, 3, 6, 7, 2, 4 };
    uint8 const byteOrder[] = { 0, 5, 7, 1, 2, 3, 4, 6 };
    uint8 guidBytes[8] = {};

    for (uint8 index : maskOrder)
        guidBytes[index] = in.ReadBit();
    for (uint8 index : byteOrder)
        in.ReadByteSeq(guidBytes[index]);

    uint64 guid = 0;
    for (uint8 index = 0; index < 8; ++index)
        guid |= uint64(guidBytes[index]) << (index * 8);
    return guid;
}

inline bool MopStablePackets::BuildPetStableList(WorldPacket& out,
    uint64 stableMasterGuid, std::vector<StablePetRecord> const& records)
{
    if (records.size() > 55)
        return false;
    bool occupiedSlots[55] = {};
    for (StablePetRecord const& record : records)
    {
        if (record.name.size() > 255 || record.slot > 54)
            return false;
        if (occupiedSlots[record.slot])
            return false;
        occupiedSlots[record.slot] = true;
    }

    uint8 const maskOrder[] = { 3, 0, 4, 7, 2, 1, 6, 5 };
    uint8 const byteOrder[] = { 3, 5, 7, 2, 0, 4, 1, 6 };

    out.Initialize(SMSG_PET_STABLE_LIST, 32 + records.size() * 24);
    for (uint8 index : maskOrder)
        out.WriteBit(MopStablePacketDetail::GuidByte(stableMasterGuid, index) != 0);
    out.WriteBits(records.size(), 19);
    for (StablePetRecord const& record : records)
        out.WriteBits(record.name.size(), 8);
    out.FlushBits();

    for (StablePetRecord const& record : records)
    {
        out << record.entry;
        out << record.level;
        out << record.state;
        out << record.modelId;
        out.append(record.name.c_str(), record.name.size());
        out << record.petNumber;
        out << record.slot;
    }
    for (uint8 index : byteOrder)
        out.WriteByteSeq(MopStablePacketDetail::GuidByte(stableMasterGuid, index));
    return true;
}

inline void MopStablePackets::BuildStableResult(WorldPacket& out, uint8 result)
{
    out.Initialize(SMSG_STABLE_RESULT, 1);
    out << result;
}

inline void MopTrainerBuyFailed::Build(WorldPacket& out, uint64 trainerGuid, uint32 reason, uint32 serviceId)
{
    for (int i = 0; i < 8; ++i)
    {
        out.WriteBit(MopTrainerPacketDetail::GuidByte(trainerGuid, MopTrainerPacketDetail::MaskOrder[i]) != 0);
    }

    // Exactly eight bits were written, so this emits one whole mask byte and
    // leaves the buffer byte-aligned for the byte block below.
    out.FlushBits();

    for (int i = 0; i < 5; ++i)
    {
        out.WriteByteSeq(MopTrainerPacketDetail::GuidByte(trainerGuid, MopTrainerPacketDetail::BytesPre[i]));
    }

    out << uint32(reason);

    for (int i = 0; i < 3; ++i)
    {
        out.WriteByteSeq(MopTrainerPacketDetail::GuidByte(trainerGuid, MopTrainerPacketDetail::BytesPost[i]));
    }

    out << uint32(serviceId);
}


struct OpcodeHandler;

namespace MopCreateGating
{
    inline uint8 ClassRequiredExpansion(uint8 class_)
    {
        switch (class_)
        {
            case CLASS_MONK:
                return EXPANSION_MOP;
            case CLASS_DEATH_KNIGHT:
                return EXPANSION_WOTLK;
            default:
                return EXPANSION_NONE;
        }
    }

    inline bool TwoSideCreateViolation(Team newTeam,
        std::vector<Team> const& existingTeams)
    {
        if (newTeam == TEAM_NONE)
            return false;

        for (Team existingTeam : existingTeams)
            if (existingTeam != TEAM_NONE && existingTeam != newTeam)
                return true;

        return false;
    }
}

namespace MopCompactPackets
{
    inline uint8 RandomRollGuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (8 * index));
    }

    inline void BuildRandomRoll(WorldPacket& out, uint64 rollerGuid,
        uint32 minimum, uint32 maximum, uint32 roll)
    {
        uint8 const maskOrder[] = { 0, 6, 7, 1, 4, 5, 2, 3 };
        uint8 const byteOrder[] = { 5, 4, 2, 0, 3, 1, 6, 7 };

        out << uint32(roll) << uint32(minimum) << uint32(maximum);
        for (uint8 index : maskOrder)
            out.WriteBit(RandomRollGuidByte(rollerGuid, index) != 0);
        out.FlushBits();
        for (uint8 index : byteOrder)
            out.WriteByteSeq(RandomRollGuidByte(rollerGuid, index));
    }
}

enum AccountDataType
{
    GLOBAL_CONFIG_CACHE             = 0,                    // 0x01 g
    PER_CHARACTER_CONFIG_CACHE      = 1,                    // 0x02 p
    GLOBAL_BINDINGS_CACHE           = 2,                    // 0x04 g
    PER_CHARACTER_BINDINGS_CACHE    = 3,                    // 0x08 p
    GLOBAL_MACROS_CACHE             = 4,                    // 0x10 g
    PER_CHARACTER_MACROS_CACHE      = 5,                    // 0x20 p
    PER_CHARACTER_LAYOUT_CACHE      = 6,                    // 0x40 p
    PER_CHARACTER_CHAT_CACHE        = 7,                    // 0x80 p
    NUM_ACCOUNT_DATA_TYPES          = 8
};

#define GLOBAL_CACHE_MASK           0x15
#define PER_CHARACTER_CACHE_MASK    0xEA

#define DB2_REPLY_ITEM 1344507586
#define DB2_REPLY_SPARSE 2442913102

struct AccountData
{
    AccountData() : Time(0), Data("") {}

    time_t Time;
    std::string Data;
};

struct AddonInfo
{
    AddonInfo(const std::string& name, uint8 enabled, uint32 crc) :
        Name(name),
        Enabled(enabled),
        CRC(crc)
    {
    }

    std::string Name;
    uint8 Enabled;
    uint32 CRC;
};

typedef std::list<AddonInfo> AddonsList;

/**
 * @brief Party operation enumeration
 */
enum PartyOperation
{
    PARTY_OP_INVITE = 0, ///< Invite to party
    PARTY_OP_LEAVE = 2,  ///< Leave party
    PARTY_OP_SWAP = 4
};

/**
 * @brief Party result enumeration
 */
enum PartyResult
{
    ERR_PARTY_RESULT_OK = 0,                     ///< Success
    ERR_BAD_PLAYER_NAME_S = 1,                   ///< Bad player name
    ERR_TARGET_NOT_IN_GROUP_S = 2,               ///< Target not in group
    ERR_TARGET_NOT_IN_INSTANCE_S = 3,            ///<
    ERR_GROUP_FULL = 4,                          ///< Group full
    ERR_ALREADY_IN_GROUP_S = 5,                  ///< Already in group
    ERR_NOT_IN_GROUP = 6,                        ///< Not in group
    ERR_NOT_LEADER = 7,                          ///< Not leader
    ERR_PLAYER_WRONG_FACTION = 8,                ///< Player wrong faction
    ERR_IGNORING_YOU_S = 9,                      ///< Ignoring you
    ERR_LFG_PENDING = 12,                        ///<
    ERR_INVITE_RESTRICTED = 13,                  ///<
    ERR_GROUP_SWAP_FAILED = 14,                  ///< if (PartyOperation == PARTY_OP_SWAP) ERR_GROUP_SWAP_FAILED else ERR_INVITE_IN_COMBAT
    ERR_INVITE_UNKNOWN_REALM = 15,
    ERR_INVITE_NO_PARTY_SERVER = 16,
    ERR_INVITE_PARTY_BUSY = 17,
    ERR_PARTY_TARGET_AMBIGUOUS = 18,
    ERR_PARTY_LFG_INVITE_RAID_LOCKED = 19,
    ERR_PARTY_LFG_BOOT_LIMIT = 20,
    ERR_PARTY_LFG_BOOT_COOLDOWN_S = 21,
    ERR_PARTY_LFG_BOOT_IN_PROGRESS = 22,
    ERR_PARTY_LFG_BOOT_TOO_FEW_PLAYERS = 23,
    ERR_PARTY_LFG_BOOT_NOT_ELIGIBLE_S = 24,
    ERR_RAID_DISALLOWED_BY_LEVEL = 25,
    ERR_PARTY_LFG_BOOT_IN_COMBAT = 26,
    ERR_VOTE_KICK_REASON_NEEDED = 27,
    ERR_PARTY_LFG_BOOT_DUNGEON_COMPLETE = 28,
    ERR_PARTY_LFG_BOOT_LOOT_ROLLS = 29,
    ERR_PARTY_LFG_TELEPORT_IN_COMBAT = 30
};

/*
 * these have been moved to LFGMgr.h for dev21
 * delete from here once all is good with the move
enum LfgUpdateType
{
    LFG_UPDATE_JOIN     = 5,
    LFG_UPDATE_LEAVE    = 7,
};

enum LfgType
{
    LFG_TYPE_NONE                 = 0,
    LFG_TYPE_DUNGEON              = 1,
    LFG_TYPE_RAID                 = 2,
    LFG_TYPE_QUEST                = 3,
    LFG_TYPE_ZONE                 = 4,
    LFG_TYPE_HEROIC_DUNGEON       = 5,
    LFG_TYPE_RANDOM_DUNGEON       = 6
};
*/

enum ChatRestrictionType
{
    ERR_CHAT_RESTRICTED = 0,
    ERR_CHAT_THROTTLED  = 1,
    ERR_USER_SQUELCHED  = 2,
    ERR_YELL_RESTRICTED = 3
};

/**
 * @brief Tutorial data state enumeration
 */
enum TutorialDataState
{
    TUTORIALDATA_UNCHANGED = 0, ///< Tutorial data unchanged
    TUTORIALDATA_CHANGED = 1,   ///< Tutorial data changed
    TUTORIALDATA_NEW = 2        ///< New tutorial data
};

/**
 * @brief Packet filter class
 *
 * Class to deal with packet processing.
 * Allows to determine if next packet is safe to be processed.
 */
class PacketFilter
{
    public:
        /**
         * @brief Constructor
         * @param pSession World session
         */
        explicit PacketFilter(WorldSession* pSession) : m_pSession(pSession) {}

        /**
         * @brief Virtual destructor
         */
        virtual ~PacketFilter() {}

        /**
         * @brief Process packet
         * @param packet World packet to process
         * @return True if processed successfully
         */
        virtual bool Process(WorldPacket* /*packet*/)
        {
            return true;
        }

        /**
         * @brief Process logout
         * @return True if logout processed
         */
        virtual bool ProcessLogout() const
        {
            return true;
        }

    protected:
        WorldSession* const m_pSession;
};
/**
 * @brief Map session filter class
 *
 * Process only thread-safe packets in Map::Update().
 */
class MapSessionFilter : public PacketFilter
{
    public:
        /**
         * @brief Constructor
         * @param pSession World session
         */
        explicit MapSessionFilter(WorldSession* pSession) : PacketFilter(pSession) {}

        /**
         * @brief Destructor
         */
        ~MapSessionFilter() {}

        /**
         * @brief Process packet
         * @param packet World packet to process
         * @return True if processed successfully
         */
        bool Process(WorldPacket* packet) override;

        /**
         * @brief Process logout
         *
         * In Map::Update() we do not process player logout.
         *
         * @return False (logout not processed)
         */
        bool ProcessLogout() const override
        {
            return false;
        }
};

/**
 * @brief World session filter class
 *
 * Class used to filter only thread-unsafe packets from queue.
 * Used in World::UpdateSessions().
 */
class WorldSessionFilter : public PacketFilter
{
    public:
        /**
         * @brief Constructor
         * @param pSession World session
         */
        explicit WorldSessionFilter(WorldSession* pSession) : PacketFilter(pSession) {}

        /**
         * @brief Destructor
         */
        ~WorldSessionFilter() {}

        /**
         * @brief Process packet
         * @param packet World packet to process
         * @return True if processed successfully
         */
        bool Process(WorldPacket* packet) override;
};

/**
 * @brief World session class
 *
 * Player session in the World.
 */
class WorldSession
{
        friend class CharacterHandler;

    public:
        /**
         * @brief Constructor
         * @param id Session ID
         * @param sock World socket
         * @param sec Account security level
         * @param mute_time Mute time
         * @param locale Locale
         */
        WorldSession(uint32 id, WorldSocket* sock, AccountTypes sec, uint8 expansion, time_t mute_time, LocaleConstant locale);

        /**
         * @brief Destructor
         */
        ~WorldSession();

        /**
         * @brief Check if player is loading
         * @return True if loading
         */
        bool PlayerLoading() const
        {
            return m_playerLoading;
        }

        /**
         * @brief Check if player is logging out
         * @return True if logging out
         */
        bool PlayerLogout() const
        {
            return m_playerLogout;
        }

        /**
         * @brief Check if player is logging out with save
         * @return True if logging out with save
         */
        bool PlayerLogoutWithSave() const
        {
            return m_playerLogout && m_playerSave;
        }


        void SizeError(WorldPacket const& packet, uint32 size) const;

        void ReadAddonsInfo(ByteBuffer &data);
        void SendAddonsInfo();

        void SendPacket(WorldPacket const* packet, bool bypassSuppress = false);
        void SendNotification(const char* format, ...) ATTR_PRINTF(2, 3);
        void SendNotification(int32 string_id, ...);
        // Eluna exposes this historical API name. Keep only a source-compatible
        // forwarding facade; the legacy opcode and packet backend no longer exist.
        template <typename... Args>
        void SendAreaTriggerMessage(char const* format, Args... args)
        {
            SendNotification(format, args...);
        }
        void SendPetNameInvalid(uint32 error, const std::string& name, DeclinedName* declinedName);
        void SendLfgJoinResult(LfgJoinResult result, LFGState state, partyForbidden const& lockedDungeons);
        void SendLfgUpdate(bool isGroup, LFGPlayerStatus status);
        void SendLfgQueueStatus(LFGQueueStatus const& status);
        void SendLfgRoleCheckUpdate(LFGRoleCheck const& roleCheck);
        void SendLfgRoleChosen(uint64 rawGuid, uint8 roles);
        void SendLfgProposalUpdate(LFGProposal const& proposal);
        void SendLfgTeleportError(uint8 error);
        void SendLfgRewards(LFGRewards const& rewards);
        void SendLfgBootUpdate(LFGBoot const& boot);
        void SendPartyResult(PartyOperation operation, const std::string& member, PartyResult res);
        void SendGroupInvite(Player* player, bool alreadyInGroup = false);
        void SendGuildInvite(Player* player, bool alreadyInGuild = false);
        void SendTransferAborted(uint32 mapid, uint8 reason, uint8 arg = 0);
        void SendSetPhaseShift(uint32 phaseMask, uint16 mapId = 0);
        void SendQueryTimeResponse();
        void SendRedirectClient(std::string& ip, uint16 port);

        AccountTypes GetSecurity() const
        {
            return _security;
        }
        uint32 GetAccountId() const
        {
            return _accountId;
        }
        Player* GetPlayer() const
        {
            return _player;
        }
        char const* GetPlayerName() const;
        void SetSecurity(AccountTypes security)
        {
            _security = security;
        }
        std::string const& GetRemoteAddress()
        {
            return m_Address;
        }
        void SetPlayer(Player* plr)
        {
            _player = plr;
        }
        uint8 Expansion() const { return m_expansion; }

        // Warden
        void InitWarden(uint16 build, BigNumber* k, std::string const& os);

        /// Session in auth.queue currently
        void SetInQueue(bool state)
        {
            m_inQueue = state;
        }

        /// Is the user engaged in a log out process?
        bool isLogingOut() const
        {
            return _logoutTime || m_playerLogout;
        }

        /// Engage the logout process for the user
        void LogoutRequest(time_t requestTime)
        {
            _logoutTime = requestTime;
        }

        /// Is logout cooldown expired?
        bool ShouldLogOut(time_t currTime) const
        {
            return (_logoutTime > 0 && currTime >= _logoutTime + 20);
        }

        void LogoutPlayer(bool Save);
        void KickPlayer();

        void QueuePacket(WorldPacket* new_packet);

        bool Update(PacketFilter& updater);

        /// Sends SMSG_AUTH_RESPONSE for the login queue (see MopAuthResponse.h).
        void SendAuthWaitQue(uint32 position);

        void SendNameQueryOpcode(Player* p);
        void SendNameQueryOpcodeFromDB(ObjectGuid guid);
        static void SendNameQueryOpcodeFromDBCallBack(QueryResult* result,
            uint32 accountId, uint64 requestedGuid);

        void SendTrainerList(ObjectGuid guid);
        void SendTrainerList(ObjectGuid guid, const std::string& strTitle);

        void SendListInventory(ObjectGuid guid);
        bool CheckBanker(ObjectGuid guid);
        void SendShowBank(ObjectGuid guid);
        bool CheckMailBox(ObjectGuid guid);
        void SendShowMailBox(ObjectGuid guid);
        void SendTabardVendorActivate(ObjectGuid guid);
        void SendSpiritResurrect();
        void SendBindPoint(Creature* npc);
        void SendGMTicketGetTicket(uint32 status, GMTicket* ticket = NULL);

        void SendAttackStop(Unit const* enemy);

        void SendBattlegGroundList(ObjectGuid guid, BattleGroundTypeId bgTypeId);

        void SendTradeStatus(TradeStatus status);
        void SendUpdateTrade(bool trader_state = true);
        void SendCancelTrade();

        void SendPetitionQueryOpcode(ObjectGuid petitionguid);

        // pet
        void SendPetNameQuery(ObjectGuid guid, uint32 petnumber);
        void SendStablePet(ObjectGuid guid);
        void SendStableResult(uint8 res);
        bool CheckStableMaster(ObjectGuid guid);

        // Account Data
        AccountData* GetAccountData(AccountDataType type) { return &m_accountData[type]; }
        void SetAccountData(AccountDataType type, time_t time_, const std::string& data);
        void SendAccountDataTimes(uint32 mask);
        void LoadGlobalAccountData();
        void LoadAccountData(QueryResult* result, uint32 mask);
        void LoadTutorialsData();
        void SendTutorialsData();
        void SaveTutorialsData();
        uint32 GetTutorialInt(uint32 intId)
        {
            return m_Tutorials[intId];
        }

        void SetTutorialInt(uint32 intId, uint32 value)
        {
            if (m_Tutorials[intId] != value)
            {
                m_Tutorials[intId] = value;
                if (m_tutorialState == TUTORIALDATA_UNCHANGED)
                {
                    m_tutorialState = TUTORIALDATA_CHANGED;
                }
            }
        }
        // used with item_page table
        bool SendItemInfo(uint32 itemid, WorldPacket data);

        // auction
        void SendAuctionHello(Unit* unit);
        void SendAuctionCommandResult(AuctionEntry* auc, AuctionAction Action, AuctionError ErrorCode, InventoryResult invError = EQUIP_ERR_OK);
        void SendAuctionSoldNotification(AuctionEntry* auction);
        void SendAuctionWonNotification(AuctionEntry* auction);
        void SendAuctionOutbidNotification(AuctionEntry* auction, uint64 newBidderGuid, uint64 newBid);
        void SendAuctionBidUpdateNotification(AuctionEntry* auction);
        void SendAuctionExpiredNotification(AuctionEntry* auction);
        static void SendAuctionOutbiddedMail(AuctionEntry* auction, Player* newBidder, uint64 newBid);
        void SendAuctionCancelledToBidderMail(AuctionEntry* auction);
        void BuildListAuctionItems(std::vector<AuctionEntry*> const& auctions, WorldPacket& data, std::wstring const& searchedname, uint32 listfrom, uint32 levelmin,
                                   uint32 levelmax, uint32 usable, uint32 inventoryType, uint32 itemClass, uint32 itemSubClass, uint32 quality, uint32& count, uint32& totalcount, bool isFull);

        AuctionHouseEntry const* GetCheckedAuctionHouseForAuctioneer(ObjectGuid guid);

        // Item Enchantment
        void SendEnchantmentLog(ObjectGuid targetGuid, ObjectGuid casterGuid, uint32 itemId, uint32 enchantId);
        void SendItemEnchantTimeUpdate(ObjectGuid playerGuid, ObjectGuid itemGuid, uint32 slot, uint32 duration);

        // Taxi
        void SendTaxiStatus(ObjectGuid guid);
        void SendTaxiMenu(Creature* unit);
        void SendDoFlight(uint32 mountDisplayId, uint32 path, uint32 pathNode = 0);
        bool SendLearnNewTaxiNode(Creature* unit);
        void SendActivateTaxiReply(ActivateTaxiReply reply);

        // Guild/Arena Team
        void SendGuildCommandResult(uint32 typecmd, const std::string& str, uint32 cmdresult);
        void SendPetitionShowList(ObjectGuid guid);
        void SendSaveGuildEmblem(uint32 msg);

        void BuildPartyMemberStatsChangedPacket(Player* player, WorldPacket* data);

        void DoLootRelease(ObjectGuid lguid);

        // Account mute time
        time_t m_muteTime;

        // Locales
        LocaleConstant GetSessionDbcLocale() const
        {
            return m_sessionDbcLocale;
        }
        int GetSessionDbLocaleIndex() const
        {
            return m_sessionDbLocaleIndex;
        }
        const char* GetMangosString(int32 entry) const;

        uint32 GetLatency() const
        {
            return m_latency;
        }
        void SetLatency(uint32 latency)
        {
            m_latency = latency;
        }
        uint32 getDialogStatus(Player* pPlayer, Object* questgiver, uint32 defstatus);

        // Misc
        void SetClientTimeDelay(uint32 delay) { m_clientTimeDelay = delay; }
        void ResetClientTimeDelay() { m_clientTimeDelay = 0; }

    public:                                                 // opcodes handlers

        // opcodes handlers
        void Handle_NULL(WorldPacket& recvPacket);          // not used
        void Handle_EarlyProccess(WorldPacket& recvPacket); // just mark packets processed in WorldSocket::OnRead
        void Handle_ServerSide(WorldPacket& recvPacket);    // sever side only, can't be accepted from client
        void Handle_Deprecated(WorldPacket& recvPacket);    // never used anymore by client

        void HandleCharEnumOpcode(WorldPacket& recvPacket);
        void SendCharacterEnum();
        void HandleCharDeleteOpcode(WorldPacket& recvPacket);
        void HandleCharCreateOpcode(WorldPacket& recvPacket);
        void HandlePlayerLoginOpcode(WorldPacket& recvPacket);
        void HandleCharEnum(QueryResult* result);
        void HandlePlayerLogin(LoginQueryHolder* holder);
        void HandleReorderCharactersOpcode(WorldPacket& recvPacket);

        // played time
        void HandlePlayedTime(WorldPacket& recvPacket);

        // new
        void HandleMoveUnRootAck(WorldPacket& recvPacket);
        void HandleMoveRootAck(WorldPacket& recvPacket);

        // new inspect
        void HandleInspectOpcode(WorldPacket& recvPacket);

        // new party stats
        void HandleInspectHonorStatsOpcode(WorldPacket& recvPacket);

        void HandleMoveWaterWalkAck(WorldPacket& recvPacket);
        void HandleFeatherFallAck(WorldPacket& recv_data);

        void HandleMoveHoverAck(WorldPacket& recv_data);

        void HandleMountSpecialAnimOpcode(WorldPacket& recvdata);

        // character view
        void HandleShowingHelmOpcode(WorldPacket& recv_data);
        void HandleShowingCloakOpcode(WorldPacket& recv_data);

        // repair
        void HandleRepairItemOpcode(WorldPacket& recvPacket);

        // Knockback
        void HandleMoveKnockBackAck(WorldPacket& recvPacket);
        void SendKnockBack(float angle, float horizontalSpeed, float verticalSpeed);

        void HandleMoveTeleportAckOpcode(WorldPacket& recvPacket);
        void HandleForceSpeedChangeAckOpcodes(WorldPacket& recv_data);

        void HandlePingOpcode(WorldPacket& recvPacket);
        void HandleAuthSessionOpcode(WorldPacket& recvPacket);
        void HandleRepopRequestOpcode(WorldPacket& recvPacket);
        void HandleAutostoreLootItemOpcode(WorldPacket& recvPacket);
        void HandleLootMoneyOpcode(WorldPacket& recvPacket);

        /**
        * Method which handles the loot Opcode sent by the client, happens when the player is actually looting the object.
        * It generates required loot on purpose.
        */
        void HandleLootOpcode(WorldPacket& recvPacket);

        /**
        * Method which handles the loot release opcode sent by the client, happens when the player has end looting the object.
        * It will take care of the looting state of the object depending on the case.
        */
        void HandleLootReleaseOpcode(WorldPacket& recvPacket);
        void HandleLootMasterGiveOpcode(WorldPacket& recvPacket);
        void HandleWhoOpcode(WorldPacket& recvPacket);
        void HandleLogoutRequestOpcode(WorldPacket& recvPacket);
        void HandlePlayerLogoutOpcode(WorldPacket& recvPacket);
        void HandleLogoutCancelOpcode(WorldPacket& recvPacket);

        void HandleGMTicketGetTicketOpcode(WorldPacket& recvPacket);
        void HandleGMTicketCreateOpcode(WorldPacket& recvPacket);
        void HandleGMTicketSystemStatusOpcode(WorldPacket& recvPacket);
        void HandleGMUpdateTicketStatusOpcode(WorldPacket& recvPacket);
        void SendGMTicketStatusUpdate(GMTicketStatus statusCode);
        void HandleGMTicketDeleteTicketOpcode(WorldPacket& recvPacket);
        void HandleGMTicketUpdateTextOpcode(WorldPacket& recvPacket);

        void HandleGMTicketSurveySubmitOpcode(WorldPacket& recvPacket);
        void HandleGMResponseResolveOpcode(WorldPacket& recv_data);

        void HandleTogglePvP(WorldPacket& recvPacket);

        void HandleZoneUpdateOpcode(WorldPacket& recvPacket);
        void HandleSetTargetOpcode(WorldPacket& recvPacket);
        void HandleSetSelectionOpcode(WorldPacket& recvPacket);
        void HandleStandStateChangeOpcode(WorldPacket& recvPacket);
        void HandleEmoteOpcode(WorldPacket& recvPacket);
        void HandleContactListOpcode(WorldPacket& recvPacket);
        void HandleAddFriendOpcode(WorldPacket& recvPacket);
        static void HandleAddFriendOpcodeCallBack(QueryResult* result, uint32 accountId, std::string friendNote);
        void HandleDelFriendOpcode(WorldPacket& recvPacket);
        void HandleAddIgnoreOpcode(WorldPacket& recvPacket);
        static void HandleAddIgnoreOpcodeCallBack(QueryResult* result, uint32 accountId);
        void HandleDelIgnoreOpcode(WorldPacket& recvPacket);
        void HandleSetContactNotesOpcode(WorldPacket& recvPacket);
        void HandleBugOpcode(WorldPacket& recvPacket);
        void HandleSetAmmoOpcode(WorldPacket& recvPacket);

        void HandleAreaTriggerOpcode(WorldPacket& recvPacket);

        void HandleSetFactionAtWarOpcode(WorldPacket& recv_data);
        void HandleSetWatchedFactionOpcode(WorldPacket& recv_data);
        void HandleSetFactionInactiveOpcode(WorldPacket& recv_data);
        void HandleRequestForcedReactionsOpcode(WorldPacket& recv_data);

        void HandleUpdateAccountData(WorldPacket& recvPacket);
        void HandleRequestAccountData(WorldPacket& recvPacket);
        void HandleSetActionButtonOpcode(WorldPacket& recvPacket);

        void HandleGameObjectUseOpcode(WorldPacket& recPacket);
        void HandleGameobjectReportUse(WorldPacket& recvPacket);

        void HandleNameQueryOpcode(WorldPacket& recvPacket);

        void HandleRealmNameQueryOpcode(WorldPacket& recvPacket);

        void HandleQueryTimeOpcode(WorldPacket& recvPacket);

        void HandleCreatureQueryOpcode(WorldPacket& recvPacket);

        void HandleGameObjectQueryOpcode(WorldPacket& recvPacket);

        // Movement Handler
        void HandleMoveWorldportAckOpcode(WorldPacket& recvPacket);
        void HandleMoveWorldportAckOpcode();                // for server-side calls

        void HandleMovementOpcodes(WorldPacket& recvPacket);
        void HandleSetActiveMoverOpcode(WorldPacket& recv_data);
        void HandleMoveNotActiveMoverOpcode(WorldPacket& recv_data);
        void HandleMoveTimeSkippedOpcode(WorldPacket& recv_data);

        void HandleDismissControlledVehicle(WorldPacket& recvPacket);
        void HandleRequestVehicleExit(WorldPacket& recvPacket);
        void HandleRequestVehicleSwitchSeat(WorldPacket& recvPacket);
        void HandleChangeSeatsOnControlledVehicle(WorldPacket& recvPacket);
        void HandleRequestVehiclePrevSeat(WorldPacket& recv_data);
        void HandleRequestVehicleNextSeat(WorldPacket& recv_data);
        void HandleRideVehicleInteract(WorldPacket& recvPacket);
        void HandleEjectPassenger(WorldPacket& recvPacket);

        void HandleRequestRaidInfoOpcode(WorldPacket& recv_data);

        void HandleGroupInviteOpcode(WorldPacket& recvPacket);
        void HandleGroupInviteResponseOpcode(WorldPacket& recvPacket);
        void HandleGroupUninviteOpcode(WorldPacket& recvPacket);
        void HandleGroupUninviteGuidOpcode(WorldPacket& recvPacket);
        void HandleGroupSetLeaderOpcode(WorldPacket& recvPacket);
        void HandleGroupDisbandOpcode(WorldPacket& recvPacket);
        void HandleOptOutOfLootOpcode(WorldPacket& recv_data);
        void HandleSetAllowLowLevelRaidOpcode(WorldPacket& recv_data);
        void HandleLootMethodOpcode(WorldPacket& recvPacket);
        void HandleLootRoll(WorldPacket& recv_data);
        void HandleRequestPartyMemberStatsOpcode(WorldPacket& recv_data);
        void HandleRaidTargetUpdateOpcode(WorldPacket& recv_data);
        void HandleRaidReadyCheckOpcode(WorldPacket& recv_data);
        void HandleRaidReadyCheckConfirmOpcode(WorldPacket& recv_data);
        void HandleRaidReadyCheckFinishedOpcode(WorldPacket& recv_data);
        void HandleGroupRaidConvertOpcode(WorldPacket& recv_data);
        void HandleGroupRequestJoinUpdates(WorldPacket& recv_data);
        void HandleGroupChangeSubGroupOpcode(WorldPacket& recv_data);
        void HandleGroupAssistantLeaderOpcode(WorldPacket& recv_data);
        void HandlePartyAssignmentOpcode(WorldPacket& recv_data);

        void HandlePetitionBuyOpcode(WorldPacket& recv_data);
        void HandlePetitionShowSignOpcode(WorldPacket& recv_data);
        void HandlePetitionQueryOpcode(WorldPacket& recv_data);
        void HandlePetitionSignOpcode(WorldPacket& recv_data);
        void HandleOfferPetitionOpcode(WorldPacket& recv_data);
        void HandleTurnInPetitionOpcode(WorldPacket& recv_data);

        void HandleGuildQueryOpcode(WorldPacket& recvPacket);
        void HandleGuildCreateOpcode(WorldPacket& recvPacket);
        void HandleGuildInviteOpcode(WorldPacket& recvPacket);
        void HandleGuildRemoveOpcode(WorldPacket& recvPacket);
        void HandleGuildAcceptOpcode(WorldPacket& recvPacket);
        void HandleGuildDeclineOpcode(WorldPacket& recvPacket);
        void HandleGuildEventLogQueryOpcode(WorldPacket& recvPacket);
        void HandleGuildRosterOpcode(WorldPacket& recvPacket);
        void HandleGuildPromoteOpcode(WorldPacket& recvPacket);
        void HandleGuildDemoteOpcode(WorldPacket& recvPacket);
        void HandleGuildSetRankOpcode(WorldPacket& recvPacket);
        void HandleGuildSwitchRankOpcode(WorldPacket& recvPacket);
        void HandleGuildLeaveOpcode(WorldPacket& recvPacket);
        void HandleGuildDisbandOpcode(WorldPacket& recvPacket);
        void HandleGuildLeaderOpcode(WorldPacket& recvPacket);
        void HandleGuildMOTDOpcode(WorldPacket& recvPacket);
        void HandleGuildSetNoteOpcode(WorldPacket& recvPacket);
        void HandleGuildRankOpcode(WorldPacket& recvPacket);
        void HandleGuildAddRankOpcode(WorldPacket& recvPacket);
        void HandleGuildDelRankOpcode(WorldPacket& recvPacket);
        void HandleGuildChangeInfoTextOpcode(WorldPacket& recvPacket);
        void HandleSaveGuildEmblemOpcode(WorldPacket& recvPacket);
        void HandleGuildQueryRanksOpcode(WorldPacket& recvPacket);
        void HandleGuildAutoDeclineToggleOpcode(WorldPacket& recvPacket);

        void HandleTaxiNodeStatusQueryOpcode(WorldPacket& recvPacket);
        void HandleTaxiQueryAvailableNodes(WorldPacket& recvPacket);
        void HandleActivateTaxiOpcode(WorldPacket& recvPacket);
        void HandleActivateTaxiExpressOpcode(WorldPacket& recvPacket);
        void HandleMoveSplineDoneOpcode(WorldPacket& recvPacket);

        void HandleTabardVendorActivateOpcode(WorldPacket& recvPacket);
        void HandleBankerActivateOpcode(WorldPacket& recvPacket);
        void HandleBuyBankSlotOpcode(WorldPacket& recvPacket);
        void HandleTrainerListOpcode(WorldPacket& recvPacket);
        void HandleTrainerBuySpellOpcode(WorldPacket& recvPacket);

        void HandlePetitionShowListOpcode(WorldPacket& recvPacket);
        void HandleGossipHelloOpcode(WorldPacket& recvPacket);
        void HandleGossipSelectOptionOpcode(WorldPacket& recvPacket);
        void HandleSpiritHealerActivateOpcode(WorldPacket& recvPacket);
        void HandleReturnToGraveyardOpcode(WorldPacket& recvPacket);
        void HandleNpcTextQueryOpcode(WorldPacket& recvPacket);
        void HandleBinderActivateOpcode(WorldPacket& recvPacket);
        void HandleListStabledPetsOpcode(WorldPacket& recvPacket);
        void HandleStablePet(WorldPacket& recvPacket);
        void HandleUnstablePet(WorldPacket& recvPacket);
        void HandleBuyStableSlot(WorldPacket& recvPacket);
        void HandleStableRevivePet(WorldPacket& recvPacket);
        void HandleStableSwapPet(WorldPacket& recvPacket);

        void HandleDuelAcceptedOpcode(WorldPacket& recvPacket);
        void HandleDuelCancelledOpcode(WorldPacket& recvPacket);

        void HandleAcceptTradeOpcode(WorldPacket& recvPacket);
        void HandleBeginTradeOpcode(WorldPacket& recvPacket);
        void HandleBusyTradeOpcode(WorldPacket& recvPacket);
        void HandleCancelTradeOpcode(WorldPacket& recvPacket);
        void HandleClearTradeItemOpcode(WorldPacket& recvPacket);
        void HandleIgnoreTradeOpcode(WorldPacket& recvPacket);
        void HandleInitiateTradeOpcode(WorldPacket& recvPacket);
        void HandleSetTradeGoldOpcode(WorldPacket& recvPacket);
        void HandleSetTradeItemOpcode(WorldPacket& recvPacket);
        void HandleUnacceptTradeOpcode(WorldPacket& recvPacket);

        void HandleAuctionHelloOpcode(WorldPacket& recvPacket);
        void HandleAuctionListItems(WorldPacket& recv_data);
        void HandleAuctionListBidderItems(WorldPacket& recv_data);
        void HandleAuctionSellItem(WorldPacket& recv_data);

        void HandleAuctionRemoveItem(WorldPacket& recv_data);
        void HandleAuctionListOwnerItems(WorldPacket& recv_data);
        void HandleAuctionPlaceBid(WorldPacket& recv_data);

        void AuctionBind(uint32 price, AuctionEntry * auction, Player * pl, Player* auction_owner);
        void HandleAuctionListPendingSales(WorldPacket& recv_data);

        void HandleGetMailList(WorldPacket& recv_data);
        void HandleSendMail(WorldPacket& recv_data);
        void HandleMailTakeMoney(WorldPacket& recv_data);
        void HandleMailTakeItem(WorldPacket& recv_data);
        void HandleMailMarkAsRead(WorldPacket& recv_data);
        void HandleMailReturnToSender(WorldPacket& recv_data);
        void HandleMailDelete(WorldPacket& recv_data);
        void HandleItemTextQuery(WorldPacket& recv_data);
        void HandleMailCreateTextItem(WorldPacket& recv_data);
        void HandleQueryNextMailTime(WorldPacket& recv_data);
        void HandleCancelChanneling(WorldPacket& recv_data);

        void SendItemPageInfo(ItemPrototype* itemProto);
        void HandleSplitItemOpcode(WorldPacket& recvPacket);
        void HandleSwapInvItemOpcode(WorldPacket& recvPacket);
        void HandleDestroyItemOpcode(WorldPacket& recvPacket);
        void HandleAutoEquipItemOpcode(WorldPacket& recvPacket);
        void HandleSellItemOpcode(WorldPacket& recvPacket);
        void HandleBuyItemOpcode(WorldPacket& recvPacket);
        void HandleListInventoryOpcode(WorldPacket& recvPacket);
        void HandleAutoStoreBagItemOpcode(WorldPacket& recvPacket);
        void HandleReadItemOpcode(WorldPacket& recvPacket);
        void HandleAutoEquipItemSlotOpcode(WorldPacket& recvPacket);
        void HandleSwapItem(WorldPacket& recvPacket);
        void HandleBuybackItem(WorldPacket& recvPacket);
        void HandleAutoBankItemOpcode(WorldPacket& recvPacket);
        void HandleAutoStoreBankItemOpcode(WorldPacket& recvPacket);
        void HandleWrapItemOpcode(WorldPacket& recvPacket);

        void HandleAttackSwingOpcode(WorldPacket& recvPacket);
        void HandleAttackStopOpcode(WorldPacket& recvPacket);
        void HandleSetSheathedOpcode(WorldPacket& recvPacket);

        void HandleUseItemOpcode(WorldPacket& recvPacket);
        void HandleOpenItemOpcode(WorldPacket& recvPacket);
        void HandleCastSpellOpcode(WorldPacket& recvPacket);
        void HandleCancelCastOpcode(WorldPacket& recvPacket);
        void HandleCancelAuraOpcode(WorldPacket& recvPacket);
        void HandleCancelGrowthAuraOpcode(WorldPacket& recvPacket);
        void HandleCancelAutoRepeatSpellOpcode(WorldPacket& recvPacket);

        void HandleLearnTalentOpcode(WorldPacket& recvPacket);
        void HandleLearnPreviewTalents(WorldPacket& recvPacket);
        void HandleTalentWipeConfirmOpcode(WorldPacket& recvPacket);
        void HandleUnlearnSkillOpcode(WorldPacket& recvPacket);

        void HandleQuestgiverStatusQueryOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverStatusMultipleQuery(WorldPacket& recvPacket);
        void HandleQuestgiverHelloOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverAcceptQuestOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverQueryQuestOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverChooseRewardOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverRequestRewardOpcode(WorldPacket& recvPacket);
        void HandleQuestQueryOpcode(WorldPacket& recvPacket);
        void HandleQuestgiverCancel(WorldPacket& recv_data);
        void HandleQuestLogSwapQuest(WorldPacket& recv_data);
        void HandleQuestLogRemoveQuest(WorldPacket& recv_data);
        void HandleQuestConfirmAccept(WorldPacket& recv_data);
        void HandleQuestgiverCompleteQuest(WorldPacket& recv_data);
        bool CanInteractWithQuestGiver(ObjectGuid guid, char const* descr);

        void HandleQuestgiverQuestAutoLaunch(WorldPacket& recvPacket);
        void HandlePushQuestToParty(WorldPacket& recvPacket);
        void HandleQuestPushResult(WorldPacket& recvPacket);

        bool processChatmessageFurtherAfterSecurityChecks(std::string&, uint32);
        void SendPlayerNotFoundNotice(const std::string& name);
        void SendPlayerAmbiguousNotice(const std::string& name);
        void SendChatRestrictedNotice(ChatRestrictionType restriction);
        void HandleMessagechatOpcode(WorldPacket& recvPacket);
        void HandleAddonMessagechatOpcode(WorldPacket& recvPacket);
        void HandleUnregisterAddonPrefixesOpcode(WorldPacket& recvPacket);
        void HandleAddonRegisteredPrefixesOpcode(WorldPacket& recvPacket);
        bool IsAddonRegistered(std::string const& prefix) const;
        void HandleTextEmoteOpcode(WorldPacket& recvPacket);
        void HandleChatIgnoredOpcode(WorldPacket& recvPacket);

        void HandleReclaimCorpseOpcode(WorldPacket& recvPacket);
        void HandleCorpseQueryOpcode(WorldPacket& recvPacket);
        void HandleCorpseMapPositionQueryOpcode(WorldPacket& recvPacket);
        void HandleResurrectResponseOpcode(WorldPacket& recvPacket);
        void HandleReturnToGraveyard(WorldPacket& recvPacket);
        void HandleSummonResponseOpcode(WorldPacket& recv_data);

        void HandleJoinChannelOpcode(WorldPacket& recvPacket);
        void HandleLeaveChannelOpcode(WorldPacket& recvPacket);
        void HandleChannelListOpcode(WorldPacket& recvPacket);
        void HandleChannelPasswordOpcode(WorldPacket& recvPacket);
        void HandleChannelSetOwnerOpcode(WorldPacket& recvPacket);
        void HandleChannelOwnerOpcode(WorldPacket& recvPacket);
        void HandleChannelModeratorOpcode(WorldPacket& recvPacket);
        void HandleChannelUnmoderatorOpcode(WorldPacket& recvPacket);
        void HandleChannelMuteOpcode(WorldPacket& recvPacket);
        void HandleChannelUnmuteOpcode(WorldPacket& recvPacket);
        void HandleChannelInviteOpcode(WorldPacket& recvPacket);
        void HandleChannelKickOpcode(WorldPacket& recvPacket);
        void HandleChannelBanOpcode(WorldPacket& recvPacket);
        void HandleChannelUnbanOpcode(WorldPacket& recvPacket);
        void HandleChannelAnnouncementsOpcode(WorldPacket& recvPacket);
        void HandleChannelModerateOpcode(WorldPacket& recvPacket);
        void HandleChannelDisplayListQueryOpcode(WorldPacket& recvPacket);
        void HandleSetChannelWatchOpcode(WorldPacket& recvPacket);

        void HandleCompleteCinematic(WorldPacket& recvPacket);
        void HandleNextCinematicCamera(WorldPacket& recvPacket);

        void HandlePageQuerySkippedOpcode(WorldPacket& recvPacket);
        void HandlePageTextQueryOpcode(WorldPacket& recvPacket);

        void HandleTutorialFlagOpcode(WorldPacket& recv_data);
        void HandleTutorialClearOpcode(WorldPacket& recv_data);
        void HandleTutorialResetOpcode(WorldPacket& recv_data);

        // Pet
        void HandlePetAction(WorldPacket& recv_data);
        void HandlePetStopAttack(WorldPacket& recv_data);
        void HandlePetNameQueryOpcode(WorldPacket& recv_data);
        void HandlePetSetAction(WorldPacket& recv_data);
        void HandlePetAbandon(WorldPacket& recv_data);
        void HandlePetRename(WorldPacket& recv_data);
        void HandlePetCancelAuraOpcode(WorldPacket& recvPacket);
        void HandlePetSpellAutocastOpcode(WorldPacket& recvPacket);
        void HandlePetCastSpellOpcode(WorldPacket& recvPacket);
        void HandlePetLearnTalent(WorldPacket& recvPacket);
        void HandleLearnPreviewTalentsPet(WorldPacket& recvPacket);
        void HandleDismissCritter(WorldPacket& recvData);

        void HandleSetActionBarTogglesOpcode(WorldPacket& recv_data);
        void HandleViolenceLevelOpcode(WorldPacket& recv_data);

        void HandleCharRenameOpcode(WorldPacket& recv_data);
        static void HandleChangePlayerNameOpcodeCallBack(QueryResult* result, uint32 accountId, std::string newname);
        void HandleSetPlayerDeclinedNamesOpcode(WorldPacket& recv_data);

        void HandleTotemDestroyed(WorldPacket& recv_data);

        // BattleGround
        void HandleBattlemasterHelloOpcode(WorldPacket& recv_data);
        void HandleBattlemasterJoinOpcode(WorldPacket& recv_data);
        void HandleBattleGroundPlayerPositionsOpcode(WorldPacket& recv_data);
        void HandlePVPLogDataOpcode(WorldPacket& recv_data);
        void HandleBattlefieldStatusOpcode(WorldPacket& recv_data);
        void HandleQueryCountdownTimerOpcode(WorldPacket& recv_data);
        void HandleBattleFieldPortOpcode(WorldPacket& recv_data);
        void HandleBattlefieldListOpcode(WorldPacket& recv_data);
        void HandleLeaveBattlefieldOpcode(WorldPacket& recv_data);
        void HandleReportPvPAFK(WorldPacket& recv_data);
        void HandleRequestPvPOptionsEnabledOpcode(WorldPacket& recv_data);
        void HandleRequestPvPRewardsOpcode(WorldPacket& recv_data);
        void HandleRequestRatedBGStatsOpcode(WorldPacket& recv_data);
        void HandleRequestConquestFormulaConstantsOpcode(WorldPacket& recv_data);

        void HandleWardenDataOpcode(WorldPacket& recv_data);
        void HandleWorldTeleportOpcode(WorldPacket& recv_data);
        void HandleMinimapPingOpcode(WorldPacket& recv_data);
        void HandleRandomRollOpcode(WorldPacket& recv_data);
        void HandleFarSightOpcode(WorldPacket& recv_data);
        void HandleSetDungeonDifficultyOpcode(WorldPacket& recv_data);
        void HandleSetRaidDifficultyOpcode(WorldPacket& recv_data);
        void HandleMoveSetCanFlyAckOpcode(WorldPacket& recv_data);
        void HandleLfgJoinOpcode(WorldPacket& recv_data);
        void HandleLfgLeaveOpcode(WorldPacket& recv_data);
        void HandleLfgGetStatusOpcode(WorldPacket& recv_data);
        void HandleSetLfgCommentOpcode(WorldPacket& recv_data);
        void HandleSetTitleOpcode(WorldPacket& recv_data);
        void HandleRealmSplitOpcode(WorldPacket& recv_data);
        void HandleTimeSyncResp(WorldPacket& recv_data);
        void HandleWhoisOpcode(WorldPacket& recv_data);
        void HandleResetInstancesOpcode(WorldPacket& recv_data);
        void HandleHearthandResurrect(WorldPacket& recv_data);

        void HandleAreaSpiritHealerQueryOpcode(WorldPacket& recv_data);
        void HandleAreaSpiritHealerQueueOpcode(WorldPacket& recv_data);
        void HandleCancelMountAuraOpcode(WorldPacket& recv_data);
        void HandleSelfResOpcode(WorldPacket& recv_data);
        void HandleComplainOpcode(WorldPacket& recv_data);
        void HandleRequestPetInfoOpcode(WorldPacket& recv_data);

        // Socket gem
        void HandleSocketOpcode(WorldPacket& recv_data);

        void HandleCancelTempEnchantmentOpcode(WorldPacket& recv_data);
        void HandleItemRefundInfoRequest(WorldPacket& recv_data);

        void HandleChannelVoiceOnOpcode(WorldPacket& recv_data);
        void HandleVoiceSessionEnableOpcode(WorldPacket& recv_data);
        void HandleSetActiveVoiceChannel(WorldPacket& recv_data);
        void HandleSetTaxiBenchmarkOpcode(WorldPacket& recv_data);

#ifdef ENABLE_PLAYERBOTS
        void HandleBotPackets();
#endif

        // for Warden
        uint16 GetClientBuild() const { return _build; }

        // Guild Bank
        void HandleGuildPermissions(WorldPacket& recv_data);
        void HandleGuildBankMoneyWithdrawn(WorldPacket& recv_data);
        void HandleGuildBankerActivate(WorldPacket& recv_data);
        void HandleGuildBankQueryTab(WorldPacket& recv_data);
        void HandleGuildBankLogQuery(WorldPacket& recv_data);
        void HandleGuildBankDepositMoney(WorldPacket& recv_data);
        void HandleGuildBankWithdrawMoney(WorldPacket& recv_data);
        void HandleGuildBankSwapItems(WorldPacket& recv_data);
        void HandleGuildBankUpdateTab(WorldPacket& recv_data);
        void HandleGuildBankBuyTab(WorldPacket& recv_data);
        void HandleQueryGuildBankTabText(WorldPacket& recv_data);
        void HandleSetGuildBankTabText(WorldPacket& recv_data);

        // Calendar
        void HandleCalendarGetCalendar(WorldPacket& recv_data);
        void HandleCalendarGetEvent(WorldPacket& recv_data);
        void HandleCalendarGuildFilter(WorldPacket& recv_data);
        void HandleCalendarEventSignup(WorldPacket& recvData);
        void HandleCalendarAddEvent(WorldPacket& recv_data);
        void HandleCalendarUpdateEvent(WorldPacket& recv_data);
        void HandleCalendarRemoveEvent(WorldPacket& recv_data);
        void HandleCalendarCopyEvent(WorldPacket& recv_data);
        void HandleCalendarEventInvite(WorldPacket& recv_data);
        void HandleCalendarEventRsvp(WorldPacket& recv_data);
        void HandleCalendarEventRemoveInvite(WorldPacket& recv_data);
        void HandleCalendarEventStatus(WorldPacket& recv_data);
        void HandleCalendarEventModeratorStatus(WorldPacket& recv_data);
        void HandleCalendarComplain(WorldPacket& recv_data);
        void HandleCalendarGetNumPending(WorldPacket& recv_data);

        // Hotfix handlers
        void HandleRequestHotfix(WorldPacket& recv_data);
        void SendItemDb2Reply(uint32 entry);
        void SendItemSparseDb2Reply(uint32 entry);

        void HandleObjectUpdateFailedOpcode(WorldPacket& recv_data);

        void HandleSpellClick(WorldPacket& recv_data);
        void HandleAlterAppearanceOpcode(WorldPacket& recv_data);
        void HandleRemoveGlyphOpcode(WorldPacket& recv_data);
        void HandleCharCustomizeOpcode(WorldPacket& recv_data);
        void HandleQueryInspectAchievementsOpcode(WorldPacket& recv_data);
        void HandleEquipmentSetSaveOpcode(WorldPacket& recv_data);
        void HandleEquipmentSetDeleteOpcode(WorldPacket& recv_data);
        void HandleEquipmentSetUseOpcode(WorldPacket& recv_data);
        void HandleUITimeRequestOpcode(WorldPacket& recv_data);
        void HandleReadyForAccountDataTimesOpcode(WorldPacket& recv_data);
        void HandleQuestPOIQueryOpcode(WorldPacket& recv_data);
        void HandleSetCurrencyFlagsOpcode(WorldPacket& recv_data);

        // Reforge
        void HandleReforgeItemOpcode(WorldPacket& recvData);
        void SendReforgeResult(bool success);

        void HandleLoadScreenOpcode(WorldPacket& recvPacket);
    private:
        friend class WorldSocket;

        /// Release the session's extra socket reference without closing the socket. Valid only
        /// before the session has been published to World.
        void AbandonUnpublishedSocket() noexcept;

        // private trade methods
        void moveItems(Item* myItems[], Item* hisItems[]);
        bool VerifyMovementInfo(MovementInfo const& movementInfo, ObjectGuid const& guid) const;
        bool VerifyMovementInfo(MovementInfo const& movementInfo) const;
        void HandleMoverRelocation(MovementInfo& movementInfo);

        void ExecuteOpcode(OpcodeHandler const& opHandle, WorldPacket* packet);

        // logging helper
        void LogUnexpectedOpcode(WorldPacket* packet, const char* reason);
        void LogUnprocessedTail(WorldPacket* packet);

        uint32 m_GUIDLow;                                   // set logged or recently logout player (while m_playerRecentlyLogout set)
        Player* _player;
        WorldSocket* m_Socket;
        std::string m_Address;

        AccountTypes _security;
        uint32 _accountId;
        uint8 m_expansion;

        // Warden
        Warden* _warden;                                    // Remains NULL if Warden system is not enabled by config
        uint16 _build;                                      // connected client build

        time_t _logoutTime;
        bool m_inQueue;                                     // session wait in auth.queue
        bool m_playerLoading;                               // code processed in LoginPlayer
        bool m_suppressWorldSends;                          // PHASE 6c: silence Cata-format sends after enter-world (MoP port scaffold)
        bool m_playerLogout;                                // code processed in LogoutPlayer
        bool m_playerRecentlyLogout;
        bool m_playerSave;                                  // code processed in LogoutPlayer with save request
        LocaleConstant m_sessionDbcLocale;
        int m_sessionDbLocaleIndex;
        uint32 m_latency;
        uint32 m_clientTimeDelay;
        AccountData m_accountData[NUM_ACCOUNT_DATA_TYPES];
        uint32 m_Tutorials[8];
        TutorialDataState m_tutorialState;
        AddonsList m_addonsList;
        std::vector<std::string> m_registeredAddonPrefixes;
        bool m_filterAddonMessages = true;
        ACE_Based::LockedQueue<WorldPacket*, ACE_Thread_Mutex> _recvQueue;
};
#endif
/// @}
