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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "MopAuthSession.h"
#include "Utilities/ByteBuffer.h"
#include <utility>

namespace MopAuth
{
    DecodeResult DecodeAuthSession(ByteBuffer& in, AuthSessionFields& out)
    {
        // Parse into a local and publish it only on success, so a rejected body can never leave
        // 'out' half-populated.
        AuthSessionFields parsed{};

        try
        {
            if (in.size() - in.rpos() < LegacyFixedPrefixBytes)
            {
                return DecodeResult::ShortBody;
            }

            // Legacy fixed prefix, reproduced verbatim from the pre-extraction inline reads.
            // The skips and the scattered digest[] indices are intentionally NOT reinterpreted:
            // this extraction is behaviour-preserving for valid input.
            in.read_skip<uint32_t>();
            in.read_skip<uint32_t>();
            in >> parsed.digest[18];
            in >> parsed.digest[14];
            in >> parsed.digest[3];
            in >> parsed.digest[4];
            in >> parsed.digest[0];
            in.read_skip<uint32_t>();
            in >> parsed.digest[11];
            in >> parsed.clientSeed;
            in >> parsed.digest[19];
            in.read_skip<uint8_t>();
            in.read_skip<uint8_t>();
            in >> parsed.digest[2];
            in >> parsed.digest[9];
            in >> parsed.digest[12];
            in.read_skip<uint64_t>();
            in.read_skip<uint32_t>();
            in >> parsed.digest[16];
            in >> parsed.digest[5];
            in >> parsed.digest[6];
            in >> parsed.digest[8];
            in >> parsed.builtNumberClient;                  // two bytes on the wire; never widen
            in >> parsed.digest[17];
            in >> parsed.digest[7];
            in >> parsed.digest[13];
            in >> parsed.digest[15];
            in >> parsed.digest[1];
            in >> parsed.digest[10];

            in >> parsed.addonSize;                          // addon data size

            // Bound the addon blob against the real remainder before allocating anything.
            if (parsed.addonSize < 4 || parsed.addonSize > in.size() - in.rpos())
            {
                return DecodeResult::BadAddonSize;
            }

            parsed.addonData.resize(parsed.addonSize);
            in.read(parsed.addonData.data(), parsed.addonSize);

            // The blob's first four little-endian bytes are its self-described inflated size.
            // Sanity-bound it here; actual inflation stays out of this decoder.
            uint32_t const inflatedSize =
                static_cast<uint32_t>(parsed.addonData[0])
                | (static_cast<uint32_t>(parsed.addonData[1]) << 8)
                | (static_cast<uint32_t>(parsed.addonData[2]) << 16)
                | (static_cast<uint32_t>(parsed.addonData[3]) << 24);

            if (inflatedSize == 0 || inflatedSize > MaxInflatedAddonBytes)
            {
                return DecodeResult::BadInflatedAddonSize;
            }

            // ReadBits(11) consumes two bytes; require them explicitly rather than via a throw.
            if (in.size() - in.rpos() < 2)
            {
                return DecodeResult::ShortBody;
            }

            // BUG-FOR-BUG: the legacy path issued ReadBits(11) with NO preceding ReadBit().
            // That is wrong for a real 5.4.8 client, but correcting it is out of scope here;
            // this extraction must not change behaviour for input the legacy code accepted.
            uint32_t const nameLength = in.ReadBits(11);

            if (nameLength == 0 || nameLength > MaxAccountNameBytes)
            {
                return DecodeResult::BadNameLength;
            }

            // ReadString() silently truncates at end-of-buffer, so reject the short case up front
            // instead of accepting a quietly shortened account name.
            if (nameLength > in.size() - in.rpos())
            {
                return DecodeResult::TruncatedName;
            }

            parsed.account = in.ReadString(nameLength);
        }
        catch (ByteBufferException const&)
        {
            // Final safety net only; the explicit bounds above should make this unreachable.
            // Deliberately logs nothing: this runs on unauthenticated, attacker-controlled input.
            return DecodeResult::ShortBody;
        }

        out = std::move(parsed);
        return DecodeResult::Ok;
    }
}
