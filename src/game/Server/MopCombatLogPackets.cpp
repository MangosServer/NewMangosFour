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

#include "MopCombatLogPackets.h"
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

bool MopCombatLogPackets::BuildSpellExecuteLog(WorldPacket& out, SpellExecuteLog const& log)
{
    bool const extraAttacks = log.kind == ExecuteKind::ExtraAttacks;
    bool const powerChange = log.kind == ExecuteKind::PowerChange;
    bool const petFeedItem = log.kind == ExecuteKind::PetFeedItem;
    bool const createdItem = log.kind == ExecuteKind::CreatedItem;
    bool const target = log.kind == ExecuteKind::Target;
    if (!extraAttacks && !powerChange && !petFeedItem && !createdItem && !target)
        return false;

    uint8 const casterMaskPre[] = { 0, 6, 5, 7, 2 };
    uint8 const extraMask[] = { 5, 4, 2, 3, 1, 0, 6, 7 };
    uint8 const powerMask[] = { 0, 3, 1, 5, 6, 4, 7, 2 };
    uint8 const targetMask[] = { 6, 5, 1, 0, 3, 4, 7, 2 };
    uint8 const targetBytes[] = { 7, 5, 1, 2, 6, 4, 0, 3 };
    uint8 const powerBytesA[] = { 3, 7, 5, 2, 0 };
    uint8 const powerBytesB[] = { 4, 1, 6 };
    uint8 const extraBytesA[] = { 0, 6, 4, 7, 2, 5, 3 };
    uint8 const casterBytes[] = { 5, 7, 1, 6, 2, 0, 4, 3 };

    WriteGuidMask(out, log.casterGuid, casterMaskPre);
    out.WriteBits(1, 19);
    out.WriteBit(GuidByte(log.casterGuid, 4) != 0);

    out.WriteBits(extraAttacks ? 1 : 0, 21);
    if (extraAttacks)
        WriteGuidMask(out, log.targetGuid, extraMask);
    out.WriteBits(powerChange ? 1 : 0, 20);
    if (powerChange)
        WriteGuidMask(out, log.targetGuid, powerMask);
    out.WriteBits(0, 21); // Unresolved vector C is deliberately never populated.
    out.WriteBits(petFeedItem ? 1 : 0, 22);
    out.WriteBits(createdItem ? 1 : 0, 22);
    out.WriteBits(target ? 1 : 0, 24);
    if (target)
        WriteGuidMask(out, log.targetGuid, targetMask);
    out.WriteBit(GuidByte(log.casterGuid, 1) != 0);
    out.WriteBit(GuidByte(log.casterGuid, 3) != 0);
    out.WriteBit(false); // No optional spell-cast-log data.
    out.FlushBits();

    if (target)
        WriteGuidBytes(out, log.targetGuid, targetBytes);
    if (powerChange)
    {
        WriteGuidBytes(out, log.targetGuid, powerBytesA);
        out << uint32(log.powerType);
        out << uint32(log.amount);
        out.WriteByteSeq(GuidByte(log.targetGuid, powerBytesB[0]));
        out.WriteByteSeq(GuidByte(log.targetGuid, powerBytesB[1]));
        out << float(log.multiplier);
        out.WriteByteSeq(GuidByte(log.targetGuid, powerBytesB[2]));
    }
    if (extraAttacks)
    {
        WriteGuidBytes(out, log.targetGuid, extraBytesA);
        out << uint32(log.amount);
        out.WriteByteSeq(GuidByte(log.targetGuid, 1));
    }
    if (petFeedItem)
        out << uint32(log.itemEntry);
    out << uint32(log.effectId);
    if (createdItem)
        out << uint32(log.itemEntry);
    out << uint32(log.spellId);
    WriteGuidBytes(out, log.casterGuid, casterBytes);
    return true;
}

void MopCombatLogPackets::BuildPeriodicAuraLog(WorldPacket& out, PeriodicAuraLog const& log)
{
    bool const damage = log.kind == PeriodicKind::Damage;
    bool const heal = log.kind == PeriodicKind::Heal;
    bool const energize = log.kind == PeriodicKind::Energize;
    bool const manaLeech = log.kind == PeriodicKind::ManaLeech;
    bool const has8 = damage || heal;
    bool const has12 = damage || energize || manaLeech;
    bool const has16 = damage || heal || manaLeech;
    bool const has20 = damage;

    out.WriteBit(GuidByte(log.targetGuid, 7) != 0);
    out.WriteBit(GuidByte(log.casterGuid, 0) != 0);
    out.WriteBit(GuidByte(log.casterGuid, 7) != 0);
    out.WriteBit(GuidByte(log.targetGuid, 1) != 0);
    out.WriteBits(1, 21);
    out.WriteBit(GuidByte(log.targetGuid, 0) != 0);
    out.WriteBit(!has8);
    out.WriteBit(!has16);
    out.WriteBit(log.critical);
    out.WriteBit(!has20);
    out.WriteBit(!has12);
    out.WriteBit(GuidByte(log.targetGuid, 5) != 0);
    out.WriteBit(GuidByte(log.targetGuid, 3) != 0);
    out.WriteBit(GuidByte(log.casterGuid, 1) != 0);
    out.WriteBit(GuidByte(log.targetGuid, 2) != 0);
    out.WriteBit(GuidByte(log.casterGuid, 6) != 0);
    out.WriteBit(GuidByte(log.casterGuid, 3) != 0);
    out.WriteBit(GuidByte(log.casterGuid, 4) != 0);
    out.WriteBit(false); // No optional power data.
    out.WriteBit(GuidByte(log.casterGuid, 2) != 0);
    out.WriteBit(GuidByte(log.targetGuid, 6) != 0);
    out.WriteBit(GuidByte(log.casterGuid, 5) != 0);
    out.WriteBit(GuidByte(log.targetGuid, 4) != 0);
    out.FlushBits();

    if (has8)
        out << uint32(log.overAmount);
    out << uint32(log.amount);
    out << uint32(log.auraType);
    if (has20)
        out << uint32(log.resist);
    if (has16)
    {
        if (manaLeech)
            out << float(log.multiplier);
        else
            out << uint32(log.absorb);
    }
    if (has12)
        out << uint32(log.schoolOrPowerType);

    uint8 const bytesCasterA[] = { 5, 3 };
    uint8 const bytesTargetA[] = { 4 };
    uint8 const bytesTargetB[] = { 6 };
    uint8 const bytesCasterB[] = { 7, 1 };
    uint8 const bytesTargetC[] = { 5 };
    uint8 const bytesCasterC[] = { 0 };
    uint8 const bytesTargetD[] = { 1, 7 };
    uint8 const bytesCasterD[] = { 4 };
    uint8 const bytesTargetE[] = { 3 };
    uint8 const bytesCasterE[] = { 2 };
    uint8 const bytesTargetF[] = { 0, 2 };
    uint8 const bytesCasterF[] = { 6 };

    WriteGuidBytes(out, log.casterGuid, bytesCasterA);
    WriteGuidBytes(out, log.targetGuid, bytesTargetA);
    out << uint32(log.spellId);
    WriteGuidBytes(out, log.targetGuid, bytesTargetB);
    WriteGuidBytes(out, log.casterGuid, bytesCasterB);
    WriteGuidBytes(out, log.targetGuid, bytesTargetC);
    WriteGuidBytes(out, log.casterGuid, bytesCasterC);
    WriteGuidBytes(out, log.targetGuid, bytesTargetD);
    WriteGuidBytes(out, log.casterGuid, bytesCasterD);
    WriteGuidBytes(out, log.targetGuid, bytesTargetE);
    WriteGuidBytes(out, log.casterGuid, bytesCasterE);
    WriteGuidBytes(out, log.targetGuid, bytesTargetF);
    WriteGuidBytes(out, log.casterGuid, bytesCasterF);
}
