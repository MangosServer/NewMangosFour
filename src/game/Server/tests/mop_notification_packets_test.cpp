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

#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
        return false;

    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;

    return true;
}

static WorldPacket Build(char const* text, size_t length)
{
    WorldPacket packet(SMSG_NOTIFICATION, 2 + length);
    packet.WriteBits(length, 12);
    packet.FlushBits();
    packet.append(text, length);
    return packet;
}

int main(int, char**)
{
    CHECK(uint32(SMSG_NOTIFICATION) == 0x0C2Au);
    CHECK(Equal(Build("abc", 3), { 0x00, 0x30, 'a', 'b', 'c' }));

    std::vector<uint8_t> expected = { 0x3F, 0xF0 };
    expected.insert(expected.end(), 1023, uint8_t('X'));
    std::string longest(1023, 'X');
    CHECK(Equal(Build(longest.data(), longest.size()), expected));

    return g_fail ? 1 : 0;
}
