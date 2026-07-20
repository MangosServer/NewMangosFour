/**
 * Byte-exact tests for the 5.4.8 client-control readers at 0x6F51AA and
 * 0xC8EB90.
 */

#include "MopControlPackets.h"
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

static void test_client_control_update()
{
    {
        WorldPacket packet(SMSG_CLIENT_CONTROL_UPDATE, 10);
        MopControlPackets::BuildClientControlUpdate(packet, 0x8070605040302010ull, true);
        CHECK(ExpectBytes(packet, {
            0xFF, 0x80, 0x21, 0x61, 0x81, 0x51, 0x31, 0x71, 0x41, 0x11
        }));
    }
    {
        WorldPacket packet(SMSG_CLIENT_CONTROL_UPDATE, 10);
        MopControlPackets::BuildClientControlUpdate(packet, 0x8070605040302010ull, false);
        CHECK(ExpectBytes(packet, {
            0xDF, 0x80, 0x21, 0x61, 0x81, 0x51, 0x31, 0x71, 0x41, 0x11
        }));
    }
    {
        WorldPacket packet(SMSG_CLIENT_CONTROL_UPDATE, 5);
        MopControlPackets::BuildClientControlUpdate(packet, 0x0070000040000010ull, true);
        CHECK(ExpectBytes(packet, { 0x3C, 0x00, 0x71, 0x41, 0x11 }));
    }
    {
        WorldPacket packet(SMSG_CLIENT_CONTROL_UPDATE, 2);
        MopControlPackets::BuildClientControlUpdate(packet, 0, true);
        CHECK(ExpectBytes(packet, { 0x20, 0x00 }));
    }
    {
        WorldPacket packet(SMSG_CLIENT_CONTROL_UPDATE, 2);
        MopControlPackets::BuildClientControlUpdate(packet, 0, false);
        CHECK(ExpectBytes(packet, { 0x00, 0x00 }));
    }
}

static void test_set_active_mover()
{
    {
        WorldPacket packet(SMSG_MOVE_SET_ACTIVE_MOVER, 9);
        MopControlPackets::BuildSetActiveMover(packet, 0x8070605040302010ull);
        CHECK(ExpectBytes(packet, {
            0xFF, 0x51, 0x71, 0x31, 0x11, 0x41, 0x81, 0x61, 0x21
        }));
    }
    {
        WorldPacket packet(SMSG_MOVE_SET_ACTIVE_MOVER, 4);
        MopControlPackets::BuildSetActiveMover(packet, 0x0070000040000010ull);
        CHECK(ExpectBytes(packet, { 0x0B, 0x71, 0x11, 0x41 }));
    }
    {
        WorldPacket packet(SMSG_MOVE_SET_ACTIVE_MOVER, 1);
        MopControlPackets::BuildSetActiveMover(packet, 0);
        CHECK(ExpectBytes(packet, { 0x00 }));
    }
}

static void test_opcode_values_are_framable()
{
    CHECK(uint32_t(SMSG_CLIENT_CONTROL_UPDATE) == 0x1043u);
    CHECK(uint32_t(SMSG_MOVE_SET_ACTIVE_MOVER) == 0x0C6Du);
    CHECK(uint32_t(SMSG_CLIENT_CONTROL_UPDATE) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_MOVE_SET_ACTIVE_MOVER) <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_client_control_update();
    test_set_active_mover();
    test_opcode_values_are_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_control_packets: all checks passed\n");
    return 0;
}
