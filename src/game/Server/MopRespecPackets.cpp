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

#include "MopRespecPackets.h"
#include "WorldPacket.h"

namespace
{
    uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (index * 8));
    }
}

void MopRespecPackets::BuildRespecWipeConfirm(WorldPacket& out,
    uint64 trainerGuid, uint8 discriminator, uint32 cost)
{
    uint8 const maskOrder[] = { 5, 7, 3, 2, 1, 0, 4, 6 };
    uint8 const firstBytes[] = { 1, 0 };
    uint8 const trailingBytes[] = { 7, 3, 2, 5, 6, 4 };

    for (uint8 index : maskOrder)
        out.WriteBit(GuidByte(trainerGuid, index) != 0);
    out.FlushBits();
    for (uint8 index : firstBytes)
        out.WriteByteSeq(GuidByte(trainerGuid, index));
    out << discriminator;
    for (uint8 index : trailingBytes)
        out.WriteByteSeq(GuidByte(trainerGuid, index));
    out << cost;
}

MopRespecPackets::ConfirmRespecWipe MopRespecPackets::ReadConfirmRespecWipe(
    WorldPacket& in)
{
    uint8 const maskOrder[] = { 7, 2, 6, 1, 4, 0, 3, 5 };
    uint8 const byteOrder[] = { 2, 4, 1, 3, 0, 5, 7, 6 };
    uint8 guidBytes[8] = {};
    ConfirmRespecWipe confirm;

    in >> confirm.discriminator;
    for (uint8 index : maskOrder)
        guidBytes[index] = in.ReadBit();
    for (uint8 index : byteOrder)
        in.ReadByteSeq(guidBytes[index]);

    confirm.trainerGuid = 0;
    for (uint8 index = 0; index < 8; ++index)
        confirm.trainerGuid |= uint64(guidBytes[index]) << (index * 8);
    return confirm;
}
