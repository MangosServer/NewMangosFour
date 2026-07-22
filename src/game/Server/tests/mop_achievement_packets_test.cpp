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
 * Byte-exact tests for the 5.4.8 ALL_ACHIEVEMENT_DATA packet body.
 */

#include "AchievementMgr.h"
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

static void test_empty_lists()
{
    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 5);
    MopAchievementPackets::BuildAllAchievementData(packet, {}, {});
    CHECK(ExpectBytes(packet, { 0x00, 0x00, 0x00, 0x00, 0x00 }));
}

static void test_completed_sparse_guid()
{
    std::vector<MopAchievementPackets::CompletedAchievement> completed = {
        { 0x11223344u, UINT64_C(0x0077005500330011), 0xDDEEFF00u,
          0x55667788u, 0x99AABBCCu }
    };

    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 26);
    MopAchievementPackets::BuildAllAchievementData(packet, completed, {});
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x00, 0x00, 0x03, 0x1C,
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99, 0x00, 0xFF, 0xEE, 0xDD,
        0x10, 0x54, 0x76, 0x32
    }));
}

static void test_progress_sparse_guids()
{
    std::vector<MopAchievementPackets::CriteriaProgress> progress = {
        { 0x12345678u, UINT64_C(0x8800660044002200),
          UINT64_C(0x0077005500330011), 0x90ABCDEFu,
          0x10203040u, 0x50607080u }
    };

    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 32);
    MopAchievementPackets::BuildAllAchievementData(packet, {}, progress);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x34, 0xEE, 0x00, 0x00, 0x00, 0x00,
        0x89, 0x40, 0x30, 0x20, 0x10, 0x78, 0x56, 0x34,
        0x12, 0x10, 0x54, 0x76, 0x23, 0x67, 0x32, 0x45,
        0x80, 0x70, 0x60, 0x50, 0xEF, 0xCD, 0xAB, 0x90
    }));
}

static void test_completed_bytes_precede_progress_bytes_with_zero_guids()
{
    std::vector<MopAchievementPackets::CompletedAchievement> completed = {
        { 0x11223344u, 0, 0xDDEEFF00u, 0x55667788u, 0x99AABBCCu }
    };
    std::vector<MopAchievementPackets::CriteriaProgress> progress = {
        { 0x12345678u, 0, 0, 0x90ABCDEFu, 0x10203040u, 0x50607080u }
    };

    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 41);
    MopAchievementPackets::BuildAllAchievementData(packet, completed, progress);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00,
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99, 0x00, 0xFF, 0xEE, 0xDD,
        0x40, 0x30, 0x20, 0x10, 0x78, 0x56, 0x34, 0x12,
        0x80, 0x70, 0x60, 0x50, 0xEF, 0xCD, 0xAB, 0x90
    }));
}

static void test_all_guid_bytes_nonzero()
{
    std::vector<MopAchievementPackets::CompletedAchievement> completed = {
        { 0x01020304u, UINT64_C(0x0807060504030201), 0x31323334u,
          0x11121314u, 0x21222324u }
    };
    std::vector<MopAchievementPackets::CriteriaProgress> progress = {
        { 0x41424344u, UINT64_C(0x100F0E0D0C0B0A09),
          UINT64_C(0x1817161514131211), 0x51525354u,
          0x61626364u, 0x71727374u }
    };

    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 65);
    MopAchievementPackets::BuildAllAchievementData(packet, completed, progress);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x3F, 0xFF, 0xC2, 0x00, 0x00, 0x3F, 0xE0,
        0x04, 0x03, 0x02, 0x01, 0x14, 0x13, 0x12, 0x11, 0x07,
        0x09, 0x24, 0x23, 0x22, 0x21, 0x34, 0x33, 0x32, 0x31,
        0x00, 0x04, 0x03, 0x06, 0x02, 0x05,
        0x11, 0x64, 0x63, 0x62, 0x61, 0x0E, 0x13,
        0x44, 0x43, 0x42, 0x41, 0x0C, 0x10, 0x14, 0x16,
        0x0B, 0x0F, 0x19, 0x12, 0x0A, 0x08, 0x15, 0x0D,
        0x74, 0x73, 0x72, 0x71, 0x17, 0x54, 0x53, 0x52, 0x51
    }));
}

static void test_opcode_value_is_framable()
{
    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 5);
    MopAchievementPackets::BuildAllAchievementData(packet, {}, {});
    CHECK(uint32_t(SMSG_ALL_ACHIEVEMENT_DATA) == 0x180Au);
    CHECK(uint32_t(packet.GetOpcode()) == 0x180Au);
    CHECK(uint32_t(packet.GetOpcode()) <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_empty_lists();
    test_completed_sparse_guid();
    test_progress_sparse_guids();
    test_completed_bytes_precede_progress_bytes_with_zero_guids();
    test_all_guid_bytes_nonzero();
    test_opcode_value_is_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_achievement_packets: all checks passed\n");
    return 0;
}
