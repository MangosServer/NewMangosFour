/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * Byte-exact tests for the 5.4.8 creature query request/response path.
 */

#include "WorldSession.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
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

static void test_missing_template()
{
    WorldPacket missing(SMSG_CREATURE_QUERY_RESPONSE, 5);
    MopQueryPackets::CreatureQueryResponse miss;
    miss.entry = 0x11223344u;
    MopQueryPackets::BuildCreatureQueryResponse(missing, miss);
    CHECK(ExpectBytes(missing, { 0x44, 0x33, 0x22, 0x11, 0x00 }));
}

static void test_minimal_hit()
{
    WorldPacket minimal(SMSG_CREATURE_QUERY_RESPONSE, 128);
    MopQueryPackets::CreatureQueryResponse record;
    record.entry = 0x11223344u;
    record.hasData = true;
    record.name = "A";
    MopQueryPackets::BuildCreatureQueryResponse(minimal, record);

    std::vector<uint8_t> expected = { 0x44, 0x33, 0x22, 0x11,
        0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    for (int i = 0; i < 10; ++i)
        AppendU32(expected, 0);
    expected.push_back('A');
    expected.push_back(0);
    for (int i = 0; i < 2 + 6 + 1 + 1 + 1; ++i)
        AppendU32(expected, 0);

    CHECK(ExpectBytes(minimal, expected));
}

static void test_populated_hit()
{
    MopQueryPackets::CreatureQueryResponse record;
    record.entry = 0x01020304u;
    record.hasData = true;
    record.name = "Wolf";
    record.subName = "Alpha";
    record.iconName = "Directions";
    record.killCredits = {{ 0xA1A2A3A4u, 0xB1B2B3B4u }};
    record.modelIds = {{ 0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u }};
    record.expansion = 4;
    record.creatureType = 7;
    record.healthMultiplier = 1.5f;
    record.creatureTypeFlags = 0x11223344u;
    record.creatureTypeFlags2 = 0;
    record.rank = 3;
    record.movementTemplateId = 5;
    record.questItems = {{ 0x11111111u, 0x22222222u, 0x33333333u,
                           0x44444444u, 0x55555555u, 0x66666666u }};
    record.powerMultiplier = 2.5f;
    record.family = 9;
    record.racialLeader = true;

    WorldPacket populated(SMSG_CREATURE_QUERY_RESPONSE, 160);
    MopQueryPackets::BuildCreatureQueryResponse(populated, record);

    uint8_t const bitPhase[] = {
        0x80, 0x60, 0x00, 0x01, 0x80, 0x00, 0x05, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xB0
    };
    std::vector<uint8_t> populatedExpected = { 0x04, 0x03, 0x02, 0x01 };
    populatedExpected.insert(populatedExpected.end(), std::begin(bitPhase), std::end(bitPhase));
    AppendU32(populatedExpected, 0xA1A2A3A4u);
    AppendU32(populatedExpected, 0x04040404u);
    AppendU32(populatedExpected, 0x02020202u);
    AppendU32(populatedExpected, 4);
    AppendU32(populatedExpected, 7);
    AppendFloat(populatedExpected, 1.5f);
    AppendU32(populatedExpected, 0x11223344u);
    AppendU32(populatedExpected, 0);
    AppendU32(populatedExpected, 3);
    AppendU32(populatedExpected, 5);
    populatedExpected.insert(populatedExpected.end(), { 'W', 'o', 'l', 'f', 0 });
    populatedExpected.insert(populatedExpected.end(), { 'A', 'l', 'p', 'h', 'a', 0 });
    AppendU32(populatedExpected, 0x01010101u);
    AppendU32(populatedExpected, 0x03030303u);
    populatedExpected.insert(populatedExpected.end(),
        { 'D', 'i', 'r', 'e', 'c', 't', 'i', 'o', 'n', 's', 0 });
    for (uint32 value : record.questItems)
        AppendU32(populatedExpected, value);
    AppendU32(populatedExpected, 0xB1B2B3B4u);
    AppendFloat(populatedExpected, 2.5f);
    AppendU32(populatedExpected, 9);
    CHECK(ExpectBytes(populated, populatedExpected));
}

static void test_opcode_values_are_framable()
{
    CHECK(uint32_t(CMSG_CREATURE_QUERY) < uint32_t(OPCODE_TABLE_SIZE));
    CHECK(uint32_t(SMSG_CREATURE_QUERY_RESPONSE) < uint32_t(OPCODE_TABLE_SIZE));
    CHECK(uint32_t(CMSG_CREATURE_QUERY) == 0x0842u);
    CHECK(uint32_t(SMSG_CREATURE_QUERY_RESPONSE) == 0x048Bu);
}

static bool PacketContains(WorldPacket const& packet, std::vector<uint8_t> const& needle)
{
    if (needle.empty() || packet.size() < needle.size())
        return false;
    for (size_t i = 0; i + needle.size() <= packet.size(); ++i)
    {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j)
            if (packet.contents()[i + j] != needle[j]) { match = false; break; }
        if (match)
            return true;
    }
    return false;
}

// Mirrors the QueryHandler creature-storm circuit-breaker record: a scoped synthetic POSITIVE for an
// unknown battle-pet creature, with a non-empty placeholder name and valid (non-zero) defaults. Proves
// the codec frames it as a well-formed cacheable positive (Codex follow-up: the golden test otherwise
// only covered a negative and an "A"-name positive, not the synthetic stub the handler now emits).
static void test_synthetic_stub()
{
    MopQueryPackets::CreatureQueryResponse stub;
    stub.entry = 60649u;
    stub.hasData = true;
    stub.name = "Battle Pet 60649";
    stub.creatureType = 10u;            // CREATURE_TYPE_NOT_SPECIFIED (a defined type, not 0)
    stub.healthMultiplier = 1.0f;
    stub.powerMultiplier = 1.0f;

    WorldPacket stubPacket(SMSG_CREATURE_QUERY_RESPONSE, 128);
    MopQueryPackets::BuildCreatureQueryResponse(stubPacket, stub);

    CHECK(stubPacket.size() > 5);       // a positive, not the 5-byte negative
    std::vector<uint8_t> name = { 'B','a','t','t','l','e',' ','P','e','t',' ','6','0','6','4','9', 0 };
    CHECK(PacketContains(stubPacket, name));
    std::vector<uint8_t> oneF = { 0x00, 0x00, 0x80, 0x3F };      // 1.0f multipliers
    CHECK(PacketContains(stubPacket, oneF));
    std::vector<uint8_t> typeTen = { 0x0A, 0x00, 0x00, 0x00 };   // creatureType = 10 (u32 LE)
    CHECK(PacketContains(stubPacket, typeTen));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_missing_template();
    test_minimal_hit();
    test_populated_hit();
    test_opcode_values_are_framable();
    test_synthetic_stub();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_creature_query: all checks passed\n");
    return 0;
}
