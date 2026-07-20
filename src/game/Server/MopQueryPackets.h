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

#ifndef MANGOS_H_MOPQUERYPACKETS
#define MANGOS_H_MOPQUERYPACKETS

#include "Common.h"

#include <array>
#include <string>
#include <vector>

class WorldPacket;

namespace MopQueryPackets
{
    void BuildQueryTimeResponse(WorldPacket& out, uint32 serverTime,
        uint32 secondsUntilReset);
    bool ReadPlayedTimeRequest(WorldPacket& in);
    void BuildPlayedTimeResponse(WorldPacket& out, uint32 totalPlayed,
        uint32 levelPlayed, bool displayEvent);

    struct CreatureQueryResponse
    {
        uint32 entry = 0;
        bool hasData = false;
        std::string name;
        std::string subName;
        std::string iconName;
        uint32 creatureType = 0;
        uint32 family = 0;
        uint32 rank = 0;
        uint32 expansion = 0;
        uint32 movementTemplateId = 0;
        uint32 creatureTypeFlags = 0;
        uint32 creatureTypeFlags2 = 0;
        std::array<uint32, 4> modelIds{};
        std::array<uint32, 2> killCredits{};
        float healthMultiplier = 0.0f;
        float powerMultiplier = 0.0f;
        bool racialLeader = false;
        std::array<uint32, 6> questItems{};
    };

    void BuildCreatureQueryResponse(WorldPacket& out,
        CreatureQueryResponse const& record);

    struct GameObjectQueryRequest
    {
        uint32 entry = 0;
        uint64 guid = 0;
    };

    struct GameObjectQueryResponse
    {
        uint32 entry = 0;
        bool hasData = false;
        uint32 type = 0;
        uint32 displayId = 0;
        std::array<std::string, 4> names;
        std::string iconName;
        std::string castBarCaption;
        std::string unknownString;
        std::array<uint32, 32> data{};
        float size = 0.0f;
        std::vector<uint32> questItems;
        uint32 trailingUnknown = 0;
    };

    GameObjectQueryRequest ReadGameObjectQueryRequest(WorldPacket& in);
    void BuildGameObjectQueryResponse(WorldPacket& out,
        GameObjectQueryResponse const& record);
}

#endif
