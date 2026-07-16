/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
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

/**
 * @file ARC4.cpp
 * @brief Implementation of ARC4 encryption algorithm using OpenSSL
 *
 * This file implements the ARC4 (Alleged RC4) stream cipher for use
 * in the MaNGOS authentication and session encryption system. ARC4
 * is used to encrypt/decrypt game traffic between the server and clients.
 *
 * The implementation uses OpenSSL's EVP interface for the cipher operations
 * and includes proper provider management for OpenSSL 3.x compatibility.
 */

#include "ARC4.h"
#include "OpenSSLProvider.h"
#include "Log/Log.h"
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
#include <openssl/provider.h>
#endif

/**
 * @brief Construct ARC4 cipher with specified key length
 * @param len Key length in bytes
 *
 * Creates an ARC4 cipher context with the specified key length.
 * The key itself is not set in this constructor - use Init() to set it.
 *
 * @note On OpenSSL 3.x, this automatically initializes the legacy provider
 * required for ARC4 support.
 */
ARC4::ARC4(uint8 len) : m_cipherContext(), m_ready(false), m_keyed(false)
{
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
    // Provider management is now handled by OpenSSLProviderManager
    if (!m_providerManager.IsInitialized())
    {
        sLog.outError("ARC4: Failed to initialize OpenSSL providers");
        return;                                  // m_ready stays false -- the context is NOT usable
    }
#endif

    if (!m_cipherContext.IsValid())
    {
        sLog.outError("ARC4: Failed to create cipher context");
        return;
    }

    // EVP_rc4() returns NULL when RC4 is unavailable. On OpenSSL 3 RC4 is a LEGACY algorithm, so
    // this is a real deployment failure, not a theoretical one -- and the old code ignored it.
    const EVP_CIPHER* cipher = EVP_rc4();
    if (!cipher)
    {
        sLog.outError("ARC4: RC4 cipher unavailable (OpenSSL legacy provider not loaded?)");
        return;
    }

    if (EVP_EncryptInit_ex(m_cipherContext.Get(), cipher, NULL, NULL, NULL) != 1)
    {
        sLog.outError("ARC4: EVP_EncryptInit_ex(RC4) failed");
        return;
    }

    if (EVP_CIPHER_CTX_set_key_length(m_cipherContext.Get(), len) != 1)
    {
        sLog.outError("ARC4: EVP_CIPHER_CTX_set_key_length(%u) failed", uint32(len));
        return;
    }

    m_ready = true;                              // selected + sized; still needs a key via Init()
}

/**
 * @brief Destructor for ARC4 cipher
 *
 * All cleanup is handled automatically by the RAII wrappers
 * (OpenSSLProviderManager and OpenSSLCipherContext).
 */
ARC4::~ARC4()
{
    // Cleanup is now handled automatically by RAII wrappers
}

/**
 * @brief Initialize or re-initialize the cipher with a new key
 * @param seed Pointer to the key bytes
 *
 * Sets or changes the encryption key for the ARC4 cipher.
 * This can be called multiple times to re-key the cipher.
 *
 * @note The key length must match the length specified in the constructor.
 */
bool ARC4::Init(uint8* seed)
{
    if (!m_ready)
    {
        sLog.outError("ARC4::Init: cipher was never selected; refusing to key.");
        return false;
    }

    if (EVP_EncryptInit_ex(m_cipherContext.Get(), NULL, NULL, seed, NULL) != 1)
    {
        // Fail CLOSED: after a rejected re-key the cipher's internal state is unknown, so it must
        // not be treated as usable just because it was usable a moment ago.
        sLog.outError("ARC4::Init: EVP_EncryptInit_ex(key) failed");
        m_keyed = false;
        return false;
    }

    m_keyed = true;
    return true;
}

/**
 * @brief Encrypt or decrypt data in-place
 * @param len Length of data to process in bytes
 * @param data Pointer to the data buffer (modified in-place)
 *
 * Processes data using the ARC4 stream cipher. Since ARC4 is a symmetric
 * stream cipher, the same operation is used for both encryption and
 * decryption. The data is modified in-place for efficiency.
 *
 * @warning The cipher must be initialized with a key before calling this.
 * @note The output length will always equal the input length for ARC4.
 */
bool ARC4::UpdateData(int len, uint8* data)
{
    if (!IsReady())
    {
        sLog.outError("ARC4::UpdateData: cipher not ready; refusing to process %d bytes.", len);
        return false;
    }

    // NO EVP_EncryptFinal_ex. The old code called it after EVERY chunk, which is wrong twice over:
    //   1. OpenSSL's contract: after EVP_EncryptFinal_ex "the encryption operation is finished and
    //      no further calls to EVP_EncryptUpdate() should be made". This context is used for the
    //      1024-byte drop and then for EVERY packet header for the socket's whole life, so
    //      finalizing between chunks ends the very stream we depend on continuing.
    //   2. RC4 is a STREAM cipher: there is no block to flush and no padding to write, so Final has
    //      nothing to do. It "worked" only because EVP short-circuits Final for block_size == 1 --
    //      an implementation detail, not a guarantee, and one the OpenSSL 3 provider path is under
    //      no obligation to preserve.
    // Removing it changes no bytes on the wire (Final produced none) and removes a documented-
    // invalid call. NOTE this also removes a hazard the return-value checks would otherwise have
    // INTRODUCED: had a provider ever returned 0 from that Final, the old code ignored it while a
    // checked version would have failed the whole auth.
    int outlen = 0;
    if (EVP_EncryptUpdate(m_cipherContext.Get(), data, &outlen, data, len) != 1)
    {
        // Fail CLOSED: after a failed operation the keystream position is unknown, so every later
        // header would be garbage even if the next call "succeeds". Refuse to be used again.
        sLog.outError("ARC4::UpdateData: EVP_EncryptUpdate failed on %d bytes; cipher invalidated.", len);
        m_keyed = false;
        return false;
    }

    // A stream cipher must consume and produce exactly len. Anything else means the keystream and
    // the data have desynchronised, which on the wire is indistinguishable from a wrong key.
    if (outlen != len)
    {
        sLog.outError("ARC4::UpdateData: produced %d bytes for %d in; cipher invalidated.", outlen, len);
        m_keyed = false;
        return false;
    }

    return true;
}
