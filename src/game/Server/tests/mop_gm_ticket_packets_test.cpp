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
 * Independent byte fixtures for the 5.4.8.18414 GM-ticket update response.
 */

#include "GMTicketMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_response_values()
{
    GMTicketResponse const responses[] = {
        GMTICKET_RESPONSE_ALREADY_EXIST,
        GMTICKET_RESPONSE_CREATE_SUCCESS,
        GMTICKET_RESPONSE_CREATE_ERROR,
        GMTICKET_RESPONSE_UPDATE_SUCCESS,
        GMTICKET_RESPONSE_UPDATE_ERROR,
        GMTICKET_RESPONSE_TICKET_DELETED
    };
    uint8 const expected[] = { 1, 2, 3, 4, 5, 9 };

    for (size_t i = 0; i < sizeof(responses) / sizeof(responses[0]); ++i)
    {
        WorldPacket packet;
        MopGMTicketPackets::BuildUpdate(packet, responses[i]);
        CHECK(packet.GetOpcode() == SMSG_GM_TICKET_UPDATE);
        CHECK(packet.size() == 1);
        CHECK(packet.contents()[0] == expected[i]);
    }
}

static void test_opcode_is_framable()
{
    CHECK(uint32(SMSG_GM_TICKET_UPDATE) == 0x02A6u);
    CHECK(uint32(SMSG_GM_TICKET_UPDATE) < uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_response_values();
    test_opcode_is_framable();
    return g_fail ? 1 : 0;
}
