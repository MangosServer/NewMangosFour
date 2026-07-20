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
    size_t OptionalCStringLength(std::string const& text, size_t bitCount)
    {
        size_t const encoded = text.empty() ? 0 : text.size() + 1;
        MANGOS_ASSERT(encoded < (size_t(1) << bitCount));
        return encoded;
    }
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
