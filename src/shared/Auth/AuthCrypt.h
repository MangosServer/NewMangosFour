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

#ifndef MANGOS_H_AUTHCRYPT
#define MANGOS_H_AUTHCRYPT

#include "Common/Common.h"
#include "ARC4.h"
#include "MopAuthKey.h"

/**
 * @brief Authentication encryption/decryption for World of Warcraft protocol
 *
 * AuthCrypt handles the session key-based encryption and decryption of
 * World of Warcraft client-server packets using a modified version of ARC4.
 * It maintains separate encryption and decryption states for bidirectional
 * communication.
 */
class AuthCrypt
{
    public:
        /**
         * @brief Constructor - initializes the crypt object
         */
        AuthCrypt();
        /**
         * @brief Destructor
         */
        ~AuthCrypt();


        /**
         * @brief Initialize the encryption/decryption state from the canonical session key.
         *
         * @param K The session key as EXACTLY 40 raw bytes. The client hashes 40 raw bytes
         *          [BINARY sub_140A6F050]; a BigNumber-shaped key silently yields 39 on ~1/256 of
         *          logins (leading zero dropped by BN_bn2hex), which fails the crypt
         *          indistinguishably. The array reference makes the length a compile-time fact.
         * @return true when BOTH ARC4 directions were keyed and dropped successfully. This is a
         *         ONE-SHOT transition: only FRESH may prepare. On any operational failure the state
         *         becomes FAILED; a second Prepare while PREPARED poisons that unpublished
         *         preparation rather than allowing stale key material to be activated; Prepare
         *         while ACTIVE is rejected before touching the live streams.
         *         On false the object is INERT unless it was already ACTIVE.
         *         DecryptRecv/EncryptSend no-op while not ACTIVE and
         *         IsInitialized() -- the wire codec discriminator (WorldSocket.cpp:218/862) --
         *         still reports pre-crypt. Callers MUST check it: proceeding on false puts
         *         PLAINTEXT HEADERS on the wire framed as encrypted.
         */
        bool Prepare(const uint8 (&K)[MopAuth::SESSION_KEY_LEN]);

        /**
         * @brief Publish a prepared crypt. `noexcept`, and the ONLY PREPARED -> ACTIVE transition.
         *
         * Split from Prepare() so that every fallible step -- HMAC, ARC4 keying, the drop-1024 --
         * happens BEFORE the caller allocates anything, while activation itself is a single store
         * that cannot fail. That is what lets HandleAuthSession's commit region be genuinely
         * infallible instead of merely short (spec 6.6b).
         *
         * Between Prepare() and Activate() the ARC4 contexts are fully keyed but IsInitialized()
         * still reports FALSE -- so Decrypt/Encrypt no-op and, critically, the wire codec
         * discriminator (WorldSocket.cpp:218/862) still frames PRE-crypt. A throw or early return
         * in that window therefore leaves the socket exactly as unauthenticated as it started.
         *
         * FAIL-CLOSED: activating from any state other than PREPARED is a no-op. Do not "simplify"
         * this to an unconditional boolean store.
         */
        void Activate() noexcept
        {
            if (_state == CRYPT_PREPARED)
            {
                _state = CRYPT_ACTIVE;
            }
        }

        /**
         * @brief Decrypt the received header in place.
         *
         * @return false if the crypt is uninitialized or OpenSSL failed. The two false cases differ
         *         and callers must assume the WEAKER one:
         *           - uninitialized -> EVP was never called, so `data` is genuinely untouched;
         *           - OpenSSL failure -> `data` is UNDEFINED (EVP works in place and may have
         *             partially written before failing).
         *         So: on false, DISCARD the frame. Never parse it, and never assume it is the
         *         original ciphertext.
         */
        bool DecryptRecv(uint8* data, size_t len);

        /**
         * @brief Encrypt the outgoing header in place.
         *
         * @return false if the crypt is uninitialized or OpenSSL failed. On false `data` is either
         *         untouched (uninitialized) or UNDEFINED (mid-EVP failure) -- in both cases it is
         *         NOT validly encrypted. The caller must DROP the packet: sending it puts plaintext
         *         or garbage on the wire under a post-crypt frame.
         */
        bool EncryptSend(uint8* data, size_t len);

        /**
         * @brief Check if the crypt object is active (published).
         * @return True if activated, false otherwise
         */
        bool IsInitialized() const { return _state == CRYPT_ACTIVE; }

        /**
         * @brief Whether Prepare() has a chance of succeeding -- both ciphers selected and sized.
         *
         * NOT a pointer check (see ARC4::IsSelected). Pure and stable: the ARC4 members select
         * their cipher in this object's constructor, so the answer is fixed for the socket's
         * lifetime. That is what lets HandleAuthSession check crypt readiness BEFORE it allocates
         * a session and still rely on it at the commit region.
         *
         * It is NOT a substitute for checking Prepare()'s return value: the HMAC, the keying and the
         * drop-1024 can all still fail after this returns true. It exists only so a caller can
         * cheaply refuse before doing expensive work.
         */
        bool IsUsable() const { return _clientDecrypt.IsSelected() && _serverEncrypt.IsSelected(); }

    private:
        enum CryptState
        {
            CRYPT_FRESH,
            CRYPT_FAILED,
            CRYPT_PREPARED,
            CRYPT_ACTIVE
        };

        ARC4 _clientDecrypt;
        ARC4 _serverEncrypt;
        // Deliberately not two booleans: two booleans admit impossible combinations and cannot
        // distinguish a fresh object from one whose contexts were partially touched before failure.
        CryptState _state;
};
#endif
