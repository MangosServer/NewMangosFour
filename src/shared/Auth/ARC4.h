/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
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

#ifndef _AUTH_SARC4_H
#define _AUTH_SARC4_H

#include <openssl/evp.h>
#include "Common/Common.h"
#include "OpenSSLProvider.h"

/**
 * @brief ARC4 encryption/decryption cipher implementation
 *
 * ARC4 is a stream cipher that uses a key to generate a keystream.
 * This class provides ARC4 encryption and decryption functionality using OpenSSL.
 */
class ARC4
{
    public:
        /**
         * @brief Constructor with key length specification
         * @param len Length of the key in bytes
         */
        ARC4(uint8 len);
        /**
         * @brief Destructor
         */
        ~ARC4();

        /**
         * @brief Install or replace the cipher key.
         * @return false if the cipher is not ready or OpenSSL rejected the key. On false the
         *         object is left UNUSABLE (m_keyed cleared) rather than half-keyed.
         */
        bool Init(uint8* seed);

        /**
         * @brief Encrypt/decrypt in place. Advances the ONE continuous RC4 keystream.
         *
         * Does NOT finalize: RC4 is a stream cipher and this context lives for the whole socket, so
         * there is nothing to flush between headers and finalizing would end the stream.
         *
         * @return false if the cipher is not ready, OpenSSL failed, or the output length did not
         *         match the input. On false `data`'s contents are UNDEFINED -- EVP works in place
         *         and may have partially written before failing -- so the caller must DISCARD the
         *         frame or packet. It must never be sent or parsed, and it must not be assumed
         *         unchanged.
         */
        bool UpdateData(int len, uint8* data);

        /**
         * @brief Whether this cipher is selected, key-length-accepted and keyed -- i.e. UpdateData
         *        can be expected to work.
         *
         * NOT a pointer check. m_cipherContext can be non-null while the cipher was never given
         * EVP_rc4() (the ctor returns early if the OpenSSL 3 legacy provider is unavailable, and
         * RC4 lives in that provider). Asking the pointer would answer "fine" on a box that cannot
         * do RC4 at all.
         */
        bool IsReady() const { return m_ready && m_keyed; }

        /// Cipher selected and key length accepted; a key may not be installed yet.
        bool IsSelected() const { return m_ready; }
    private:
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
        OpenSSLProviderManager m_providerManager;  /**< RAII provider management */
#endif
        OpenSSLCipherContext m_cipherContext;        /**< RAII cipher context */
        bool m_ready;                                /**< cipher selected + key length accepted */
        bool m_keyed;                                /**< a key has been installed */
};

#endif
