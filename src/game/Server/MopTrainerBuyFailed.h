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

#ifndef MANGOS_H_MOPTRAINERBUYFAILED
#define MANGOS_H_MOPTRAINERBUYFAILED

#include "Common.h"

class WorldPacket;

/**
 * @brief SMSG_TRAINER_BUY_FAILED (0x042E) body serializer for WoW 5.4.8.18414.
 *
 * Writes the binary-confirmed wire layout, taken from the client's reader
 * sub_6D369B (constructor sub_6FDF83, family dispatcher sub_659694 case 0xAA).
 * See claude/successors/WIRE-FORMAT.md for how the reader was located and for
 * the verified stream primitives.
 *
 * The server previously sent SMSG_TRAINER_SERVICE (0x6A05) here. That value is
 * above 0x1FFF and cannot be expressed by the MoP packed header
 * ((size << 13) | (cmd & 0x1FFF)), so the message never arrived as sent. Three
 * things were wrong at once and all three are corrected here: the opcode, the
 * field order (the client reads the reason BEFORE the service id), and the
 * reason encoding, which was inverted with respect to the client.
 */
namespace MopTrainerBuyFailed
{
    /**
     * @brief Failure reason, in the CLIENT's encoding.
     *
     * Handler sub_7AB176 branches on this value:
     *   0 -> "Trainer service %d unavailable"
     *   1 -> "Not enough money for trainer service %d"
     *   otherwise -> "Unknown trainer fail reason %d"
     *
     * NOTE: the server's internal `trainState` used the opposite convention
     * (0 = not enough money, 1 = unavailable). Callers must translate.
     */
    enum Reason
    {
        REASON_UNAVAILABLE      = 0,
        REASON_NOT_ENOUGH_MONEY = 1
    };

    /**
     * @brief Serialize the SMSG_TRAINER_BUY_FAILED body into @p out.
     *
     * Wire order, exactly as sub_6D369B consumes it:
     *   1. eight guid mask bits, byte order 3,0,4,7,6,1,5,2 (one full byte)
     *   2. guid bytes 1,2,0,3,4   (each written only when non-zero, XORed with 1)
     *   3. uint32 reason
     *   4. guid bytes 5,6,7
     *   5. uint32 serviceId
     *
     * @param out         packet to append to; caller sets the opcode
     * @param trainerGuid raw 64-bit trainer GUID
     * @param reason      one of Reason, in the client's encoding
     * @param serviceId   the spell/service id the client prints as %d
     */
    void Build(WorldPacket& out, uint64 trainerGuid, uint32 reason, uint32 serviceId);
}

#endif
