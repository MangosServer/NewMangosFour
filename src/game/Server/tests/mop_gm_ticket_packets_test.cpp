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
#include <vector>

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

static WorldPacket make_create_request(std::vector<uint8> const& body)
{
    WorldPacket packet(CMSG_GMTICKET_CREATE, body.size());
    if (!body.empty())
        packet.append(body.data(), body.size());
    return packet;
}

static void test_create_request()
{
    std::vector<uint8> const body = {
        0x44,0x33,0x22,0x11, // map ID
        0x00,0x00,0xA0,0x3F, // Z = 1.25
        0x00,0x00,0x20,0xC0, // Y = -2.5
        0x5A,                // category / unknown
        0x00,0x00,0x70,0x40, // X = 3.75
        0x00,0x00,0x00,0x00, // no chat attachment
        0x80,0x10,           // need response, not follow-up, 11-bit length 2
        'O','K'
    };
    WorldPacket packet = make_create_request(body);
    MopGMTicketPackets::CreateRequest request;

    CHECK(MopGMTicketPackets::ReadCreateRequest(packet, request));
    CHECK(request.mapId == 0x11223344u);
    CHECK(request.x == 3.75f);
    CHECK(request.y == -2.5f);
    CHECK(request.z == 1.25f);
    CHECK(request.category == 0x5Au);
    CHECK(request.attachmentSize == 0u);
    CHECK(request.needResponse);
    CHECK(!request.isFollowup);
    CHECK(request.message == "OK");
    CHECK(packet.rpos() == packet.size());
    CHECK(uint32(CMSG_GMTICKET_CREATE) == 0x1A86u);
    CHECK(uint32(CMSG_GMTICKET_CREATE) < uint32(OPCODE_TABLE_SIZE));
}

static void test_create_request_attachment()
{
    std::vector<uint8> const body = {
        0x01,0x00,0x00,0x00,
        0x00,0x00,0x80,0x3F,
        0x00,0x00,0x00,0x40,
        0x7F,
        0x00,0x00,0x40,0x40,
        0x03,0x00,0x00,0x00,
        0xAA,0xBB,0xCC,
        0x40,0x08, // no response, follow-up, 11-bit length 1
        'X'
    };
    WorldPacket packet = make_create_request(body);
    MopGMTicketPackets::CreateRequest request;

    CHECK(MopGMTicketPackets::ReadCreateRequest(packet, request));
    CHECK(request.attachmentSize == 3u);
    CHECK(!request.needResponse);
    CHECK(request.isFollowup);
    CHECK(request.message == "X");
    CHECK(packet.rpos() == packet.size());
}

static void test_create_request_11_bit_message_length()
{
    std::vector<uint8> body = {
        0x01,0x00,0x00,0x00,
        0x00,0x00,0x80,0x3F,
        0x00,0x00,0x00,0x40,
        0x7F,
        0x00,0x00,0x40,0x40,
        0x00,0x00,0x00,0x00,
        0xA0,0x08 // need response, not follow-up, literal 11-bit length 1025
    };
    body.insert(body.end(), 1025, uint8('Q'));
    WorldPacket packet = make_create_request(body);
    MopGMTicketPackets::CreateRequest request;

    CHECK(MopGMTicketPackets::ReadCreateRequest(packet, request));
    CHECK(request.message == std::string(1025, 'Q'));
    CHECK(packet.rpos() == packet.size());
}

static void check_create_request_rejected(std::vector<uint8> const& body)
{
    WorldPacket packet = make_create_request(body);
    MopGMTicketPackets::CreateRequest request;
    request.message = "stale";
    CHECK(!MopGMTicketPackets::ReadCreateRequest(packet, request));
    CHECK(request.message.empty());
    CHECK(packet.rpos() == packet.size());
}

static void test_create_request_rejects_malformed_bodies()
{
    check_create_request_rejected({
        0x44,0x33,0x22,0x11, 0x00,0x00,0xA0,0x3F,
        0x00,0x00,0x20,0xC0, 0x5A, 0x00,0x00,0x70,0x40,
        0x00,0x00,0x00 // fixed prefix truncated by one byte
    });
    check_create_request_rejected({
        0x44,0x33,0x22,0x11, 0x00,0x00,0xA0,0x3F,
        0x00,0x00,0x20,0xC0, 0x5A, 0x00,0x00,0x70,0x40,
        0x04,0x00,0x00,0x00, 0xAA,0xBB,0xCC // attachment overruns tail
    });
    check_create_request_rejected({
        0x44,0x33,0x22,0x11, 0x00,0x00,0xA0,0x3F,
        0x00,0x00,0x20,0xC0, 0x5A, 0x00,0x00,0x70,0x40,
        0x00,0x00,0x00,0x00, 0x80 // 13-bit header truncated
    });
    check_create_request_rejected({
        0x44,0x33,0x22,0x11, 0x00,0x00,0xA0,0x3F,
        0x00,0x00,0x20,0xC0, 0x5A, 0x00,0x00,0x70,0x40,
        0x00,0x00,0x00,0x00, 0x80,0x18, 'O','K' // length 3, tail 2
    });
    check_create_request_rejected({
        0x44,0x33,0x22,0x11, 0x00,0x00,0xA0,0x3F,
        0x00,0x00,0x20,0xC0, 0x5A, 0x00,0x00,0x70,0x40,
        0x00,0x00,0x00,0x00, 0x80,0x10, 'O','K','X' // trailing byte
    });
}

int main(int /*argc*/, char** /*argv*/)
{
    test_response_values();
    test_opcode_is_framable();
    test_get_ticket_response();
    test_system_status_response();
    test_case_status_response();
    test_create_request();
    test_create_request_attachment();
    test_create_request_11_bit_message_length();
    test_create_request_rejects_malformed_bodies();
    return g_fail ? 1 : 0;
}
