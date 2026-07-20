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

#include "MopWorldEntryPackets.h"
#include "WorldPacket.h"

namespace
{
    inline uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (8 * index));
    }

    template <size_t N>
    void WriteGuidBytes(WorldPacket& out, uint64 guid, uint8 const (&order)[N])
    {
        for (uint8 index : order)
            out.WriteByteSeq(GuidByte(guid, index));
    }
}

void MopWorldEntryPackets::BuildLoginVerifyWorld(WorldPacket& out, uint32 mapId,
    float x, float y, float z, float orientation)
{
    out << float(x) << float(orientation) << float(y) << uint32(mapId) << float(z);
}

void MopWorldEntryPackets::BuildNewWorld(WorldPacket& out, uint32 mapId,
    float x, float y, float z, float orientation)
{
    out << float(x) << uint32(mapId) << float(y) << float(z) << float(orientation);
}

void MopWorldEntryPackets::BuildLoginSetTimeSpeed(WorldPacket& out,
    uint32 packedGameTime, float speed)
{
    out << uint32(0) << uint32(packedGameTime) << uint32(0)
        << uint32(packedGameTime) << float(speed);
}

void MopWorldEntryPackets::BuildMoveTeleport(WorldPacket& out, uint64 moverGuid,
    uint64 transportGuid, uint32 counter, float x, float y, float z,
    float orientation)
{
    bool const hasTransport = transportGuid != 0;
    uint8 const moverMaskA[] = { 0, 6, 5, 7, 2 };
    uint8 const transportMask[] = { 1, 3, 6, 4, 5, 2, 0, 7 };
    uint8 const moverMaskB[] = { 3, 1 };

    for (uint8 index : moverMaskA)
        out.WriteBit(GuidByte(moverGuid, index) != 0);
    out.WriteBit(hasTransport);
    out.WriteBit(GuidByte(moverGuid, 4) != 0);
    if (hasTransport)
        for (uint8 index : transportMask)
            out.WriteBit(GuidByte(transportGuid, index) != 0);
    for (uint8 index : moverMaskB)
        out.WriteBit(GuidByte(moverGuid, index) != 0);
    out.WriteBit(false);
    out.FlushBits();

    if (hasTransport)
    {
        uint8 const transportBytes[] = { 4, 3, 7, 1, 6, 0, 2, 5 };
        WriteGuidBytes(out, transportGuid, transportBytes);
    }

    uint8 const moverBytesA[] = { 4, 7 };
    uint8 const moverBytesB[] = { 2, 3, 5 };
    uint8 const moverBytesC[] = { 0, 6, 1 };
    WriteGuidBytes(out, moverGuid, moverBytesA);
    out << float(z) << float(y);
    WriteGuidBytes(out, moverGuid, moverBytesB);
    out << float(x) << uint32(counter);
    WriteGuidBytes(out, moverGuid, moverBytesC);
    out << float(orientation);
}
