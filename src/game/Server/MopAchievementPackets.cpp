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

#include "MopAchievementPackets.h"
#include "WorldPacket.h"

namespace
{
    inline uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (8 * index));
    }
}

void MopAchievementPackets::BuildAllAchievementData(WorldPacket& out,
    std::vector<CompletedAchievement> const& completed,
    std::vector<CriteriaProgress> const& progress)
{
    MANGOS_ASSERT(progress.size() < (size_t(1) << 19));
    MANGOS_ASSERT(completed.size() < (size_t(1) << 20));

    out.WriteBits(uint32(progress.size()), 19);
    for (CriteriaProgress const& record : progress)
    {
        out.WriteBit(GuidByte(record.counterGuid, 3) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 3) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 6) != 0);
        out.WriteBit(GuidByte(record.counterGuid, 0) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 7) != 0);
        out.WriteBit(GuidByte(record.counterGuid, 1) != 0);
        out.WriteBit(GuidByte(record.counterGuid, 5) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 2) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 1) != 0);
        out.WriteBit(GuidByte(record.counterGuid, 7) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 4) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 0) != 0);
        out.WriteBit(GuidByte(record.counterGuid, 2) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 5) != 0);
        out.WriteBit(GuidByte(record.counterGuid, 4) != 0);
        out.WriteBits(0, 4);
        out.WriteBit(GuidByte(record.counterGuid, 6) != 0);
    }

    out.WriteBits(uint32(completed.size()), 20);
    for (CompletedAchievement const& record : completed)
    {
        out.WriteBit(GuidByte(record.playerGuid, 0) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 7) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 1) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 5) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 2) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 4) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 6) != 0);
        out.WriteBit(GuidByte(record.playerGuid, 3) != 0);
    }
    out.FlushBits();

    for (CompletedAchievement const& record : completed)
    {
        out << record.id << record.realmFirst;
        out.WriteByteSeq(GuidByte(record.playerGuid, 5));
        out.WriteByteSeq(GuidByte(record.playerGuid, 7));
        out << record.realm << record.packedDate;
        out.WriteByteSeq(GuidByte(record.playerGuid, 0));
        out.WriteByteSeq(GuidByte(record.playerGuid, 4));
        out.WriteByteSeq(GuidByte(record.playerGuid, 1));
        out.WriteByteSeq(GuidByte(record.playerGuid, 6));
        out.WriteByteSeq(GuidByte(record.playerGuid, 2));
        out.WriteByteSeq(GuidByte(record.playerGuid, 3));
    }

    for (CriteriaProgress const& record : progress)
    {
        out.WriteByteSeq(GuidByte(record.counterGuid, 7));
        out << record.timer1;
        out.WriteByteSeq(GuidByte(record.counterGuid, 6));
        out.WriteByteSeq(GuidByte(record.playerGuid, 1));
        out << record.id;
        out.WriteByteSeq(GuidByte(record.counterGuid, 4));
        out.WriteByteSeq(GuidByte(record.playerGuid, 0));
        out.WriteByteSeq(GuidByte(record.playerGuid, 4));
        out.WriteByteSeq(GuidByte(record.playerGuid, 6));
        out.WriteByteSeq(GuidByte(record.counterGuid, 1));
        out.WriteByteSeq(GuidByte(record.counterGuid, 5));
        out.WriteByteSeq(GuidByte(record.playerGuid, 7));
        out.WriteByteSeq(GuidByte(record.playerGuid, 2));
        out.WriteByteSeq(GuidByte(record.counterGuid, 2));
        out.WriteByteSeq(GuidByte(record.counterGuid, 0));
        out.WriteByteSeq(GuidByte(record.playerGuid, 3));
        out.WriteByteSeq(GuidByte(record.counterGuid, 3));
        out << record.timer2;
        out.WriteByteSeq(GuidByte(record.playerGuid, 5));
        out << record.packedDate;
    }
}
