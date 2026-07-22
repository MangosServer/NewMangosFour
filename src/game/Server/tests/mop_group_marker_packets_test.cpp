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
 * Independent byte fixtures for the 5.4.8.18414 group-marker exchange.
 */

#include "Group.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cmath>
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

static void Append(WorldPacket& packet, std::vector<uint8> const& bytes)
{
    if (!bytes.empty())
        packet.append(bytes.data(), bytes.size());
}

static void test_minimap_ping_request()
{
    WorldPacket packet(CMSG_MINIMAP_PING, 9);
    Append(packet, {
        0x00, 0x00, 0x20, 0xC0, // Y = -2.5
        0x00, 0x00, 0xA0, 0x3F, // X = 1.25
        0x7F
    });

    MopGroupMarkerPackets::MinimapPingRequest const decoded =
        MopGroupMarkerPackets::ReadMinimapPingRequest(packet);
    CHECK(std::fabs(decoded.x - 1.25f) < 0.0001f);
    CHECK(std::fabs(decoded.y + 2.5f) < 0.0001f);
    CHECK(decoded.context == 0x7F);
}

static void test_minimap_ping_response()
{
    WorldPacket packet;
    MopGroupMarkerPackets::BuildMinimapPing(packet,
        0x0123456789ABCDEFull, 1.25f, -2.5f);

    CHECK(packet.GetOpcode() == SMSG_MINIMAP_PING);
    CHECK(Equal(packet, {
        0x00, 0x00, 0x20, 0xC0,
        0x00, 0x00, 0xA0, 0x3F,
        0xFF,
        0x22, 0x44, 0x00, 0xAA, 0xEE, 0x88, 0xCC, 0x66
    }));

    WorldPacket zero;
    MopGroupMarkerPackets::BuildMinimapPing(zero, 0, 0.0f, 0.0f);
    CHECK(Equal(zero, {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00
    }));

    WorldPacket sparse;
    MopGroupMarkerPackets::BuildMinimapPing(sparse,
        0x0000200000000010ull, 0.0f, 0.0f);
    CHECK(Equal(sparse, {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0xC0, 0x21, 0x11
    }));
}

static void test_raid_target_request()
{
    WorldPacket packet(CMSG_RAID_TARGET_UPDATE, 11);
    Append(packet, {
        0x01, 0x07, 0xFF,
        0xAA, 0x88, 0xEE, 0x00, 0x44, 0xCC, 0x22, 0x66
    });

    MopGroupMarkerPackets::RaidTargetRequest const decoded =
        MopGroupMarkerPackets::ReadRaidTargetRequest(packet);
    CHECK(decoded.context == 1);
    CHECK(decoded.icon == 7);
    CHECK(decoded.targetGuid == 0x0123456789ABCDEFull);

    WorldPacket clear(CMSG_RAID_TARGET_UPDATE, 3);
    Append(clear, { 0x7F, 0x03, 0x00 });
    MopGroupMarkerPackets::RaidTargetRequest const cleared =
        MopGroupMarkerPackets::ReadRaidTargetRequest(clear);
    CHECK(cleared.context == 0x7F);
    CHECK(cleared.icon == 3);
    CHECK(cleared.targetGuid == 0);

    WorldPacket sparse(CMSG_RAID_TARGET_UPDATE, 5);
    Append(sparse, { 0x00, 0x02, 0x84, 0x21, 0x41 });
    MopGroupMarkerPackets::RaidTargetRequest const sparseDecoded =
        MopGroupMarkerPackets::ReadRaidTargetRequest(sparse);
    CHECK(sparseDecoded.context == 0);
    CHECK(sparseDecoded.icon == 2);
    CHECK(sparseDecoded.targetGuid == 0x0040000020000000ull);
}

static void test_single_raid_target_response()
{
    WorldPacket packet;
    MopGroupMarkerPackets::BuildRaidTargetSingle(packet,
        0x0123456789ABCDEFull, 0x1020304050607080ull, 7, 1);

    CHECK(packet.GetOpcode() == SMSG_RAID_TARGET_UPDATE_SINGLE);
    CHECK(Equal(packet, {
        0xFF, 0xFF,
        0x71, 0x01, 0xEE, 0x44, 0x88, 0x11, 0x21, 0xCC,
        0x61, 0x41, 0x81, 0x51, 0x31, 0x22, 0x07, 0x66,
        0xAA, 0x00
    }));

    WorldPacket clear;
    MopGroupMarkerPackets::BuildRaidTargetSingle(clear, 0, 0, 3, 0x7F);
    CHECK(Equal(clear, { 0x00, 0x00, 0x7F, 0x03 }));

    WorldPacket sparse;
    MopGroupMarkerPackets::BuildRaidTargetSingle(sparse,
        0x0000200000000010ull, 0x0040000000003000ull, 4, 0);
    CHECK(Equal(sparse, {
        0x2C, 0x10, 0x31, 0x00, 0x11, 0x21, 0x41, 0x04
    }));
}

static void test_all_raid_targets_response()
{
    MopGroupMarkerPackets::TargetIcon target;
    target.icon = 7;
    target.targetGuid = 0x0123456789ABCDEFull;

    WorldPacket packet;
    CHECK(MopGroupMarkerPackets::BuildRaidTargetAll(packet, { target }, 0));
    CHECK(packet.GetOpcode() == SMSG_RAID_TARGET_UPDATE_ALL);
    CHECK(Equal(packet, {
        0x00, 0x00, 0xFF, 0x80,
        0x66, 0x00, 0xCC, 0xEE, 0x22, 0x44, 0x88, 0x07, 0xAA,
        0x00
    }));

    WorldPacket empty;
    CHECK(MopGroupMarkerPackets::BuildRaidTargetAll(empty, {}, 1));
    CHECK(Equal(empty, { 0x00, 0x00, 0x00, 0x01 }));

    MopGroupMarkerPackets::TargetIcon first;
    first.icon = 3;
    first.targetGuid = 0x0000200000100000ull;
    MopGroupMarkerPackets::TargetIcon second;
    second.icon = 4;
    second.targetGuid = 0x0000004000000030ull;

    WorldPacket sparse;
    CHECK(MopGroupMarkerPackets::BuildRaidTargetAll(sparse,
        { first, second }, 1));
    CHECK(Equal(sparse, {
        0x00, 0x01, 0x40, 0x83, 0x00,
        0x21, 0x03, 0x11,
        0x41, 0x31, 0x04,
        0x01
    }));
}

int main(int, char**)
{
    test_minimap_ping_request();
    test_minimap_ping_response();
    test_raid_target_request();
    test_single_raid_target_response();
    test_all_raid_targets_response();
    if (g_fail)
        return 1;
    std::printf("mop_group_marker_packets: all checks passed\n");
    return 0;
}
