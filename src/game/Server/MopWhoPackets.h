/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_H_MOP_WHO_PACKETS
#define MANGOS_H_MOP_WHO_PACKETS

#include "Common.h"
#include "ByteBuffer.h"
#include "WorldPacket.h"
#include "Object/ObjectGuid.h"

#include <string>
#include <vector>

/**
 * @brief CMSG_WHO (0x18A3) and SMSG_WHO (0x161B) for client build 18414.
 *
 * Both bodies were derived from the client binary and then confirmed against real
 * capture payloads. Nothing here is carried over from 3.3.5 -- the two shapes share
 * no field order at all, which is why the inherited handler had to be rewritten
 * rather than simply registered.
 *
 * REQUEST -- writer sub_66E005 (vtable body writer; header writer sub_6624C1 calls
 * sub_40F075(pkt, 6307)). Bit widths recovered from the helper thunks: sub_665157 = 1
 * bit, sub_664D4F = 3, sub_664DCD = 4, sub_664EC9 = 6, sub_664F47 = 7, and sub_665185
 * writes a whole byte -- so a "byte then one bit" pair is a 9-bit length.
 *
 *     uint32 x4                              race mask, class mask, level max, level min
 *     bit x3                                 showEnemies, exactName, serverInfo
 *     bits8(len>>1) + bit(len&1)             guild name length   (9 bits)
 *     bit                                    unknown
 *     bits6  len                             player name length
 *     bits4  count                           zone count
 *     bits8(len>>1) + bit(len&1)             realm name length   (9 bits)
 *     bits7  len                             fourth string length
 *     bits3  count                           word count
 *       per word: bits7 len
 *     FlushBits
 *     word bytes, guild name, uint32 zone ids, player name, realm name, fourth string
 *     if (serverInfo) uint32 x3
 *
 * Verified byte-exact, zero leftover, against five build-18414 captures under
 * catalogueGenerationId 2BE10C89...88752:
 *   capture-000135 seq 177671   name "Zynakinka"
 *   capture-000146 seq 1464949  word "twi"
 *   capture-000059 seq 1637595  accented UTF-8 name
 *   capture-000146 seq 1375738  name "Pawclaws" + realm "Magtheridon"
 *   capture-000161 seq 82808    name "Discocandy" + realm "Magtheridon"
 *
 * RESPONSE -- read by the client in sub_720854, which fills a 536-byte JamWhoEntry
 * (RTTI confirms the name) per result. sub_691684 reads 6 bits, sub_6650D3 reads 7.
 * The client stores the parsed entries via sub_A6BD8F, and GetWhoInfo surfaces
 * name, guild, level, race, class and zone to Lua.
 *
 * The response is TWO passes over the entries: a bit block for every entry first,
 * then FlushBits, then a byte block for every entry. Writing them interleaved
 * desynchronises the client's reader.
 *
 * An empty result is a single 0x00 byte -- a 6-bit count of zero, flushed. Confirmed
 * at capture-000059 seq 1637608 and capture-000326 seq 921262.
 */
namespace MopWhoPackets
{
    /// A parsed CMSG_WHO query.
    struct WhoRequest
    {
        uint32 raceMask = 0;
        uint32 classMask = 0;
        uint32 levelMax = 0;
        uint32 levelMin = 0;

        std::string playerName;     ///< 6-bit length. "Zynakinka" in capture-000135 seq 177671.
        std::string guildName;      ///< 9-bit length.
        std::string realmName;      ///< 9-bit length. "Magtheridon" in capture-000146 seq 1375738.
        std::string extraName;      ///< 7-bit length. Empty in every observed capture.

        std::vector<uint32> zoneIds;        ///< 4-bit count, so at most 15; the UI caps at 10.
        std::vector<std::string> words;     ///< 3-bit count, so at most 7; the UI caps at 4.

        bool showEnemies = false;
        bool exactName = false;
        bool serverInfo = false;
    };

    /// One row of an SMSG_WHO reply, in the order the client stores it.
    struct WhoEntry
    {
        ObjectGuid playerGuid;
        ObjectGuid accountGuid;
        ObjectGuid guildGuid;

        std::string name;
        std::string guildName;

        uint32 nameVirtualRealm = 0;
        uint32 guildVirtualRealm = 0;
        uint32 zoneId = 0;
        uint32 unknown4 = 0;        ///< entry+4. Zero in everything we send.

        uint8 race = 0;
        uint8 gender = 0;
        uint8 classId = 0;
        uint8 level = 0;

        bool isGameMaster = false;  ///< entry+532, the only bool the client keeps.
    };

    inline uint8 GuidByte(ObjectGuid const& guid, size_t index)
    {
        return uint8(guid.GetRawValue() >> (index * 8));
    }

    /**
     * @brief Read a CMSG_WHO body. Returns false on anything malformed.
     *
     * Refusing is deliberate: a body we cannot account for must not be half-applied,
     * because the reader shares the packet's bit cursor and a wrong length silently
     * consumes the rest of the stream.
     */
    inline bool ParseWhoRequest(WorldPacket& in, WhoRequest& req)
    {
        if (in.size() - in.rpos() < 16)
        {
            return false;
        }

        in >> req.raceMask;
        in >> req.classMask;
        in >> req.levelMax;
        in >> req.levelMin;

        req.showEnemies = in.ReadBit();
        req.exactName   = in.ReadBit();
        req.serverInfo  = in.ReadBit();

        uint32 guildLen = in.ReadBits(8) << 1;
        guildLen |= in.ReadBit() ? 1 : 0;

        in.ReadBit();                                   // unknown, zero in every capture

        uint32 const nameLen  = in.ReadBits(6);
        uint32 const zoneCount = in.ReadBits(4);

        uint32 realmLen = in.ReadBits(8) << 1;
        realmLen |= in.ReadBit() ? 1 : 0;

        uint32 const extraLen  = in.ReadBits(7);
        uint32 const wordCount = in.ReadBits(3);

        std::vector<uint32> wordLens;
        wordLens.reserve(wordCount);
        for (uint32 i = 0; i < wordCount; ++i)
        {
            wordLens.push_back(in.ReadBits(7));
        }

        // The client caps zones at 10 and words at 4. The wire fields are wider than
        // that, so a hostile or broken body can claim more; refuse rather than trust it.
        if (zoneCount > 10 || wordCount > 4)
        {
            return false;
        }

        size_t needed = guildLen + nameLen + realmLen + extraLen + zoneCount * 4;
        for (std::vector<uint32>::const_iterator it = wordLens.begin(); it != wordLens.end(); ++it)
        {
            needed += *it;
        }
        if (req.serverInfo)
        {
            needed += 12;
        }
        if (in.size() - in.rpos() < needed)
        {
            return false;
        }

        req.words.reserve(wordCount);
        for (std::vector<uint32>::const_iterator it = wordLens.begin(); it != wordLens.end(); ++it)
        {
            std::string word;
            if (*it)
            {
                word.assign((char const*)in.contents() + in.rpos(), *it);
                in.read_skip(*it);
            }
            req.words.push_back(word);
        }

        if (guildLen)
        {
            req.guildName.assign((char const*)in.contents() + in.rpos(), guildLen);
            in.read_skip(guildLen);
        }

        req.zoneIds.reserve(zoneCount);
        for (uint32 i = 0; i < zoneCount; ++i)
        {
            uint32 zone;
            in >> zone;
            req.zoneIds.push_back(zone);
        }

        if (nameLen)
        {
            req.playerName.assign((char const*)in.contents() + in.rpos(), nameLen);
            in.read_skip(nameLen);
        }
        if (realmLen)
        {
            req.realmName.assign((char const*)in.contents() + in.rpos(), realmLen);
            in.read_skip(realmLen);
        }
        if (extraLen)
        {
            req.extraName.assign((char const*)in.contents() + in.rpos(), extraLen);
            in.read_skip(extraLen);
        }

        if (req.serverInfo)
        {
            uint32 ignored;
            in >> ignored;
            in >> ignored;
            in >> ignored;
        }

        return true;
    }

    /**
     * @brief Build an SMSG_WHO reply.
     *
     * Field and GUID-byte order taken from the client's reader sub_720854. The three
     * GUIDs live at entry offsets +8..+15 (player), +16..+23 (account) and
     * +416..+423 (guild); the interleaving below is that reader's exact sequence, not
     * a tidied-up version of it.
     */
    inline void BuildWhoResponse(WorldPacket& out, std::vector<WhoEntry> const& entries)
    {
        // 6-bit count. Zero entries therefore flushes to the single 0x00 byte retail
        // sends for "no results".
        out.WriteBits(uint32(entries.size()), 6);

        for (std::vector<WhoEntry>::const_iterator it = entries.begin(); it != entries.end(); ++it)
        {
            ObjectGuid const& p = it->playerGuid;
            ObjectGuid const& a = it->accountGuid;
            ObjectGuid const& g = it->guildGuid;

            out.WriteBit(GuidByte(p, 2) != 0);
            out.WriteBit(GuidByte(a, 2) != 0);
            out.WriteBit(GuidByte(p, 7) != 0);
            out.WriteBit(GuidByte(g, 5) != 0);
            out.WriteBits(uint32(it->guildName.size()), 7);
            out.WriteBit(GuidByte(p, 1) != 0);
            out.WriteBit(GuidByte(p, 5) != 0);
            out.WriteBit(GuidByte(g, 7) != 0);
            out.WriteBit(GuidByte(a, 5) != 0);
            out.WriteBit(false);                            // entry+0
            out.WriteBit(GuidByte(g, 1) != 0);
            out.WriteBit(GuidByte(a, 6) != 0);
            out.WriteBit(GuidByte(g, 2) != 0);
            out.WriteBit(GuidByte(a, 4) != 0);
            out.WriteBit(GuidByte(g, 0) != 0);
            out.WriteBit(GuidByte(g, 3) != 0);
            out.WriteBit(GuidByte(p, 6) != 0);
            out.WriteBit(it->isGameMaster);                 // entry+532
            out.WriteBit(GuidByte(a, 1) != 0);
            out.WriteBit(GuidByte(g, 4) != 0);
            out.WriteBit(GuidByte(p, 0) != 0);

            // Five word slots, always present. We never echo search words back, so
            // every one is zero length and contributes no bytes below.
            for (int w = 0; w < 5; ++w)
            {
                out.WriteBits(0, 7);
            }

            out.WriteBit(GuidByte(a, 3) != 0);
            out.WriteBit(GuidByte(g, 6) != 0);
            out.WriteBit(GuidByte(a, 0) != 0);
            out.WriteBit(GuidByte(p, 4) != 0);
            out.WriteBit(GuidByte(p, 3) != 0);
            out.WriteBit(GuidByte(a, 7) != 0);
            out.WriteBits(uint32(it->name.size()), 6);
        }

        out.FlushBits();

        for (std::vector<WhoEntry>::const_iterator it = entries.begin(); it != entries.end(); ++it)
        {
            ObjectGuid const& p = it->playerGuid;
            ObjectGuid const& a = it->accountGuid;
            ObjectGuid const& g = it->guildGuid;

            out.WriteByteSeq(GuidByte(a, 1));
            out << uint32(it->nameVirtualRealm);
            out.WriteByteSeq(GuidByte(a, 7));
            out << uint32(it->guildVirtualRealm);
            out.WriteByteSeq(GuidByte(a, 4));
            if (!it->name.empty())
            {
                out.append(it->name.c_str(), it->name.size());
            }
            out.WriteByteSeq(GuidByte(g, 1));
            out.WriteByteSeq(GuidByte(a, 0));
            out.WriteByteSeq(GuidByte(g, 2));
            out.WriteByteSeq(GuidByte(g, 0));
            out.WriteByteSeq(GuidByte(g, 4));
            out.WriteByteSeq(GuidByte(a, 3));
            out.WriteByteSeq(GuidByte(g, 6));
            out << uint32(it->unknown4);
            if (!it->guildName.empty())
            {
                out.append(it->guildName.c_str(), it->guildName.size());
            }
            out.WriteByteSeq(GuidByte(g, 3));
            out.WriteByteSeq(GuidByte(p, 4));
            out << uint8(it->classId);
            out.WriteByteSeq(GuidByte(p, 7));
            out.WriteByteSeq(GuidByte(a, 6));
            out.WriteByteSeq(GuidByte(a, 2));
            // five zero-length words: nothing to emit
            out.WriteByteSeq(GuidByte(p, 2));
            out.WriteByteSeq(GuidByte(p, 3));
            out << uint8(it->race);
            out.WriteByteSeq(GuidByte(g, 7));
            out.WriteByteSeq(GuidByte(p, 1));
            out.WriteByteSeq(GuidByte(p, 5));
            out.WriteByteSeq(GuidByte(p, 6));
            out.WriteByteSeq(GuidByte(a, 5));
            out.WriteByteSeq(GuidByte(p, 0));
            out << uint8(it->gender);
            out.WriteByteSeq(GuidByte(g, 5));
            out << uint8(it->level);
            out << uint32(it->zoneId);
        }
    }
}

#endif
