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

#ifndef MANGOS_H_MOPPARTYSTATSPACKETS
#define MANGOS_H_MOPPARTYSTATSPACKETS

#include "Common.h"

class ByteBuffer;
class WorldPacket;

namespace MopPartyStatsPackets
{
    struct Request
    {
        uint8 mode = 0;
        uint64 memberGuid = 0;
    };

    Request ReadRequest(WorldPacket& in);
    void BuildResponse(WorldPacket& out, uint64 memberGuid, uint32 updateMask,
        bool resetState, bool associateGuid, ByteBuffer const& payload);
}

#endif
