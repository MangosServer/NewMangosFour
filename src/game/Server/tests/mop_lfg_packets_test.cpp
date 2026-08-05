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
 * Independent byte fixtures for the 5.4.8.18414 LFG boot-vote packet.
 */

#include "LFGMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    bool const equal = packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
    if (!equal)
    {
        std::fprintf(stderr, "actual (%zu):", packet.size());
        for (size_t i = 0; i < packet.size(); ++i)
            std::fprintf(stderr, " %02X", packet.contents()[i]);
        std::fprintf(stderr, "\n");
    }
    return equal;
}

static MopLfgPackets::StatusUpdate StatusFixture()
{
    MopLfgPackets::StatusUpdate update;
    update.requesterGuid = 0x0807060504030201ULL;
    update.suspendedPlayerGuids.push_back(0x100F0E0D0C0B0A09ULL);
    update.dungeonEntries.push_back(0x01020304u);
    update.dungeonEntries.push_back(0xA1A2A3A4u);
    update.comment = "AB";
    update.needs = {{ 1, 2, 3 }};
    update.isParty = true;
    update.joined = true;
    update.notifyUi = true;
    update.lfgJoined = true;
    update.queued = false;
    update.updateReason = 0x11;
    update.requestedRoles = 0x22334455u;
    update.ticketId = 0x66778899u;
    update.ticketTime = 0xAABBCCDDu;
    update.dungeonCategory = 0x0Du;
    update.ticketType = 3;
    return update;
}

static void test_update_status_exact_fixture()
{
    WorldPacket packet(SMSG_LFG_UPDATE_STATUS, 80);
    CHECK(MopLfgPackets::BuildUpdateStatus(packet, StatusFixture()));

    // Direct 18414 reader order: selector 298, reader 0x71EF25.
    CHECK(Equal(packet, {
        0x02,0xC0,0x00,0x02,0xFF,0x00,0x00,0x00,0xFF,0xE0,
        0x05,0x01,0x02,0x03,0x04,
        0x11,0x08,0x0B,0x0E,0x0C,0x0F,0x0A,0x0D,
        0x06,0x11,
        0x55,0x44,0x33,0x22,
        0x99,0x88,0x77,0x66,
        0x07,0x41,0x42,0x02,
        0x04,0x03,0x02,0x01,
        0xA4,0xA3,0xA2,0xA1,
        0x00,0x03,
        0xDD,0xCC,0xBB,0xAA,
        0x0D,
        0x03,0x00,0x00,0x00,
        0x09
    }));
}

static void test_queue_status_exact_fixture()
{
    MopLfgPackets::QueueStatusUpdate update;
    update.queueGuid = 0x0807060504030201ULL;
    update.flags = 3;
    update.queuedTime = 0x11223344;
    update.waitTimeAvg = 0x01020304;
    update.waitTimeTank = 0x11121314;
    update.tanks = 1;
    update.waitTimeHealer = 0x21222324;
    update.healers = 2;
    update.waitTimeDps = 0x31323334;
    update.dps = 3;
    update.joinTime = 0x41424344;
    update.clientQueueId = 0x51525354;
    update.waitTime = 0x61626364;
    update.dungeonEntry = 0x71727374;

    WorldPacket packet(SMSG_LFG_QUEUE_STATUS, 52);
    MopLfgPackets::BuildQueueStatus(packet, update);
    CHECK(Equal(packet, {
        0xFF,
        0x03,0x00,0x00,0x00, 0x00,
        0x44,0x33,0x22,0x11, 0x04,
        0x04,0x03,0x02,0x01,
        0x14,0x13,0x12,0x11, 0x01,
        0x24,0x23,0x22,0x21, 0x02,
        0x34,0x33,0x32,0x31, 0x03,
        0x44,0x43,0x42,0x41,
        0x54,0x53,0x52,0x51, 0x03,
        0x64,0x63,0x62,0x61, 0x09,0x02,
        0x74,0x73,0x72,0x71, 0x07,0x05,0x06
    }));
}

/// SMSG_LFG_JOIN_RESULT, pinned to the three real payload shapes in the corpus
/// (build 18414, catalogue 2BE10C89). These are captured bytes, not synthesised
/// ones: the previous body was the 3.3.5 layout and would fail every one of them
/// on the first byte, so a shape-only test would not have caught it.
///
/// The GUID mask is SPLIT either side of the 22-bit lock count, which is what makes
/// the length identity len == 18 + popcount(byte0) + popcount(byte3).
static void test_join_result_refusal_fixture()
{
    // capture-000059 seq 490545: role check failed, detail 6 (LFG_ROLECHECK_NO_ROLE).
    // A refusal zeroes the GUID and the entire ticket -- that is what makes it 18 bytes.
    MopLfgPackets::JoinResult update;
    update.result = 0x1C;
    update.detail = 6;

    WorldPacket packet(SMSG_LFG_JOIN_RESULT, 24);
    MopLfgPackets::BuildJoinResult(packet, update);
    CHECK(Equal(packet, {
        0x00,0x00,0x00,0x00,
        0x1C, 0x06,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00
    }));
}

static void test_join_result_success_23_fixture()
{
    // capture-000044 seq 1547. The ticket here is the SAME one carried by
    // SMSG_LFG_QUEUE_STATUS seq 1577 in this capture: joinTime 0x54146107,
    // queueId 0x9BFF. Three of the eight GUID bytes are zero, hence 23 not 26.
    MopLfgPackets::JoinResult update;
    update.requesterGuid = 0x0400000006296291ULL;
    update.joinTime = 0x54146107;
    update.clientQueueId = 0x9BFF;
    update.ticketType = 3;

    WorldPacket packet(SMSG_LFG_JOIN_RESULT, 32);
    MopLfgPackets::BuildJoinResult(packet, update);
    CHECK(Equal(packet, {
        0xB0, 0x00, 0x00, 0x14,
        0x00, 0x00,
        0x28,
        0x07, 0x61, 0x14, 0x54,
        0xFF, 0x9B, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00,
        0x63, 0x90, 0x05, 0x07
    }));
}

static void test_join_result_success_24_fixture()
{
    // capture-000075 seq 891753: a different GUID with one more non-zero byte.
    MopLfgPackets::JoinResult update;
    update.requesterGuid = 0x1F5400001249B4F0ULL;
    update.joinTime = 0x53D28F06;
    update.clientQueueId = 0x4692;
    update.ticketType = 3;

    WorldPacket packet(SMSG_LFG_JOIN_RESULT, 32);
    MopLfgPackets::BuildJoinResult(packet, update);
    CHECK(Equal(packet, {
        0xF0, 0x00, 0x00, 0x14,
        0x00, 0x00,
        0x48,
        0x06, 0x8F, 0xD2, 0x53,
        0x92, 0x46, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00,
        0x55, 0xB5, 0xF1, 0x1E, 0x13
    }));
}

static void test_lock_info_request()
{
    WorldPacket player(CMSG_LFG_LOCK_INFO_REQUEST, 2);
    player << uint8(0x7F);
    player.WriteBit(true);
    player.FlushBits();
    CHECK(Equal(player, { 0x7F,0x80 }));

    bool forPlayer = false;
    CHECK(MopLfgPackets::ParseLockInfoRequest(player, forPlayer));
    CHECK(forPlayer);
    CHECK(player.rpos() == player.size());

    WorldPacket party(CMSG_LFG_LOCK_INFO_REQUEST, 2);
    party << uint8(0x7F);
    party.WriteBit(false);
    party.FlushBits();
    CHECK(Equal(party, { 0x7F,0x00 }));

    forPlayer = true;
    CHECK(MopLfgPackets::ParseLockInfoRequest(party, forPlayer));
    CHECK(!forPlayer);
    CHECK(party.rpos() == party.size());
}

static WorldPacket LfrRequestPacket(std::initializer_list<uint8> bytes)
{
    WorldPacket packet(CMSG_LFG_LFR_JOIN, bytes.size());
    for (uint8 byte : bytes)
        packet << byte;
    return packet;
}

static void test_lfr_search_request()
{
    WorldPacket packet = LfrRequestPacket({ 0x45, 0x23, 0x01, 0x03 });
    MopLfgPackets::LfrSearchRequest request;
    CHECK(MopLfgPackets::ParseLfrSearchRequest(packet, request));
    CHECK(request.lfgId == 0x12345u);
    CHECK(request.typeId == 3u);
    CHECK(packet.rpos() == packet.size());
}

int main(int /*argc*/, char** /*argv*/)
{
    test_update_status_exact_fixture();
    test_queue_status_exact_fixture();
    test_join_result_refusal_fixture();
    test_join_result_success_23_fixture();
    test_join_result_success_24_fixture();
    test_lock_info_request();
    test_lfr_search_request();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_lfg_packets: all checks passed\n");
    return 0;
}
