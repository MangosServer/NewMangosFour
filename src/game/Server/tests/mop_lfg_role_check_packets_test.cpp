/**
 * Byte-exact coverage for the SMSG_LFG_ROLE_CHECK_UPDATE (0x12BB) writer.
 *
 * The expected bodies below are REAL captured server bytes at build 18414. The test
 * feeds the writer the values decoded out of each capture and asserts the writer
 * reproduces that capture byte for byte -- so this is not a round-trip of our own
 * assumptions, it is a comparison against traffic a retail server actually sent.
 *
 * Corpus catalogueGenerationId
 *   2BE10C899585BAECD237705AC13BBF9262D81B6BDC085B462808C6869CE88752
 *
 * Layout (see MopLfgPackets::BuildRoleCheckUpdate):
 *
 *   uint8  partyIndex
 *   uint8  state
 *   bits   WriteBits(memberCount, 21)
 *          per member: WriteBit(answered), guid mask [3,0,5,2,7,1,4,6]
 *          rdg[3], rdg[5], WriteBits(dungeonCount, 22), rdg[0,7,6,1,4,2],
 *          WriteBit(state == LFG_ROLECHECK_INITIALITING)
 *          FlushBits
 *   bytes  ByteSeq rdg[0]
 *          per member: uint8 level, seq[3], seq[6], uint32 roles, seq[2,4,0,1,5,7]
 *          ByteSeq rdg[1,7,6,4,3,2,5]
 *          dungeonCount x uint32 dungeon entry
 *
 * The two cases differ in size, member count, party index and dungeon type, which is
 * what makes the agreement meaningful -- a layout that only fits one shape proves
 * nothing.
 */

#include "LFGMgr.h"
#include "WorldPacket.h"

#include "Database/DatabaseEnv.h"
#include <cstdio>
#include <vector>

// Linker stubs. The server defines these in mangosd; a test binary that reaches any
// game.lib translation unit needs them. This test did not need them before only
// because its checks lived inside assert(), which is not compiled under NDEBUG --
// so it never actually referenced the code under test.
DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace
{
    void AssertBytes(WorldPacket const& packet, std::vector<uint8> const& expected,
                     char const* label)
    {
        if (packet.size() != expected.size())
        {
            std::printf("%s: size %u, expected %u\n", label,
                        unsigned(packet.size()), unsigned(expected.size()));
            CHECK(false);

            // Stop here. CHECK records the failure but does not abort, and the loop
            // below indexes packet.contents() by expected.size() -- past the end of a
            // packet that is short. The vector's spare capacity usually absorbs that,
            // which is worse than a crash: it prints a wall of byte mismatches for
            // bytes that do not exist, so a size bug reads as a content bug at exactly
            // the moment someone is trying to diagnose it.
            return;
        }

        for (size_t i = 0; i < expected.size(); ++i)
        {
            if (packet.contents()[i] != expected[i])
            {
                std::printf("%s: byte %u is 0x%02X, expected 0x%02X\n", label,
                            unsigned(i), packet.contents()[i], expected[i]);
                CHECK(false);
            }
        }
    }

    /// capture-000075 seq 891708, 35 bytes.
    ///
    /// A two-man role check. Member 0 is the leader and has answered with 0x0A --
    /// TANK|DAMAGE, a hybrid -- while member 1 has not answered at all (roles 0,
    /// answered bit clear). Both are level 90. One dungeon, entry 0x010002CD.
    void test_two_member_role_check()
    {
        std::vector<uint8> const expected = {
            0x00, 0x02, 0x00, 0x00, 0x17, 0x71, 0xB8, 0x00, 0x00, 0x02, 0x04,
            0x5A, 0x04, 0x0A, 0x00, 0x00, 0x00, 0x49, 0xD0, 0x28, 0x05,
            0x5A, 0x04, 0x00, 0x00, 0x00, 0x00, 0x4B, 0xD5, 0xC3, 0x05,
            0xCD, 0x02, 0x00, 0x01
        };

        MopLfgPackets::RoleCheckUpdate update;
        update.partyIndex = 0;
        update.state = LFG_ROLECHECK_INITIALITING;

        MopLfgPackets::RoleCheckMember leader;
        leader.guid = 0x04000000054829D1ULL;
        leader.roles = 0x0A;
        leader.level = 90;
        update.members.push_back(leader);

        MopLfgPackets::RoleCheckMember other;
        other.guid = 0x04000000054AC2D4ULL;
        other.roles = 0;
        other.level = 90;
        update.members.push_back(other);

        update.dungeonEntries.push_back(0x010002CDu);

        WorldPacket packet(SMSG_LFG_ROLE_CHECK_UPDATE, expected.size());
        MopLfgPackets::BuildRoleCheckUpdate(packet, update);

        AssertBytes(packet, expected, "two_member_role_check");
    }

    /// capture-000059 seq 719547, 68 bytes.
    ///
    /// A five-man role check, and the case that proves partyIndex is a real field: it
    /// carries 1, not 0. The leader has answered 0x03 (LEADER|TANK); the other four
    /// have not. One dungeon, entry 0x060001CE -- type 6, a different dungeon type from
    /// the case above.
    void test_five_member_role_check()
    {
        std::vector<uint8> const expected = {
            0x01, 0x02, 0x00, 0x00, 0x2F, 0x71, 0xB8, 0xD8, 0x6E, 0x37, 0x00,
            0x00, 0x00, 0x40, 0x80,
            0x5A, 0x04, 0x03, 0x00, 0x00, 0x00, 0xFF, 0xD5, 0x4D, 0x05,
            0x5A, 0x04, 0x00, 0x00, 0x00, 0x00, 0x39, 0x4D, 0xDF, 0x05,
            0x5A, 0x04, 0x00, 0x00, 0x00, 0x00, 0x4B, 0xB1, 0x05, 0x5A,
            0x07, 0x00, 0x00, 0x00, 0x00, 0x54, 0x59, 0x6A, 0x07,
            0x5A, 0x04, 0x00, 0x00, 0x00, 0x00, 0x3B, 0xA9, 0x57, 0x05,
            0xCE, 0x01, 0x00, 0x06
        };

        MopLfgPackets::RoleCheckUpdate update;
        update.partyIndex = 1;
        update.state = LFG_ROLECHECK_INITIALITING;

        uint64 const guids[5] = {
            0x0400000005FE4CD4ULL,
            0x040000000538DE4CULL,
            0x04000000054A00B0ULL,
            0x0600000006556B58ULL,
            0x04000000053A56A8ULL
        };
        uint32 const roles[5] = { 0x03, 0, 0, 0, 0 };

        for (size_t i = 0; i < 5; ++i)
        {
            MopLfgPackets::RoleCheckMember member;
            member.guid = guids[i];
            member.roles = roles[i];
            member.level = 90;
            update.members.push_back(member);
        }

        update.dungeonEntries.push_back(0x060001CEu);

        WorldPacket packet(SMSG_LFG_ROLE_CHECK_UPDATE, expected.size());
        MopLfgPackets::BuildRoleCheckUpdate(packet, update);

        AssertBytes(packet, expected, "five_member_role_check");
    }
}

int main()
{
    test_two_member_role_check();
    test_five_member_role_check();
    std::printf(g_fail ? "mop_lfg_role_check_packets_test: FAILED (%d)\n" : "mop_lfg_role_check_packets_test: OK\n", g_fail);
    return g_fail ? 1 : 0;
}
