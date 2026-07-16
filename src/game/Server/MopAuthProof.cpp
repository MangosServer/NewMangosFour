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

#include "MopAuthProof.h"
#include "Auth/Sha1.h"
#include <openssl/crypto.h>
#include <cstring>

namespace MopAuth
{
    void ComputeAuthProof(const std::string& account, uint32 clientSeed, uint32 serverSeed,
                          const uint8 (&K)[SESSION_KEY_LEN], uint8 out[AUTH_PROOF_LEN])
    {
        const uint32 zero = 0;

        Sha1Hash sha;
        sha.UpdateData(account);                                   // 1: strlen, no NUL
        sha.UpdateData((const uint8*)&zero, 4);                    // 2: zero dword
        sha.UpdateData((const uint8*)&clientSeed, 4);              // 3: clientSeed, raw LE
        sha.UpdateData((const uint8*)&serverSeed, 4);              // 4: serverSeed, raw LE
        sha.UpdateData(K, int(SESSION_KEY_LEN));                   // 5: K, 40 RAW -- never 39
        sha.Finalize();

        memcpy(out, sha.GetDigest(), AUTH_PROOF_LEN);
    }

    bool ProofEquals(const uint8* a, const uint8* b)
    {
        return CRYPTO_memcmp(a, b, AUTH_PROOF_LEN) == 0;
    }
}
