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

#include "MopCharEnum.h"
#include "WorldPacket.h"

namespace
{
    /// Byte @p i (0..7) of a raw 64-bit GUID, little-endian (matches ObjectGuid::operator[]).
    inline uint8 GuidByte(uint64 guid, int i)
    {
        return uint8((guid >> (8 * i)) & 0xFF);
    }

    /// Packed-GUID byte: present only if non-zero, and written as value ^ 1.
    inline void WriteGuidByteXor(WorldPacket& out, uint64 guid, int i)
    {
        uint8 b = GuidByte(guid, i);
        if (b != 0)
        {
            out << uint8(b ^ 1);
        }
    }
}

void MopCharEnum::Build(WorldPacket& out, const std::vector<Entry>& chars)
{
    // ---- preamble (bit block) ----
    out.WriteBits(0, 21);                            // extensionCount (none)
    out.WriteBits(uint32(chars.size()), 16);         // characterCount

    // ---- per-character bit block (exact sec.4 order) ----
    for (const Entry& c : chars)
    {
        out.WriteBit(GuidByte(c.guildGuid, 4) != 0);
        out.WriteBit(GuidByte(c.charGuid, 0) != 0);
        out.WriteBit(GuidByte(c.guildGuid, 3) != 0);
        out.WriteBit(GuidByte(c.charGuid, 3) != 0);
        out.WriteBit(GuidByte(c.charGuid, 7) != 0);
        out.WriteBit(c.flagA);                       // +124 flag A [capture-confirm]
        out.WriteBit(c.flagFirstLogin);              // +108 flag B (firstLogin)
        out.WriteBit(GuidByte(c.charGuid, 6) != 0);
        out.WriteBit(GuidByte(c.guildGuid, 6) != 0);
        out.WriteBits(uint32(c.name.length()), 6);   // 6-bit name length
        out.WriteBit(GuidByte(c.charGuid, 1) != 0);
        out.WriteBit(GuidByte(c.guildGuid, 1) != 0);
        out.WriteBit(GuidByte(c.guildGuid, 0) != 0);
        out.WriteBit(GuidByte(c.charGuid, 4) != 0);
        out.WriteBit(GuidByte(c.guildGuid, 7) != 0);
        out.WriteBit(GuidByte(c.charGuid, 2) != 0);
        out.WriteBit(GuidByte(c.charGuid, 5) != 0);
        out.WriteBit(GuidByte(c.guildGuid, 2) != 0);
        out.WriteBit(GuidByte(c.guildGuid, 5) != 0);
    }

    out.WriteBit(1);                // hasData/success bit, read LAST. Always 1 = a valid response;
                                    // the 16-bit count field carries emptiness. An empty list must
                                    // still report success or the client shows "error retrieving
                                    // character list". [capture-confirm: empty-list value]
    out.FlushBits();                // byte-align before the byte block (also fixes zero-char case)

    // ---- per-character byte block (exact sec.5 order, 41 ops) ----
    for (const Entry& c : chars)
    {
        // The client reader stores these single bytes to record offsets +57..+66;
        // the UI-populate (sub_140A6FA00) then copies +58..+66 -> glue+400..+408 as
        // race,class,gender,skin,face,hairStyle,hairColor,facialHair,level. The wire
        // op->reader-offset routing is fixed by the client, so each op must carry the
        // field the consumer expects at its offset. Verified byte-for-byte vs a live
        // crash minidump (Human Paladin: glue+400=race,+401=class,+402=gender,...).
        out << uint32(0);                                 // op1  +132 -> glue+396 (0)
        WriteGuidByteXor(out, c.charGuid, 1);             // op2  charGUID[1]
        out << uint8(c.slot);                             // op3  +57  -> glue+57 (list byte; not read by char-list) [capture-confirm]
        out << uint8(c.hairStyle);                        // op4  +63  -> glue+405 hairStyle
        WriteGuidByteXor(out, c.guildGuid, 2);            // op5  guildGUID[2]
        WriteGuidByteXor(out, c.guildGuid, 0);            // op6  guildGUID[0]
        WriteGuidByteXor(out, c.guildGuid, 6);            // op7  guildGUID[6]
        out.append(c.name.c_str(), c.name.length());      // op8  +8 name (no NUL)
        WriteGuidByteXor(out, c.guildGuid, 3);            // op9  guildGUID[3]
        out << float(c.posX);                             // op10 +76 posX -> glue+80
        out << uint32(c.petFamily);                       // op11 +104 -> glue+388 petFamily
        out << uint8(c.face);                             // op12 +62  -> glue+404 face
        out << uint8(c.class_);                           // op13 +59  -> glue+401 class (was gender; NULL ChrClasses -> #132)
        WriteGuidByteXor(out, c.guildGuid, 5);            // op14 guildGUID[5]
        for (int e = 0; e < 23; ++e)                      // op15 equipment x23 (wire order)
        {
            out << uint32(c.equipment[e].enchant);        //      +140 enchant   [capture-confirm]
            out << uint8(c.equipment[e].invType);         //      +144 invType
            out << uint32(c.equipment[e].displayId);      //      +136 displayId  [capture-confirm]
        }
        out << uint32(c.customizeFlags);                  // op16 +100 -> customizeFlags (LIVE-CONFIRMED: client reads customize here; petLevel here made all hunters show "customize")
        WriteGuidByteXor(out, c.charGuid, 3);             // op17 charGUID[3]
        WriteGuidByteXor(out, c.charGuid, 5);             // op18 charGUID[5]
        out << uint32(0);                                 // op19 +120 flags cluster [capture-confirm]
        WriteGuidByteXor(out, c.guildGuid, 4);            // op20 guildGUID[4]
        out << uint32(c.zone);                            // op21 +72 zone -> glue+60
        out << uint8(c.race);                             // op22 +58  -> glue+400 race (was class)
        out << uint8(c.skin);                             // op23 +61  -> glue+403 skin
        WriteGuidByteXor(out, c.guildGuid, 1);            // op24 guildGUID[1]
        out << uint8(c.level);                            // op25 +66  -> glue+408 level (was slot)
        WriteGuidByteXor(out, c.charGuid, 0);             // op26 charGUID[0]
        WriteGuidByteXor(out, c.charGuid, 2);             // op27 charGUID[2]
        out << uint8(c.hairColor);                        // op28 +64  -> glue+406 hairColor
        out << uint8(c.gender);                           // op29 +60  -> glue+402 gender
        out << uint8(c.facialHair);                       // op30 +65  -> glue+407 facialHair
        out << uint32(c.petLevel);                        // op31 +116 -> petLevel (moved off the client's customize-read slot)
        WriteGuidByteXor(out, c.charGuid, 4);             // op32 charGUID[4]
        WriteGuidByteXor(out, c.charGuid, 7);             // op33 charGUID[7]
        out << float(c.posY);                             // op34 +80 posY
        out << uint32(c.petDisplayId);                    // op35 +112 -> petDisplayId (moved off the client's charFlags-read slot)
        out << uint32(0);                                 // op36 +128 MoP-extra [capture-confirm]
        WriteGuidByteXor(out, c.charGuid, 6);             // op37 charGUID[6]
        out << uint32(c.charFlags);                       // op38 +96 -> charFlags (LIVE-CONFIRMED: client reads ghost/charFlags here; petDisplayId here ghosted any pet whose displayId had bit 0x2000, e.g. Draenei moth 29056 / Pandaren turtle 42656)
        out << uint32(c.map);                             // op39 +68 map [capture-confirm]
        WriteGuidByteXor(out, c.guildGuid, 7);            // op40 guildGUID[7]
        out << float(c.posZ);                             // op41 +84 posZ
    }

    // extension tail: extensionCount == 0, nothing follows.
}
