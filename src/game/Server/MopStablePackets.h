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

#ifndef MANGOS_H_MOPSTABLEPACKETS
#define MANGOS_H_MOPSTABLEPACKETS

#include "Common.h"

#include <string>
#include <vector>

class WorldPacket;

namespace MopStablePackets
{
    struct StablePetRecord
    {
        uint32 entry = 0;
        uint32 level = 0;
        uint8 state = 0;
        uint32 modelId = 0;
        std::string name;
        uint32 petNumber = 0;
        uint32 slot = 0;
    };

    uint64 ReadStableListRequest(WorldPacket& in);
    bool BuildPetStableList(WorldPacket& out, uint64 stableMasterGuid,
        std::vector<StablePetRecord> const& records);
    void BuildStableResult(WorldPacket& out, uint8 result);
}

#endif
