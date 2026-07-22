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
 * Pins the four 5.4.8 server opcode names established by migration Wave 1.
 * Value and semantic-name provenance are recorded in
 * docs/opcode-migration/evidence/wave-1-foundation.md.
 */

#include "Opcodes.h"

#include <cstdint>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void CheckOpcode(std::uint32_t actual, std::uint32_t expected)
{
    CHECK(actual == expected);
    CHECK(actual <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    CheckOpcode(std::uint32_t(SMSG_SPELL_EXECUTE_LOG), 0x00D8u);
    CheckOpcode(std::uint32_t(SMSG_ATTACKSWING_ERROR), 0x11E1u);
    CheckOpcode(std::uint32_t(SMSG_RANDOM_ROLL), 0x141Au);
    CheckOpcode(std::uint32_t(SMSG_INSPECT_RATED_BG_STATS), 0x041Fu);

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_opcode_foundation: all checks passed\n");
    return 0;
}
