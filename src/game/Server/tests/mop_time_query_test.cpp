/**
 * Byte-exact tests for the 5.4.8 query-time and played-time packet bodies.
 */

#include "MopQueryPackets.h"
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

static void test_query_time_response()
{
    WorldPacket packet(SMSG_QUERY_TIME_RESPONSE, 8);
    MopQueryPackets::BuildQueryTimeResponse(packet, 0x11223344u, 0x55667788u);
    CHECK(ExpectBytes(packet, {
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55
    }));
}

static void test_played_time_request()
{
    WorldPacket trueRequest(CMSG_PLAYED_TIME, 1);
    trueRequest << uint8(0x80);
    CHECK(MopQueryPackets::ReadPlayedTimeRequest(trueRequest));

    WorldPacket falseRequest(CMSG_PLAYED_TIME, 1);
    falseRequest << uint8(0x00);
    CHECK(!MopQueryPackets::ReadPlayedTimeRequest(falseRequest));

    WorldPacket lowBitRequest(CMSG_PLAYED_TIME, 1);
    lowBitRequest << uint8(0x01);
    CHECK(!MopQueryPackets::ReadPlayedTimeRequest(lowBitRequest));
}

static void test_played_time_response()
{
    WorldPacket trueResponse(SMSG_PLAYED_TIME, 9);
    MopQueryPackets::BuildPlayedTimeResponse(
        trueResponse, 0x11223344u, 0x55667788u, true);
    CHECK(ExpectBytes(trueResponse, {
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55, 0x80
    }));

    WorldPacket falseResponse(SMSG_PLAYED_TIME, 9);
    MopQueryPackets::BuildPlayedTimeResponse(
        falseResponse, 0x11223344u, 0x55667788u, false);
    CHECK(ExpectBytes(falseResponse, {
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55, 0x00
    }));
}

static void test_opcode_values_are_framable()
{
    struct ExpectedOpcode { uint32_t actual; uint32_t expected; };
    ExpectedOpcode const values[] = {
        { uint32_t(CMSG_QUERY_TIME), 0x0640u },
        { uint32_t(SMSG_QUERY_TIME_RESPONSE), 0x100Fu },
        { uint32_t(CMSG_PLAYED_TIME), 0x03F6u },
        { uint32_t(SMSG_PLAYED_TIME), 0x11E2u }
    };

    for (ExpectedOpcode const& value : values)
    {
        CHECK(value.actual == value.expected);
        CHECK(value.actual < uint32_t(OPCODE_TABLE_SIZE));
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    test_query_time_response();
    test_played_time_request();
    test_played_time_response();
    test_opcode_values_are_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_time_query: all checks passed\n");
    return 0;
}
