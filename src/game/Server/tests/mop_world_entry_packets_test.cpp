/**
 * Byte-exact tests for 5.4.8 world-entry packet bodies recovered from the
 * client readers at 0x6BB595, 0x6BB104, 0x6BE5A5, and 0xC90474.
 */

#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(WorldPacket const& packet, std::vector<uint8_t> const& expected)
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

static void test_login_verify_world()
{
    WorldPacket packet(SMSG_LOGIN_VERIFY_WORLD, 20);
    MopWorldEntryPackets::BuildLoginVerifyWorld(packet, 0x11223344u, 1.0f, 2.0f, 3.0f, 4.0f);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x40, 0x00, 0x00,
        0x00, 0x40, 0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0x40, 0x40
    }));
}

static void test_new_world()
{
    WorldPacket packet(SMSG_NEW_WORLD, 20);
    MopWorldEntryPackets::BuildNewWorld(packet, 0x11223344u, 1.0f, 2.0f, 3.0f, 4.0f);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x80, 0x3F, 0x44, 0x33, 0x22, 0x11, 0x00, 0x00,
        0x00, 0x40, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x80, 0x40
    }));
}

static void test_login_set_time_speed()
{
    WorldPacket packet(SMSG_LOGIN_SETTIMESPEED, 20);
    MopWorldEntryPackets::BuildLoginSetTimeSpeed(packet, 0x11223344u, 1.0f);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11, 0x00, 0x00,
        0x00, 0x00, 0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0x80, 0x3F
    }));
}

static void test_move_teleport()
{
    {
        WorldPacket packet(SMSG_MOVE_TELEPORT, 39);
        MopWorldEntryPackets::BuildMoveTeleport(packet, 0x8070605040302010ull, 0x8877665544332211ull,
            0xA1B2C3D4u, 1.0f, 2.0f, 3.0f, 4.0f);
        CHECK(ExpectBytes(packet, {
            0xFF, 0xFF, 0x80, 0x54, 0x45, 0x89, 0x23, 0x76, 0x10, 0x32,
            0x67, 0x51, 0x81, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00,
            0x40, 0x31, 0x41, 0x61, 0x00, 0x00, 0x80, 0x3F, 0xD4, 0xC3,
            0xB2, 0xA1, 0x11, 0x71, 0x21, 0x00, 0x00, 0x80, 0x40
        }));
    }
    {
        WorldPacket packet(SMSG_MOVE_TELEPORT, 29);
        MopWorldEntryPackets::BuildMoveTeleport(packet, 0x8070605040302010ull, 0,
            0xA1B2C3D4u, 1.0f, 2.0f, 3.0f, 4.0f);
        CHECK(ExpectBytes(packet, {
            0xFB, 0x80, 0x51, 0x81, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00,
            0x00, 0x40, 0x31, 0x41, 0x61, 0x00, 0x00, 0x80, 0x3F, 0xD4,
            0xC3, 0xB2, 0xA1, 0x11, 0x71, 0x21, 0x00, 0x00, 0x80, 0x40
        }));
    }
    {
        WorldPacket packet(SMSG_MOVE_TELEPORT, 20);
        MopWorldEntryPackets::BuildMoveTeleport(packet, 0, 0, 0xA1B2C3D4u,
            1.0f, 2.0f, 3.0f, 4.0f);
        CHECK(ExpectBytes(packet, {
            0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00, 0x40,
            0x00, 0x00, 0x80, 0x3F, 0xD4, 0xC3, 0xB2, 0xA1, 0x00, 0x00,
            0x80, 0x40
        }));
    }
    {
        WorldPacket packet(SMSG_MOVE_TELEPORT, 31);
        MopWorldEntryPackets::BuildMoveTeleport(packet, 0x0070000040000010ull, 0x8800660044002200ull,
            0xA1B2C3D4u, 1.0f, 2.0f, 3.0f, 4.0f);
        CHECK(ExpectBytes(packet, {
            0xC5, 0x93, 0x00, 0x45, 0x89, 0x23, 0x67, 0x00, 0x00, 0x40,
            0x40, 0x00, 0x00, 0x00, 0x40, 0x41, 0x00, 0x00, 0x80, 0x3F,
            0xD4, 0xC3, 0xB2, 0xA1, 0x11, 0x71, 0x00, 0x00, 0x80, 0x40
        }));
    }
}

static void test_opcode_values_are_framable()
{
    CHECK(uint32_t(SMSG_LOGIN_VERIFY_WORLD) == 0x1C0Fu);
    CHECK(uint32_t(SMSG_NEW_WORLD) == 0x1C3Bu);
    CHECK(uint32_t(SMSG_LOGIN_SETTIMESPEED) == 0x082Bu);
    CHECK(uint32_t(SMSG_MOVE_TELEPORT) == 0x0B39u);
    CHECK(uint32_t(SMSG_LOGIN_VERIFY_WORLD) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_NEW_WORLD) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_LOGIN_SETTIMESPEED) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_MOVE_TELEPORT) <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_login_verify_world();
    test_new_world();
    test_login_set_time_speed();
    test_move_teleport();
    test_opcode_values_are_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_world_entry_packets: all checks passed\n");
    return 0;
}
