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

#ifndef MANGOS_H_MOPAUTHPROOF
#define MANGOS_H_MOPAUTHPROOF

#include "Common/Common.h"
#include "Auth/MopAuthKey.h"
#include <string>

namespace MopAuth
{
    /** Length of the SHA-1 auth proof, in bytes. */
    inline constexpr size_t AUTH_PROOF_LEN = 20;

    /**
     * @brief Computes the CMSG_AUTH_SESSION proof digest.
     *
     * CONFIRMED FROM THE CLIENT BINARY (sub_140A6F050 / sub_A5A684), spec 3.2: a SINGLE SHA1 over
     * exactly five chunks, in order --
     *   1. account name  strlen(), ASCII, NOT NUL-terminated
     *   2. zero dword    4
     *   3. clientSeed    4, raw little-endian
     *   4. serverSeed    4, raw little-endian
     *   5. K             40, RAW bytes
     * Total hashed = strlen(account) + 52.
     *
     * There is NO de-interleave step: the client writes 20 contiguous bytes from one SHA1_Final.
     * The scatter that spreads those 20 bytes across non-sequential body offsets is the client
     * SERIALIZER's doing and belongs to the decoder (MopAuthSession.cpp), not here.
     *
     * K is taken as an array reference so the length is a compile-time fact: the client hashes 40
     * and nothing else. Do NOT route this through Sha1Hash::UpdateBigNumbers -- it hashes
     * GetNumBytes(), which is 39 on ~1/256 of logins, AND Sha1Hash is linked into realmd
     * (AuthSocket.cpp:253/269/635), so it must not be changed to suit us.
     */
    void ComputeAuthProof(const std::string& account, uint32 clientSeed, uint32 serverSeed,
                          const uint8 (&K)[SESSION_KEY_LEN], uint8 out[AUTH_PROOF_LEN]);

    /**
     * @brief Constant-time comparison of two 20-byte proofs.
     *
     * plain memcmp() short-circuits on the first differing byte, leaking how many leading bytes an
     * attacker guessed right. OpenSSL is already a hard dependency and provides CRYPTO_memcmp; no
     * constant-time helper existed in this tree.
     *
     * @return true when the proofs are equal.
     */
    bool ProofEquals(const uint8* a, const uint8* b);
}

#endif
