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

#ifndef MANGOS_H_MOPCOMPACTPACKETS
#define MANGOS_H_MOPCOMPACTPACKETS

#include "Common.h"

class WorldPacket;

/**
 * Pure body serializers for compact server packets consumed by WoW
 * 5.4.8.18414. Callers own opcode selection and packet delivery.
 */
namespace MopCompactPackets
{
    enum AttackSwingReason
    {
        ATTACK_SWING_NOT_IN_RANGE = 0,
        ATTACK_SWING_BAD_FACING   = 1,
        ATTACK_SWING_DEAD_TARGET  = 2,
        ATTACK_SWING_CANT_ATTACK  = 3
    };

    void BuildAttackSwingError(WorldPacket& out, uint8 reason);
    void BuildMoveSetSwimSpeed(WorldPacket& out, uint64 moverGuid, uint32 counter, float speed);
    void BuildRandomRoll(WorldPacket& out, uint64 rollerGuid, uint32 minimum, uint32 maximum, uint32 roll);
    bool BuildInstanceEncounter(WorldPacket& out, uint32 type, uint64 sourceGuid, uint8 param1, uint8 param2);
    void BuildSetRaidDifficulty(WorldPacket& out, uint32 difficulty);
}

#endif
