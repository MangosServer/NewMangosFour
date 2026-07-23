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
 * Independent byte fixtures for the 5.4.8.18414 death-release location.
 */

#include "Player.h"
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
        return false;

    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;
    return true;
}

static void test_graveyard_location()
{
    WorldPacket packet;
    MopDeathPackets::BuildDeathReleaseLocation(packet, 0x11223344u,
        1.0f, 2.0f, 3.0f);

    CHECK(packet.GetOpcode() == SMSG_DEATH_RELEASE_LOC);
    CHECK(ExpectBytes(packet, {
        0x44, 0x33, 0x22, 0x11,
        0x00, 0x00, 0x00, 0x40,
        0x00, 0x00, 0x80, 0x3F,
        0x00, 0x00, 0x40, 0x40
    }));
}

static void test_clear_location()
{
    WorldPacket packet;
    MopDeathPackets::BuildDeathReleaseLocation(packet, uint32(-1),
        0.0f, 0.0f, 0.0f);

    CHECK(ExpectBytes(packet, {
        0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }));
}

static void test_opcode_is_framable()
{
    CHECK(uint32(SMSG_DEATH_RELEASE_LOC) == 0x1063u);
    CHECK(uint32(SMSG_DEATH_RELEASE_LOC) < uint32(OPCODE_TABLE_SIZE));
}

static void test_empty_cemetery_list_response()
{
    WorldPacket packet;
    MopDeathPackets::BuildCemeteryListResponse(packet, {}, false);

    CHECK(packet.GetOpcode() == SMSG_REQUEST_CEMETERY_LIST_RESPONSE);
    CHECK(ExpectBytes(packet, { 0x00, 0x00, 0x00 }));
}

static void test_cemetery_list_response()
{
    std::vector<uint32> const cemeteryIds = { 0x11223344u, 0xA1B2C3D4u };

    WorldPacket scheduled;
    MopDeathPackets::BuildCemeteryListResponse(scheduled, cemeteryIds, false);
    CHECK(ExpectBytes(scheduled, {
        0x00, 0x00, 0x08,
        0x44, 0x33, 0x22, 0x11,
        0xD4, 0xC3, 0xB2, 0xA1
    }));

    WorldPacket gossip;
    MopDeathPackets::BuildCemeteryListResponse(gossip, cemeteryIds, true);
    CHECK(ExpectBytes(gossip, {
        0x00, 0x00, 0x0A,
        0x44, 0x33, 0x22, 0x11,
        0xD4, 0xC3, 0xB2, 0xA1
    }));
}

static void test_cemetery_list_response_is_bounded()
{
    std::vector<uint32> cemeteryIds;
    for (uint32 id = 1; id <= 17; ++id)
        cemeteryIds.push_back(id);

    WorldPacket packet;
    MopDeathPackets::BuildCemeteryListResponse(packet, cemeteryIds, false);

    std::vector<uint8_t> expected = { 0x00, 0x00, 0x40 };
    for (uint32 id = 1; id <= 16; ++id)
    {
        expected.push_back(uint8(id));
        expected.push_back(0x00);
        expected.push_back(0x00);
        expected.push_back(0x00);
    }
    CHECK(ExpectBytes(packet, expected));
}

static void test_cemetery_opcodes_are_framable()
{
    CHECK(uint32(CMSG_REQUEST_CEMETERY_LIST) == 0x06E4u);
    CHECK(uint32(SMSG_REQUEST_CEMETERY_LIST_RESPONSE) == 0x042Au);
    CHECK(uint32(CMSG_REQUEST_CEMETERY_LIST) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_REQUEST_CEMETERY_LIST_RESPONSE) < uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_graveyard_location();
    test_clear_location();
    test_opcode_is_framable();
    test_empty_cemetery_list_response();
    test_cemetery_list_response();
    test_cemetery_list_response_is_bounded();
    test_cemetery_opcodes_are_framable();
    return g_fail ? 1 : 0;
}
