/**
 * Byte-exact tests for the 5.4.8 game-object query request/response path.
 */

#include "MopQueryPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
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

static void AppendU32(std::vector<uint8_t>& out, uint32 value)
{
    out.push_back(uint8_t(value));
    out.push_back(uint8_t(value >> 8));
    out.push_back(uint8_t(value >> 16));
    out.push_back(uint8_t(value >> 24));
}

static void AppendFloat(std::vector<uint8_t>& out, float value)
{
    uint32 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    AppendU32(out, bits);
}

static void AppendCString(std::vector<uint8_t>& out, std::string const& value)
{
    out.insert(out.end(), value.begin(), value.end());
    out.push_back(0);
}

static void test_request_guid_layouts()
{
    {
        uint8_t const fixture[] = {
            0x44, 0x33, 0x22, 0x11, 0xFF,
            0x21, 0x61, 0x41, 0x51, 0x71, 0x31, 0x81, 0x11
        };
        WorldPacket packet(CMSG_GAMEOBJECT_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));
        MopQueryPackets::GameObjectQueryRequest const request =
            MopQueryPackets::ReadGameObjectQueryRequest(packet);
        CHECK(request.entry == 0x11223344u);
        CHECK(request.guid == 0x8070605040302010ull);
        CHECK(packet.rpos() == packet.size());
    }

    {
        uint8_t const fixture[] = {
            0x44, 0x33, 0x22, 0x11, 0x82, 0x61, 0x11
        };
        WorldPacket packet(CMSG_GAMEOBJECT_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));
        MopQueryPackets::GameObjectQueryRequest const request =
            MopQueryPackets::ReadGameObjectQueryRequest(packet);
        CHECK(request.entry == 0x11223344u);
        CHECK(request.guid == 0x0000600000000010ull);
        CHECK(packet.rpos() == packet.size());
    }
}

static void test_missing_template()
{
    WorldPacket packet(SMSG_GAMEOBJECT_QUERY_RESPONSE, 9);
    MopQueryPackets::GameObjectQueryResponse record;
    record.entry = 0x11223344u;
    MopQueryPackets::BuildGameObjectQueryResponse(packet, record);
    CHECK(ExpectBytes(packet, {
        0x00, 0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0x00, 0x00
    }));
}

static void test_minimal_hit()
{
    WorldPacket packet(SMSG_GAMEOBJECT_QUERY_RESPONSE, 161);
    MopQueryPackets::GameObjectQueryResponse record;
    record.entry = 0x11223344u;
    record.hasData = true;
    MopQueryPackets::BuildGameObjectQueryResponse(packet, record);

    std::vector<uint8_t> expected = {
        0x80, 0x44, 0x33, 0x22, 0x11, 0x98, 0x00, 0x00, 0x00
    };
    for (int i = 0; i < 2; ++i)
        AppendU32(expected, 0);
    expected.insert(expected.end(), 7, uint8_t(0));
    for (int i = 0; i < 32; ++i)
        AppendU32(expected, 0);
    AppendFloat(expected, 0.0f);
    expected.push_back(0);
    AppendU32(expected, 0);

    CHECK(expected.size() == 9u + 152u);
    CHECK(ExpectBytes(packet, expected));
}

static void test_populated_hit()
{
    MopQueryPackets::GameObjectQueryResponse record;
    record.entry = 0x01020304u;
    record.hasData = true;
    record.type = 0x11121314u;
    record.displayId = 0x15161718u;
    record.names = {{ "One", "Two", "Three", "Four" }};
    record.iconName = "Icon";
    record.castBarCaption = "Cast";
    record.unknownString = "Unknown";
    for (size_t i = 0; i < record.data.size(); ++i)
        record.data[i] = 0x20000000u + uint32(i);
    record.size = 1.5f;
    record.questItems = {
        0x30000001u, 0x30000002u, 0x30000003u,
        0x30000004u, 0x30000005u, 0x30000006u
    };
    record.trailingUnknown = 0;

    WorldPacket packet(SMSG_GAMEOBJECT_QUERY_RESPONSE, 256);
    MopQueryPackets::BuildGameObjectQueryResponse(packet, record);

    size_t const independentlyComputedBlobSize =
        2u * sizeof(uint32) +
        (4u + 4u + 6u + 5u + 5u + 5u + 8u) +
        32u * sizeof(uint32) + sizeof(float) + sizeof(uint8) +
        6u * sizeof(uint32) + sizeof(uint32);
    CHECK(independentlyComputedBlobSize == 206u);

    std::vector<uint8_t> expected = { 0x80, 0x04, 0x03, 0x02, 0x01 };
    AppendU32(expected, uint32(independentlyComputedBlobSize));
    AppendU32(expected, record.type);
    AppendU32(expected, record.displayId);
    AppendCString(expected, "One");
    AppendCString(expected, "Two");
    AppendCString(expected, "Three");
    AppendCString(expected, "Four");
    AppendCString(expected, "Icon");
    AppendCString(expected, "Cast");
    AppendCString(expected, "Unknown");
    for (size_t i = 0; i < 32; ++i)
        AppendU32(expected, 0x20000000u + uint32(i));
    AppendFloat(expected, 1.5f);
    expected.push_back(6);
    for (uint32 value = 0x30000001u; value <= 0x30000006u; ++value)
        AppendU32(expected, value);
    AppendU32(expected, 0);

    CHECK(expected.size() == 9u + independentlyComputedBlobSize);
    CHECK(ExpectBytes(packet, expected));
}

static void test_opcode_values_are_framable()
{
    CHECK(uint32_t(CMSG_GAMEOBJECT_QUERY) == 0x1461u);
    CHECK(uint32_t(SMSG_GAMEOBJECT_QUERY_RESPONSE) == 0x06BFu);
    CHECK(uint32_t(CMSG_GAMEOBJECT_QUERY) < uint32_t(OPCODE_TABLE_SIZE));
    CHECK(uint32_t(SMSG_GAMEOBJECT_QUERY_RESPONSE) < uint32_t(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_request_guid_layouts();
    test_missing_template();
    test_minimal_hit();
    test_populated_hit();
    test_opcode_values_are_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_gameobject_query: all checks passed\n");
    return 0;
}
