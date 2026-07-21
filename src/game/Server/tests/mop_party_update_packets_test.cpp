/**
 * Independent byte fixtures for the 5.4.8.18414 party-update exchange.
 */

#include "MopPartyUpdatePackets.h"
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

static MopPartyUpdatePackets::PartyUpdate BaseUpdate()
{
    MopPartyUpdatePackets::PartyUpdate update;
    update.groupGuid = 0x0102030405060708ULL;
    update.leaderGuid = 0x1112131415161718ULL;
    update.looterGuid = 0x2122232425262728ULL;
    update.hasInstanceDifficulty = true;
    update.raidDifficulty = 3;
    update.dungeonDifficulty = 2;
    update.hasLootMode = true;
    update.lootMethod = 2;
    update.lootThreshold = 3;
    update.groupType = 1;
    update.partyIndex = 0;
    update.groupPosition = 0;
    update.sequence = 0x11223344;

    MopPartyUpdatePackets::Member first;
    first.guid = 0x3132333435363738ULL;
    first.name = "Alice";
    first.status = 1;
    update.members.push_back(first);

    MopPartyUpdatePackets::Member second;
    second.guid = 0x0042004400460048ULL;
    second.name = "Bob";
    second.roles = 4;
    second.status = 3;
    second.subgroup = 1;
    second.flags = 2;
    update.members.push_back(second);
    return update;
}

static void test_request()
{
    WorldPacket packet(CMSG_GROUP_REQUEST_JOIN_UPDATES, 1);
    packet << uint8(1);
    CHECK(MopPartyUpdatePackets::ReadRequest(packet) == 1);
}

static void test_normal_update()
{
    WorldPacket packet;
    CHECK(MopPartyUpdatePackets::BuildPartyUpdate(packet, BaseUpdate()));
    CHECK(packet.GetOpcode() == SMSG_GROUP_LIST);
    CHECK(Equal(packet, {
        0xFE,0x00,0x00,0x2F,0x17,0xD4,0x33,0xFF,0xFE,0xF0,0x19,
        0x03,0x00,0x00,0x00, 0x02,0x00,0x00,0x00,
        0x33,0x34,0x00,0x01,0x30,0x35,0x36,
        'A','l','i','c','e',0x32,0x37,0x00,0x39,0x00,
        0x43,0x04,0x03,0x45,'B','o','b',0x47,0x01,0x49,0x02,
        0x06,0x15,0x17,0x02,0x29,0x22,0x25,0x24,0x27,0x03,
        0x20,0x26,0x23,0x03,0x05,0x01,0x00,0x00,0x00,0x00,
        0x00,0x00,0x14,0x16,0x44,0x33,0x22,0x11,0x09,0x07,
        0x02,0x04,0x10,0x00,0x12,0x13
    }));
}

static void test_removed_update()
{
    MopPartyUpdatePackets::PartyUpdate update = BaseUpdate();
    update.leaderGuid = 0;
    update.looterGuid = 0;
    update.members.clear();
    update.hasInstanceDifficulty = false;
    update.hasLootMode = false;
    update.groupType = 0x10;
    update.groupPosition = -1;
    update.sequence = 5;

    WorldPacket packet;
    CHECK(MopPartyUpdatePackets::BuildPartyUpdate(packet, update));
    CHECK(Equal(packet, {
        0x88,0x00,0x00,0x01,0xE5,0x06,0x03,0x05,0x10,0x00,
        0xFF,0xFF,0xFF,0xFF,0x00,0x05,0x00,0x00,0x00,0x09,
        0x07,0x02,0x04,0x00
    }));
}

static void test_lfg_optional_block()
{
    MopPartyUpdatePackets::PartyUpdate update = BaseUpdate();
    update.looterGuid = 0;
    update.members.resize(1);
    update.members[0].name = "Z";
    update.members[0].roles = 5;
    update.hasInstanceDifficulty = false;
    update.hasLootMode = false;
    update.isLfg = true;
    update.lfgDungeonEntry = 0x11223344;
    update.lfgTail = 0x55667788;
    update.groupType = 8;
    update.partyIndex = 1;
    update.groupPosition = 1;
    update.sequence = 2;

    WorldPacket packet;
    CHECK(MopPartyUpdatePackets::BuildPartyUpdate(packet, update));
    CHECK(Equal(packet, {
        0xEE,0x00,0x00,0x1F,0x07,0xF7,0xF3,0x19,
        0x33,0x34,0x05,0x01,0x30,0x35,0x36,'Z',0x32,0x37,
        0x00,0x39,0x00,0x06,0x00,0x00,0x80,0x3F,0x00,0x00,
        0x44,0x33,0x22,0x11,0x00,0x00,0x00,0x88,0x77,0x66,
        0x55,0x15,0x17,0x03,0x05,0x08,0x01,0x01,0x00,0x00,
        0x00,0x00,0x14,0x16,0x02,0x00,0x00,0x00,0x09,0x07,
        0x02,0x04,0x10,0x00,0x12,0x13
    }));
}

static void test_invalid_name_preserves_packet()
{
    MopPartyUpdatePackets::PartyUpdate update = BaseUpdate();
    update.members[0].name = std::string(64, 'X');

    WorldPacket packet(SMSG_NOTIFICATION, 2);
    packet << uint8(0xAA) << uint8(0xBB);
    CHECK(!MopPartyUpdatePackets::BuildPartyUpdate(packet, update));
    CHECK(packet.GetOpcode() == SMSG_NOTIFICATION);
    CHECK(Equal(packet, { 0xAA, 0xBB }));
}

static void test_opcodes()
{
    CHECK(uint32(CMSG_GROUP_REQUEST_JOIN_UPDATES) == 0x178Au);
    CHECK(uint32(SMSG_GROUP_LIST) == 0x0CBBu);
    CHECK(uint32(CMSG_GROUP_REQUEST_JOIN_UPDATES) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_GROUP_LIST) < uint32(OPCODE_TABLE_SIZE));
}

int main(int, char**)
{
    test_request();
    test_normal_update();
    test_removed_update();
    test_lfg_optional_block();
    test_invalid_name_preserves_packet();
    test_opcodes();

    if (g_fail)
        return 1;
    std::printf("mop_party_update_packets: all checks passed\n");
    return 0;
}
