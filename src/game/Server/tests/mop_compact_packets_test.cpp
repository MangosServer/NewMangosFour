/**
 * Byte-exact tests for compact 5.4.8 server packet bodies recovered from the
 * client readers at 0x6F568B, 0xC8CBBE, 0x6D18F6, 0x94E111, and 0xCCDD26.
 */

#include "MopCompactPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool BytesEqual(WorldPacket const& packet, std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        std::fprintf(stderr, "  size %u, wanted %u\n", unsigned(packet.size()), unsigned(expected.size()));
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (packet.contents()[i] != expected[i])
        {
            std::fprintf(stderr, "  byte %u = 0x%02X, wanted 0x%02X\n",
                         unsigned(i), packet.contents()[i], expected[i]);
            return false;
        }
    }
    return true;
}

static void test_attack_swing_reasons()
{
    const uint8_t expected[] = { 0x00, 0x40, 0x80, 0xC0 };
    for (uint8_t reason = 0; reason < 4; ++reason)
    {
        WorldPacket packet(SMSG_ATTACKSWING_ERROR, 1);
        MopCompactPackets::BuildAttackSwingError(packet, reason);
        CHECK(BytesEqual(packet, { expected[reason] }));
        CHECK(packet.GetOpcode() == SMSG_ATTACKSWING_ERROR);
    }
}

static void test_swim_speed_guid_layouts()
{
    {
        WorldPacket packet(SMSG_MOVE_SET_SWIM_SPEED, 17);
        MopCompactPackets::BuildMoveSetSwimSpeed(packet, 0x0123456789ABCDEFull, 0x12345678u, 1.0f);
        CHECK(BytesEqual(packet, {
            0xFF,
            0x78, 0x56, 0x34, 0x12,
            0xCC, 0x88,
            0x00, 0x00, 0x80, 0x3F,
            0x22, 0x00, 0xEE, 0x44, 0xAA, 0x66
        }));
    }
    {
        WorldPacket packet(SMSG_MOVE_SET_SWIM_SPEED, 10);
        MopCompactPackets::BuildMoveSetSwimSpeed(packet, 0xFFull, 0, 1.0f);
        CHECK(BytesEqual(packet, {
            0x40,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, 0x3F,
            0xFE
        }));
    }
    {
        WorldPacket packet(SMSG_MOVE_SET_SWIM_SPEED, 9);
        MopCompactPackets::BuildMoveSetSwimSpeed(packet, 0, 7, 1.0f);
        CHECK(BytesEqual(packet, {
            0x00,
            0x07, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, 0x3F
        }));
    }
}

static void test_random_roll_guid_layouts()
{
    {
        WorldPacket packet(SMSG_RANDOM_ROLL, 21);
        MopCompactPackets::BuildRandomRoll(packet, 0x0123456789ABCDEFull, 1, 100, 42);
        CHECK(BytesEqual(packet, {
            0x2A, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00,
            0x64, 0x00, 0x00, 0x00,
            0xFF,
            0x44, 0x66, 0xAA, 0xEE, 0x88, 0xCC, 0x22, 0x00
        }));
    }
    {
        WorldPacket packet(SMSG_RANDOM_ROLL, 14);
        MopCompactPackets::BuildRandomRoll(packet, 0xFFull, 5, 9, 7);
        CHECK(BytesEqual(packet, {
            0x07, 0x00, 0x00, 0x00,
            0x05, 0x00, 0x00, 0x00,
            0x09, 0x00, 0x00, 0x00,
            0x80, 0xFE
        }));
    }
    {
        WorldPacket packet(SMSG_RANDOM_ROLL, 13);
        MopCompactPackets::BuildRandomRoll(packet, 0, 0, 0, 0);
        CHECK(BytesEqual(packet, {
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00
        }));
    }
}

static void test_instance_encounter_variants()
{
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 5);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, 0, 0, 0xA5, 0x5A));
        CHECK(BytesEqual(packet, { 0x00, 0x00, 0x00, 0x00, 0xA5 }));
    }
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 4);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, 1, 0, 0xA5, 0x5A));
        CHECK(BytesEqual(packet, { 0x01, 0x00, 0x00, 0x00 }));
    }
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 14);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, 2, 0x0123456789ABCDEFull, 0xA5, 0x5A));
        CHECK(BytesEqual(packet, {
            0x02, 0x00, 0x00, 0x00,
            0xFF, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
            0xA5
        }));
    }
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 7);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, 3, 0xFFull, 0xA5, 0x5A));
        CHECK(BytesEqual(packet, {
            0x03, 0x00, 0x00, 0x00,
            0x01, 0xFF,
            0xA5
        }));
    }
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 6);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, 7, 0, 0xA5, 0x5A));
        CHECK(BytesEqual(packet, { 0x07, 0x00, 0x00, 0x00, 0xA5, 0x5A }));
    }

    for (uint32_t type : { 4u, 5u, 6u, 8u, 9u, 10u })
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 14);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, type, 0xFFull, 0xA5, 0x5A));
        if (type == 4)
            CHECK(BytesEqual(packet, { 0x04, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xA5 }));
        else if (type == 5 || type == 6 || type == 8)
            CHECK(BytesEqual(packet, { uint8_t(type), 0x00, 0x00, 0x00, 0xA5 }));
        else
            CHECK(BytesEqual(packet, { uint8_t(type), 0x00, 0x00, 0x00 }));
    }

    WorldPacket invalid(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 4);
    CHECK(!MopCompactPackets::BuildInstanceEncounter(invalid, 11, 0, 0, 0));
    CHECK(invalid.empty());
}

static void test_raid_difficulty()
{
    WorldPacket packet(SMSG_SET_RAID_DIFFICULTY, 4);
    MopCompactPackets::BuildSetRaidDifficulty(packet, 3);
    CHECK(BytesEqual(packet, { 0x03, 0x00, 0x00, 0x00 }));
}

static void test_opcode_values_are_framable()
{
    CHECK(uint32_t(SMSG_ATTACKSWING_ERROR) == 0x11E1u);
    CHECK(uint32_t(SMSG_MOVE_SET_SWIM_SPEED) == 0x0817u);
    CHECK(uint32_t(SMSG_RANDOM_ROLL) == 0x141Au);
    CHECK(uint32_t(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT) == 0x0332u);
    CHECK(uint32_t(SMSG_SET_RAID_DIFFICULTY) == 0x0591u);

    CHECK(uint32_t(SMSG_ATTACKSWING_ERROR) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_MOVE_SET_SWIM_SPEED) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_RANDOM_ROLL) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_SET_RAID_DIFFICULTY) <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_attack_swing_reasons();
    test_swim_speed_guid_layouts();
    test_random_roll_guid_layouts();
    test_instance_encounter_variants();
    test_raid_difficulty();
    test_opcode_values_are_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_compact_packets: all checks passed\n");
    return 0;
}
