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

#ifndef MANGOS_H_MOPAUTHSESSION
#define MANGOS_H_MOPAUTHSESSION

#include "WorldHandlers/AccountMgr.h"                // MAX_ACCOUNT_STR

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class ByteBuffer;

/**
 * @brief Bounded decoder for the legacy CMSG_AUTH_SESSION body.
 *
 * This is a behaviour-preserving extraction of the inline reads that used to live in
 * WorldSocket::HandleAuthSession. The read/skip order and the digest[] scatter are
 * reproduced bug-for-bug on purpose: notably ReadBits(11) is issued with NO preceding
 * ReadBit(), which is wrong for a real 5.4.8 client but is what the legacy path did.
 * Correcting the wire semantics is deliberately out of scope here.
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
     * Upper bound on a decoded account name, in BYTES.
     *
     * Sized so that every username AccountMgr can create fits, and nothing narrower. MAX_ACCOUNT_STR
     * is a CHARACTER limit, not a byte limit: AccountMgr applies it as
     * utf8length(username) > MAX_ACCOUNT_STR (AccountMgr.cpp:101, :139), and utf8length is
     * utf8::distance (Util.cpp:544-547), which counts code points. A 32-character Cyrillic username
     * is 64 bytes and is perfectly creatable, so equating a BYTE cap with MAX_ACCOUNT_STR would
     * reject it -- the legacy inline parser capped nothing at all, so that would lock such accounts
     * out exactly as an earlier 16-byte cap locked out 17..32 character ASCII ones. UTF-8 encodes a
     * code point in at most 4 bytes, hence * 4: at 128 bytes there is no username AccountMgr can
     * create that this rejects.
     *
     * It exists ONLY to bound the allocation, never to validate the name. It buys that bound
     * cheaply: the 11-bit length field permits 2047, and 128 still rejects that. Nothing else here
     * depends on it -- length against the bytes actually present is the separate TruncatedName
     * check, and MopFrameReader's payloadSize is a uint16, so the worst case even with no cap at
     * all would be 2047 bytes. A tighter cap would therefore buy no safety and only narrow.
     */
    static constexpr size_t MaxAccountNameBytes = MAX_ACCOUNT_STR * 4;

    /**
     * @brief Outcome of DecodeAuthSession. Anything other than Ok means 'out' is untouched.
     */
    enum class DecodeResult : uint8_t
    {
        Ok = 0,
        ShortBody,
        BadAddonSize,
        BadNameLength,
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
