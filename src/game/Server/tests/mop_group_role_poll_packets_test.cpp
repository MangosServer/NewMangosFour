/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Byte-exact fixtures for the 5.4.8.18414 role-check prompt,
 * SMSG_GROUP_ROLE_POLL_INFORM 0x1007.
 *
 * These are REAL CAPTURED BODIES fed back through the builder. They matter more
 * than usual here: an earlier reading of this packet took its second byte for a
 * length field, because it equals popcount(mask) in every captured sample. It
 * is GUID[7]. The coincidence holds only because every observed mask is 0x7C or
 * 0x7D, and these fixtures pin the real interpretation.
 */

#include "Group.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void CheckBytes(char const* what, uint64 guid, uint8 partyIndex,
    std::initializer_list<uint8> expected)
{
    MopGroupPromotePackets::RolePollInform inform;
    inform.initiatorGuid = ObjectGuid(guid);
    inform.partyIndex = partyIndex;

    WorldPacket built;
    CHECK(MopGroupPromotePackets::BuildRolePollInform(built, inform));
    CHECK(built.GetOpcode() == SMSG_GROUP_ROLE_POLL_INFORM);

    std::vector<uint8> const want(expected.begin(), expected.end());
    if (built.size() != want.size())
    {
        std::fprintf(stderr, "  %s: built %u bytes, expected %u\n", what,
                     uint32(built.size()), uint32(want.size()));
        ++g_fail;
        return;
    }

    for (size_t i = 0; i < want.size(); ++i)
    {
        if (built.contents()[i] != want[i])
        {
            std::fprintf(stderr, "  %s: byte %u is 0x%02X, expected 0x%02X\n",
                         what, uint32(i), built.contents()[i], want[i]);
            ++g_fail;
            return;
        }
    }
}

int main()
{
    // capture-000112 seq 2869142, 7 bytes, party index 1.
    CheckBytes("c112 index 1", 0x04000000054D9DE7ULL, 1,
        { 0x7C, 0x05, 0x01, 0xE6, 0x9C, 0x4C, 0x04 });

    // capture-000377 seq 23676, 8 bytes, party index 0, GUID[6] present so the
    // tail order is exercised differently from the first case.
    CheckBytes("c377 index 0", 0x078000000577F763ULL, 0,
        { 0x7D, 0x06, 0x00, 0x81, 0x62, 0xF6, 0x76, 0x04 });

    // capture-000628 seq 816648, a different initiator with the same mask.
    CheckBytes("c628 index 0", 0x07800000046CF868ULL, 0,
        { 0x7D, 0x06, 0x00, 0x81, 0x69, 0xF9, 0x6D, 0x05 });

    // Exact size is 2 + popcount(GUID): two bytes present here, so four total.
    {
        MopGroupPromotePackets::RolePollInform inform;
        inform.initiatorGuid = ObjectGuid(uint64(0x0000000000001234ULL));
        WorldPacket built;
        CHECK(MopGroupPromotePackets::BuildRolePollInform(built, inform));
        CHECK(built.size() == 2 + 2);
    }

    if (g_fail)
    {
        std::fprintf(stderr, "mop_group_role_poll_packets: %d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_group_role_poll_packets: all checks passed\n");
    return 0;
}
