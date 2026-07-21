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

#ifndef MANGOS_H_MOPREADYCHECKPACKETS
#define MANGOS_H_MOPREADYCHECKPACKETS

#include "Common.h"

class WorldPacket;

namespace MopReadyCheckPackets
{
    struct ResponseRequest
    {
        uint8 partyIndex = 0;
        uint64 reservedGuid = 0;
        bool ready = false;
    };

    uint8 ReadStartRequest(WorldPacket& in);
    ResponseRequest ReadResponseRequest(WorldPacket& in);

    void BuildStarted(WorldPacket& out, uint64 groupGuid, uint64 initiatorGuid,
        uint32 duration, uint8 partyIndex);
    void BuildResponse(WorldPacket& out, uint64 groupGuid, uint64 playerGuid,
        bool ready);
    void BuildCompleted(WorldPacket& out, uint64 groupGuid, uint8 partyIndex);
}

#endif
