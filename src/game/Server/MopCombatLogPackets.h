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

#ifndef MANGOS_H_MOPCOMBATLOGPACKETS
#define MANGOS_H_MOPCOMBATLOGPACKETS

#include "Common.h"

class WorldPacket;

namespace MopCombatLogPackets
{
    enum class ExecuteKind
    {
        ExtraAttacks,
        PowerChange,
        PetFeedItem,
        CreatedItem,
        Target
    };

    struct SpellExecuteLog
    {
        uint64 casterGuid;
        uint64 targetGuid;
        uint32 spellId;
        uint32 effectId;
        uint32 amount;
        uint32 powerType;
        uint32 itemEntry;
        float multiplier;
        ExecuteKind kind;
    };

    enum class PeriodicKind
    {
        Damage,
        Heal,
        Energize,
        ManaLeech
    };

    struct PeriodicAuraLog
    {
        uint64 targetGuid;
        uint64 casterGuid;
        uint32 spellId;
        uint32 auraType;
        uint32 amount;
        uint32 overAmount;
        uint32 schoolOrPowerType;
        uint32 absorb;
        uint32 resist;
        float multiplier;
        bool critical;
        PeriodicKind kind;
    };

    bool BuildSpellExecuteLog(WorldPacket& out, SpellExecuteLog const& log);
    void BuildPeriodicAuraLog(WorldPacket& out, PeriodicAuraLog const& log);
}

#endif
