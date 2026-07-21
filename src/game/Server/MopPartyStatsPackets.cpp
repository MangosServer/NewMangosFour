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

#include "MopPartyStatsPackets.h"
#include "WorldPacket.h"

namespace
{
    uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (index * 8));
    }
}

MopPartyStatsPackets::Request MopPartyStatsPackets::ReadRequest(WorldPacket& in)
{
    uint8 const maskOrder[] = { 7, 4, 0, 1, 3, 6, 2, 5 };
    uint8 const byteOrder[] = { 3, 6, 5, 2, 1, 4, 0, 7 };
    uint8 guidBytes[8] = {};
    Request request;

    in >> request.mode;
    for (uint8 index : maskOrder)
        guidBytes[index] = in.ReadBit();
    for (uint8 index : byteOrder)
        in.ReadByteSeq(guidBytes[index]);

    for (uint8 index = 0; index < 8; ++index)
        request.memberGuid |= uint64(guidBytes[index]) << (index * 8);
    return request;
}

void MopPartyStatsPackets::BuildResponse(WorldPacket& out, uint64 memberGuid,
    uint32 updateMask, bool resetState, bool associateGuid,
    ByteBuffer const& payload)
{
    uint8 const firstBytes[] = { 3, 2, 6, 7, 5 };
    uint8 const trailingBytes[] = { 1, 4, 0 };

    out.Initialize(SMSG_PARTY_MEMBER_STATS,
        2 + 8 + sizeof(updateMask) + sizeof(uint32) + payload.size());
    out.WriteBit(GuidByte(memberGuid, 0) != 0);
    out.WriteBit(GuidByte(memberGuid, 5) != 0);
    out.WriteBit(resetState);
    out.WriteBit(GuidByte(memberGuid, 1) != 0);
    out.WriteBit(GuidByte(memberGuid, 4) != 0);
    out.WriteBit(associateGuid);
    out.WriteBit(GuidByte(memberGuid, 6) != 0);
    out.WriteBit(GuidByte(memberGuid, 2) != 0);
    out.WriteBit(GuidByte(memberGuid, 7) != 0);
    out.WriteBit(GuidByte(memberGuid, 3) != 0);
    out.FlushBits();

    for (uint8 index : firstBytes)
        out.WriteByteSeq(GuidByte(memberGuid, index));
    out << updateMask;
    for (uint8 index : trailingBytes)
        out.WriteByteSeq(GuidByte(memberGuid, index));
    out << uint32(payload.size());
    if (!payload.empty())
        out.append(payload.contents(), payload.size());
}
