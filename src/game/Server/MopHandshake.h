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

#ifndef MANGOS_MOP_HANDSHAKE_H
#define MANGOS_MOP_HANDSHAKE_H
#include <cstdint>
#include <cstddef>
#include <vector>
namespace MopHs
{
    enum ConnectionState { CONN_GREETING, CONN_CHALLENGED, CONN_AUTHENTICATING, CONN_AUTHED };
    /// Production maps a raw opcode -> a class. Handshake opcodes only in their state; EVERYTHING ELSE
    /// (ping/keepalive/disconnect/normal) is AUTHED-only, preventing pre-auth socket pinning. If a gate-2b
    /// capture proves the client sends ping/keepalive pre-auth, add a class + relax (documented).
    enum OpcodeClass { OPC_GREETING, OPC_AUTH_SESSION, OPC_NORMAL };
    inline bool IsHandshakeOpcodeLegal(ConnectionState st, OpcodeClass cl)
    {
        switch (cl)
        {
            case OPC_GREETING:     return st == CONN_GREETING;
            case OPC_AUTH_SESSION: return st == CONN_CHALLENGED;
            case OPC_NORMAL:       return st == CONN_AUTHED;      // Phase 2 never AUTHED -> all rejected pre-auth
        }
        return false;
    }
    inline bool RateLimitElapsed(int64_t lastSec, int64_t nowSec) { return (nowSec - lastSec) >= 1; }
    typedef bool (*RandomBytesFn)(uint8_t* out, size_t len);
    inline bool BuildAuthChallengePayload(RandomBytesFn rng, std::vector<uint8_t>& out, uint32_t& seed)
    {
        uint8_t field[32], s[4];
        if (!rng(field, sizeof(field)) || !rng(s, sizeof(s))) { out.clear(); return false; }
        seed = uint32_t(s[0]) | (uint32_t(s[1])<<8) | (uint32_t(s[2])<<16) | (uint32_t(s[3])<<24);
        out.clear(); out.reserve(39);
        out.push_back(0); out.push_back(0);
        out.insert(out.end(), field, field + 32);
        out.push_back(1);
        out.insert(out.end(), s, s + 4);                      // LE bytes == input bytes (identity)
        return true;
    }
}
#endif
