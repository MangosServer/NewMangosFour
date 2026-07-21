/**
 * Independent byte fixtures for the 5.4.8.18414 LFG boot-vote packet.
 */

#include "MopLfgPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    return packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static MopLfgPackets::BootUpdate Fixture(std::string const& reason)
{
    MopLfgPackets::BootUpdate update;
    update.victimGuid = 0x0807060504030201ULL;
    update.reason = reason;
    update.inProgress = true;
    update.didVote = true;
    update.votePassed = false;
    update.agree = true;
    update.votesNeeded = 3;
    update.timeLeft = 120;
    update.agreeCount = 2;
    update.voteCount = 4;
    return update;
}

static void test_reason_present()
{
    WorldPacket packet(SMSG_LFG_BOOT_PLAYER, 32);
    CHECK(MopLfgPackets::BuildBootPlayer(packet, Fixture("ABC")));
    CHECK(Equal(packet, {
        0x6C,0x0F,0xF8,
        0x02,0x04,0x05,0x06,
        0x03,0x00,0x00,0x00,
        0x78,0x00,0x00,0x00,
        0x41,0x42,0x43,
        0x07,0x00,
        0x02,0x00,0x00,0x00,
        0x09,
        0x04,0x00,0x00,0x00,
        0x03
    }));
}

static void test_reason_absent()
{
    WorldPacket packet(SMSG_LFG_BOOT_PLAYER, 32);
    CHECK(MopLfgPackets::BuildBootPlayer(packet, Fixture("")));
    CHECK(Equal(packet, {
        0xEF,0xF8,
        0x02,0x04,0x05,0x06,
        0x03,0x00,0x00,0x00,
        0x78,0x00,0x00,0x00,
        0x07,0x00,
        0x02,0x00,0x00,0x00,
        0x09,
        0x04,0x00,0x00,0x00,
        0x03
    }));
}

static void test_reason_length_boundary()
{
    WorldPacket valid(SMSG_LFG_BOOT_PLAYER, 300);
    CHECK(MopLfgPackets::BuildBootPlayer(valid, Fixture(std::string(255, 'x'))));

    WorldPacket rejected(SMSG_LFG_BOOT_PLAYER, 0);
    CHECK(!MopLfgPackets::BuildBootPlayer(rejected, Fixture(std::string(256, 'x'))));
    CHECK(rejected.empty());
}

static void test_opcode()
{
    CHECK(uint32(SMSG_LFG_BOOT_PLAYER) == 0x183Au);
    CHECK(uint32(SMSG_LFG_BOOT_PLAYER) < uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_reason_present();
    test_reason_absent();
    test_reason_length_boundary();
    test_opcode();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_lfg_packets: all checks passed\n");
    return 0;
}
