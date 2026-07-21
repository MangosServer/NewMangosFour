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

#include "MopReadyCheckPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

namespace
{
    uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (index * 8));
    }

    uint64 AssembleGuid(uint8 const bytes[8])
    {
        uint64 guid = 0;
        for (uint8 index = 0; index < 8; ++index)
            guid |= uint64(bytes[index]) << (index * 8);
        return guid;
    }
}

uint8 MopReadyCheckPackets::ReadStartRequest(WorldPacket& in)
{
    uint8 partyIndex = 0;
    in >> partyIndex;
    return partyIndex;
}

MopReadyCheckPackets::ResponseRequest
MopReadyCheckPackets::ReadResponseRequest(WorldPacket& in)
{
    ResponseRequest request;
    uint8 guidBytes[8] = {};

    in >> request.partyIndex;
    uint8 const maskOrder[] = { 2, 1, 0, 3, 6 };
    for (uint8 index : maskOrder)
        guidBytes[index] = in.ReadBit();
    request.ready = in.ReadBit();
    uint8 const finalMaskOrder[] = { 7, 4, 5 };
    for (uint8 index : finalMaskOrder)
        guidBytes[index] = in.ReadBit();

    uint8 const byteOrder[] = { 1, 0, 3, 2, 4, 5, 7, 6 };
    for (uint8 index : byteOrder)
        in.ReadByteSeq(guidBytes[index]);

    request.reservedGuid = AssembleGuid(guidBytes);
    return request;
}

void MopReadyCheckPackets::BuildStarted(WorldPacket& out, uint64 groupGuid,
    uint64 initiatorGuid, uint32 duration, uint8 partyIndex)
{
    uint8 const maskOwner[] = { 4, 2 };
    uint8 const maskInitiatorFirst[] = { 4 };
    uint8 const maskOwnerMiddle[] = { 3, 7, 1, 0 };
    uint8 const maskInitiatorMiddle[] = { 6, 5 };
    uint8 const maskOwnerLast[] = { 6, 5 };
    uint8 const maskInitiatorLast[] = { 0, 1, 2, 7, 3 };

    out.Initialize(SMSG_RAID_READY_CHECK, 32);
    for (uint8 index : maskOwner)
        out.WriteBit(GuidByte(groupGuid, index) != 0);
    for (uint8 index : maskInitiatorFirst)
        out.WriteBit(GuidByte(initiatorGuid, index) != 0);
    for (uint8 index : maskOwnerMiddle)
        out.WriteBit(GuidByte(groupGuid, index) != 0);
    for (uint8 index : maskInitiatorMiddle)
        out.WriteBit(GuidByte(initiatorGuid, index) != 0);
    for (uint8 index : maskOwnerLast)
        out.WriteBit(GuidByte(groupGuid, index) != 0);
    for (uint8 index : maskInitiatorLast)
        out.WriteBit(GuidByte(initiatorGuid, index) != 0);
    out.FlushBits();

    out << duration;
    out.WriteByteSeq(GuidByte(groupGuid, 2));
    out.WriteByteSeq(GuidByte(groupGuid, 7));
    out.WriteByteSeq(GuidByte(groupGuid, 3));
    out.WriteByteSeq(GuidByte(initiatorGuid, 4));
    out.WriteByteSeq(GuidByte(groupGuid, 1));
    out.WriteByteSeq(GuidByte(groupGuid, 0));
    out.WriteByteSeq(GuidByte(initiatorGuid, 1));
    out.WriteByteSeq(GuidByte(initiatorGuid, 2));
    out.WriteByteSeq(GuidByte(initiatorGuid, 6));
    out.WriteByteSeq(GuidByte(initiatorGuid, 5));
    out.WriteByteSeq(GuidByte(groupGuid, 6));
    out.WriteByteSeq(GuidByte(initiatorGuid, 0));
    out << partyIndex;
    out.WriteByteSeq(GuidByte(initiatorGuid, 7));
    out.WriteByteSeq(GuidByte(groupGuid, 4));
    out.WriteByteSeq(GuidByte(initiatorGuid, 3));
    out.WriteByteSeq(GuidByte(groupGuid, 5));
}

void MopReadyCheckPackets::BuildResponse(WorldPacket& out, uint64 groupGuid,
    uint64 playerGuid, bool ready)
{
    out.Initialize(SMSG_RAID_READY_CHECK_CONFIRM, 24);
    out.WriteBit(GuidByte(groupGuid, 4) != 0);
    out.WriteBit(GuidByte(playerGuid, 5) != 0);
    out.WriteBit(GuidByte(playerGuid, 3) != 0);
    out.WriteBit(ready);
    out.WriteBit(GuidByte(groupGuid, 2) != 0);
    out.WriteBit(GuidByte(playerGuid, 6) != 0);
    out.WriteBit(GuidByte(groupGuid, 3) != 0);
    out.WriteBit(GuidByte(playerGuid, 0) != 0);
    out.WriteBit(GuidByte(playerGuid, 1) != 0);
    out.WriteBit(GuidByte(groupGuid, 1) != 0);
    out.WriteBit(GuidByte(groupGuid, 5) != 0);
    out.WriteBit(GuidByte(playerGuid, 7) != 0);
    out.WriteBit(GuidByte(playerGuid, 4) != 0);
    out.WriteBit(GuidByte(groupGuid, 6) != 0);
    out.WriteBit(GuidByte(playerGuid, 2) != 0);
    out.WriteBit(GuidByte(groupGuid, 0) != 0);
    out.WriteBit(GuidByte(groupGuid, 7) != 0);
    out.FlushBits();

    out.WriteByteSeq(GuidByte(playerGuid, 4));
    out.WriteByteSeq(GuidByte(playerGuid, 2));
    out.WriteByteSeq(GuidByte(playerGuid, 1));
    out.WriteByteSeq(GuidByte(groupGuid, 4));
    out.WriteByteSeq(GuidByte(groupGuid, 2));
    out.WriteByteSeq(GuidByte(playerGuid, 0));
    out.WriteByteSeq(GuidByte(groupGuid, 5));
    out.WriteByteSeq(GuidByte(groupGuid, 3));
    out.WriteByteSeq(GuidByte(playerGuid, 7));
    out.WriteByteSeq(GuidByte(groupGuid, 6));
    out.WriteByteSeq(GuidByte(groupGuid, 1));
    out.WriteByteSeq(GuidByte(playerGuid, 6));
    out.WriteByteSeq(GuidByte(playerGuid, 3));
    out.WriteByteSeq(GuidByte(playerGuid, 5));
    out.WriteByteSeq(GuidByte(groupGuid, 0));
    out.WriteByteSeq(GuidByte(groupGuid, 7));
}

void MopReadyCheckPackets::BuildCompleted(WorldPacket& out, uint64 groupGuid,
    uint8 partyIndex)
{
    uint8 const maskOrder[] = { 4, 2, 5, 7, 1, 0, 3, 6 };
    uint8 const firstBytes[] = { 6, 0, 3, 1, 5 };
    uint8 const finalBytes[] = { 7, 2, 4 };

    out.Initialize(SMSG_RAID_READY_CHECK_COMPLETED, 12);
    for (uint8 index : maskOrder)
        out.WriteBit(GuidByte(groupGuid, index) != 0);
    out.FlushBits();
    for (uint8 index : firstBytes)
        out.WriteByteSeq(GuidByte(groupGuid, index));
    out << partyIndex;
    for (uint8 index : finalBytes)
        out.WriteByteSeq(GuidByte(groupGuid, index));
}
