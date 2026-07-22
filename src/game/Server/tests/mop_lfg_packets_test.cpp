/**
 * Independent byte fixtures for the 5.4.8.18414 LFG boot-vote packet.
 */

#include "LFGMgr.h"
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
    bool const equal = packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
    if (!equal)
    {
        std::fprintf(stderr, "actual (%zu):", packet.size());
        for (size_t i = 0; i < packet.size(); ++i)
            std::fprintf(stderr, " %02X", packet.contents()[i]);
        std::fprintf(stderr, "\n");
    }
    return equal;
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

static MopLfgPackets::StatusUpdate StatusFixture()
{
    MopLfgPackets::StatusUpdate update;
    update.requesterGuid = 0x0807060504030201ULL;
    update.suspendedPlayerGuids.push_back(0x100F0E0D0C0B0A09ULL);
    update.dungeonEntries.push_back(0x01020304u);
    update.dungeonEntries.push_back(0xA1A2A3A4u);
    update.comment = "AB";
    update.needs = {{ 1, 2, 3 }};
    update.isParty = true;
    update.joined = true;
    update.notifyUi = true;
    update.lfgJoined = true;
    update.queued = false;
    update.updateReason = 0x11;
    update.requestedRoles = 0x22334455u;
    update.ticketId = 0x66778899u;
    update.ticketTime = 0xAABBCCDDu;
    update.dungeonCategory = 0x0Du;
    update.ticketType = 3;
    return update;
}

static void test_update_status_exact_fixture()
{
    WorldPacket packet(SMSG_LFG_UPDATE_STATUS, 80);
    CHECK(MopLfgPackets::BuildUpdateStatus(packet, StatusFixture()));

    // Direct 18414 reader order: selector 298, reader 0x71EF25.
    CHECK(Equal(packet, {
        0x02,0xC0,0x00,0x02,0xFF,0x00,0x00,0x00,0xFF,0xE0,
        0x05,0x01,0x02,0x03,0x04,
        0x11,0x08,0x0B,0x0E,0x0C,0x0F,0x0A,0x0D,
        0x06,0x11,
        0x55,0x44,0x33,0x22,
        0x99,0x88,0x77,0x66,
        0x07,0x41,0x42,0x02,
        0x04,0x03,0x02,0x01,
        0xA4,0xA3,0xA2,0xA1,
        0x00,0x03,
        0xDD,0xCC,0xBB,0xAA,
        0x0D,
        0x03,0x00,0x00,0x00,
        0x09
    }));
}

static void test_update_status_empty_optional_fields()
{
    MopLfgPackets::StatusUpdate update;
    WorldPacket packet(SMSG_LFG_UPDATE_STATUS, 32);
    CHECK(MopLfgPackets::BuildUpdateStatus(packet, update));
    CHECK(Equal(packet, {
        0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,
        0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,
        0x03,0x00,0x00,0x00
    }));
}

static void test_update_status_bounds()
{
    MopLfgPackets::StatusUpdate update;
    update.comment.assign(255, 'x');
    WorldPacket valid(SMSG_LFG_UPDATE_STATUS, 300);
    CHECK(MopLfgPackets::BuildUpdateStatus(valid, update));

    update.comment.push_back('x');
    WorldPacket invalidComment(SMSG_LFG_UPDATE_STATUS, 0);
    CHECK(!MopLfgPackets::BuildUpdateStatus(invalidComment, update));
    CHECK(invalidComment.empty());
}

static void test_update_status_opcode()
{
    CHECK(uint32(SMSG_LFG_UPDATE_STATUS) == 0x0C2Eu);
    CHECK(uint32(SMSG_LFG_UPDATE_STATUS) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(CMSG_LFG_GET_STATUS) == 0x032Du);
}

static void test_default_player_status()
{
    LFGPlayerStatus status;
    CHECK(status.state == LFG_STATE_NONE);
    CHECK(status.updateType == LFG_UPDATE_DEFAULT);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_reason_present();
    test_reason_absent();
    test_reason_length_boundary();
    test_opcode();
    test_update_status_exact_fixture();
    test_update_status_empty_optional_fields();
    test_update_status_bounds();
    test_update_status_opcode();
    test_default_player_status();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_lfg_packets: all checks passed\n");
    return 0;
}
