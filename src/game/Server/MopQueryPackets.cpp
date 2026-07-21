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
 */

#include "MopQueryPackets.h"
#include "WorldPacket.h"

namespace
{
    uint8 GuidByte(uint64 guid, size_t index)
    {
        return uint8(guid >> (index * 8));
    }

    size_t OptionalCStringLength(std::string const& text, size_t bitCount)
    {
        size_t const encoded = text.empty() ? 0 : text.size() + 1;
        MANGOS_ASSERT(encoded < (size_t(1) << bitCount));
        return encoded;
    }
}

MopQueryPackets::NameQueryRequest MopQueryPackets::ReadNameQueryRequest(
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

void MopQueryPackets::BuildNameQueryResponse(WorldPacket& out,
    NameQueryResponse const& record)
{
    for (size_t index : { 3u, 6u, 7u, 2u, 5u, 4u, 0u, 1u })
        out.WriteBit(GuidByte(record.guid, index) != 0);
    out.FlushBits();

    for (size_t index : { 5u, 4u, 7u, 6u, 1u, 2u })
        out.WriteByteSeq(GuidByte(record.guid, index));
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

    out.WriteByteSeq(GuidByte(record.guid, 0));
    out.WriteByteSeq(GuidByte(record.guid, 3));
    if (record.result != 0)
        return;

    MANGOS_ASSERT(record.name.size() <= 48);
    for (std::string const& name : record.declinedNames)
        MANGOS_ASSERT(name.size() <= 64);

    out.WriteBit(GuidByte(record.auxiliaryGuid, 2) != 0);
    out.WriteBit(GuidByte(record.auxiliaryGuid, 7) != 0);
    out.WriteBit(GuidByte(record.displayGuid, 7) != 0);
    out.WriteBit(GuidByte(record.displayGuid, 2) != 0);
    out.WriteBit(GuidByte(record.displayGuid, 0) != 0);
    out.WriteBit(record.isDeleted);
    out.WriteBit(GuidByte(record.auxiliaryGuid, 4) != 0);
    out.WriteBit(GuidByte(record.displayGuid, 5) != 0);
    out.WriteBit(GuidByte(record.auxiliaryGuid, 1) != 0);
    out.WriteBit(GuidByte(record.auxiliaryGuid, 3) != 0);
    out.WriteBit(GuidByte(record.auxiliaryGuid, 0) != 0);
    for (std::string const& name : record.declinedNames)
        out.WriteBits(uint32(name.size()), 7);
    out.WriteBit(GuidByte(record.displayGuid, 6) != 0);
    out.WriteBit(GuidByte(record.displayGuid, 3) != 0);
    out.WriteBit(GuidByte(record.auxiliaryGuid, 5) != 0);
    out.WriteBit(GuidByte(record.displayGuid, 1) != 0);
    out.WriteBit(GuidByte(record.displayGuid, 4) != 0);
    out.WriteBits(uint32(record.name.size()), 6);
    out.WriteBit(GuidByte(record.auxiliaryGuid, 6) != 0);
    out.FlushBits();

    out.WriteByteSeq(GuidByte(record.displayGuid, 6));
    out.WriteByteSeq(GuidByte(record.displayGuid, 0));
    if (!record.name.empty())
        out.append(record.name.c_str(), record.name.size());
    out.WriteByteSeq(GuidByte(record.auxiliaryGuid, 5));
    out.WriteByteSeq(GuidByte(record.auxiliaryGuid, 2));
    out.WriteByteSeq(GuidByte(record.displayGuid, 3));
    out.WriteByteSeq(GuidByte(record.auxiliaryGuid, 4));
    out.WriteByteSeq(GuidByte(record.auxiliaryGuid, 3));
    out.WriteByteSeq(GuidByte(record.displayGuid, 4));
    out.WriteByteSeq(GuidByte(record.displayGuid, 2));
    out.WriteByteSeq(GuidByte(record.auxiliaryGuid, 7));
    for (std::string const& name : record.declinedNames)
        if (!name.empty())
            out.append(name.c_str(), name.size());
    out.WriteByteSeq(GuidByte(record.auxiliaryGuid, 6));
    out.WriteByteSeq(GuidByte(record.displayGuid, 7));
    out.WriteByteSeq(GuidByte(record.displayGuid, 1));
    out.WriteByteSeq(GuidByte(record.auxiliaryGuid, 1));
    out.WriteByteSeq(GuidByte(record.displayGuid, 5));
    out.WriteByteSeq(GuidByte(record.auxiliaryGuid, 0));
}

void MopQueryPackets::BuildQueryTimeResponse(WorldPacket& out, uint32 serverTime,
    uint32 secondsUntilReset)
{
    out << serverTime;
    out << secondsUntilReset;
}

bool MopQueryPackets::ReadPlayedTimeRequest(WorldPacket& in)
{
    return in.ReadBit();
}

void MopQueryPackets::BuildPlayedTimeResponse(WorldPacket& out, uint32 totalPlayed,
    uint32 levelPlayed, bool displayEvent)
{
    out << totalPlayed;
    out << levelPlayed;
    out.WriteBit(displayEvent);
    out.FlushBits();
}

bool MopQueryPackets::BuildMailQueryNextTimeResult(WorldPacket& out,
    std::vector<MailNextTimeEntry> const& records, float nextMailTime)
{
    // The 18414 client retains only three records, and the reference server
    // stops producing records at that same bound.
    if (records.size() > 3)
        return false;

    out.WriteBits(records.size(), 20);
    for (MailNextTimeEntry const& record : records)
    {
        out.WriteBit(GuidByte(record.senderGuid, 3) != 0);
        out.WriteBit(record.hasVirtualRealmAddress);
        out.WriteBit(GuidByte(record.senderGuid, 2) != 0);
        out.WriteBit(record.hasNativeRealmAddress);
        out.WriteBit(GuidByte(record.senderGuid, 6) != 0);
        out.WriteBit(GuidByte(record.senderGuid, 1) != 0);
        out.WriteBit(GuidByte(record.senderGuid, 4) != 0);
        out.WriteBit(GuidByte(record.senderGuid, 0) != 0);
        out.WriteBit(GuidByte(record.senderGuid, 5) != 0);
        out.WriteBit(GuidByte(record.senderGuid, 7) != 0);
    }
    out.FlushBits();

    for (MailNextTimeEntry const& record : records)
    {
        out << record.nonPlayerSender;
        out.WriteByteSeq(GuidByte(record.senderGuid, 5));
        out.WriteByteSeq(GuidByte(record.senderGuid, 4));
        out.WriteByteSeq(GuidByte(record.senderGuid, 6));
        out.WriteByteSeq(GuidByte(record.senderGuid, 1));
        out << record.messageType;
        out.WriteByteSeq(GuidByte(record.senderGuid, 0));
        out << record.deliveryTime;
        if (record.hasNativeRealmAddress)
            out << record.nativeRealmAddress;
        out << record.stationery;
        out.WriteByteSeq(GuidByte(record.senderGuid, 3));
        out.WriteByteSeq(GuidByte(record.senderGuid, 2));
        if (record.hasVirtualRealmAddress)
            out << record.virtualRealmAddress;
        out.WriteByteSeq(GuidByte(record.senderGuid, 7));
    }

    out << nextMailTime;
    return true;
}

void MopQueryPackets::BuildCreatureQueryResponse(WorldPacket& out,
    CreatureQueryResponse const& record)
{
    out << record.entry;
    out.WriteBit(record.hasData);
    if (!record.hasData)
    {
        out.FlushBits();
        return;
    }

    size_t const subNameLength = OptionalCStringLength(record.subName, 11);
    size_t const nameLength = OptionalCStringLength(record.name, 11);
    size_t const iconLength = OptionalCStringLength(record.iconName, 6);
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

MopQueryPackets::GameObjectQueryRequest MopQueryPackets::ReadGameObjectQueryRequest(
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

void MopQueryPackets::BuildGameObjectQueryResponse(WorldPacket& out,
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
