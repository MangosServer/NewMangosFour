/**
 * Byte-exact coverage for the CMSG_LFG_SET_ROLES (0x08A2) reader.
 *
 * The bodies below are REAL captured client bytes at build 18414, not inverses of our
 * own writer. Provenance is recorded per case so the fixture can be re-derived.
 *
 * Corpus catalogueGenerationId
 *   2BE10C899585BAECD237705AC13BBF9262D81B6BDC085B462808C6869CE88752
 *
 * Layout derived from the client's body writer sub_6688D0, reached as vtable slot 1
 * behind the opcode thunk sub_6615FE (which writes 2210):
 *
 *     sub_40F075(pkt, *(uint32*)(this + 16));   // WriteUInt32 -- role mask
 *     sub_40F018(pkt, *(uint8 *)(this + 20));   // WriteUInt8  -- role check counter
 *
 * Flat, so there is no bit packing and no GUID obfuscation to undo.
 */

#include "Group.h"
#include "WorldPacket.h"

#include <cassert>
#include <cstdio>
#include <vector>

namespace
{
    WorldPacket MakeBody(std::vector<uint8> const& bytes)
    {
        WorldPacket packet(CMSG_LFG_SET_ROLES, bytes.size());
        packet.append(bytes.data(), bytes.size());
        return packet;
    }

    /// capture-000086 seq 16621, 5 bytes: 08 00 00 00 00
    /// A plain damage-only selection -- the single-role case.
    void test_single_role_body()
    {
        std::vector<uint8> const body = { 0x08, 0x00, 0x00, 0x00, 0x00 };

        WorldPacket packet = MakeBody(body);
        MopLfgSetRolesPackets::Request request;
        assert(MopLfgSetRolesPackets::ParseRequest(packet, request));

        assert(request.roles == 0x08);              // PLAYER_ROLE_DAMAGE
        assert(request.roleCheckCounter == 0);
        assert(packet.rpos() == packet.size());     // no tail left unread
    }

    /// capture-000112 seq 90341, 5 bytes: 0A 00 00 00 00
    ///
    /// The case that matters: 0x0A is TANK|DAMAGE, one player offering either role. This
    /// is direct wire proof that the mask is a bitmask and not an enum -- the reader must
    /// not try to match it against a single role value.
    void test_hybrid_role_body()
    {
        std::vector<uint8> const body = { 0x0A, 0x00, 0x00, 0x00, 0x00 };

        WorldPacket packet = MakeBody(body);
        MopLfgSetRolesPackets::Request request;
        assert(MopLfgSetRolesPackets::ParseRequest(packet, request));

        assert(request.roles == 0x0A);
        assert((request.roles & PLAYER_ROLE_TANK) != 0);
        assert((request.roles & PLAYER_ROLE_DAMAGE) != 0);
        assert((request.roles & PLAYER_ROLE_HEALER) == 0);
        assert(request.roleCheckCounter == 0);
        assert(packet.rpos() == packet.size());
    }

    /// A body one byte short must be refused outright rather than read past its end.
    void test_short_body_is_refused()
    {
        std::vector<uint8> const body = { 0x08, 0x00, 0x00, 0x00 };

        WorldPacket packet = MakeBody(body);
        MopLfgSetRolesPackets::Request request;
        assert(!MopLfgSetRolesPackets::ParseRequest(packet, request));
    }
}

int main()
{
    test_single_role_body();
    test_hybrid_role_body();
    test_short_body_is_refused();

    std::printf("mop_lfg_set_roles_packets_test: OK\n");
    return 0;
}
