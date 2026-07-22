/**
 * Byte-exact tests for the 5.4.8 corpse-location query pair and its
 * transport map-position follow-up.
 */

#include "WorldSession.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(WorldPacket const& packet,
    std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        std::fprintf(stderr, "  size %u, wanted %u\n",
            unsigned(packet.size()), unsigned(expected.size()));
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

static void test_map_position_request()
{
    {
        uint8_t const fixture[] = {
            0xFF, 0x03, 0x06, 0x00, 0x07, 0x05, 0x02, 0x04, 0x09
        };
        WorldPacket packet(CMSG_CORPSE_MAP_POSITION_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));
        CHECK(MopQueryPackets::ReadCorpseMapPositionQuery(packet) ==
            UINT64_C(0x0807060504030201));
        CHECK(packet.rpos() == packet.size());
    }
    {
        uint8_t const fixture[] = { 0x70, 0xC2, 0xA0, 0xB3 };
        WorldPacket packet(CMSG_CORPSE_MAP_POSITION_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));
        CHECK(MopQueryPackets::ReadCorpseMapPositionQuery(packet) ==
            UINT64_C(0x00C30000B20000A1));
        CHECK(packet.rpos() == packet.size());
    }
}

static void test_corpse_query_response()
{
    MopQueryPackets::CorpseQueryResponse response;
    response.found = true;
    response.displayMapId = 0x11223344;
    response.x = 1.0f;
    response.y = 2.0f;
    response.z = 3.0f;
    response.corpseMapId = 0x55667788u;
    response.transportGuid = UINT64_C(0x0807060504030201);

    WorldPacket packet(SMSG_CORPSE_QUERY_RESPONSE, 30);
    MopQueryPackets::BuildCorpseQueryResponse(packet, response);
    CHECK(ExpectBytes(packet, {
        0xFF, 0x80,
        0x07,
        0x00, 0x00, 0x40, 0x40,
        0x03,
        0x44, 0x33, 0x22, 0x11,
        0x06, 0x04,
        0x00, 0x00, 0x80, 0x3F,
        0x05, 0x09, 0x02, 0x00,
        0x88, 0x77, 0x66, 0x55,
        0x00, 0x00, 0x00, 0x40
    }));

    WorldPacket missing(SMSG_CORPSE_QUERY_RESPONSE, 22);
    MopQueryPackets::BuildCorpseQueryResponse(missing, {});
    CHECK(ExpectBytes(missing, std::vector<uint8_t>(22, 0)));
}

static void test_map_position_response()
{
    WorldPacket packet(SMSG_CORPSE_MAP_POSITION_QUERY_RESPONSE, 16);
    MopQueryPackets::BuildCorpseMapPositionQueryResponse(packet,
        1.0f, 2.0f, 3.0f, 4.0f);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x80, 0x3F,
        0x00, 0x00, 0x80, 0x40,
        0x00, 0x00, 0x40, 0x40,
        0x00, 0x00, 0x00, 0x40
    }));
}

static void test_opcode_values()
{
    CHECK(uint32_t(CMSG_CORPSE_QUERY) == 0x1FBEu);
    CHECK(uint32_t(SMSG_CORPSE_QUERY_RESPONSE) == 0x0E0Bu);
    CHECK(uint32_t(CMSG_CORPSE_MAP_POSITION_QUERY) == 0x0A16u);
    CHECK(uint32_t(SMSG_CORPSE_MAP_POSITION_QUERY_RESPONSE) == 0x1A3Au);
    CHECK(uint32_t(CMSG_CORPSE_QUERY) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_CORPSE_QUERY_RESPONSE) <= 0x1FFFu);
    CHECK(uint32_t(CMSG_CORPSE_MAP_POSITION_QUERY) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_CORPSE_MAP_POSITION_QUERY_RESPONSE) <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_map_position_request();
    test_corpse_query_response();
    test_map_position_response();
    test_opcode_values();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_corpse_query: all checks passed\n");
    return 0;
}
