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
#include <cstring>

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

static void test_get_ticket_response()
{
    WorldPacket none;
    CHECK(MopGMTicketPackets::BuildGetTicket(none, nullptr, 0x0Au));
    uint8 const noneExpected[] = { 0x00, 0x0A,0x00,0x00,0x00 };
    CHECK(none.size() == sizeof(noneExpected));
    CHECK(std::memcmp(none.contents(), noneExpected, sizeof(noneExpected)) == 0);

    MopGMTicketPackets::TicketInfo ticket;
    ticket.ticketId = 0x11223344u;
    ticket.escalationStatus = 0xA1;
    ticket.openedByGm = 0xB2;
    ticket.category = 0xC3;
    ticket.mapId = 0xDDEEFF00u;
    ticket.message = "AB";
    ticket.unknownTime = 0x01020304u;
    ticket.oldestTicketTime = 0x55667788u;
    ticket.lastChangeTime = 0x99AABBCCu;
    ticket.waitTimeOverride = "C";

    WorldPacket present;
    CHECK(MopGMTicketPackets::BuildGetTicket(present, &ticket, 0x06u));
    uint8 const presentExpected[] = {
        0x80,0x20,0x04,
        0x44,0x33,0x22,0x11,
        0xA1,0xB2,0xC3,
        0x00,0xFF,0xEE,0xDD,
        0x41,0x42,
        0x04,0x03,0x02,0x01,
        0x88,0x77,0x66,0x55,
        0xCC,0xBB,0xAA,0x99,
        0x43,
        0x06,0x00,0x00,0x00
    };
    CHECK(present.size() == sizeof(presentExpected));
    CHECK(std::memcmp(present.contents(), presentExpected, sizeof(presentExpected)) == 0);
}

static void test_system_status_response()
{
    WorldPacket enabled;
    MopGMTicketPackets::BuildSystemStatus(enabled, true);
    uint8 const expected[] = { 0x01,0x00,0x00,0x00 };
    CHECK(enabled.GetOpcode() == SMSG_GMTICKET_SYSTEMSTATUS);
    CHECK(enabled.size() == sizeof(expected));
    CHECK(std::memcmp(enabled.contents(), expected, sizeof(expected)) == 0);
    CHECK(uint32(CMSG_GMTICKET_GETTICKET) == 0x1F89u);
    CHECK(uint32(SMSG_GMTICKET_GETTICKET) == 0x129Bu);
    CHECK(uint32(CMSG_GMTICKET_SYSTEMSTATUS) == 0x0A82u);
    CHECK(uint32(SMSG_GMTICKET_SYSTEMSTATUS) == 0x163Bu);
}

static void test_case_status_response()
{
    WorldPacket packet;
    MopGMTicketPackets::BuildCaseStatus(packet, 0x11223344u, 0x55667788u);
    uint8 const expected[] = {
        0x00,0x00,0x00,
        0x44,0x33,0x22,0x11,
        0x88,0x77,0x66,0x55
    };
    CHECK(packet.GetOpcode() == SMSG_GM_TICKET_CASE_STATUS);
    CHECK(packet.size() == sizeof(expected));
    CHECK(std::memcmp(packet.contents(), expected, sizeof(expected)) == 0);
    CHECK(uint32(CMSG_GM_UPDATE_TICKET_STATUS) == 0x15A8u);
    CHECK(uint32(SMSG_GM_TICKET_CASE_STATUS) == 0x148Eu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_response_values();
    test_opcode_is_framable();
    test_get_ticket_response();
    test_system_status_response();
    test_case_status_response();
    return g_fail ? 1 : 0;
}
