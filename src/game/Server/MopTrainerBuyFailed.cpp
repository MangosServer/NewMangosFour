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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "MopTrainerBuyFailed.h"
#include "WorldPacket.h"

namespace
{
    /// Byte @p i of a raw 64-bit guid, little-endian (byte 0 is the low byte).
    inline uint8 GuidByte(uint64 guid, int i)
    {
        return uint8(guid >> (8 * i));
    }

    /// Mask-bit order, from the eight sub_665262 calls at the top of sub_6D369B.
    const int kMaskOrder[8] = { 3, 0, 4, 7, 6, 1, 5, 2 };
    /// Guid bytes read before the first uint32.
    const int kBytesPre[5]  = { 1, 2, 0, 3, 4 };
    /// Guid bytes read between the two uint32s.
    const int kBytesPost[3] = { 5, 6, 7 };
}

void MopTrainerBuyFailed::Build(WorldPacket& out, uint64 trainerGuid, uint32 reason, uint32 serviceId)
{
    for (int i = 0; i < 8; ++i)
    {
        out.WriteBit(GuidByte(trainerGuid, kMaskOrder[i]) != 0);
    }

    // Exactly eight bits were written, so this emits one whole mask byte and
    // leaves the buffer byte-aligned for the byte block below.
    out.FlushBits();

    for (int i = 0; i < 5; ++i)
    {
        out.WriteByteSeq(GuidByte(trainerGuid, kBytesPre[i]));
    }

    out << uint32(reason);

    for (int i = 0; i < 3; ++i)
    {
        out.WriteByteSeq(GuidByte(trainerGuid, kBytesPost[i]));
    }

    out << uint32(serviceId);
}
