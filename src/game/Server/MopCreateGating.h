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

#ifndef MANGOS_H_MOPCREATEGATING
#define MANGOS_H_MOPCREATEGATING

#include "Common.h"

/**
 * @brief Character-creation class-expansion entitlement for WoW 5.4.8.18414.
 *
 * Returns the minimum account expansion (see enum Expansions: EXPANSION_NONE..EXPANSION_MOP)
 * required to create a character of a given class. HandleCharCreateOpcode compares the result
 * against WorldSession::Expansion() and rejects with CHAR_CREATE_EXPANSION_CLASS if the account is
 * not entitled.
 *
 * RACES ARE NOT GATED: since patch 5.0.4 every playable race -- Pandaren included -- is creatable
 * regardless of account expansion ("All Races Available in Patch 5.0.4", Blizzard). Only certain
 * classes stayed gated, and expansion-specific *content* (levels 86-90 / the continent of Pandaria;
 * the Pandaren Wandering Isle start stays open) is enforced at enter-world, not at creation. So
 * there is intentionally no race function here.
 *
 * WHY A HELPER AND NOT A DBC READ: the 5.4.8.18414 client ChrClasses.dbc carries NO graded
 * required-expansion column -- col 10 (which TrinityCore reads as `expansion`) is actually
 * AttackPowerPerStrength, so the reference forks' `classEntry->expansion` reads are misaligned
 * against the real client data. The class->expansion requirement is a fixed game fact, encoded here.
 */
namespace MopCreateGating
{
    /// Minimum account expansion required to create the given class (default EXPANSION_NONE).
    uint8 ClassRequiredExpansion(uint8 class_);
}

#endif
