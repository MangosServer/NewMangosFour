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

#include "MopCurrencyPackets.h"
#include "WorldPacket.h"

namespace MopCurrencyPackets
{
    bool BuildUpdateCurrency(WorldPacket& out, CurrencyUpdate const& update)
    {
        out << update.currencyId;
        out << update.gainContext;
        out << update.totalQuantity;
        out.WriteBit(update.hasWeeklyQuantity);
        out.WriteBit(update.suppressGainMessage);
        out.WriteBit(update.hasTrackedQuantity);

        if (update.hasWeeklyQuantity)
            out << update.weeklyQuantity;
        if (update.hasTrackedQuantity)
            out << update.trackedQuantity;
        out.FlushBits();

        return true;
    }

    bool BuildSetupCurrency(WorldPacket& out, std::vector<CurrencySetupEntry> const& entries)
    {
        if (entries.size() > 0x7FFFFu)
            return false;

        for (CurrencySetupEntry const& entry : entries)
        {
            if (entry.flags > 7)
                return false;
        }

        out.WriteBits(entries.size(), 19);
        for (CurrencySetupEntry const& entry : entries)
        {
            out.WriteBit(entry.hasTrackedQuantity);
            out.WriteBits(entry.flags, 3);
            out.WriteBit(entry.hasMaximumWeeklyQuantity);
            out.WriteBit(entry.hasWeeklyQuantity);
        }

        for (CurrencySetupEntry const& entry : entries)
        {
            if (entry.hasWeeklyQuantity)
                out << entry.weeklyQuantity;
            out << entry.currencyId;
            if (entry.hasTrackedQuantity)
                out << entry.trackedQuantity;
            out << entry.totalQuantity;
            if (entry.hasMaximumWeeklyQuantity)
                out << entry.maximumWeeklyQuantity;
        }
        out.FlushBits();

        return true;
    }
}
