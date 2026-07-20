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

#include "MopControlPackets.h"
#include "WorldPacket.h"

namespace
{
    inline uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (8 * index));
    }

    template <size_t N>
    void WriteGuidMask(WorldPacket& out, uint64 guid, uint8 const (&order)[N])
    {
        for (uint8 index : order)
            out.WriteBit(GuidByte(guid, index) != 0);
    }

    template <size_t N>
    void WriteGuidBytes(WorldPacket& out, uint64 guid, uint8 const (&order)[N])
    {
        for (uint8 index : order)
            out.WriteByteSeq(GuidByte(guid, index));
    }
}

void MopControlPackets::BuildClientControlUpdate(WorldPacket& out, uint64 guid,
    bool allowMove)
{
    uint8 const maskA[] = { 2, 7 };
    uint8 const maskB[] = { 0, 3, 6, 5, 1, 4 };
    uint8 const bytes[] = { 1, 5, 7, 4, 2, 6, 3, 0 };

    WriteGuidMask(out, guid, maskA);
    out.WriteBit(allowMove);
    WriteGuidMask(out, guid, maskB);
    out.FlushBits();
    WriteGuidBytes(out, guid, bytes);
}

void MopControlPackets::BuildSetActiveMover(WorldPacket& out, uint64 guid)
{
    uint8 const mask[] = { 5, 1, 4, 2, 3, 7, 0, 6 };
    uint8 const bytes[] = { 4, 6, 2, 0, 3, 7, 5, 1 };

    WriteGuidMask(out, guid, mask);
    out.FlushBits();
    WriteGuidBytes(out, guid, bytes);
}
