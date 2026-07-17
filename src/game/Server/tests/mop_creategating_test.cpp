/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "MopCreateGating.h"
#include <cstdio>

// RelWithDebInfo elides assert(); use a non-elidable check (mirrors mop_charenum_test).
static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

// Expected required-expansion values are asserted as LITERALS (0=classic,1=TBC,2=WotLK,3=Cata,
// 4=MoP) so the test independently pins the game facts, not just the enum names used in the impl.

int main(int /*argc*/, char** /*argv*/)
{
    using namespace MopCreateGating;

    // Races are NOT expansion-gated in 5.4.8: patch 5.0.4 made every race -- Pandaren included --
    // creatable at any expansion, so MopCreateGating exposes no race function. Nothing to assert for
    // races; only classes carry a requirement.

    // --- Class expansion requirements ---
    CHECK(ClassRequiredExpansion(1)  == 0);  // Warrior
    CHECK(ClassRequiredExpansion(2)  == 0);  // Paladin
    CHECK(ClassRequiredExpansion(3)  == 0);  // Hunter
    CHECK(ClassRequiredExpansion(5)  == 0);  // Priest
    CHECK(ClassRequiredExpansion(11) == 0);  // Druid
    CHECK(ClassRequiredExpansion(6)  == 2);  // DeathKnight -> WotLK
    CHECK(ClassRequiredExpansion(10) == 4);  // Monk        -> MoP

    // --- Gate semantics: a lower-expansion account is blocked from the gated classes only. ---
    const uint8 classicAccount = 0;
    const uint8 mopAccount     = 4;
    CHECK(ClassRequiredExpansion(10) > classicAccount);    // Monk blocked on a Classic account
    CHECK(!(ClassRequiredExpansion(10) > mopAccount));     // Monk allowed on a MoP account
    CHECK(ClassRequiredExpansion(6)  > classicAccount);    // DeathKnight blocked on a Classic account
    CHECK(!(ClassRequiredExpansion(6)  > mopAccount));     // DeathKnight allowed on a MoP account
    CHECK(!(ClassRequiredExpansion(1)  > classicAccount)); // Warrior always allowed

    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURES\n", g_failures);
    return 1;
}
