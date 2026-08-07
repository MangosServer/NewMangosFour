/**
 * Byte-exact coverage for the SMSG_LFG_PROPOSAL_UPDATE (0x1E3B) writer.
 *
 * The expected bodies are REAL captured server bytes at build 18414. The test feeds the
 * writer the values decoded out of each capture and asserts it reproduces that capture
 * byte for byte, so this compares against traffic a retail server actually sent rather
 * than round-tripping our own assumptions.
 *
 * Corpus catalogueGenerationId
 *   2BE10C899585BAECD237705AC13BBF9262D81B6BDC085B462808C6869CE88752
 *
 * The two cases are deliberately as different as the corpus allows -- a 5-man dungeon
 * proposal and a 25-man raid finder proposal -- because a layout that only fits one
 * shape proves nothing. The raid case is a useful independent check on the decode: its
 * 2 tank / 6 healer / 17 dps composition is exactly what LfgDungeons.dbc carries in
 * Count_tank/Count_healer/Count_damage for LFR rows, established from the DBC and not
 * from this packet.
 */

#include "LFGMgr.h"
#include "WorldPacket.h"

#include <cstdio>
#include <vector>

namespace
{
    /// Set by AssertBytes on any mismatch; main() returns non-zero if it is set.
    ///
    /// This used to call assert(false), which expands to nothing under NDEBUG. These
    /// tests build in Release, so a mismatch printed its diagnostic and then exited 0 --
    /// the harness reported "OK" while the bytes disagreed, which is the one thing a
    /// byte-exactness test exists to prevent.
    ///
    /// It had in fact been failing. raid_finder_proposal's INPUT role array carried 0x09
    /// one slot late, so the writer was fed the wrong roles for players 15 and 16. The
    /// captured expected[] bytes were right all along and so was the writer -- only the
    /// hand-transcribed input was wrong, and nothing could surface it.
    bool g_failed = false;

    void AssertBytes(WorldPacket const& packet, std::vector<uint8> const& expected,
                     char const* label)
    {
        if (packet.size() != expected.size())
        {
            std::printf("%s: FAIL size %u, expected %u\n", label,
                        unsigned(packet.size()), unsigned(expected.size()));
            g_failed = true;
            return;
        }

        for (size_t i = 0; i < expected.size(); ++i)
        {
            if (packet.contents()[i] != expected[i])
            {
                std::printf("%s: FAIL byte %u is 0x%02X, expected 0x%02X\n", label,
                            unsigned(i), packet.contents()[i], expected[i]);
                g_failed = true;
            }
        }
    }

    MopLfgPackets::ProposalPlayer Player(uint32 roles, bool isSelf)
    {
        MopLfgPackets::ProposalPlayer entry;
        entry.roles = roles;
        entry.isSelf = isSelf;
        return entry;
    }

    /// capture-000044 seq 1948, 64 bytes.
    ///
    /// A five-man proposal in its initial state: nobody has answered yet, and the only
    /// bit set on any player is "this is you" on entry 0. Roles are 0x03 (LEADER|TANK),
    /// 0x04 (HEALER) and three 0x08 (DAMAGE) -- a textbook 1/1/3.
    void test_five_man_proposal()
    {
        std::vector<uint8> const expected = {
            0xF0, 0xB8, 0x00, 0x01, 0x50, 0x00, 0x00, 0x13, 0x2C, 0x05, 0x28,
            0x90, 0x03, 0x01, 0x00, 0x06, 0x00, 0xFF, 0x9B, 0x00, 0x00, 0x45,
            0x9C, 0x84, 0x00, 0x00, 0x07, 0x07, 0x61, 0x14, 0x54, 0x03, 0x00,
            0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08,
            0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x1E, 0x63, 0x04, 0xD6, 0x03, 0x00, 0x00, 0x00, 0x10
        };

        MopLfgPackets::ProposalUpdate update;
        update.requesterGuid = 0x0400000006296291ULL;
        update.instanceGuid = 0x1F44000011D72D05ULL;
        update.dungeonEntry = 0x06000103u;
        update.state = 0;
        update.clientQueueId = 39935;
        update.proposalId = 33948;
        update.joinTime = 1410621703u;
        update.encounters = 0;
        update.flags = 3;
        update.silent = false;

        update.players.push_back(Player(0x03, true));
        update.players.push_back(Player(0x04, false));
        update.players.push_back(Player(0x08, false));
        update.players.push_back(Player(0x08, false));
        update.players.push_back(Player(0x08, false));

        WorldPacket packet(SMSG_LFG_PROPOSAL_UPDATE, expected.size());
        MopLfgPackets::BuildProposalUpdate(packet, update);

        AssertBytes(packet, expected, "five_man_proposal");
    }

    /// capture-000059 seq 2063424, 156 bytes.
    ///
    /// A 25-man raid finder proposal. The recipient is entry 6, not entry 0, which is
    /// why the "is this you" bit cannot be assumed to sit on the first player.
    ///
    /// Roles include 0x32 and 0x09: bits above DAMAGE are real and must be passed
    /// through verbatim rather than masked to the four known role bits.
    void test_raid_finder_proposal()
    {
        std::vector<uint8> const expected = {
            0xB0, 0xB8, 0x00, 0x06, 0x40, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x30, 0xF3,
            0x05, 0xFF, 0xD5, 0xCC, 0x02, 0x00, 0x01, 0x00, 0x6F, 0x93, 0x00,
            0x00, 0x45, 0x7C, 0x2B, 0x00, 0x00, 0x04, 0xE7, 0x2D, 0xFF, 0x53,
            0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
            0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00,
            0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x32,
            0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
            0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
            0x00, 0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x08, 0x00,
            0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08,
            0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
            0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x4D, 0x4D, 0x03, 0x00, 0x00,
            0x00, 0x10
        };

        static uint32 const roles[25] = {
            0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x32, 0x08, 0x32, 0x08,
            0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x08, 0x08, 0x08, 0x08,
            0x08, 0x08, 0x08, 0x08, 0x08
        };

        MopLfgPackets::ProposalUpdate update;
        update.requesterGuid = 0x0400000005FE4CD4ULL;
        update.instanceGuid = 0x1F440000114CF200ULL;
        update.dungeonEntry = 0x010002CCu;
        update.state = 0;
        update.clientQueueId = 37743;
        update.proposalId = 11132;
        update.joinTime = 1409232359u;
        update.encounters = 0;
        update.flags = 3;
        update.silent = false;

        for (size_t i = 0; i < 25; ++i)
        {
            update.players.push_back(Player(roles[i], i == 6));
        }

        WorldPacket packet(SMSG_LFG_PROPOSAL_UPDATE, expected.size());
        MopLfgPackets::BuildProposalUpdate(packet, update);

        AssertBytes(packet, expected, "raid_finder_proposal");
    }
}

int main()
{
    test_five_man_proposal();
    test_raid_finder_proposal();

    if (g_failed)
    {
        std::printf("mop_lfg_proposal_packets_test: FAILED\n");
        return 1;
    }

    std::printf("mop_lfg_proposal_packets_test: OK\n");
    return 0;
}
