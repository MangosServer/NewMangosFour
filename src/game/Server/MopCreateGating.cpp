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
//
// There is deliberately NO race requirement. Since patch 5.0.4 every playable race -- Pandaren
// included -- is creatable regardless of the account's expansion ("All Races Available in Patch
// 5.0.4", Blizzard). Only certain classes stayed expansion-gated, and expansion-specific *content*
// (levels 86-90 / the continent of Pandaria; the Pandaren Wandering Isle start remains open) is
// enforced separately at enter-world, not at character creation.

uint8 MopCreateGating::ClassRequiredExpansion(uint8 class_)
{
    switch (class_)
    {
        case CLASS_MONK:                    // 10 -> Mists of Pandaria (confirmed: Monk needs MoP)
            return EXPANSION_MOP;

        case CLASS_DEATH_KNIGHT:            // 6  -> Wrath of the Lich King (the hero class also needs a
            return EXPANSION_WOTLK;         //        level-55 character, enforced elsewhere)

        // All other classes are available from Classic.
        default:
            return EXPANSION_NONE;
    }
}

bool MopCreateGating::TwoSideCreateViolation(Team newTeam, const std::vector<Team>& existingTeams)
{
    if (newTeam == TEAM_NONE)
    {
        return false;
    }
    for (Team t : existingTeams)
    {
        if (t == TEAM_NONE)
        {
            continue;
        }
        if (t != newTeam)
        {
            return true;
        }
    }
    return false;
}
