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
 * Independent byte fixture for the 5.4.8.18414 rated-BG self-stat result.
 */

#include "Player.h"
#include "BattleGround.h"
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

static void test_fixed_four_by_eight_body()
{
    MopRatedBattlegroundPackets::RatedStats records{};
    uint32 value = 0xA0000001u;
    for (auto& record : records)
        for (uint32& field : record)
            field = value++;

    WorldPacket packet(SMSG_BATTLEFIELD_RATED_INFO, 128);
    MopRatedBattlegroundPackets::BuildBattlefieldRatedInfo(packet, records);
    CHECK(Equal(packet, {
        0x01,0x00,0x00,0xA0, 0x02,0x00,0x00,0xA0, 0x03,0x00,0x00,0xA0, 0x04,0x00,0x00,0xA0,
        0x05,0x00,0x00,0xA0, 0x06,0x00,0x00,0xA0, 0x07,0x00,0x00,0xA0, 0x08,0x00,0x00,0xA0,
        0x09,0x00,0x00,0xA0, 0x0A,0x00,0x00,0xA0, 0x0B,0x00,0x00,0xA0, 0x0C,0x00,0x00,0xA0,
        0x0D,0x00,0x00,0xA0, 0x0E,0x00,0x00,0xA0, 0x0F,0x00,0x00,0xA0, 0x10,0x00,0x00,0xA0,
        0x11,0x00,0x00,0xA0, 0x12,0x00,0x00,0xA0, 0x13,0x00,0x00,0xA0, 0x14,0x00,0x00,0xA0,
        0x15,0x00,0x00,0xA0, 0x16,0x00,0x00,0xA0, 0x17,0x00,0x00,0xA0, 0x18,0x00,0x00,0xA0,
        0x19,0x00,0x00,0xA0, 0x1A,0x00,0x00,0xA0, 0x1B,0x00,0x00,0xA0, 0x1C,0x00,0x00,0xA0,
        0x1D,0x00,0x00,0xA0, 0x1E,0x00,0x00,0xA0, 0x1F,0x00,0x00,0xA0, 0x20,0x00,0x00,0xA0
    }));
}

static void test_opcodes()
{
    CHECK(uint32(CMSG_REQUEST_RATED_BG_STATS) == 0x0826u);
    CHECK(uint32(SMSG_BATTLEFIELD_RATED_INFO) == 0x0EBAu);
    CHECK(uint32(CMSG_REQUEST_CONQUEST_FORMULA_CONSTANTS) == 0x0365u);
    CHECK(uint32(SMSG_CONQUEST_FORMULA_CONSTANTS) == 0x0EABu);
    CHECK(uint32(CMSG_REQUEST_RATED_BG_STATS) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_BATTLEFIELD_RATED_INFO) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(CMSG_REQUEST_CONQUEST_FORMULA_CONSTANTS) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_CONQUEST_FORMULA_CONSTANTS) < uint32(OPCODE_TABLE_SIZE));
}

static void test_conquest_formula_constants_body()
{
    WorldPacket packet;
    MopBattleGroundPackets::BuildConquestFormulaConstants(packet);

    CHECK(packet.GetOpcode() == SMSG_CONQUEST_FORMULA_CONSTANTS);
    CHECK(Equal(packet, {
        0xD0,0x07,0x00,0x00, // minimum weekly conquest points
        0xF6,0xE8,0xCC,0x44, // exponential coefficient: 1639.28f
        0x11,0x01,0x87,0x3B, // exponent numerator: 0.00412f
        0xAC,0x0D,0x00,0x00, // maximum weekly conquest points
        0x52,0xE8,0xBC,0x44  // base coefficient: 1511.26f
    }));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_fixed_four_by_eight_body();
    test_conquest_formula_constants_body();
    test_opcodes();
    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    std::printf("mop_rated_bg_packets: all checks passed\n");
    return 0;
}
