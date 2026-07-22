/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "MopAuthKey.h"
#include "BigNumber.h"

namespace MopAuth
{
    bool SessionKeyFromHex(const char* hex, uint8 out[SESSION_KEY_LEN])
    {
        if (!IsPlausibleSessionKeyHex(hex))
        {
            return false;
        }

        BigNumber k;
        k.SetHexStr(hex);

        // AsByteArray() is ALREADY LITTLE-ENDIAN and needs NO reversal: BN_bn2bin writes
        // big-endian and AsByteArray then does std::reverse over the whole length
        // (BigNumber.cpp:179-197). What it returns is exactly K's own byte order -- just SHORT,
        // because BN_bn2hex dropped any leading zero bytes when realmd wrote the row.
        //
        //   DO NOT reverse it again. That was plan v2's defect: a double reversal yields a
        //   big-endian K and every proof fails deterministically.
        //   DO NOT pass minSize=40. AsByteArray(40) byte-ROTATES rather than pads (see the header).
        //
        // Copy immediately: BigNumber OWNS the returned buffer and frees it on the next call, so a
        // unique_ptr around it would DOUBLE FREE and holding the pointer would dangle.
        return Raw40FromLittleEndian(k.AsByteArray(), size_t(k.GetNumBytes()), out);
    }
}
