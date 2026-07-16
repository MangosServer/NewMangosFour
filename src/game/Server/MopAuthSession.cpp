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

            // Bound the addon blob against the real remainder before allocating anything: addonSize
            // is attacker-controlled and drives the resize() below, and the legacy path would have
            // thrown ByteBufferException here anyway. There is deliberately NO lower bound: a blob
            // too small to carry a 4-byte header is not malformed. Legacy accepted 0..3 -- it did
            // resize(n) then read(..., n), which copies n bytes and cannot throw while n <= the
            // bytes remaining -- and ReadAddonsInfo likewise bails benignly on such a blob
            // ("if (data.rpos() + 4 > data.size()) { return; }"). Rejecting it would change
            // behaviour on valid input.
            if (parsed.addonSize > in.size() - in.rpos())
            {
                return DecodeResult::BadAddonSize;
            }

            parsed.addonData.resize(parsed.addonSize);

            // Guarded because addonSize may now be 0: data() on an empty vector may be nullptr, and
            // read() would index &_storage[_rpos] with _rpos possibly == size(). Skipping the call
            // is behaviour-identical, NOT just a shortcut -- read() resets the bit reader
            // (_bitpos = 8), but the preceding 'in >> parsed.addonSize' already did so via
            // read<T>(), so ReadBits(11) below sees exactly the same state either way.
            if (parsed.addonSize > 0)
            {
                in.read(parsed.addonData.data(), parsed.addonSize);
            }

            // The blob's self-described inflated size is deliberately NOT inspected here, and must
            // never decide authentication. The real consumer, WorldSession::ReadAddonsInfo
            // (WorldSession::ReadAddonsInfo), treats both out-of-range cases as benign: size 0 returns
            // immediately, and size > 0xFFFFF logs "addon info too big" and returns. Both mean "no
            // usable addon info", never "reject this login". The blob is passed through as-is and
            // ReadAddonsInfo applies its own bound at the point it actually inflates.

            // The flag bit + the 11-bit length span two bytes; require them explicitly rather than
            // via a throw.
            if (in.size() - in.rpos() < 2)
            {
                return DecodeResult::ShortBody;
            }

            // Spec 3.4: ONE flag bit, THEN an 11-bit length -- both MSB-first. The legacy path
            // issued ReadBits(11) with no leading ReadBit(), which reads the wrong 11 bits: the
            // gate2 capture's `00 D0` (strlen 13, flag 0) decodes to 6 that way and to 13 this way.
            // Do NOT collapse this into a single ReadBits(12): equivalent only while the flag is 0,
            // and it silently rejects a valid packet (length >= 2048) the moment it is ever 1.
            parsed.useIPv6 = in.ReadBit() != 0;
            uint32_t const nameLength = in.ReadBits(11);

            // There is deliberately NO cap on nameLength, and no rejection of 0. The legacy path
            // applied neither -- it did ReadString(ReadBits(11)) and carried on to the build check
            // and the account lookup, so a name we found odd still produced a build mismatch or
            // AUTH_UNKNOWN_ACCOUNT rather than the AUTH_FAILED a decode rejection short-circuits to,
            // and it produced it in a different order. A cap would bound nothing either:
            // ReadString() self-bounds on the buffer ("while (rpos() < size() && rpos() < start +
            // count)"), so it cannot over-read whatever the count claims, and ReadString(0) simply
            // returns "" without throwing. The 11-bit field's own maximum is 2047, which is the
            // real worst case with or without a cap.

            // ReadString() silently truncates at end-of-buffer, so reject the short case up front
            // instead of accepting a quietly shortened account name. THIS bound stays: a body
            // claiming more name bytes than it carries is genuinely malformed.
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
