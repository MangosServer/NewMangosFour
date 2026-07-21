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

#include "MopStablePackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

namespace
{
    uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (index * 8));
    }
}

uint64 MopStablePackets::ReadStableListRequest(WorldPacket& in)
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

bool MopStablePackets::BuildPetStableList(WorldPacket& out,
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
        out.WriteBit(GuidByte(stableMasterGuid, index) != 0);
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
        out.WriteByteSeq(GuidByte(stableMasterGuid, index));
    return true;
}

void MopStablePackets::BuildStableResult(WorldPacket& out, uint8 result)
{
    out.Initialize(SMSG_STABLE_RESULT, 1);
    out << result;
}
