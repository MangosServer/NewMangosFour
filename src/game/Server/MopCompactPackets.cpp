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

#include "MopCompactPackets.h"
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
        out.FlushBits();
    }

    template <size_t N>
    void WriteGuidBytes(WorldPacket& out, uint64 guid, uint8 const (&order)[N])
    {
        for (uint8 index : order)
            out.WriteByteSeq(GuidByte(guid, index));
    }
}

void MopCompactPackets::BuildAttackSwingError(WorldPacket& out, uint8 reason)
{
    MANGOS_ASSERT(reason < 4);
    out.WriteBits(reason, 2);
    out.FlushBits();
}

void MopCompactPackets::BuildMoveSetSwimSpeed(WorldPacket& out, uint64 moverGuid, uint32 counter, float speed)
{
    // Reader sub_C8CBBE.
    const uint8 maskOrder[] = { 5, 0, 6, 3, 7, 2, 4, 1 };
    const uint8 bytesBeforeSpeed[] = { 1, 3 };
    const uint8 bytesAfterSpeed[] = { 6, 7, 0, 5, 2, 4 };

    WriteGuidMask(out, moverGuid, maskOrder);
    out << uint32(counter);
    WriteGuidBytes(out, moverGuid, bytesBeforeSpeed);
    out << float(speed);
    WriteGuidBytes(out, moverGuid, bytesAfterSpeed);
}

void MopCompactPackets::BuildRandomRoll(WorldPacket& out, uint64 rollerGuid, uint32 minimum, uint32 maximum, uint32 roll)
{
    // Reader sub_6D18F6 stores roll, minimum, maximum before its GUID block.
    const uint8 maskOrder[] = { 0, 6, 7, 1, 4, 5, 2, 3 };
    const uint8 byteOrder[] = { 5, 4, 2, 0, 3, 1, 6, 7 };

    out << uint32(roll);
    out << uint32(minimum);
    out << uint32(maximum);
    WriteGuidMask(out, rollerGuid, maskOrder);
    WriteGuidBytes(out, rollerGuid, byteOrder);
}

bool MopCompactPackets::BuildInstanceEncounter(WorldPacket& out, uint32 type, uint64 sourceGuid, uint8 param1, uint8 param2)
{
    // Dynamic callback sub_94E111 handles only discriminator values 0 through 10.
    if (type > 10)
        return false;

    out << uint32(type);
    switch (type)
    {
        case 0:
        case 5:
        case 6:
        case 8:
            out << uint8(param1);
            break;
        case 1:
        case 9:
        case 10:
            break;
        case 2:
        case 3:
        case 4:
            out.appendPackGUID(sourceGuid);
            out << uint8(param1);
            break;
        case 7:
            out << uint8(param1);
            out << uint8(param2);
            break;
    }
    return true;
}

void MopCompactPackets::BuildSetRaidDifficulty(WorldPacket& out, uint32 difficulty)
{
    // Dynamic callback sub_CCDD26 reads exactly one uint32.
    out << uint32(difficulty);
}
