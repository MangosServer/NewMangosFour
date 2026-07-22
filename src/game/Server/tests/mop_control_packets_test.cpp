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
 * Byte-exact tests for the 5.4.8 client-control readers at 0x6F51AA and
 * 0xC8EB90.
 */

#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
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

static void test_client_control_update()
{
    {
        WorldPacket packet(SMSG_CLIENT_CONTROL_UPDATE, 10);
        MopControlPackets::BuildClientControlUpdate(packet, 0x8070605040302010ull, true);
        CHECK(ExpectBytes(packet, {
            0xFF, 0x80, 0x21, 0x61, 0x81, 0x51, 0x31, 0x71, 0x41, 0x11
        }));
    }
    {
        WorldPacket packet(SMSG_CLIENT_CONTROL_UPDATE, 10);
        MopControlPackets::BuildClientControlUpdate(packet, 0x8070605040302010ull, false);
        CHECK(ExpectBytes(packet, {
            0xDF, 0x80, 0x21, 0x61, 0x81, 0x51, 0x31, 0x71, 0x41, 0x11
        }));
    }
    {
        WorldPacket packet(SMSG_CLIENT_CONTROL_UPDATE, 5);
        MopControlPackets::BuildClientControlUpdate(packet, 0x0070000040000010ull, true);
        CHECK(ExpectBytes(packet, { 0x3C, 0x00, 0x71, 0x41, 0x11 }));
    }
    {
        WorldPacket packet(SMSG_CLIENT_CONTROL_UPDATE, 2);
        MopControlPackets::BuildClientControlUpdate(packet, 0, true);
        CHECK(ExpectBytes(packet, { 0x20, 0x00 }));
    }
    {
        WorldPacket packet(SMSG_CLIENT_CONTROL_UPDATE, 2);
        MopControlPackets::BuildClientControlUpdate(packet, 0, false);
        CHECK(ExpectBytes(packet, { 0x00, 0x00 }));
    }
}

static void test_set_active_mover()
{
    {
        WorldPacket packet(SMSG_MOVE_SET_ACTIVE_MOVER, 9);
        MopControlPackets::BuildSetActiveMover(packet, 0x8070605040302010ull);
        CHECK(ExpectBytes(packet, {
            0xFF, 0x51, 0x71, 0x31, 0x11, 0x41, 0x81, 0x61, 0x21
        }));
    }
    {
        WorldPacket packet(SMSG_MOVE_SET_ACTIVE_MOVER, 4);
        MopControlPackets::BuildSetActiveMover(packet, 0x0070000040000010ull);
        CHECK(ExpectBytes(packet, { 0x0B, 0x71, 0x11, 0x41 }));
    }
    {
        WorldPacket packet(SMSG_MOVE_SET_ACTIVE_MOVER, 1);
        MopControlPackets::BuildSetActiveMover(packet, 0);
        CHECK(ExpectBytes(packet, { 0x00 }));
    }
}

static void test_client_move_time_skipped()
{
    ObjectGuid guid(0x8070605040302010ull);
    WorldPacket packet(CMSG_MOVE_TIME_SKIPPED, 13);
    packet << uint32(0x11223344u);
    packet.WriteGuidMask<5, 0, 7, 4, 1, 2, 6, 3>(guid);
    packet.FlushBits();
    packet.WriteGuidBytes<7, 2, 0, 6, 1, 5, 3, 4>(guid);

    MopControlPackets::MoveTimeSkippedRequest request =
        MopControlPackets::ReadMoveTimeSkipped(packet);
    CHECK(request.timeSkipped == 0x11223344u);
    CHECK(request.moverGuid == guid.GetRawValue());
    CHECK(packet.rpos() == packet.size());
}

static void test_client_set_active_mover()
{
    ObjectGuid guid(0x8070605040302010ull);
    WorldPacket packet(CMSG_SET_ACTIVE_MOVER, 10);
    packet.WriteBit(false); // GUID is not zero
    packet.WriteGuidMask<3, 0, 2, 1, 5, 4, 7, 6>(guid);
    packet.FlushBits();
    packet.WriteGuidBytes<3, 4, 5, 2, 7, 0, 1, 6>(guid);

    MopControlPackets::ActiveMoverRequest request;
    CHECK(MopControlPackets::ReadSetActiveMover(packet, request));
    CHECK(request.moverGuid == guid.GetRawValue());
    CHECK(packet.rpos() == packet.size());

    WorldPacket malformed(CMSG_SET_ACTIVE_MOVER, 2);
    malformed.WriteBit(false); // says nonzero, but all GUID mask bits are clear
    for (uint8 i = 0; i < 8; ++i)
        malformed.WriteBit(false);
    malformed.FlushBits();
    CHECK(!MopControlPackets::ReadSetActiveMover(malformed, request));
}

static void test_opcode_values_are_framable()
{
    CHECK(uint32_t(SMSG_CLIENT_CONTROL_UPDATE) == 0x1043u);
    CHECK(uint32_t(SMSG_MOVE_SET_ACTIVE_MOVER) == 0x0C6Du);
    CHECK(uint32_t(SMSG_CLIENT_CONTROL_UPDATE) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_MOVE_SET_ACTIVE_MOVER) <= 0x1FFFu);
    CHECK(uint32_t(CMSG_MOVE_TIME_SKIPPED) == 0x0150u);
    CHECK(uint32_t(CMSG_SET_ACTIVE_MOVER) == 0x09F0u);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_client_control_update();
    test_set_active_mover();
    test_client_move_time_skipped();
    test_client_set_active_mover();
    test_opcode_values_are_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_control_packets: all checks passed\n");
    return 0;
}
