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

#ifndef MANGOS_H_MOPCHARENUM
#define MANGOS_H_MOPCHARENUM

#include "Common.h"
#include <string>
#include <vector>

class WorldPacket;

/**
 * @brief SMSG_CHAR_ENUM (0x11C3) body serializer for WoW 5.4.8.18414.
 *
 * Writes the binary-confirmed wire layout (reader sub_74863B) from plain per-character
 * structs, with no DB/Player coupling. See claude/Phase4_facts_binary_charenum_consolidated.md
 * (bit block sec.4, byte block sec.5, encoder recipe sec.10). Scalar-role labels marked
 * [capture-confirm] are best-guess pending a live capture (byte offsets are certain).
 */
namespace MopCharEnum
{
    /// One equipment/visual slot (23 per character: 19 equipped + 4 folded "bag" slots).
    struct EquipmentSlot
    {
        uint32 displayId;
        uint8  invType;
        uint32 enchant;
    };

    /// One character's fields, in logical form (the serializer knows the wire order).
    struct Entry
    {
        uint64 charGuid;        ///< full player GUID (raw 64-bit)
        uint64 guildGuid;       ///< full guild GUID (raw 64-bit), 0 if none
        std::string name;

        uint8 race;
        uint8 class_;
        uint8 gender;
        uint8 skin;
        uint8 face;
        uint8 hairStyle;
        uint8 hairColor;
        uint8 facialHair;
        uint8 level;
        uint8 slot;             ///< character-list order id

        uint32 zone;
        uint32 map;
        float  posX;
        float  posY;
        float  posZ;

        uint32 charFlags;
        uint32 customizeFlags;
        uint32 petDisplayId;
        uint32 petLevel;
        uint32 petFamily;

        bool flagFirstLogin;    ///< bit +108 (AT_LOGIN_FIRST)
        bool flagA;             ///< bit +124 (best-guess false) [capture-confirm]

        EquipmentSlot equipment[23];
    };

    /**
     * @brief Serialize the full SMSG_CHAR_ENUM body for @p chars into @p out.
     * Writes: preamble bit-block (21-bit extensionCount=0, 16-bit count), per-character
     * bit-blocks, the trailing hasData bit, then the byte-block and the (empty) extension tail.
     */
    void Build(WorldPacket& out, const std::vector<Entry>& chars);
}

#endif
