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

#ifndef MANGOS_H_MOPCURRENCYPACKETS
#define MANGOS_H_MOPCURRENCYPACKETS

#include "Common.h"

#include <vector>

class WorldPacket;

namespace MopCurrencyPackets
{
    struct CurrencyUpdate
    {
        uint32 currencyId;
        uint32 gainContext;
        uint32 totalQuantity;
        bool hasWeeklyQuantity;
        bool suppressGainMessage;
        bool hasTrackedQuantity;
        uint32 weeklyQuantity;
        uint32 trackedQuantity;
    };

    struct CurrencySetupEntry
    {
        uint32 currencyId;
        uint32 totalQuantity;
        uint32 weeklyQuantity;
        uint32 trackedQuantity;
        uint32 maximumWeeklyQuantity;
        uint8 flags;
        bool hasWeeklyQuantity;
        bool hasTrackedQuantity;
        bool hasMaximumWeeklyQuantity;
    };

    bool BuildUpdateCurrency(WorldPacket& out, CurrencyUpdate const& update);
    bool BuildSetupCurrency(WorldPacket& out, std::vector<CurrencySetupEntry> const& entries);
}

#endif
