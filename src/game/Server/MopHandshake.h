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
