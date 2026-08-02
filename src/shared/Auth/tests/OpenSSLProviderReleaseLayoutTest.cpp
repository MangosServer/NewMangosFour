/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#include "Auth/OpenSSLProvider.h"

#include <openssl/evp.h>

#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

int main()
{
#ifdef _WIN32
    SetEnvironmentVariableW(L"OPENSSL_MODULES", nullptr);
#endif

    OpenSSLProviderManager providerManager;
    if (!providerManager.IsInitialized())
    {
        return 1;
    }

    const char* modules = std::getenv("OPENSSL_MODULES");
    if (modules && modules[0] != '\0')
    {
        return 2;
    }

    return EVP_rc4() ? 0 : 3;
}
