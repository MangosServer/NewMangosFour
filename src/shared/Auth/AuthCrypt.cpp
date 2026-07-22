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

#include "AuthCrypt.h"
#include "HMACSHA1.h"
#include "Log/Log.h"

/**
 * Initializes the authentication crypt state in an uninitialized state.
 */
AuthCrypt::AuthCrypt() : _clientDecrypt(SHA_DIGEST_LENGTH), _serverEncrypt(SHA_DIGEST_LENGTH)
{
    _state = CRYPT_FRESH;
}

AuthCrypt::~AuthCrypt()
{
}

// World-crypt HMAC seeds for 5.4.8.18414. BINARY-CONFIRMED IN BOTH CLIENT BINARIES (spec 3.1) --
// not fork-derived. Construction confirmed at sub_140A7F0B0 / sub_A6331B; the object is the world
// PacketPipe. Direction was DERIVED from the client's send/recv paths, not assumed.
//
// DO NOT substitute the Battle.net seeds (68E0C72E.../DEA965AE... @ 0x14100DD10): different
// subsystem (BSN::Hard::Decoder, SHA256).
//
// These REPLACED the 4.3.4-era seeds (CC98AE04... / C2B3723C...) that shipped here through Stage 1.
// Those are wrong for MoP and fail the crypt indistinguishably from a wrong K.

/// Server-encrypt / client-decrypt. Wow-64 VA 0x140F4CCA0 / Wow.exe VA 0xDC6FD0.
static const uint8 SeedServerEncrypt[SEED_KEY_SIZE] =
{
    0x08, 0xF1, 0x95, 0x9F, 0x47, 0xE5, 0xD2, 0xDB, 0xA1, 0x3D, 0x77, 0x8F, 0x3F, 0x3E, 0xE7, 0x00
};

/// Server-decrypt / client-encrypt. Wow-64 VA 0x140F4CCB0 / Wow.exe VA 0xDC6FE0.
static const uint8 SeedServerDecrypt[SEED_KEY_SIZE] =
{
    0x40, 0xAA, 0xD3, 0x92, 0x26, 0x71, 0x43, 0x47, 0x3A, 0x31, 0x08, 0xA6, 0xE7, 0xDC, 0x98, 0x2A
};

bool AuthCrypt::Prepare(const uint8 (&K)[MopAuth::SESSION_KEY_LEN])
{
    // ONE-SHOT STATE MACHINE. Reject repeats before touching either live stream.
    //
    // A repeated call while PREPARED poisons that unpublished preparation: a caller that ignored
    // the false return must not be able to Activate() stale key material from the earlier success.
    // A call while ACTIVE is rejected without changing state or touching either ARC4 context, so it
    // cannot corrupt an already-live stream.
    if (_state != CRYPT_FRESH)
    {
        sLog.outError("AuthCrypt::Prepare: invalid state transition from %u.", uint32(_state));
        if (_state == CRYPT_PREPARED)
        {
            _state = CRYPT_FAILED;
        }
        return false;
    }

    // Pessimistically enter FAILED before the first fallible operation. Every early return below
    // therefore leaves an explicit terminal failure state, never a fresh-looking or activatable
    // half-keyed object.
    _state = CRYPT_FAILED;

    if (!IsUsable())
    {
        sLog.outError("AuthCrypt::Prepare: RC4 unavailable; crypt NOT prepared.");
        return false;
    }

    // Per direction: HMAC-SHA1(key = 16-byte seed, message = K [40 raw]) -> 20-byte digest
    //             -> ARC4 KSA -> drop-1024. Shape binary-confirmed; drop-1024 binary-confirmed.
    // The raw-40 K is passed straight through: HMACSHA1::ComputeHash(BigNumber*) would hash
    // GetNumBytes(), not 40, reintroducing the short-K bug this migration removes.
    HMACSHA1 serverEncryptHmac(SEED_KEY_SIZE, SeedServerEncrypt);
    serverEncryptHmac.UpdateData(K, int(MopAuth::SESSION_KEY_LEN));
    serverEncryptHmac.Finalize();

    HMACSHA1 clientDecryptHmac(SEED_KEY_SIZE, SeedServerDecrypt);
    clientDecryptHmac.UpdateData(K, int(MopAuth::SESSION_KEY_LEN));
    clientDecryptHmac.Finalize();

    if (!serverEncryptHmac.IsValid() || !clientDecryptHmac.IsValid())
    {
        sLog.outError("AuthCrypt::Prepare: HMAC-SHA1 failed; crypt NOT prepared.");
        return false;
    }

    uint8* encryptHash = serverEncryptHmac.GetDigest();
    uint8* decryptHash = clientDecryptHmac.GetDigest();

    // EVERY step below is checked. This is the whole point of the task: ARC4 used to swallow every
    // EVP failure, so "Init ran" and "Init worked" were indistinguishable -- and IsInitialized() is
    // what selects the post-crypt header, so the difference is plaintext on the wire.
    if (!_clientDecrypt.Init(decryptHash) || !_serverEncrypt.Init(encryptHash))
    {
        sLog.outError("AuthCrypt::Prepare: ARC4 keying failed; crypt NOT prepared.");
        return false;
    }

    // drop-1024 (binary-confirmed). Checked too: a failed drop leaves the keystream at the wrong
    // position, which desynchronises every subsequent header exactly like a wrong key -- and
    // silently.
    uint8 syncBuf[1024];

    memset(syncBuf, 0, 1024);
    if (!_serverEncrypt.UpdateData(1024, syncBuf))
    {
        sLog.outError("AuthCrypt::Prepare: server-encrypt drop-1024 failed; crypt NOT prepared.");
        return false;
    }

    memset(syncBuf, 0, 1024);
    if (!_clientDecrypt.UpdateData(1024, syncBuf))
    {
        sLog.outError("AuthCrypt::Prepare: client-decrypt drop-1024 failed; crypt NOT prepared.");
        return false;
    }

    // MUST REMAIN THE LAST STATE CHANGE, and it is only reached when the ENTIRE chain succeeded:
    // RC4 selected -> HMAC digests valid -> both directions keyed -> drop-1024 applied to both.
    //
    // NOTE THIS SETS PREPARED, NOT ACTIVE. The crypt is now fully keyed but still INERT:
    // IsInitialized() is false, so Decrypt/Encrypt no-op and the wire codec still frames pre-crypt.
    // Only Activate() publishes it. That separation is what makes the caller's commit region
    // infallible -- see Activate()'s doc and Task 8.
    //
    // Hoisting this, or adding an early return BELOW it, silently turns a failure into an
    // activatable half-keyed crypt. The state-transition tests pin fresh, prepared, failed and
    // active behaviour -- unlike Stage 1, where the equivalent rule had no test.
    _state = CRYPT_PREPARED;
    return true;
}

/**
 * Decrypts the fixed-size encrypted receive header in place.
 */
bool AuthCrypt::DecryptRecv(uint8* data, size_t len)
{
    if (_state != CRYPT_ACTIVE)
    {
        return false;                            // pre-crypt: nothing to do, and nothing was done
    }

    return _clientDecrypt.UpdateData(int(len), data);
}

/**
 * Encrypts the fixed-size outgoing packet header in place.
 */
bool AuthCrypt::EncryptSend(uint8* data, size_t len)
{
    if (_state != CRYPT_ACTIVE)
    {
        return false;
    }

    return _serverEncrypt.UpdateData(int(len), data);
}
