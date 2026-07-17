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
#include "SharedDefines.h"

// See MopCreateGating.h for why these are hard-coded game facts rather than a DBC column read.

uint8 MopCreateGating::RaceRequiredExpansion(uint8 race)
{
    switch (race)
    {
        // The Burning Crusade races.
        case RACE_BLOODELF:                 // 10
        case RACE_DRAENEI:                  // 11
            return EXPANSION_TBC;

        // Cataclysm races.
        case RACE_GOBLIN:                   // 9
        case RACE_WORGEN:                   // 22
            return EXPANSION_CATA;

        // Mists of Pandaria races (24 is the neutral create-race; 25/26 are the post level-10
        // faction-choice variants, gated the same for completeness).
        case RACE_PANDAREN_NEUTRAL:         // 24
        case RACE_PANDAREN_ALLI:            // 25
        case RACE_PANDAREN_HORDE:           // 26
            return EXPANSION_MOP;

        // Classic races (RACE_HUMAN..RACE_TROLL) and anything else.
        default:
            return EXPANSION_NONE;
    }
}

uint8 MopCreateGating::ClassRequiredExpansion(uint8 class_)
{
    switch (class_)
    {
        case CLASS_DEATH_KNIGHT:            // 6  -> Wrath of the Lich King
            return EXPANSION_WOTLK;

        case CLASS_MONK:                    // 10 -> Mists of Pandaria
            return EXPANSION_MOP;

        // All other classes are available from Classic.
        default:
            return EXPANSION_NONE;
    }
}
