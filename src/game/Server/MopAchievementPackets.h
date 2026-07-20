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

#ifndef MANGOS_H_MOPACHIEVEMENTPACKETS
#define MANGOS_H_MOPACHIEVEMENTPACKETS

#include "Common.h"

#include <vector>

class WorldPacket;

namespace MopAchievementPackets
{
    struct CompletedAchievement
    {
        uint32 id = 0;
        uint64 playerGuid = 0;
        uint32 packedDate = 0;
        uint32 realmFirst = 0;
        uint32 realm = 0;
    };

    struct CriteriaProgress
    {
        uint32 id = 0;
        uint64 counterGuid = 0;
        uint64 playerGuid = 0;
        uint32 packedDate = 0;
        uint32 timer1 = 0;
        uint32 timer2 = 0;
    };

    void BuildAllAchievementData(WorldPacket& out,
        std::vector<CompletedAchievement> const& completed,
        std::vector<CriteriaProgress> const& progress);
}

#endif
