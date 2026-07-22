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
 * Independent byte fixtures for the 5.4.8.18414 completed-quest protocol.
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

static void test_initial_setup_with_epoch()
{
    MopQuestPackets::CompletedQuestBits bits{};
    CHECK(MopQuestPackets::SetCompletedQuestBit(bits, 1));
    CHECK(MopQuestPackets::SetCompletedQuestBit(bits, 10));
    CHECK(MopQuestPackets::SetCompletedQuestBit(bits, 16384));
    CHECK(!MopQuestPackets::SetCompletedQuestBit(bits, 0));
    CHECK(!MopQuestPackets::SetCompletedQuestBit(bits, 16385));

    WorldPacket packet;
    MopQuestPackets::BuildInitialSetup(packet, bits, 3, 0,
        true, 1135753200u, 4);

    CHECK(packet.GetOpcode() == SMSG_INITIAL_SETUP);
    CHECK(packet.size() == 2062);
    CHECK(packet.contents()[0] == 0x00);
    CHECK(packet.contents()[1] == 0x04);
    CHECK(packet.contents()[2] == 0x00);
    CHECK(packet.contents()[3] == 0x00);
    CHECK(packet.contents()[4] == 0x03);
    CHECK(packet.contents()[5] == 0x00);
    CHECK(packet.contents()[6] == 0x00);
    CHECK(packet.contents()[7] == 0x00);
    CHECK(packet.contents()[8] == 0x00);
    CHECK(packet.contents()[9] == 0xF0);
    CHECK(packet.contents()[10] == 0x37);
    CHECK(packet.contents()[11] == 0xB2);
    CHECK(packet.contents()[12] == 0x43);
    CHECK(packet.contents()[13] == 0x01);
    CHECK(packet.contents()[14] == 0x02);
    for (size_t i = 15; i < 2060; ++i)
        CHECK(packet.contents()[i] == 0x00);
    CHECK(packet.contents()[2060] == 0x80);
    CHECK(packet.contents()[2061] == 0x04);
}

static void test_initial_setup_without_epoch()
{
    MopQuestPackets::CompletedQuestBits bits{};
    WorldPacket packet;
    MopQuestPackets::BuildInitialSetup(packet, bits, 1, 2,
        false, 0, 4);

    CHECK(packet.GetOpcode() == SMSG_INITIAL_SETUP);
    CHECK(packet.size() == 2058);
    CHECK(packet.contents()[0] == 0x80);
    CHECK(packet.contents()[1] == 0x04);
    CHECK(packet.contents()[2] == 0x00);
    CHECK(packet.contents()[3] == 0x00);
    CHECK(packet.contents()[4] == 0x01);
    CHECK(packet.contents()[8] == 0x02);
    for (size_t i = 9; i < 2057; ++i)
        CHECK(packet.contents()[i] == 0x00);
    CHECK(packet.contents()[2057] == 0x04);
}

static void test_single_bit_mutations()
{
    WorldPacket set;
    CHECK(MopQuestPackets::BuildSetQuestCompletedBit(set, 0x1234));
    CHECK(set.GetOpcode() == SMSG_SET_QUEST_COMPLETED_BIT);
    CHECK(ExpectBytes(set, { 0x34, 0x12, 0x00, 0x00 }));

    WorldPacket clear;
    CHECK(MopQuestPackets::BuildClearQuestCompletedBit(clear, 16384));
    CHECK(clear.GetOpcode() == SMSG_CLEAR_QUEST_COMPLETED_BIT);
    CHECK(ExpectBytes(clear, { 0x00, 0x40, 0x00, 0x00 }));

    WorldPacket clearAll;
    CHECK(MopQuestPackets::BuildClearQuestCompletedBit(clearAll, 0));
    CHECK(ExpectBytes(clearAll, { 0x00, 0x00, 0x00, 0x00 }));

    WorldPacket invalidSet;
    CHECK(!MopQuestPackets::BuildSetQuestCompletedBit(invalidSet, 0));
    CHECK(invalidSet.empty());
    WorldPacket invalidClear;
    CHECK(!MopQuestPackets::BuildClearQuestCompletedBit(invalidClear, 16385));
    CHECK(invalidClear.empty());
}

static void test_many_bit_clear()
{
    WorldPacket packet;
    CHECK(MopQuestPackets::BuildClearQuestCompletedBits(packet,
        { 1, 0x1234 }));
    CHECK(packet.GetOpcode() == SMSG_CLEAR_QUEST_COMPLETED_BITS);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x08,
        0x01, 0x00, 0x00, 0x00,
        0x34, 0x12, 0x00, 0x00
    }));

    WorldPacket invalid;
    CHECK(!MopQuestPackets::BuildClearQuestCompletedBits(invalid,
        { 1, 0 }));
    CHECK(invalid.empty());
}

static void test_opcode_values()
{
    CHECK(uint32(SMSG_INITIAL_SETUP) == 0x0A8Bu);
    CHECK(uint32(SMSG_SET_QUEST_COMPLETED_BIT) == 0x0354u);
    CHECK(uint32(SMSG_CLEAR_QUEST_COMPLETED_BIT) == 0x03ECu);
    CHECK(uint32(SMSG_CLEAR_QUEST_COMPLETED_BITS) == 0x0364u);
    CHECK(uint32(SMSG_INITIAL_SETUP) < uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_initial_setup_with_epoch();
    test_initial_setup_without_epoch();
    test_single_bit_mutations();
    test_many_bit_clear();
    test_opcode_values();
    return g_fail ? 1 : 0;
}
