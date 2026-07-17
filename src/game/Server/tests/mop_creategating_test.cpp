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

    // --- Races ---
    // Classic (0): representative sample across both factions.
    CHECK(RaceRequiredExpansion(1)  == 0);   // Human
    CHECK(RaceRequiredExpansion(2)  == 0);   // Orc
    CHECK(RaceRequiredExpansion(3)  == 0);   // Dwarf   (this is the one the old DBC gate rejected: enemyRace=8)
    CHECK(RaceRequiredExpansion(4)  == 0);   // NightElf
    CHECK(RaceRequiredExpansion(5)  == 0);   // Undead
    CHECK(RaceRequiredExpansion(6)  == 0);   // Tauren
    CHECK(RaceRequiredExpansion(7)  == 0);   // Gnome
    CHECK(RaceRequiredExpansion(8)  == 0);   // Troll
    // TBC (1)
    CHECK(RaceRequiredExpansion(10) == 1);   // BloodElf
    CHECK(RaceRequiredExpansion(11) == 1);   // Draenei
    // Cata (3)
    CHECK(RaceRequiredExpansion(9)  == 3);   // Goblin
    CHECK(RaceRequiredExpansion(22) == 3);   // Worgen
    // MoP (4)
    CHECK(RaceRequiredExpansion(24) == 4);   // Pandaren (neutral, the create-race)
    CHECK(RaceRequiredExpansion(25) == 4);   // Pandaren (Alliance)
    CHECK(RaceRequiredExpansion(26) == 4);   // Pandaren (Horde)

    // --- Classes ---
    CHECK(ClassRequiredExpansion(1)  == 0);  // Warrior
    CHECK(ClassRequiredExpansion(2)  == 0);  // Paladin
    CHECK(ClassRequiredExpansion(5)  == 0);  // Priest
    CHECK(ClassRequiredExpansion(11) == 0);  // Druid
    CHECK(ClassRequiredExpansion(6)  == 2);  // DeathKnight -> WotLK
    CHECK(ClassRequiredExpansion(10) == 4);  // Monk        -> MoP

    // --- Gate semantics: a Classic account (Expansion()==0) is blocked from MoP content,
    //     a MoP account (Expansion()==4) is entitled to everything. ---
    const uint8 classicAccount = 0;
    const uint8 mopAccount     = 4;
    CHECK(RaceRequiredExpansion(24)  > classicAccount);   // Pandaren blocked on Classic
    CHECK(ClassRequiredExpansion(10) > classicAccount);   // Monk blocked on Classic
    CHECK(!(RaceRequiredExpansion(24)  > mopAccount));    // Pandaren allowed on MoP
    CHECK(!(ClassRequiredExpansion(10) > mopAccount));    // Monk allowed on MoP
    CHECK(!(RaceRequiredExpansion(1)   > classicAccount));// Human always allowed

    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURES\n", g_failures);
    return 1;
}
