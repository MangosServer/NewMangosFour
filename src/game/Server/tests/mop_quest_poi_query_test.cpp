/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 5.4.8 client build 18414.
 */

/**
 * Byte-exact tests for the 5.4.8 quest-POI request and response pair.
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

static void test_request()
{
    {
        uint8_t const fixture[] = {
            0x00, 0x00, 0x08,             // two entries in a 22-bit count
            0x44, 0x33, 0x22, 0x11,
            0x88, 0x77, 0x66, 0x55
        };
        WorldPacket packet(CMSG_QUEST_POI_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));

        std::vector<uint32> questIds;
        CHECK(MopQueryPackets::ParseQuestPoiQueryRequest(packet, questIds));
        CHECK(questIds.size() == 2);
        CHECK(questIds[0] == 0x11223344u);
        CHECK(questIds[1] == 0x55667788u);
        CHECK(packet.rpos() == packet.size());
    }
    {
        uint8_t const fixture[] = {
            0x00, 0x00, 0x68              // 26 exceeds the 25-slot quest log
        };
        WorldPacket packet(CMSG_QUEST_POI_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));

        std::vector<uint32> questIds{ 7 };
        CHECK(!MopQueryPackets::ParseQuestPoiQueryRequest(packet, questIds));
        CHECK(questIds.size() == 1 && questIds[0] == 7);
        CHECK(packet.rpos() == packet.size());
    }
    {
        uint8_t const fixture[] = {
            0x00, 0x00, 0x04,             // one quest ID, but no body follows
        };
        WorldPacket packet(CMSG_QUEST_POI_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));

        std::vector<uint32> questIds;
        CHECK(!MopQueryPackets::ParseQuestPoiQueryRequest(packet, questIds));
        CHECK(questIds.empty());
        CHECK(packet.rpos() == packet.size());
    }
    {
        uint8_t const fixture[] = {
            0x00, 0x00, 0x00, 0xAA       // zero entries with trailing garbage
        };
        WorldPacket packet(CMSG_QUEST_POI_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));

        std::vector<uint32> questIds;
        CHECK(!MopQueryPackets::ParseQuestPoiQueryRequest(packet, questIds));
        CHECK(questIds.empty());
        CHECK(packet.rpos() == packet.size());
    }
}

static void test_response()
{
    MopQueryPackets::QuestPoiRecord poi;
    poi.poiId = 0x51525354u;
    poi.objectiveIndex = -2;
    poi.unknown2 = 0x61626364u;
    poi.mapId = 0x81828384u;
    poi.mapAreaId = 0xA1A2A3A4u;
    poi.worldEffectId = 0x01020304u;
    poi.playerConditionId = 0xD1D2D3D4u;
    poi.unknown1 = 0xC1C2C3C4u;
    poi.unknown3 = 0xB1B2B3B4u;
    poi.unknown4 = 0x71727374u;
    poi.floorId = 0x91929394u;
    poi.points.push_back({ 0x11121314, 0x21222324 });
    poi.points.push_back({ 0x31323334, 0x41424344 });

    MopQueryPackets::QuestPoiResponse quest;
    quest.questId = 0xE1E2E3E4u;
    quest.pois.push_back(poi);

    WorldPacket packet;
    CHECK(MopQueryPackets::BuildQuestPoiQueryResponse(packet, { quest }));
    CHECK(packet.GetOpcode() == SMSG_QUEST_POI_QUERY_RESPONSE);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x10, 0x00, 0x04, 0x00, 0x00, 0x40,
        0x04, 0x03, 0x02, 0x01,
        0x14, 0x13, 0x12, 0x11,
        0x24, 0x23, 0x22, 0x21,
        0x34, 0x33, 0x32, 0x31,
        0x44, 0x43, 0x42, 0x41,
        0xFE, 0xFF, 0xFF, 0xFF,
        0x54, 0x53, 0x52, 0x51,
        0x64, 0x63, 0x62, 0x61,
        0x74, 0x73, 0x72, 0x71,
        0x84, 0x83, 0x82, 0x81,
        0x94, 0x93, 0x92, 0x91,
        0xA4, 0xA3, 0xA2, 0xA1,
        0xB4, 0xB3, 0xB2, 0xB1,
        0xC4, 0xC3, 0xC2, 0xC1,
        0xD4, 0xD3, 0xD2, 0xD1,
        0xE4, 0xE3, 0xE2, 0xE1,
        0x01, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00
    }));

    WorldPacket empty;
    CHECK(MopQueryPackets::BuildQuestPoiQueryResponse(empty, {}));
    CHECK(empty.GetOpcode() == SMSG_QUEST_POI_QUERY_RESPONSE);
    CHECK(ExpectBytes(empty, {
        0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }));
}

static void test_opcode_values()
{
    CHECK(uint32_t(CMSG_QUEST_POI_QUERY) == 0x10C2u);
    CHECK(uint32_t(SMSG_QUEST_POI_QUERY_RESPONSE) == 0x067Fu);
    CHECK(uint32_t(CMSG_QUEST_POI_QUERY) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_QUEST_POI_QUERY_RESPONSE) <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_request();
    test_response();
    test_opcode_values();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_quest_poi_query: all checks passed\n");
    return 0;
}
