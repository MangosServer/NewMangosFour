/**
 * Independent byte fixtures for the 5.4.8.18414 calendar updates.
 */

#include "Calendar.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    return packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static void test_initial_invite_list()
{
    MopCalendarPackets::InitialInvite entry;
    entry.guid = 0x0807060504030201ULL;
    entry.level = 0x55;

    WorldPacket packet(SMSG_CALENDAR_EVENT_INITIAL_INVITE, 16);
    CHECK(MopCalendarPackets::BuildCalendarInitialInvite(packet, { entry }));
    CHECK(Equal(packet, {
        0x00,0x00,0x03,0xFE,
        0x55, 0x05,0x07,0x04,0x06,0x09,0x00,0x02,0x03
    }));
}

static void test_invite_status()
{
    MopCalendarPackets::InviteStatus status;
    status.inviteeGuid = 0x0807060504030201ULL;
    status.eventId = 0x8877665544332211ULL;
    status.eventFlags = 0xA1B2C3D4u;
    status.lastUpdateTime = 0x11223344u;
    status.eventTime = 0x55667788u;
    status.status = 6;
    status.displayPendingAction = false;

    WorldPacket packet(SMSG_CALENDAR_EVENT_INVITE_STATUS, 40);
    MopCalendarPackets::BuildCalendarInviteStatus(packet, status);
    CHECK(Equal(packet, {
        0xDF,0x80,
        0x03,0x06,0x09,
        0x44,0x33,0x22,0x11,
        0x02,0x04,
        0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
        0x00,0x05,0x07,
        0x06,
        0xD4,0xC3,0xB2,0xA1,
        0x88,0x77,0x66,0x55
    }));
}

static void test_moderator_status()
{
    MopCalendarPackets::ModeratorStatus status;
    status.inviteeGuid = 0x0807060504030201ULL;
    status.eventId = 0x8877665544332211ULL;
    status.rank = 1;
    status.displayPendingAction = false;

    WorldPacket packet(SMSG_CALENDAR_EVENT_MODERATOR_STATUS, 24);
    MopCalendarPackets::BuildCalendarModeratorStatus(packet, status);
    CHECK(Equal(packet, {
        0xFE,0x80,
        0x07,0x04,0x06,0x03,0x09,0x02,0x05,
        0x01,
        0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
        0x00
    }));
}

static void test_opcodes()
{
    CHECK(uint32(SMSG_CALENDAR_EVENT_INITIAL_INVITE) == 0x16AEu);
    CHECK(uint32(SMSG_CALENDAR_EVENT_INVITE_STATUS) == 0x1C9Bu);
    CHECK(uint32(SMSG_CALENDAR_EVENT_MODERATOR_STATUS) == 0x048Fu);
    CHECK(uint32(SMSG_CALENDAR_EVENT_INITIAL_INVITE) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_CALENDAR_EVENT_INVITE_STATUS) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_CALENDAR_EVENT_MODERATOR_STATUS) < uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_initial_invite_list();
    test_invite_status();
    test_moderator_status();
    test_opcodes();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_calendar_packets: all checks passed\n");
    return 0;
}
