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

#ifndef MANGOS_H_MOPAUTHKEY
#define MANGOS_H_MOPAUTHKEY

#include "Common/Common.h"
#include <cstring>

namespace MopAuth
{
    /// The client hashes EXACTLY 40 raw bytes [BINARY sub_140A6F050]. Anything else fails the proof
    /// compare indistinguishably, as a generic AUTH_FAILED.
    inline constexpr size_t SESSION_KEY_LEN = 40;

    /**
     * @brief Little-endian K bytes -> canonical raw-40 K, padding the TAIL.
     *
     * The input is ALREADY in K's own byte order; this only pads it out to 40. It does NOT reverse.
     *
     * WHY THE PAD IS AT THE TAIL, AND WHY BigNumber::AsByteArray(40) CANNOT BE USED:
     *   AsByteArray(40) does not zero-pad, it BYTE-ROTATES (BigNumber.cpp:179-197). It memsets 40,
     *   writes GetNumBytes() bytes at offset 0, then std::reverse()s the WHOLE 40 -- so the pad
     *   lands at index 0 and every real byte shifts by one. realmd stores K via AsHexStr()
     *   (= BN_bn2hex), which DROPS LEADING ZEROS, so a K whose most-significant raw byte is 0x00
     *   comes back SHORT. K is regenerated per authentication (realmd AuthSocket.cpp:668), so that
     *   is ~1/256 of LOGINS, not of accounts: auth succeeds 255 times and fails inexplicably on the
     *   256th, and it will NOT reproduce on a retry. That is what makes it expensive.
     *
     * AND DO NOT "FIX" BigNumber: it is LINKED INTO REALMD (AuthSocket.cpp:488/492/494/645/882);
     * changing its padding would alter realmd's SRP6 wire bytes. The fix is local, and this is it.
     * (Sha1Hash is realmd-linked too -- AuthSocket.cpp:253/269/635 -- so its UpdateBigNumbers is
     * equally off-limits; consumers pass raw bytes instead.)
     *
     * Prefer SessionKeyFromHex(): it owns the whole DB-hex -> K chain so no caller has to know
     * which end is padded or which direction the bytes run. This primitive is exposed only so that
     * rule can be unit-tested on its own.
     *
     * @param le    little-endian K bytes -- e.g. BigNumber::AsByteArray()'s return, which is
     *              ALREADY little-endian (bn2bin writes big-endian, AsByteArray then reverses).
     * @param leLen how many; 1..40. Zero is REJECTED, never zero-filled (spec 6.2a): an empty
     *              sessionkey would otherwise become forty 0x00s -- a well-formed-looking K of
     *              zeros, which is the stale/NULL-K confounder the guard exists to catch.
     * @param out   receives the 40 raw bytes; untouched on failure.
     * @return false if le is null, leLen == 0, or leLen > 40. Never truncates.
     */
    inline bool Raw40FromLittleEndian(const uint8* le, size_t leLen, uint8 out[SESSION_KEY_LEN])
    {
        if (!le || leLen == 0 || leLen > SESSION_KEY_LEN)
        {
            return false;
        }

        memset(out, 0, SESSION_KEY_LEN);
        memcpy(out, le, leLen);                 // pad the TAIL -- never copy to out + (40 - leLen)

        return true;
    }

    /// Structural plausibility of an account.sessionkey hex string: non-empty, hex-only, even
    /// length, <= 80 chars (40 bytes). Deliberately does NOT enumerate lengths -- BN_bn2hex drops
    /// leading zero bytes, so 78/76/74/... are all NORMAL, not corruption. This is a cheap
    /// pre-filter for BN_hex2bn; the binding check is that the CONVERTED value is 1..40 bytes.
    inline bool IsPlausibleSessionKeyHex(const char* hex)
    {
        if (!hex)
        {
            return false;
        }

        size_t len = 0;
        for (const char* p = hex; *p; ++p, ++len)
        {
            const bool isHexDigit = (*p >= '0' && *p <= '9')
                                 || (*p >= 'a' && *p <= 'f')
                                 || (*p >= 'A' && *p <= 'F');
            if (!isHexDigit)
            {
                return false;
            }
        }

        return len != 0 && (len % 2) == 0 && len <= (SESSION_KEY_LEN * 2);
    }

    /**
     * @brief account.sessionkey hex -> canonical raw-40 K. THE ONLY SANCTIONED WAY TO OBTAIN K.
     *
     * Owns the entire chain -- validate, convert, pad, re-check -- so that no caller has to reason
     * about BigNumber's byte order. That encapsulation is the POINT, not a convenience: plan v2
     * composed these steps at the call site, passed the already-little-endian AsByteArray() into a
     * big-endian-taking helper, reversed K twice, and would have failed every proof
     * deterministically. Both ends were individually "correct"; the SEAM between them was wrong,
     * and no unit test of either end could see it. Do not re-open the seam.
     *
     * @param hex the account.sessionkey column. May be NULL or empty -- both are rejected.
     * @param out receives the 40 raw bytes; untouched on failure.
     * @return false if hex is NULL, empty, malformed, or converts outside 1..40 bytes.
     */
    bool SessionKeyFromHex(const char* hex, uint8 out[SESSION_KEY_LEN]);
}

#endif
