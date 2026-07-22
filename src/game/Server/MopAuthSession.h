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

#ifndef MANGOS_H_MOPAUTHSESSION
#define MANGOS_H_MOPAUTHSESSION

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class ByteBuffer;

/**
 * @brief Bounded decoder for the legacy CMSG_AUTH_SESSION body.
 *
 * This is a behaviour-preserving extraction of the inline reads that used to live in
 * WorldSocket::HandleAuthSession, EXCEPT for the name-length bit read, which Stage 2 corrected.
 *
 * The name-length block reads ONE flag bit then an 11-bit length (spec 3.4), which is what a real
 * 5.4.8 client writes: the gate2 capture's `00 D0` decodes to flag=0, length=13 only under this
 * reading. Stage 1 preserved the legacy ReadBits(11)-with-no-leading-ReadBit bug-for-bug while the
 * handler was dormant; Stage 2 corrected it.
 *
 * The digest[] scatter and the seven read_skips are NOT a legacy quirk and must not be "cleaned
 * up": they reproduce the client serializer's own permutation, binary-confirmed in both x86 and
 * x64 (facts/FACTS_mop548_digest_permutation.md 2; campaign research doc, not in this tree).
 * Body 23 is a constant 1 -- not digest[20] --
 * and body 28 is an 8-byte field the server deliberately does NOT interpret (spec 3.3 trap 2:
 * [OPEN]; the facts doc's 5 still claims it must be echoed -- that claim was RETRACTED).
 *
 * The added value over the inline version is malformed-input safety: every read is
 * bounds-checked up front and the output is written atomically (only on Ok), so a
 * truncated or hostile body can no longer leave a half-populated result behind.
 */
namespace MopAuth
{
    /** Bytes consumed by the fixed legacy prefix, up to and including the addon size field. */
    static constexpr size_t LegacyFixedPrefixBytes = 56;

    /**
     * @brief Outcome of DecodeAuthSession. Anything other than Ok means 'out' is untouched.
     */
    enum class DecodeResult : uint8_t
    {
        Ok = 0,
        ShortBody,
        BadAddonSize,
        TruncatedName
    };

    /**
     * @brief Fields carried by the legacy CMSG_AUTH_SESSION body.
     */
    struct AuthSessionFields
    {
        uint8_t digest[20];
        uint32_t clientSeed;
        uint16_t builtNumberClient; // binding: the wire field is two bytes
        uint32_t addonSize;
        std::vector<uint8_t> addonData;
        std::string account;
        bool useIPv6;               // body 420 flag bit; name inferred (TC), OFFSET confirmed
    };

    /**
     * @brief Decodes the legacy auth session body.
     *
     * @param in  Buffer positioned at the start of the body.
     * @param out Receives the decoded fields, and is written ONLY when the result is Ok.
     * @return DecodeResult::Ok on success, otherwise the specific rejection reason.
     */
    DecodeResult DecodeAuthSession(ByteBuffer& in, AuthSessionFields& out);
}

#endif
