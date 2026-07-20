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

#ifndef MANGOS_H_MOPCONTROLPACKETS
#define MANGOS_H_MOPCONTROLPACKETS

#include "Common.h"

class WorldPacket;

namespace MopControlPackets
{
    void BuildClientControlUpdate(WorldPacket& out, uint64 guid, bool allowMove);
    void BuildSetActiveMover(WorldPacket& out, uint64 guid);
}

#endif
