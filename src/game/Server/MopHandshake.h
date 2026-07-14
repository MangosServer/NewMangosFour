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
    // bool BuildAuthChallengePayload(RandomBytesFn, std::vector<uint8_t>& out, uint32_t& seed);  // Task 2.4
}
#endif
