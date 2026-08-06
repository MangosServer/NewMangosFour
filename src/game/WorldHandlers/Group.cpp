/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * @file Group.cpp
 * @brief Player group/party implementation
 *
 * This file implements the Group class which manages player parties:
 *
 * - Group creation and disbanding
 * - Member invite/accept/decline/kick
 * - Leadership transfer
 * - Loot method and master selection
 * - Experience sharing
 * - Quest credit sharing
 * - Group chat
 * - Roll-based loot distribution
 *
 * Groups support up to 5 members (regular) or 40 members (raid).
 *
 * @see Group for the group class
 * @see GroupMgr for group management
 */

#include "Common.h"
#include "DBCStores.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Formulas.h"
#include "ObjectAccessor.h"
#include "BattleGround/BattleGround.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "Util.h"
#include "LootMgr.h"

#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

#define LOOT_ROLL_TIMEOUT  (1*MINUTE*IN_MILLISECONDS)

bool MopGroupInvitePackets::ParseResponse(WorldPacket& in, Response& out)
{
    // Build 18414 writer sub_903D69/sub_903DC7 and serializer sub_66AA95:
    // marker byte, has-roles bit, accepted bit, then optional uint32 roles.
    auto fail = [&in]()
    {
        in.rfinish();
        return false;
    };

    if (in.rpos() != 0 || in.size() - in.rpos() < 2)
        return fail();

    Response parsed;
    uint8 marker = 0;
    in >> marker;
    if (marker != 0x7F)
        return fail();

    parsed.hasRoles = in.ReadBit();
    parsed.accepted = in.ReadBit();
    uint8 const padding = uint8(in.ReadBits(6));
    in.ResetBitReader();
    if (padding != 0)
        return fail();

    size_t const expectedRemaining = parsed.hasRoles ? sizeof(uint32) : 0;
    size_t const remaining = in.size() - in.rpos();
    if (remaining != expectedRemaining)
        return fail();

    if (parsed.hasRoles)
        in >> parsed.roles;

    if (in.rpos() != in.size())
        return fail();

    out = parsed;
    return true;
}

bool MopGroupInvitePackets::ParseRequest(WorldPacket& in, Request& out)
{
    // Build 18414 writer sub_66CBDC (Wow.exe.c:883281-883356). Fixed head is
    // a uint32 realm-selector hint, the 0x7F marker and a uint32 role mask.
    // Then an MSB-first packed header: guid[7], a 9-bit realm length,
    // guid[3], a 9-bit target length, guid[2,5,4,0,1,6], six zero padding
    // bits. The byte-aligned tail is guid[7,6,0,4], the realm string,
    // guid[1,2,3], the target string, guid[5]. Neither string is terminated.
    //
    // The client writes both lengths as (len >> 1) in eight bits followed by
    // (len & 1) in one, which is simply a nine-bit big-endian length. The
    // legacy reader took the target length as ten bits, read the mask in a
    // different order, skipped uint32+uint32 instead of uint32+uint8+uint32
    // and read the target before the realm, so it could not decode even the
    // ordinary twenty-byte body.
    auto fail = [&in]()
    {
        in.rfinish();
        return false;
    };

    // uint32 + uint8 + uint32 + the four packed header bytes
    if (in.rpos() != 0 || in.size() - in.rpos() < 13)
    {
        return fail();
    }

    Request parsed;
    uint8 marker = 0;
    in >> parsed.realmSelectorHint;
    in >> marker;
    if (marker != 0x7F)
    {
        return fail();
    }
    in >> parsed.roleMask;

    bool present[8] = { false };
    present[7] = in.ReadBit();
    uint32 const realmLength = in.ReadBits(9);
    present[3] = in.ReadBit();
    uint32 const targetLength = in.ReadBits(9);
    present[2] = in.ReadBit();
    present[5] = in.ReadBit();
    present[4] = in.ReadBit();
    present[0] = in.ReadBit();
    present[1] = in.ReadBit();
    present[6] = in.ReadBit();
    uint8 const padding = uint8(in.ReadBits(6));
    in.ResetBitReader();
    if (padding != 0)
    {
        return fail();
    }

    size_t presentCount = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        if (present[i])
        {
            ++presentCount;
        }
    }

    // Exact tail. Both declared lengths are bounded by this equality, so a
    // huge length cannot over-read; it simply fails to match.
    if (in.size() - in.rpos() != presentCount + realmLength + targetLength)
    {
        return fail();
    }

    uint8 bytes[8] = { 0 };
    bool canonical = true;
    auto readGuidByte = [&in, &present, &bytes, &canonical](uint8 index)
    {
        if (!present[index])
        {
            return;
        }
        uint8 raw = 0;
        in >> raw;
        // WriteByteSeq emits (value ^ 1) for a present byte, so a raw one
        // would decode to zero and contradict its own presence bit.
        if (raw == 1)
        {
            canonical = false;
        }
        bytes[index] = raw ^ 1;
    };

    readGuidByte(7);
    readGuidByte(6);
    readGuidByte(0);
    readGuidByte(4);
    parsed.realmName = in.ReadString(realmLength);
    readGuidByte(1);
    readGuidByte(2);
    readGuidByte(3);
    parsed.targetName = in.ReadString(targetLength);
    readGuidByte(5);

    if (!canonical || in.rpos() != in.size())
    {
        return fail();
    }

    // An embedded NUL would truncate every downstream comparison and let two
    // different wire bodies resolve to the same name.
    if (parsed.realmName.find('\0') != std::string::npos ||
        parsed.targetName.find('\0') != std::string::npos)
    {
        return fail();
    }

    uint64 rawGuid = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        rawGuid |= uint64(bytes[i]) << (i * 8);
    }
    parsed.targetGuid = ObjectGuid(rawGuid);

    out = parsed;
    return true;
}

bool MopGroupUninvitePackets::ParseRequest(WorldPacket& in, Request& out)
{
    // Build 18414 writer sub_66A7CE, reached through vtable D636E8 slot 1;
    // slot 2 sub_66238A writes opcode 3297. Layout:
    //
    //   uint8  0x7F marker, raw, BEFORE the bit stream
    //   bits   GUID presence in order 6,4,3,2,0,1,7,5
    //          reason length, 8 bits (sub_665185 takes a uint8)
    //          FlushBits
    //   bytes  reason string, not NUL terminated
    //          GUID bytes in order 5,6,1,4,3,2,7,0, each ^1, omitted when zero
    //
    // Verified byte-exact against three real bodies. Two from one capture
    // decode to the SAME GUID, one of them carrying reason "afk", which checks
    // the byte order semantically rather than only self-consistently.
    //
    // The legacy reader took a RAW ObjectGuid and skipped a std::string, with no
    // marker, so it could not decode even the ordinary 8-byte body.
    auto fail = [&in]()
    {
        in.rfinish();
        return false;
    };

    // marker + mask + length
    if (in.rpos() != 0 || in.size() - in.rpos() < 3)
    {
        return fail();
    }

    uint8 marker = 0;
    in >> marker;
    if (marker != 0x7F)
    {
        return fail();
    }

    static uint8 const maskOrder[8] = { 6, 4, 3, 2, 0, 1, 7, 5 };
    static uint8 const byteOrder[8] = { 5, 6, 1, 4, 3, 2, 7, 0 };

    bool present[8] = { false };
    for (size_t i = 0; i < 8; ++i)
    {
        present[maskOrder[i]] = in.ReadBit();
    }
    uint32 const reasonLength = in.ReadBits(8);
    in.ResetBitReader();

    size_t presentCount = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        if (present[i])
        {
            ++presentCount;
        }
    }

    // Exact tail, so a declared length cannot over-read.
    if (in.size() - in.rpos() != reasonLength + presentCount)
    {
        return fail();
    }

    Request parsed;
    parsed.reason = in.ReadString(reasonLength);

    uint8 bytes[8] = { 0 };
    bool canonical = true;
    for (size_t i = 0; i < 8; ++i)
    {
        uint8 const index = byteOrder[i];
        if (!present[index])
        {
            continue;
        }
        uint8 raw = 0;
        in >> raw;
        // WriteByteSeq emits (value ^ 1) for a present byte, so a raw one would
        // decode to zero and contradict its own presence bit.
        if (raw == 1)
        {
            canonical = false;
        }
        bytes[index] = raw ^ 1;
    }

    if (!canonical || in.rpos() != in.size())
    {
        return fail();
    }

    // An embedded NUL would truncate every downstream comparison.
    if (parsed.reason.find('\0') != std::string::npos)
    {
        return fail();
    }

    uint64 rawGuid = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        rawGuid |= uint64(bytes[i]) << (i * 8);
    }
    parsed.targetGuid = ObjectGuid(rawGuid);

    out = parsed;
    return true;
}

bool MopGroupLootMethodPackets::ParseRequest(WorldPacket& in, Request& out)
{
    // Build 18414 writer sub_6678EB, reached through vtable D634E0 slot 1;
    // slot 2 sub_661728 writes opcode 3553. Layout:
    //
    //   uint8  0x7F marker
    //   uint8  loot method
    //   uint32 loot threshold  (sub_40F075 writes four bytes)
    //   bits   GUID presence in order 7,1,2,0,4,5,6,3, then FlushBits
    //   bytes  GUID in order 7,1,3,4,6,5,0,2, each ^1, omitted when zero
    //
    // Verified byte-exact against two captured bodies, decoding to method 0
    // (free-for-all) and 3 (group loot), both at threshold 2 (uncommon) with no
    // master looter.
    //
    // CAVEAT: every captured body has an empty GUID mask, because a master
    // looter is only carried for MASTER_LOOT. The GUID orders above therefore
    // come from the client writer alone and are not corroborated by traffic. If
    // a body with a non-empty mask ever appears, it decides them.
    //
    // The legacy reader took uint32 + raw ObjectGuid + uint32 with no marker,
    // so it decoded the marker and method as one bogus 32-bit loot method.
    auto fail = [&in]()
    {
        in.rfinish();
        return false;
    };

    // marker + method + threshold + mask
    if (in.rpos() != 0 || in.size() - in.rpos() < 7)
    {
        return fail();
    }

    uint8 marker = 0;
    uint8 method = 0;
    uint32 threshold = 0;
    in >> marker;
    if (marker != 0x7F)
    {
        return fail();
    }
    in >> method;
    in >> threshold;

    // Both are cast straight onto enums by the caller. NOT_GROUP_TYPE_LOOT is
    // internal and must never arrive from the wire.
    if (method >= NOT_GROUP_TYPE_LOOT || threshold > ITEM_QUALITY_HEIRLOOM)
    {
        return fail();
    }

    static uint8 const maskOrder[8] = { 7, 1, 2, 0, 4, 5, 6, 3 };
    static uint8 const byteOrder[8] = { 7, 1, 3, 4, 6, 5, 0, 2 };

    bool present[8] = { false };
    for (size_t i = 0; i < 8; ++i)
    {
        present[maskOrder[i]] = in.ReadBit();
    }
    in.ResetBitReader();

    size_t presentCount = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        if (present[i])
        {
            ++presentCount;
        }
    }

    if (in.size() - in.rpos() != presentCount)
    {
        return fail();
    }

    uint8 bytes[8] = { 0 };
    bool canonical = true;
    for (size_t i = 0; i < 8; ++i)
    {
        uint8 const index = byteOrder[i];
        if (!present[index])
        {
            continue;
        }
        uint8 raw = 0;
        in >> raw;
        if (raw == 1)
        {
            canonical = false;
        }
        bytes[index] = raw ^ 1;
    }

    if (!canonical || in.rpos() != in.size())
    {
        return fail();
    }

    Request parsed;
    parsed.method = method;
    parsed.threshold = threshold;

    uint64 rawGuid = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        rawGuid |= uint64(bytes[i]) << (i * 8);
    }
    parsed.looterGuid = ObjectGuid(rawGuid);

    out = parsed;
    return true;
}

namespace
{
    /// Shared shape of the 18414 group-management requests: a 0x7F family
    /// marker, a bit-packed GUID presence mask, then the present bytes each
    /// XORed with one. Only the mask and byte orders differ per opcode.
    /// `leadingByte`, when given, is read BEFORE the marker. Most of the family
    /// leads with the marker, but CMSG_GROUP_CHANGE_SUB_GROUP puts its subgroup
    /// number first and the marker second.
    bool ReadMarkedGuid(WorldPacket& in, uint8 const (&maskOrder)[8],
        uint8 const (&byteOrder)[8], ObjectGuid& guid, bool* extraBit,
        size_t extraBitPosition, uint8* leadingByte = nullptr)
    {
        // The mask is 8 bits normally, but a caller supplying extraBit reads
        // NINE, which spills into a second byte. Budgeting one byte for the
        // mask let a two-byte body reach the ninth ReadBit and throw past the
        // end instead of being refused cleanly.
        size_t const fixed = (leadingByte ? 3u : 2u) + (extraBit ? 1u : 0u);
        if (in.rpos() != 0 || in.size() - in.rpos() < fixed)
        {
            return false;
        }

        if (leadingByte)
        {
            in >> *leadingByte;
        }

        uint8 marker = 0;
        in >> marker;
        if (marker != 0x7F)
        {
            return false;
        }

        bool present[8] = { false };
        size_t maskIndex = 0;
        size_t const total = extraBit ? 9 : 8;
        for (size_t i = 0; i < total; ++i)
        {
            bool const bit = in.ReadBit();
            if (extraBit && i == extraBitPosition)
            {
                *extraBit = bit;
                continue;
            }
            // Guards a future caller passing an extraBitPosition beyond the
            // mask: without this, maskOrder[8] is an out-of-bounds read and
            // present[] an out-of-bounds write.
            if (maskIndex >= 8)
            {
                return false;
            }
            present[maskOrder[maskIndex++]] = bit;
        }
        in.ResetBitReader();

        size_t presentCount = 0;
        for (size_t i = 0; i < 8; ++i)
        {
            if (present[i])
            {
                ++presentCount;
            }
        }

        // Exact tail: the mask is the only thing that may describe the length.
        if (in.size() - in.rpos() != presentCount)
        {
            return false;
        }

        uint8 bytes[8] = { 0 };
        for (size_t i = 0; i < 8; ++i)
        {
            uint8 const index = byteOrder[i];
            if (!present[index])
            {
                continue;
            }
            uint8 raw = 0;
            in >> raw;
            // A raw one would decode to zero and contradict its presence bit.
            if (raw == 1)
            {
                return false;
            }
            bytes[index] = raw ^ 1;
        }

        if (in.rpos() != in.size())
        {
            return false;
        }

        uint64 rawGuid = 0;
        for (size_t i = 0; i < 8; ++i)
        {
            rawGuid |= uint64(bytes[i]) << (i * 8);
        }
        guid = ObjectGuid(rawGuid);
        return true;
    }
}

bool MopGroupPromotePackets::ParseSetLeader(WorldPacket& in, SetLeaderRequest& out)
{
    // Build 18414 writer sub_668775, vtable D63170 slot 1; slot 2 sub_661A93
    // writes opcode 5563. Marker, mask order 1,7,0,2,5,3,4,6, byte order
    // 1,5,7,6,0,2,4,3. Verified byte-exact against a captured 7-byte body.
    static uint8 const maskOrder[8] = { 1, 7, 0, 2, 5, 3, 4, 6 };
    static uint8 const byteOrder[8] = { 1, 5, 7, 6, 0, 2, 4, 3 };

    SetLeaderRequest parsed;
    if (!ReadMarkedGuid(in, maskOrder, byteOrder, parsed.targetGuid, nullptr, 0))
    {
        in.rfinish();
        return false;
    }

    out = parsed;
    return true;
}

bool MopGroupPromotePackets::ParseAssistant(WorldPacket& in, AssistantRequest& out)
{
    // Build 18414 writer sub_668266, vtable D636FC slot 1; slot 2 sub_661913
    // writes opcode 6295. Mask order 2,0,6,3,1,[promote],4,5,7 -- the promote
    // flag is a NINTH BIT at position 5, not a trailing byte. Byte order
    // 5,1,0,7,3,6,2,4.
    //
    // Prior research read the last byte of a captured 8-byte body as the
    // promote/demote value; it is a GUID byte, which is why the body is 8 bytes
    // and not 9. Verified byte-exact against that same capture.
    static uint8 const maskOrder[8] = { 2, 0, 6, 3, 1, 4, 5, 7 };
    static uint8 const byteOrder[8] = { 5, 1, 0, 7, 3, 6, 2, 4 };

    AssistantRequest parsed;
    if (!ReadMarkedGuid(in, maskOrder, byteOrder, parsed.targetGuid, &parsed.promote, 5))
    {
        in.rfinish();
        return false;
    }

    out = parsed;
    return true;
}

bool MopLfgProposalResponsePackets::ParseRequest(WorldPacket& in, Request& out)
{
    // 16 flat bytes, then 17 bits (1 accept + 16 GUID mask), then up to 16 GUID bytes.
    // The minimum body is therefore 16 + 3 = 19 bytes with both GUIDs entirely zero.
    if (in.size() - in.rpos() < 19)
    {
        return false;
    }

    in >> out.proposalId;
    in >> out.clientQueueId;
    in >> out.flags;
    in >> out.joinTime;

    out.accepted = in.ReadBit();

    uint8 maskA[8] = { 0 };
    uint8 maskB[8] = { 0 };

    // Mask order straight off the writer: A6 A0 A2 A4 B6 B7 A3 B4 A7 B1 A5 B0 A1 B2 B3 B5.
    uint8* const maskOrder[16] =
    {
        &maskA[6], &maskA[0], &maskA[2], &maskA[4], &maskB[6], &maskB[7],
        &maskA[3], &maskB[4], &maskA[7], &maskB[1], &maskA[5], &maskB[0],
        &maskA[1], &maskB[2], &maskB[3], &maskB[5]
    };

    for (size_t i = 0; i < 16; ++i)
    {
        *maskOrder[i] = in.ReadBit() ? 1 : 0;
    }

    uint8 bytesA[8] = { 0 };
    uint8 bytesB[8] = { 0 };

    // Byte order, again off the writer: A3 A6 A4 A1 B7 B0 A7 B6 A5 B3 B1 B5 B4 A0 A2 B2.
    struct Slot { uint8 const* mask; uint8* value; };
    Slot const byteOrder[16] =
    {
        { &maskA[3], &bytesA[3] }, { &maskA[6], &bytesA[6] },
        { &maskA[4], &bytesA[4] }, { &maskA[1], &bytesA[1] },
        { &maskB[7], &bytesB[7] }, { &maskB[0], &bytesB[0] },
        { &maskA[7], &bytesA[7] }, { &maskB[6], &bytesB[6] },
        { &maskA[5], &bytesA[5] }, { &maskB[3], &bytesB[3] },
        { &maskB[1], &bytesB[1] }, { &maskB[5], &bytesB[5] },
        { &maskB[4], &bytesB[4] }, { &maskA[0], &bytesA[0] },
        { &maskA[2], &bytesA[2] }, { &maskB[2], &bytesB[2] }
    };

    // Bound the read before touching it: a truncated body must be refused, not read
    // past its end.
    size_t present = 0;
    for (size_t i = 0; i < 16; ++i)
    {
        if (*byteOrder[i].mask)
        {
            ++present;
        }
    }

    if (in.size() - in.rpos() < present)
    {
        return false;
    }

    for (size_t i = 0; i < 16; ++i)
    {
        if (*byteOrder[i].mask)
        {
            uint8 value = 0;
            in >> value;
            *byteOrder[i].value = uint8(value ^ 1);   // WriteByteSeq obfuscation
        }
    }

    uint64 rawA = 0;
    uint64 rawB = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        rawA |= uint64(bytesA[i]) << (i * 8);
        rawB |= uint64(bytesB[i]) << (i * 8);
    }

    out.guidA = ObjectGuid(rawA);
    out.guidB = ObjectGuid(rawB);

    // Every byte must be accounted for. Leftover tail means the mask was misread and
    // the GUIDs are wrong, which is worse than refusing the packet.
    return in.rpos() == in.size();
}

bool MopLfgSetRolesPackets::ParseRequest(WorldPacket& in, Request& out)
{
    // Fixed 5 bytes. Refuse anything else rather than reading past the end -- a short
    // body would otherwise leave the role mask half-populated and silently queue the
    // player as the wrong role.
    if (in.size() - in.rpos() < 5)
    {
        return false;
    }

    in >> out.roles;
    in >> out.roleCheckCounter;

    // The body is exactly 5 bytes. A longer one is not this packet, and accepting it
    // would leave unread tail data -- the cheapest signal there is that a body was read
    // wrongly, so it must not be swallowed silently.
    return in.rpos() == in.size();
}

bool MopLfgLeavePackets::ParseRequest(WorldPacket& in, Request& out)
{
    // Build 18414 writer sub_6674C9 (Wow.exe.c:879339-879394). Layout:
    //
    //   uint32 ticketType
    //   uint32 ticketFlags
    //   uint32 ticketTime
    //   uint32 clientQueueId
    //   bits   GUID presence in index order 6,0,2,3,1,5,4,7, then alignment
    //   bytes  GUID in index order 2,0,4,6,3,1,5,7, each ^1, omitted when zero
    //
    // Exact size is 17 + popcount(GUID). Verified byte-exact against four real
    // captured bodies, including the 17-byte zero-mask form a client sends when
    // it has no ticket -- so an empty mask is legitimate and must parse, not be
    // rejected.
    //
    // CAVEAT: retail coverage carries only two distinct nonzero masks, so GUID
    // indices 4 and 5 are never present in any captured body. Their positions
    // rest on the writer alone.
    auto fail = [&in]()
    {
        in.rfinish();
        return false;
    };

    if (in.rpos() != 0 || in.size() < 17)
    {
        return fail();
    }

    Request parsed;
    in >> parsed.ticketType;
    in >> parsed.ticketFlags;
    in >> parsed.ticketTime;
    in >> parsed.clientQueueId;

    static uint8 const maskOrder[8] = { 6, 0, 2, 3, 1, 5, 4, 7 };
    static uint8 const byteOrder[8] = { 2, 0, 4, 6, 3, 1, 5, 7 };

    bool present[8] = { false };
    for (uint8 index : maskOrder)
    {
        present[index] = in.ReadBit();
    }
    in.ResetBitReader();

    size_t presentCount = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        if (present[i])
        {
            ++presentCount;
        }
    }

    if (in.size() - in.rpos() != presentCount)
    {
        return fail();
    }

    uint8 bytes[8] = { 0 };
    for (uint8 index : byteOrder)
    {
        if (!present[index])
        {
            continue;
        }
        uint8 raw = 0;
        in >> raw;
        if (raw == 1)
        {
            return fail();
        }
        bytes[index] = raw ^ 1;
    }

    if (in.rpos() != in.size())
    {
        return fail();
    }

    uint64 rawGuid = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        rawGuid |= uint64(bytes[i]) << (i * 8);
    }
    parsed.ticketGuid = ObjectGuid(rawGuid);

    out = parsed;
    return true;
}

bool MopGroupPromotePackets::BuildRolePollInform(WorldPacket& out, RolePollInform const& inform)
{
    // The prompt a role check sends to every member. Layout:
    //
    //   bits   GUID mask 5,7,3,1,2,0,4,6, then FlushBits
    //   bytes  GUID[7]
    //          uint8 party index
    //          GUID[6],[5],[0],[1],[4],[2],[3]
    //
    // Each present byte is written ^1 and omitted when zero, so exact size is
    // 2 + popcount(GUID).
    //
    // Verified byte-exact against three captured bodies:
    //   7C 05 01 E6 9C 4C 04        (7 B, index 1)
    //   7D 06 00 81 62 F6 76 04     (8 B, index 0)
    //   7D 06 00 81 69 F9 6D 05     (8 B, index 0)
    //
    // Note the second byte is GUID[7], NOT a count. It equals popcount in all
    // three samples purely because every captured mask is 0x7C or 0x7D; reading
    // it as a length field was an earlier mistake this comment exists to
    // prevent repeating.
    uint64 const rawGuid = inform.initiatorGuid.GetRawValue();
    auto guidByte = [rawGuid](uint8 index)
    {
        return uint8((rawGuid >> (index * 8)) & 0xFF);
    };

    static uint8 const maskOrder[8] = { 5, 7, 3, 1, 2, 0, 4, 6 };
    static uint8 const tailOrder[7] = { 6, 5, 0, 1, 4, 2, 3 };

    out.Initialize(SMSG_GROUP_ROLE_POLL_INFORM, 2 + 8);
    for (uint8 index : maskOrder)
    {
        out.WriteBit(guidByte(index) != 0);
    }
    out.FlushBits();

    auto writeGuidByte = [&out, &guidByte](uint8 index)
    {
        uint8 const value = guidByte(index);
        if (value != 0)
        {
            out << uint8(value ^ 1);
        }
    };

    writeGuidByte(7);
    out << uint8(inform.partyIndex);
    for (uint8 index : tailOrder)
    {
        writeGuidByte(index);
    }

    return true;
}

bool MopGroupPromotePackets::ParsePartyAssignment(WorldPacket& in, PartyAssignmentRequest& out)
{
    // Build 18414 writer sub_665D70, vtable D63430 slot 1; slot 2 sub_66105D
    // writes opcode 6146. Layout:
    //
    //   uint8  assignment, 0 main tank / 1 main assist
    //   uint8  0x7F marker
    //   bits   mask 5,6,2,3,1,0,4,7 then the APPLY flag as a ninth bit,
    //          then FlushBits
    //   bytes  guid 2,5,1,0,6,3,4,7, each ^1, omitted when zero
    //
    // Verified byte-exact against 00 7F 3D 00 34 27 59 04 05 -- assignment 0,
    // apply clear, five present GUID bytes.
    //
    // A reference fork names the second byte "partyindex"; the captured body
    // shows 0x7F, the same family marker as the rest of these requests.
    //
    // The legacy reader took uint8 + uint8 + raw ObjectGuid, so it read the
    // marker as the apply flag and never found the GUID at all.
    static uint8 const maskOrder[8] = { 5, 6, 2, 3, 1, 0, 4, 7 };
    static uint8 const byteOrder[8] = { 2, 5, 1, 0, 6, 3, 4, 7 };

    PartyAssignmentRequest parsed;
    uint8 assignment = 0;
    bool apply = false;
    if (!ReadMarkedGuid(in, maskOrder, byteOrder, parsed.targetGuid, &apply, 8, &assignment))
    {
        in.rfinish();
        return false;
    }

    // Only main tank and main assist exist; the handler switches on this.
    if (assignment > 1)
    {
        in.rfinish();
        return false;
    }

    parsed.assignment = assignment;
    parsed.apply = apply;
    out = parsed;
    return true;
}

bool MopGroupPromotePackets::ParseChangeSubGroup(WorldPacket& in, ChangeSubGroupRequest& out)
{
    // Build 18414 writer sub_66920A, vtable D63250 slot 1; slot 2 sub_661D90
    // writes opcode 6041. The subgroup number LEADS and the 0x7F marker is
    // second -- the only member of this family that does not lead with the
    // marker. Mask order 1,4,6,3,7,2,0,5, byte order 2,6,1,5,3,4,0,7.
    //
    // Verified byte-exact against a captured body: 05 7F 9E 88 E8 07 A6 07,
    // decoding to subgroup 5 with five present GUID bytes.
    //
    // Two reference forks disagree on the two leading bytes and the binary
    // settles it: one reads them as PartyIndex then groupNr, which takes the
    // 0x7F marker as the subgroup number and so rejects every request against
    // MAX_RAID_SUBGROUPS. The order below is what the client actually writes.
    //
    // The legacy reader took a std::string first, so it would have read the
    // subgroup byte 0x05 as a string length.
    static uint8 const maskOrder[8] = { 1, 4, 6, 3, 7, 2, 0, 5 };
    static uint8 const byteOrder[8] = { 2, 6, 1, 5, 3, 4, 0, 7 };

    ChangeSubGroupRequest parsed;
    uint8 subGroup = 0;
    if (!ReadMarkedGuid(in, maskOrder, byteOrder, parsed.targetGuid, nullptr, 0, &subGroup))
    {
        in.rfinish();
        return false;
    }

    if (subGroup >= MAX_RAID_SUBGROUPS)
    {
        in.rfinish();
        return false;
    }

    parsed.subGroup = subGroup;
    out = parsed;
    return true;
}

bool MopGroupPromotePackets::BuildSetLeader(WorldPacket& out, SetLeaderBroadcast const& broadcast)
{
    // The 18414 body is a plain byte then a SIX-BIT name length then the raw
    // name, NOT the NUL-terminated string the inherited sender wrote. Recovered
    // from three captured bodies whose second byte is exactly (length << 2):
    //
    //   0x20 -> 8  "Jazharka"        (10 B)
    //   0x18 -> 6  "Shaoli"          ( 8 B)
    //   0x5C -> 23 "Réést????-Darksorrow" (25 B)
    //
    // Exact size is 2 + nameLength, which reproduces all three.
    if (broadcast.leaderName.empty() || broadcast.leaderName.size() >= (size_t(1) << 6))
    {
        return false;
    }

    out.Initialize(SMSG_GROUP_SET_LEADER, 2 + broadcast.leaderName.size());
    out << uint8(broadcast.partyIndex);
    out.WriteBits(uint32(broadcast.leaderName.size()), 6);
    out.FlushBits();
    out.append(broadcast.leaderName.data(), broadcast.leaderName.size());
    return true;
}

bool MopGroupInvitePackets::BuildInvite(WorldPacket& out, Invite const& invite)
{
    // Build 18414 popup grammar, recovered from the client reader and proved
    // byte-exact against real captured bodies (58 B capture-000033/196326 and
    // 76 B capture-000033/63033, plus 62 B and 102 B in the derivation brief).
    //
    // MSB-first packed header, then a byte-aligned tail. Exact size is
    //   32 + compactRealmLen + displayRealmLen + inviterNameLen + popcount(GUID)
    // which reproduces all four observed lengths and pins the field boundaries
    // independently of what the scalars are called.
    //
    // The inherited builder this replaces came from mangosthree in 2012. It was
    // not repairable: it wrote ONE realm length at nine bits where the client
    // reads TWO at eight, a seven-bit name length where the client reads six, a
    // 24-bit count where the client reads 22, and omitted the uint64 and four
    // uint32 scalars entirely. Its ceiling was 43 bytes against an observed
    // minimum of 56 over 134 packets.
    //
    // A reference fork writes the second realm length as zero and omits the
    // second string; retail bodies refute that, so it is not followed here.
    //
    // ONE POSITION IS NOT PROVEN BY CAPTURED TRAFFIC. GUID byte [4] is written
    // before the display realm string here, on the derivation's client-reader
    // provenance. A packet parser recovered from a neighbouring build places it
    // after that string instead. Every observed body -- all four fixtures and
    // both extremes of the 56..102 range -- has byte [4] absent, so the two
    // orderings emit identical bytes and no capture can separate them. It is
    // unobservable for ordinary player GUIDs, whose byte [4] is zero, but if a
    // body ever appears with [4] present, that sample decides it and this is the
    // line to revisit.
    if (invite.inviterName.size() >= (size_t(1) << 6) ||
        invite.compactRealmName.size() >= (size_t(1) << 8) ||
        invite.displayRealmName.size() >= (size_t(1) << 8))
    {
        return false;
    }

    uint64 const rawGuid = invite.inviterGuid.GetRawValue();
    auto guidByte = [rawGuid](uint8 index)
    {
        return uint8((rawGuid >> (index * 8)) & 0xFF);
    };

    out.Initialize(SMSG_GROUP_INVITE);

    out.WriteBits(uint32(invite.compactRealmName.size()), 8);
    out.WriteBits(uint32(invite.displayRealmName.size()), 8);
    out.WriteBit(guidByte(2) != 0);
    out.WriteBit(invite.flagA);
    out.WriteBits(uint32(invite.inviterName.size()), 6);
    out.WriteBit(guidByte(7) != 0);
    out.WriteBit(guidByte(5) != 0);
    out.WriteBit(invite.notAlreadyInGroup);
    out.WriteBit(invite.flagB);
    out.WriteBit(guidByte(1) != 0);
    out.WriteBit(invite.crossRealmName);
    out.WriteBit(invite.realmTransferWarning);
    out.WriteBits(0, 22);                                   // extraCount
    out.WriteBit(guidByte(3) != 0);
    out.WriteBit(guidByte(0) != 0);
    out.WriteBit(guidByte(4) != 0);
    out.WriteBit(guidByte(6) != 0);
    out.FlushBits();

    // WriteByteSeq emits (value ^ 1) and writes nothing for a zero byte, which
    // is what makes the presence bits above load-bearing.
    auto writeGuidByte = [&out, &guidByte](uint8 index)
    {
        uint8 const value = guidByte(index);
        if (value != 0)
        {
            out << uint8(value ^ 1);
        }
    };

    writeGuidByte(6);
    if (!invite.compactRealmName.empty())
    {
        out.append(invite.compactRealmName.data(), invite.compactRealmName.size());
    }
    writeGuidByte(7);
    writeGuidByte(2);
    writeGuidByte(0);
    out << uint64(invite.accountId);
    out << uint32(invite.virtualRealmAddress);
    out << uint32(invite.realmId);
    writeGuidByte(1);
    writeGuidByte(5);
    writeGuidByte(4);
    if (!invite.displayRealmName.empty())
    {
        out.append(invite.displayRealmName.data(), invite.displayRealmName.size());
    }
    out << uint32(0);                                       // reserved, zero in every observed body
    if (!invite.inviterName.empty())
    {
        out.append(invite.inviterName.data(), invite.inviterName.size());
    }
    writeGuidByte(3);
    out << uint32(0);                                       // reserved, zero in every observed body

    return true;
}

//===================================================
//============== Roll ===============================
//===================================================

void Roll::targetObjectBuildLink()
{
    // called from link()
    getTarget()->addLootValidatorRef(this);
}

void Roll::CalculateCommonVoteMask(uint32 max_enchanting_skill)
{
    m_commonVoteMask = ROLL_VOTE_MASK_ALL;

    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(itemid);

    if (itemProto->Flags2 & ITEM_FLAG2_NEED_ROLL_DISABLED)
    {
        m_commonVoteMask = RollVoteMask(m_commonVoteMask & ~ROLL_VOTE_MASK_NEED);
    }

    if (!itemProto->DisenchantID || uint32(itemProto->RequiredDisenchantSkill) > max_enchanting_skill)
    {
        m_commonVoteMask = RollVoteMask(m_commonVoteMask & ~ROLL_VOTE_MASK_DISENCHANT);
    }
}

RollVoteMask Roll::GetVoteMaskFor(Player* player) const
{
    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(itemid);

    // In NEED_BEFORE_GREED need disabled for non-usable item for player
    if (m_method != NEED_BEFORE_GREED || player->CanUseItem(itemProto) == EQUIP_ERR_OK)
    {
        return m_commonVoteMask;
    }
    else
    {
        return RollVoteMask(m_commonVoteMask & ~ROLL_VOTE_MASK_NEED);
    }
}

//===================================================
//============== Group ==============================
//===================================================

Group::Group() : m_Id(0), m_groupType(GROUPTYPE_NORMAL),
    m_dungeonDifficulty(REGULAR_DIFFICULTY), m_raidDifficulty(REGULAR_DIFFICULTY),
    m_bgGroup(NULL), m_lootMethod(FREE_FOR_ALL), m_lootThreshold(ITEM_QUALITY_UNCOMMON),
    m_subGroupsCounts(NULL), m_groupUpdateCounter(0), m_readyCheckActive(false), m_readyCheckPartyIndex(0)
{
}

Group::~Group()
{
    if (m_bgGroup)
    {
        DEBUG_LOG("Group::~Group: battleground group being deleted.");
        if (m_bgGroup->GetBgRaid(ALLIANCE) == this)
        {
            m_bgGroup->SetBgRaid(ALLIANCE, NULL);
        }
        else if (m_bgGroup->GetBgRaid(HORDE) == this)
        {
            m_bgGroup->SetBgRaid(HORDE, NULL);
        }
        else
        {
            sLog.outError("Group::~Group: battleground group is not linked to the correct battleground.");
        }
    }
    Rolls::iterator itr;
    while (!RollId.empty())
    {
        itr = RollId.begin();
        Roll* r = *itr;
        RollId.erase(itr);
        delete(r);
    }

    // it is undefined whether objectmgr (which stores the groups) or instancesavemgr
    // will be unloaded first so we must be prepared for both cases
    // this may unload some dungeon persistent state
    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        for (BoundInstancesMap::iterator itr2 = m_boundInstances[i].begin(); itr2 != m_boundInstances[i].end(); ++itr2)
        {
            itr2->second.state->RemoveGroup(this);
        }
    }

    // Sub group counters clean up
    delete[] m_subGroupsCounts;
}

/**
 * @brief Creates a new group with the specified leader and persists it when needed.
 *
 * @param guid The leader player GUID.
 * @param name The leader player name.
 * @return true if the group and its first member were created successfully; otherwise false.
 */
bool Group::Create(ObjectGuid guid, const char* name)
{
    m_leaderGuid = guid;
    m_leaderName = name;

    m_groupType  = isBGGroup() ? GROUPTYPE_BGRAID : GROUPTYPE_NORMAL;

    if (m_groupType & GROUPTYPE_RAID)
    {
        _initRaidSubGroupsCounter();
    }

    m_lootMethod = GROUP_LOOT;
    m_lootThreshold = ITEM_QUALITY_UNCOMMON;
    m_masterLooterGuid = guid;
    m_currentLooterGuid = guid;                                             // used for round robin looter

    m_dungeonDifficulty = DUNGEON_DIFFICULTY_NORMAL;
    m_raidDifficulty = RAID_DIFFICULTY_10MAN_NORMAL;
    if (!isBGGroup())
    {
        m_Id = sObjectMgr.GenerateGroupLowGuid();

        Player* leader = sObjectMgr.GetPlayer(guid);
        if (leader)
        {
            m_dungeonDifficulty = leader->GetDungeonDifficulty();
            m_raidDifficulty = leader->GetRaidDifficulty();
        }

        Player::ConvertInstancesToGroup(leader, this, guid);

        // store group in database
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId` ='%u'", m_Id);
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId` ='%u'", m_Id);

        CharacterDatabase.PExecute("INSERT INTO `groups` (`groupId`,`leaderGuid`,`mainTank`,`mainAssistant`,`lootMethod`,`looterGuid`,`lootThreshold`,`icon1`,`icon2`,`icon3`,`icon4`,`icon5`,`icon6`,`icon7`,`icon8`,`groupType`,`difficulty`,`raiddifficulty`) "
                                   "VALUES ('%u','%u','%u','%u','%u','%u','%u','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','%u','%u','%u')",
                                   m_Id, m_leaderGuid.GetCounter(), m_mainTankGuid.GetCounter(), m_mainAssistantGuid.GetCounter(), uint32(m_lootMethod),
                                   m_masterLooterGuid.GetCounter(), uint32(m_lootThreshold),
                                   m_targetIcons[0].GetRawValue(), m_targetIcons[1].GetRawValue(),
                                   m_targetIcons[2].GetRawValue(), m_targetIcons[3].GetRawValue(),
                                   m_targetIcons[4].GetRawValue(), m_targetIcons[5].GetRawValue(),
                                   m_targetIcons[6].GetRawValue(), m_targetIcons[7].GetRawValue(),
                                   uint8(m_groupType), uint32(m_dungeonDifficulty), uint32(m_raidDifficulty));
    }

    if (!AddMember(guid, name))
    {
        return false;
    }

    if (!isBGGroup())
    {
        CharacterDatabase.CommitTransaction();
    }

    _updateLeaderFlag();

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnCreate(this, m_leaderGuid, m_groupType);
    }
#endif /* ENABLE_ELUNA */

    return true;
}

/**
 * @brief Loads the core group record from a database row.
 *
 * @param fields The database fields containing group metadata.
 * @return true if the group data was loaded successfully; otherwise false.
 */
bool Group::LoadGroupFromDB(Field* fields)
{
    //                                           0           1                2             3             4                5        6        7        8        9        10       11       12       13           14            15                16            17
    // result = CharacterDatabase.Query("SELECT `mainTank`, `mainAssistant`, `lootMethod`, `looterGuid`, `lootThreshold`, `icon1`, `icon2`, `icon3`, `icon4`, `icon5`, `icon6`, `icon7`, `icon8`, `groupType`, `difficulty`, `raiddifficulty`, `leaderGuid`, `groupId` FROM `groups`");

    m_Id = fields[17].GetUInt32();
    m_leaderGuid = ObjectGuid(HIGHGUID_PLAYER, fields[16].GetUInt32());

    // group leader not exist
    if (!sObjectMgr.GetPlayerNameByGUID(m_leaderGuid, m_leaderName))
    {
        return false;
    }

    m_groupType  = GroupType(fields[13].GetUInt8());

    if (m_groupType & GROUPTYPE_RAID)
    {
        _initRaidSubGroupsCounter();
    }

    uint32 diff = fields[14].GetUInt8();
    // Refuse CHALLENGE here too -- `groups`.`difficulty` was written from the same raw cast
    // as `characters`.`dungeon_difficulty`, so it carries the same stale key space. See the
    // matching clamp in Player::LoadFromDB for why internal 2 is not loadable.
    if (diff > DUNGEON_DIFFICULTY_HEROIC)
    {
        diff = DUNGEON_DIFFICULTY_NORMAL;
    }
    m_dungeonDifficulty = Difficulty(diff);

    uint32 r_diff = fields[15].GetUInt8();
    if (r_diff >= MAX_RAID_DIFFICULTY)
    {
        r_diff = RAID_DIFFICULTY_10MAN_NORMAL;
    }
    m_raidDifficulty = Difficulty(r_diff);

    m_mainTankGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
    m_mainAssistantGuid = ObjectGuid(HIGHGUID_PLAYER, fields[1].GetUInt32());
    m_lootMethod = LootMethod(fields[2].GetUInt8());
    m_looterGuid = ObjectGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
    m_lootThreshold = ItemQualities(fields[4].GetUInt16());

    for (int i = 0; i < TARGET_ICON_COUNT; ++i)
    {
        m_targetIcons[i] = ObjectGuid(fields[5 + i].GetUInt64());
    }

    return true;
}

/**
 * @brief Loads a member slot from database data and updates subgroup counters.
 *
 * @param guidLow The low GUID of the member player.
 * @param subgroup The subgroup assignment.
 * @param assistant True if the member is an assistant.
 * @return true if the member was loaded successfully; otherwise false.
 */
bool Group::LoadMemberFromDB(uint32 guidLow, uint8 subgroup, bool assistant)
{
    MemberSlot member;
    member.guid      = ObjectGuid(HIGHGUID_PLAYER, guidLow);

    // skip nonexistent member
    if (!sObjectMgr.GetPlayerNameByGUID(member.guid, member.name))
    {
        return false;
    }

    member.group     = subgroup;
    member.assistant = assistant;
    member.readyCheckHasResponded = false;
    member.roles     = 0;
    m_memberSlots.push_back(member);

    SubGroupCounterIncrease(subgroup);

    return true;
}

/**
 * @brief Converts the group to raid mode and refreshes related state.
 */
void Group::ConvertToRaid()
{
    m_groupType = GroupType(m_groupType | GROUPTYPE_RAID);

    _initRaidSubGroupsCounter();

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `groupType` = %u WHERE `groupId`='%u'", uint8(m_groupType), m_Id);
    }
    SendUpdate();

    // update quest related GO states (quest activity dependent from raid membership)
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
        if (Player* player = sObjectMgr.GetPlayer(citr->guid))
        {
            player->UpdateForQuestWorldObjects();
        }
}

/**
 * @brief Converts a raid back to an ordinary party.
 *
 * The 18414 client offers this as a separate unit-popup button, ConvertToParty,
 * which shares CMSG_GROUP_RAID_CONVERT with ConvertToRaid and is distinguished
 * only by a single cleared bit.
 *
 * @return false when the group cannot be converted, so the caller can refuse
 *         rather than silently do nothing.
 */
bool Group::ConvertToParty()
{
    if (!isRaidGroup())
    {
        return false;
    }

    // A party has no subgroups, so anyone parked outside the first one would be
    // unreachable in the party frame. Refuse rather than move people silently.
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        if (citr->group != 0)
        {
            return false;
        }
    }

    if (m_memberSlots.size() > MAX_GROUP_SIZE)
    {
        return false;
    }

    m_groupType = GroupType(m_groupType & ~GROUPTYPE_RAID);

    // Raid-only rank must not survive the downgrade. An assistant keeps the
    // uninvite right granted by Player::CanUninviteFromGroup, which has no raid
    // test, so a former assistant could kick anyone in what is now a 5-man
    // party while the client shows them as an ordinary member.
    for (member_witerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
    {
        itr->assistant = false;
    }
    m_mainTankGuid.Clear();
    m_mainAssistantGuid.Clear();

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `groupType` = %u WHERE `groupId`='%u'", uint8(m_groupType), m_Id);
    }
    SendUpdate();

    // Raid membership gates some quest object states, so the same refresh the
    // raid direction performs is needed coming back.
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
        if (Player* player = sObjectMgr.GetPlayer(citr->guid))
        {
            player->UpdateForQuestWorldObjects();
        }

    return true;
}

/**
 * @brief Adds a pending invitation for a player.
 *
 * @param player The invited player.
 * @return true if the invite was recorded; otherwise false.
 */
bool Group::AddInvite(Player* player)
{
    if (!player || player->GetGroupInvite())
    {
        return false;
    }
    Group* group = player->GetGroup();
    if (group && group->isBGGroup())
    {
        group = player->GetOriginalGroup();
    }
    if (group)
    {
        return false;
    }

    RemoveInvite(player);

    m_invitees.insert(player);

    player->SetGroupInvite(this);

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnInviteMember(this, player->GetObjectGuid());
    }
#endif /* ENABLE_ELUNA */

    return true;
}

/**
 * @brief Adds an invitation and assigns the invited player as provisional leader.
 *
 * @param player The invited player.
 * @return true if the invite was added; otherwise false.
 */
bool Group::AddLeaderInvite(Player* player)
{
    if (!AddInvite(player))
    {
        return false;
    }

    _updateLeaderFlag(true);
    m_leaderGuid = player->GetObjectGuid();
    m_leaderName = player->GetName();
    _updateLeaderFlag();
    return true;
}

/**
 * @brief Removes a pending invitation from the group.
 *
 * @param player The player whose invite is being removed.
 * @return uint32 The current member count.
 */
uint32 Group::RemoveInvite(Player* player)
{
    m_invitees.erase(player);

    player->SetGroupInvite(NULL);
    return GetMembersCount();
}

/**
 * @brief Clears all pending invitations from the group.
 */
void Group::RemoveAllInvites()
{
    for (InvitesList::iterator itr = m_invitees.begin(); itr != m_invitees.end(); ++itr)
    {
        (*itr)->SetGroupInvite(NULL);
    }

    m_invitees.clear();
}

/**
 * @brief Finds an invited player by GUID.
 *
 * @param guid The invited player GUID.
 * @return Player* The invited player if present; otherwise NULL.
 */
Player* Group::GetInvited(ObjectGuid guid) const
{
    for (InvitesList::const_iterator itr = m_invitees.begin(); itr != m_invitees.end(); ++itr)
        if ((*itr)->GetObjectGuid() == guid)
        {
            return (*itr);
        }

    return NULL;
}

/**
 * @brief Finds an invited player by name.
 *
 * @param name The invited player name.
 * @return Player* The invited player if present; otherwise NULL.
 */
Player* Group::GetInvited(const std::string& name) const
{
    for (InvitesList::const_iterator itr = m_invitees.begin(); itr != m_invitees.end(); ++itr)
    {
        if ((*itr)->GetName() == name)
        {
            return (*itr);
        }
    }
    return NULL;
}

/**
 * @brief Adds a member to the group and synchronizes related player and LFG state.
 *
 * @param guid The member player GUID.
 * @param name The member player name.
 * @return true if the member was added successfully; otherwise false.
 */
bool Group::AddMember(ObjectGuid guid, const char* name)
{
    if (!_addMember(guid, name))
    {
        return false;
    }

    SendUpdate();

    if (Player* player = sObjectMgr.GetPlayer(guid))
    {
        if (!IsLeader(player->GetObjectGuid()) && !isBGGroup())
        {
            // reset the new member's instances, unless he is currently in one of them
            // including raid/heroic instances that they are not permanently bound to!
            player->ResetInstances(INSTANCE_RESET_GROUP_JOIN, false);
            player->ResetInstances(INSTANCE_RESET_GROUP_JOIN, true);

            if (player->getLevel() >= LEVELREQUIREMENT_HEROIC)
            {
                if (player->GetDungeonDifficulty() != GetDungeonDifficulty())
                {
                    player->SetDungeonDifficulty(GetDungeonDifficulty());
                    player->SendDungeonDifficulty(true);
                }
                if (player->GetRaidDifficulty() != GetRaidDifficulty())
                {
                    player->SetRaidDifficulty(GetRaidDifficulty());
                    player->SendRaidDifficulty(true);
                }
            }
        }
        player->SetGroupUpdateFlag(GROUP_UPDATE_FULL);
        UpdatePlayerOutOfRange(player);

        // Used by Eluna
#ifdef ENABLE_ELUNA
        if (Eluna* e = sWorld.GetEluna())
        {
            e->OnAddMember(this, player->GetObjectGuid());
        }
#endif /* ENABLE_ELUNA */

        // quest related GO state dependent from raid membership
        if (isRaidGroup())
        {
            player->UpdateForQuestWorldObjects();
        }
    }

    return true;
}

/**
 * @brief Removes a member from the group or disbands the group if too few members remain.
 *
 * @param guid The member player GUID.
 * @param removeMethod The reason or removal method.
 * @return uint32 The remaining member count.
 */
uint32 Group::RemoveMember(ObjectGuid guid, uint8 removeMethod)
{
    if (IsReadyCheckInitiator(guid))
        CompleteReadyCheck();

    // remove member and change leader (if need) only if strong more 2 members _before_ member remove
    if (GetMembersCount() > uint32(isBGGroup() ? 1 : 2))    // in BG group case allow 1 members group
    {
        bool leaderChanged = _removeMember(guid);

        if (ReadyCheckAllResponded())
            CompleteReadyCheck();

        if (Player* player = sObjectMgr.GetPlayer(guid))
        {
            // quest related GO state dependent from raid membership
            if (isRaidGroup())
            {
                player->UpdateForQuestWorldObjects();
            }

            WorldPacket data;

            if (removeMethod == 1)
            {
                data.Initialize(SMSG_GROUP_UNINVITE, 0);
                player->GetSession()->SendPacket(&data);
            }

            // we already removed player from group and in player->GetGroup() is his original group!
            if (Group* group = player->GetGroup())
            {
                group->SendUpdate();
            }
            else
            {
                SendRemovedUpdate(player);
            }

            _homebindIfInstance(player);
        }

        if (leaderChanged)
        {
            MopGroupPromotePackets::SetLeaderBroadcast broadcast;
            broadcast.leaderName = m_memberSlots.front().name;

            WorldPacket data;
            if (MopGroupPromotePackets::BuildSetLeader(data, broadcast))
            {
                BroadcastPacket(&data, true);
            }
        }

        SendUpdate();

        // Offer to backfill the slot that just opened.
        //
        // Retail sends SMSG_LFG_OFFER_CONTINUE alongside the roster-shrink
        // SMSG_GROUP_LIST, in the same second -- capture-000326 seq 582508 and 582522,
        // capture-000656 seq 255287/255318, capture-000913 seq 636664/636680. The order
        // of the two varies between captures, so adjacency is real but strict ordering is
        // not an invariant.
        //
        // Only while the run is still live: a group that has finished its dungeon has no
        // slot worth filling, and one with no status is not in a run at all.
        if (isLFGGroup())
        {
            if (uint32 const dungeonEntry = sLFGMgr.GetGroupDungeonEntry(GetObjectGuid()))
            {
                if (sLFGMgr.GetGroupLfgState(GetObjectGuid()) != LFG_STATE_FINISHED_DUNGEON)
                {
                    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
                    {
                        if (Player* pMember = sObjectMgr.GetPlayer(citr->guid))
                        {
                            pMember->GetSession()->SendLfgOfferContinue(dungeonEntry);
                        }
                    }
                }
            }
        }
    }
    // if group before remove <= 2 disband it
    else
    {
        Disband(true);
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnRemoveMember(this, guid, removeMethod); // Kicker and Reason not a part of Mangos, implement?
    }
#endif /* ENABLE_ELUNA */

    return m_memberSlots.size();
}

/**
 * @brief Transfers group leadership to another member.
 *
 * @param guid The new leader GUID.
 */
void Group::ChangeLeader(ObjectGuid guid)
{
    member_citerator slot = _getMemberCSlot(guid);
    if (slot == m_memberSlots.end())
    {
        return;
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnChangeLeader(this, guid, GetLeaderGuid());
    }
#endif /* ENABLE_ELUNA */

    _setLeader(guid);

    MopGroupPromotePackets::SetLeaderBroadcast broadcast;
    broadcast.leaderName = slot->name;

    WorldPacket data;
    if (MopGroupPromotePackets::BuildSetLeader(data, broadcast))
    {
        BroadcastPacket(&data, true);
    }
    SendUpdate();
}

/**
 * @brief Disbands the group, removes all members, and clears persistent state.
 *
 * @param hideDestroy True to suppress the destroyed notification packet.
 */
void Group::Disband(bool hideDestroy)
{
    CompleteReadyCheck();

    // Release the LFG status here rather than when the dungeon finishes: while the Group
    // still reports GROUPTYPE_LFD, SendUpdate needs the status to fill the LFG block, and
    // a missing status makes it emit a zero dungeon slot.
    if (isLFGGroup())
    {
        sLFGMgr.ReleaseGroupLfgStatus(GetObjectGuid());
    }

    Player* player;

    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        player = sObjectMgr.GetPlayer(citr->guid);
        if (!player)
        {
            continue;
        }

        // we can not call _removeMember because it would invalidate member iterator
        // if we are removing player from battleground raid
        if (isBGGroup())
        {
            player->RemoveFromBattleGroundRaid();
        }
        else
        {
            // we can remove player who is in battleground from his original group
            if (player->GetOriginalGroup() == this)
            {
                player->SetOriginalGroup(NULL);
            }
            else
            {
                player->SetGroup(NULL);
            }
        }

        // quest related GO state dependent from raid membership
        if (isRaidGroup())
        {
            player->UpdateForQuestWorldObjects();
        }

        if (!player->GetSession())
        {
            continue;
        }

        WorldPacket data;
        if (!hideDestroy)
        {
            data.Initialize(SMSG_GROUP_DESTROYED, 0);
            player->GetSession()->SendPacket(&data);
        }

        // we already removed player from group and in player->GetGroup() is his original group, send update
        if (Group* group = player->GetGroup())
        {
            group->SendUpdate();
        }
        else
        {
            SendRemovedUpdate(player);
        }

        _homebindIfInstance(player);
    }
    RollId.clear();
    m_memberSlots.clear();

    RemoveAllInvites();

    if (!isBGGroup())
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId`='%u'", m_Id);
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId`='%u'", m_Id);
        CharacterDatabase.CommitTransaction();
        ResetInstances(INSTANCE_RESET_GROUP_DISBAND, false, NULL);
        ResetInstances(INSTANCE_RESET_GROUP_DISBAND, true, NULL);
    }

    _updateLeaderFlag(true);
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnDisband(this);
    }
#endif /* ENABLE_ELUNA */

    m_leaderGuid.Clear();
    m_leaderName.clear();
}

/*********************************************************/
/***                   LOOT SYSTEM                     ***/
/*********************************************************/

static bool BuildMopGroupLootItem(Roll const& roll,
    MopLootPackets::LootItem& item)
{
    ItemPrototype const* prototype = ObjectMgr::GetItemPrototype(roll.itemid);
    if (!prototype)
        return false;

    item.itemId = roll.itemid;
    item.displayInfoId = prototype->DisplayInfoID;
    item.count = roll.itemCount;
    item.randomPropertyId = roll.itemRandomPropId;
    item.randomSuffix = int32(roll.itemRandomSuffix);
    item.lootListId = roll.itemSlot;
    item.hasLootListId = true;
    item.slotType = LOOT_SLOT_NORMAL;
    item.situ.assign(4, 0); // Client-compatible empty item-modifier block.
    return true;
}

void Group::SendLootStartRoll(uint32 CountDown, uint32 mapid, const Roll& r)
{
    MopGroupLootPackets::StartRoll packet;
    packet.lootGuid = r.lootedTargetGUID.GetRawValue();
    packet.mapId = mapid;
    packet.durationMs = CountDown;
    packet.itemSlot = r.itemSlot;
    if (!BuildMopGroupLootItem(r, packet.item))
        return;

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second == ROLL_NOT_VALID)
        {
            continue;
        }

        // The offered need/greed/disenchant mask is recipient-specific.
        packet.offeredVoteMask = uint8(r.GetVoteMaskFor(p));
        WorldPacket data;
        if (MopGroupLootPackets::BuildStartRoll(data, packet))
            p->GetSession()->SendPacket(&data);
    }
}

/**
 * @brief Sends a roll result update to all players participating in a loot roll.
 *
 * @param targetGuid The player GUID associated with the roll update.
 * @param rollNumber The rolled number or pass marker.
 * @param rollType The roll type being reported.
 * @param r The roll state.
 */
void Group::SendLootRoll(ObjectGuid const& targetGuid, uint32 rollNumber, uint8 rollType, const Roll& r)
{
    MopGroupLootPackets::RollUpdate packet;
    packet.lootGuid = r.lootedTargetGUID.GetRawValue();
    packet.participantGuid = targetGuid.GetRawValue();
    packet.rollNumber = rollNumber;
    packet.itemSlot = r.itemSlot;
    packet.rollType = rollType;
    if (!BuildMopGroupLootItem(r, packet.item))
        return;

    WorldPacket data;
    if (!MopGroupLootPackets::BuildRollUpdate(data, packet))
        return;

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second != ROLL_NOT_VALID)
        {
            p->GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Sends the final winner notification for a completed loot roll.
 *
 * @param targetGuid The winning player GUID.
 * @param rollNumber The winning roll number.
 * @param rollType The winning roll type.
 * @param r The completed roll state.
 */
void Group::SendLootRollWon(ObjectGuid const& targetGuid, uint32 rollNumber, RollVote rollType, const Roll& r)
{
    MopGroupLootPackets::RollWinner packet;
    packet.lootGuid = r.lootedTargetGUID.GetRawValue();
    packet.winnerGuid = targetGuid.GetRawValue();
    packet.rollNumber = rollNumber;
    packet.itemSlot = r.itemSlot;
    packet.rollType = uint8(rollType);
    if (!BuildMopGroupLootItem(r, packet.item))
        return;

    WorldPacket data;
    if (!MopGroupLootPackets::BuildRollWinner(data, packet))
        return;

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second != ROLL_NOT_VALID)
        {
            p->GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Sends the notification that all players passed on a loot roll.
 *
 * @param r The completed roll state.
 */
void Group::SendLootAllPassed(Roll const& r)
{
    MopGroupLootPackets::AllPassed packet;
    packet.lootGuid = r.lootedTargetGUID.GetRawValue();
    packet.itemSlot = r.itemSlot;
    if (!BuildMopGroupLootItem(r, packet.item))
        return;

    WorldPacket data;
    if (!MopGroupLootPackets::BuildAllPassed(data, packet))
        return;

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second != ROLL_NOT_VALID)
        {
            p->GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Starts group-loot rolls for loot items above the threshold.
 *
 * @param pSource The looted world object.
 * @param loot The loot container being processed.
 */
void Group::GroupLoot(WorldObject* pSource, Loot* loot)
{
    uint32 maxEnchantingSkill = GetMaxSkillValueForGroup(SKILL_ENCHANTING);

    for (uint8 itemSlot = 0; itemSlot < loot->items.size(); ++itemSlot)
    {
        LootItem& lootItem = loot->items[itemSlot];
        if (lootItem.currency)
        {
            continue;
        }

        ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(lootItem.itemid);
        if (!itemProto)
        {
            DEBUG_LOG("Group::GroupLoot: missing item prototype for item with id: %d", lootItem.itemid);
            continue;
        }

        // roll for over-threshold item if it's one-player loot
        if (itemProto->Quality >= uint32(m_lootThreshold) && !lootItem.freeforall)
        {
            StartLootRoll(pSource, GROUP_LOOT, loot, itemSlot, maxEnchantingSkill);
        }
        else
        {
            lootItem.is_underthreshold = 1;
        }
    }
}

/**
 * @brief Starts need-before-greed rolls for loot items above the threshold.
 *
 * @param pSource The looted world object.
 * @param loot The loot container being processed.
 */
void Group::NeedBeforeGreed(WorldObject* pSource, Loot* loot)
{
    uint32 maxEnchantingSkill = GetMaxSkillValueForGroup(SKILL_ENCHANTING);

    for (uint8 itemSlot = 0; itemSlot < loot->items.size(); ++itemSlot)
    {
        LootItem& lootItem = loot->items[itemSlot];
        if (lootItem.currency)
        {
            continue;
        }

        ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(lootItem.itemid);
        if (!itemProto)
        {
            DEBUG_LOG("Group::NeedBeforeGreed: missing item prototype for item with id: %d", lootItem.itemid);
            continue;
        }

        // only roll for one-player items, not for ones everyone can get
        if (itemProto->Quality >= uint32(m_lootThreshold) && !lootItem.freeforall)
        {
            StartLootRoll(pSource, NEED_BEFORE_GREED, loot, itemSlot, maxEnchantingSkill);
        }
        else
        {
            lootItem.is_underthreshold = 1;
        }
    }
}

/**
 * @brief Prepares master-loot distribution data for nearby group members.
 *
 * @param pSource The looted world object.
 * @param loot The loot container being processed.
 */
void Group::MasterLoot(WorldObject* pSource, Loot* loot)
{
    for (LootItemList::iterator i = loot->items.begin(); i != loot->items.end(); ++i)
    {
        if (i->currency)
        {
            continue;
        }

        ItemPrototype const* item = ObjectMgr::GetItemPrototype(i->itemid);
        if (!item)
        {
            continue;
        }
        if (item->Quality < uint32(m_lootThreshold))
        {
            i->is_underthreshold = 1;
        }
    }

    uint32 real_count = 0;

    WorldPacket data(SMSG_LOOT_MASTER_LIST, 330);
    data << uint8(GetMembersCount());

    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* looter = itr->getSource();
        if (!looter->IsInWorld())
        {
            continue;
        }

        if (looter->IsWithinDist(pSource, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
        {
            data << looter->GetObjectGuid();
            ++real_count;
        }
    }

    data.put<uint8>(0, real_count);

    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* looter = itr->getSource();
        if (looter->IsWithinDist(pSource, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
        {
            looter->GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Records a loot-roll vote by locating the matching roll entry.
 *
 * @param player The player casting the vote.
 * @param lootedTarget The GUID of the looted object.
 * @param itemSlot The loot slot being rolled on.
 * @param vote The selected roll vote.
 * @return true if a matching roll was found; otherwise false.
 */
bool Group::CountRollVote(Player* player, ObjectGuid const& lootedTarget, uint32 itemSlot, RollVote vote)
{
    Rolls::iterator rollI = RollId.begin();
    for (; rollI != RollId.end(); ++rollI)
        if ((*rollI)->isValid() && (*rollI)->lootedTargetGUID == lootedTarget && (*rollI)->itemSlot == itemSlot)
        {
            break;
        }

    if (rollI == RollId.end())
    {
        return false;
    }

    // possible cheating
    RollVoteMask voteMask = (*rollI)->GetVoteMaskFor(player);
    if ((voteMask & (1 << vote)) == 0)
    {
        return false;
    }

    CountRollVote(player->GetObjectGuid(), rollI, vote);    // result not related this function result meaning, ignore
    return true;
}

/**
 * @brief Applies a loot-roll vote to an existing roll entry.
 *
 * @param playerGUID The voting player GUID.
 * @param rollI Iterator pointing to the roll entry.
 * @param vote The selected roll vote.
 * @return true if processing should continue safely; otherwise false.
 */
bool Group::CountRollVote(ObjectGuid const& playerGUID, Rolls::iterator& rollI, RollVote vote)
{
    Roll* roll = *rollI;

    Roll::PlayerVote::iterator itr = roll->playerVote.find(playerGUID);
    // this condition means that player joins to the party after roll begins
    if (itr == roll->playerVote.end())
    {
        return true;                                         // result used for need iterator ++, so avoid for end of list
    }

    if (roll->getLoot())
        if (roll->getLoot()->items.empty())
        {
            return false;
        }

    switch (vote)
    {
        case ROLL_PASS:                                     // Player choose pass
        {
            SendLootRoll(playerGUID, uint32(-1), ROLL_PASS, *roll);
            ++roll->totalPass;
            itr->second = ROLL_PASS;
            break;
        }
        case ROLL_NEED:                                     // player choose Need
        {
            SendLootRoll(playerGUID, 0, ROLL_NEED, *roll);
            ++roll->totalNeed;
            itr->second = ROLL_NEED;
            break;
        }
        case ROLL_GREED:                                    // player choose Greed
        {
            SendLootRoll(playerGUID, uint32(-1), ROLL_GREED, *roll);
            ++roll->totalGreed;
            itr->second = ROLL_GREED;
            break;
        }
        case ROLL_DISENCHANT:                               // player choose Disenchant
        {
            SendLootRoll(playerGUID, uint32(-1), ROLL_DISENCHANT, *roll);
            ++roll->totalGreed;
            itr->second = ROLL_DISENCHANT;
            break;
        }
        default:                                            // Roll removed case
            break;
    }

    if (roll->totalPass + roll->totalNeed + roll->totalGreed >= roll->totalPlayersRolling)
    {
        CountTheRoll(rollI);
        return true;
    }

    return false;
}

/**
 * @brief Starts a loot roll for a specific item slot and eligible nearby members.
 *
 * @param lootTarget The looted world object.
 * @param method The loot method driving the roll.
 * @param loot The loot container.
 * @param itemSlot The loot slot to roll on.
 */
void Group::StartLootRoll(WorldObject* lootTarget, LootMethod method, Loot* loot, uint8 itemSlot, uint32 maxEnchantingSkill)
{
    if (itemSlot >= loot->items.size())
    {
        return;
    }

    LootItem const& lootItem = loot->items[itemSlot];
    if (lootItem.currency)
    {
        return;
    }

    Roll* r = new Roll(lootTarget->GetObjectGuid(), method, lootItem);

    // a vector is filled with only near party members
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* playerToRoll = itr->getSource();
        if (!playerToRoll || !playerToRoll->GetSession())
        {
            continue;
        }

        if (playerToRoll->IsOptingOutOfLoot())
        {
            continue;
        }

        if (lootItem.AllowedForPlayer(playerToRoll, lootTarget))
        {
            if (playerToRoll->IsWithinDistInMap(lootTarget, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
            {
                r->playerVote[playerToRoll->GetObjectGuid()] = ROLL_NOT_EMITED_YET;
                ++r->totalPlayersRolling;
            }
        }
    }

    if (r->totalPlayersRolling > 0)                         // has looters
    {
        r->setLoot(loot);
        r->itemSlot = itemSlot;

        if (r->totalPlayersRolling == 1)                    // single looter
        {
            r->playerVote.begin()->second = ROLL_NEED;
        }
        else
        {
            // Only GO-group looting and NPC-group looting possible
            MANGOS_ASSERT(lootTarget->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT));

            r->CalculateCommonVoteMask(maxEnchantingSkill); // dependent from item and possible skill

            SendLootStartRoll(LOOT_ROLL_TIMEOUT, lootTarget->GetMapId(), *r);
            loot->items[itemSlot].is_blocked = true;

            lootTarget->StartGroupLoot(this, LOOT_ROLL_TIMEOUT);
        }

        RollId.push_back(r);
    }
    else                                            // no looters??
    {
        delete r;
    }
}

// called when roll timer expires
void Group::EndRoll()
{
    while (!RollId.empty())
    {
        // need more testing here, if rolls disappear
        Rolls::iterator itr = RollId.begin();
        CountTheRoll(itr);                                  // i don't have to edit player votes, who didn't vote ... he will pass
    }
}

/**
 * @brief Resolves a completed loot roll and awards or unlocks the item.
 *
 * @param rollI Iterator pointing to the roll entry.
 */
void Group::CountTheRoll(Rolls::iterator& rollI)
{
    Roll* roll = *rollI;

    if (!roll->isValid())                                   // is loot already deleted ?
    {
        rollI = RollId.erase(rollI);
        delete roll;
        return;
    }

    // end of the roll
    if (roll->totalNeed > 0)
    {
        if (!roll->playerVote.empty())
        {
            uint32 maxresul = 0;
            ObjectGuid maxguid  = (*roll->playerVote.begin()).first;
            Player* player;

            for (Roll::PlayerVote::const_iterator itr = roll->playerVote.begin(); itr != roll->playerVote.end(); ++itr)
            {
                if (itr->second != ROLL_NEED)
                {
                    continue;
                }

                uint32 randomN = urand(1, 100);
                SendLootRoll(itr->first, randomN, ROLL_NEED, *roll);
                if (maxresul < randomN)
                {
                    maxguid  = itr->first;
                    maxresul = randomN;
                }
            }
            SendLootRollWon(maxguid, maxresul, ROLL_NEED, *roll);
            player = sObjectMgr.GetPlayer(maxguid);

            if (player && player->GetSession())
            {
                player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED_ON_LOOT, roll->itemid, maxresul);

                ItemPosCountVec dest;
                LootItem* item = &(roll->getLoot()->items[roll->itemSlot]);
                InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, roll->itemid, item->count);
                if (msg == EQUIP_ERR_OK)
                {
                    item->is_looted = true;
                    roll->getLoot()->NotifyItemRemoved(roll->itemSlot);
                    --roll->getLoot()->unlootedCount;
                    player->StoreNewItem(dest, roll->itemid, true, item->randomPropertyId);
                    player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM, roll->itemid, item->count);
                    player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE, roll->getLoot()->loot_type, item->count);
                    player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM, roll->itemid, item->count);
                }
                else
                {
                    item->is_blocked = false;
                    player->SendEquipError(msg, NULL, NULL, roll->itemid);
                }
            }
        }
    }
    else if (roll->totalGreed > 0)
    {
        if (!roll->playerVote.empty())
        {
            uint32 maxresul = 0;
            ObjectGuid maxguid = (*roll->playerVote.begin()).first;
            Player* player;
            RollVote rollvote = ROLL_PASS;                  // Fixed: Using uninitialized memory 'rollvote'

            Roll::PlayerVote::iterator itr;
            for (itr = roll->playerVote.begin(); itr != roll->playerVote.end(); ++itr)
            {
                if (itr->second != ROLL_GREED && itr->second != ROLL_DISENCHANT)
                {
                    continue;
                }

                uint32 randomN = urand(1, 100);
                SendLootRoll(itr->first, randomN, itr->second, *roll);
                if (maxresul < randomN)
                {
                    maxguid  = itr->first;
                    maxresul = randomN;
                    rollvote = itr->second;
                }
            }
            SendLootRollWon(maxguid, maxresul, rollvote, *roll);
            player = sObjectMgr.GetPlayer(maxguid);

            if (player && player->GetSession())
            {
                player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED_ON_LOOT, roll->itemid, maxresul);

                LootItem* item = &(roll->getLoot()->items[roll->itemSlot]);

                if (rollvote == ROLL_GREED)
                {
                    ItemPosCountVec dest;
                    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, roll->itemid, item->count);
                    if (msg == EQUIP_ERR_OK)
                    {
                        item->is_looted = true;
                        roll->getLoot()->NotifyItemRemoved(roll->itemSlot);
                        --roll->getLoot()->unlootedCount;
                        player->StoreNewItem(dest, roll->itemid, true, item->randomPropertyId);
                        player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM, roll->itemid, item->count);
                        player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE, roll->getLoot()->loot_type, item->count);
                        player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM, roll->itemid, item->count);
                    }
                    else
                    {
                        item->is_blocked = false;
                        player->SendEquipError(msg, NULL, NULL, roll->itemid);
                    }
                }
                else if (rollvote == ROLL_DISENCHANT)
                {
                    item->is_looted = true;
                    roll->getLoot()->NotifyItemRemoved(roll->itemSlot);
                    --roll->getLoot()->unlootedCount;

                    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(roll->itemid);
                    player->AutoStoreLoot(roll->getLoot()->GetLootTarget(), pProto->DisenchantID, LootTemplates_Disenchant, true);
                }
            }
        }
    }
    else
    {
        SendLootAllPassed(*roll);
        LootItem* item = &(roll->getLoot()->items[roll->itemSlot]);
        if (item)
        {
            item->is_blocked = false;
        }
    }

    rollI = RollId.erase(rollI);
    delete roll;
}

/**
 * @brief Sets or clears a raid target icon and broadcasts the change.
 *
 * @param id The icon slot index.
 * @param targetGuid The target GUID assigned to the icon.
 */
void Group::SetTargetIcon(uint8 id, ObjectGuid whoGuid, ObjectGuid targetGuid,
    uint8 context)
{
    if (id >= TARGET_ICON_COUNT)
    {
        return;
    }

    // clean other icons
    if (targetGuid)
        for (int i = 0; i < TARGET_ICON_COUNT; ++i)
            if (m_targetIcons[i] == targetGuid)
            {
                SetTargetIcon(i, ObjectGuid(), ObjectGuid(), context);
            }

    m_targetIcons[id] = targetGuid;

    WorldPacket data;
    MopGroupMarkerPackets::BuildRaidTargetSingle(data,
        whoGuid.GetRawValue(), targetGuid.GetRawValue(), id, context);
    BroadcastPacket(&data, true);
}

/**
 * @brief Accumulates group XP reward data for a single qualifying player.
 *
 * @param player The player contributing to the calculation.
 * @param victim The defeated unit.
 * @param sum_level Running sum of qualifying player levels.
 * @param member_with_max_level Tracks the highest-level qualifying member.
 * @param not_gray_member_with_max_level Tracks the highest-level non-gray qualifying member.
 */
static void GetDataForXPAtKill_helper(Player* player, Unit const* victim, uint32& sum_level, Player*& member_with_max_level, Player*& not_gray_member_with_max_level)
{
    sum_level += player->getLevel();
    if (!member_with_max_level || member_with_max_level->getLevel() < player->getLevel())
    {
        member_with_max_level = player;
    }

    uint32 gray_level = MaNGOS::XP::GetGrayLevel(player->getLevel());
    if (victim->getLevel() > gray_level && (!not_gray_member_with_max_level
                                            || not_gray_member_with_max_level->getLevel() < player->getLevel()))
    {
        not_gray_member_with_max_level = player;
    }
}

/**
 * @brief Collects qualifying group member data used for XP distribution on kill.
 *
 * @param victim The defeated unit.
 * @param count Running count of qualifying players.
 * @param sum_level Running sum of qualifying player levels.
 * @param member_with_max_level Tracks the highest-level qualifying member.
 * @param not_gray_member_with_max_level Tracks the highest-level non-gray qualifying member.
 * @param additional Optional extra player to include after iterating group members.
 */
void Group::GetDataForXPAtKill(Unit const* victim, uint32& count, uint32& sum_level, Player*& member_with_max_level, Player*& not_gray_member_with_max_level, Player* additional)
{
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* member = itr->getSource();
        if (!member || !member->IsAlive())                  // only for alive
        {
            continue;
        }

        // will proccesed later
        if (member == additional)
        {
            continue;
        }

        if (!member->IsAtGroupRewardDistance(victim))       // at req. distance
        {
            continue;
        }

        ++count;
        GetDataForXPAtKill_helper(member, victim, sum_level, member_with_max_level, not_gray_member_with_max_level);
    }

    if (additional)
    {
        if (additional->IsAtGroupRewardDistance(victim))    // at req. distance
        {
            ++count;
            GetDataForXPAtKill_helper(additional, victim, sum_level, member_with_max_level, not_gray_member_with_max_level);
        }
    }
}

/**
 * @brief Sends the current raid target icon assignments to a session.
 *
 * @param session The session receiving the icon list.
 */
void Group::SendTargetIconList(WorldSession* session)
{
    if (!session)
    {
        return;
    }

    std::vector<MopGroupMarkerPackets::TargetIcon> targets;

    for (int i = 0; i < TARGET_ICON_COUNT; ++i)
    {
        if (!m_targetIcons[i])
        {
            continue;
        }

        MopGroupMarkerPackets::TargetIcon target;
        target.icon = uint8(i);
        target.targetGuid = m_targetIcons[i].GetRawValue();
        targets.push_back(target);
    }

    Player* player = session->GetPlayer();
    uint8 const context = player && player->GetOriginalGroup() != this &&
        player->GetGroup() == this && (isBGGroup() || isLFGGroup()) ? 1 : 0;
    WorldPacket data;
    if (MopGroupMarkerPackets::BuildRaidTargetAll(data, targets, context))
        session->SendPacket(&data);
}

/**
 * @brief Sends a full group list update to every connected member.
 */
void Group::SendUpdate()
{
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
        SendUpdateToPlayer(citr->guid);
}

void Group::SendUpdateToPlayer(ObjectGuid guid)
{
    member_citerator recipient = _getMemberCSlot(guid);
    if (recipient == m_memberSlots.end())
        return;

    Player* player = sObjectMgr.GetPlayer(guid);
    if (!player || !player->GetSession() ||
        (player->GetGroup() != this && player->GetOriginalGroup() != this))
        return;

    MopPartyUpdatePackets::PartyUpdate update;
    update.groupGuid = GetObjectGuid().GetRawValue();
    update.leaderGuid = m_leaderGuid.GetRawValue();
    update.looterGuid = m_looterGuid.GetRawValue();
    update.hasInstanceDifficulty = true;
    // SMSG_GROUP_LIST reports both tiers to the client, so both are RAW client DifficultyIDs
    // on the wire. Sending the internal mode made the party frame disagree with the difficulty
    // the same client had just been told over SMSG_SET_DUNGEON_DIFFICULTY, which does convert:
    // an internal 1 (heroic) arrived as raw 1, which the client reads as normal.
    update.raidDifficulty = ToClientDifficulty(m_raidDifficulty, true);
    update.dungeonDifficulty = ToClientDifficulty(m_dungeonDifficulty, false);
    update.hasLootMode = true;
    update.lootMethod = uint8(m_lootMethod);
    update.lootThreshold = uint8(m_lootThreshold);
    // isLfg only when we can actually name the dungeon.
    //
    // GROUPTYPE_LFD is one-way: SetAsLfgGroup only ORs it in, it is set at PROPOSAL
    // CREATION -- before anyone has answered -- and nothing in the tree ever clears it.
    // So a declined or expired proposal leaves an ordinary party flagged for the rest of
    // its life, and every SendUpdate then advertised an LFG block whose dungeon slot
    // resolved through a group status that no longer exists, i.e. isLfg = 1 with A = 0.
    //
    // That is the state the note below calls worse than sending no block at all: the
    // client copies it, party+232 becomes 0, and IsPartyLFG() goes false -- while the
    // party keeps claiming to be a finder group in every other respect.
    //
    // Gating on the entry rather than clearing the flag: the flag is also what
    // Group::Disband keys its LFG status release on, and what marks the party for the
    // cooldown waiver, so clearing it would cost more than it fixes.
    update.isLfg = isLFGGroup() && sLFGMgr.GetGroupDungeonEntry(GetObjectGuid()) != 0;
    if (update.isLfg)
    {
        // The LFG block has TWO dungeon slots and they are not interchangeable.
        //
        // Slot A (lfgDungeonEntry) is the gating one: it is what the client copies to
        // party+232, which is precisely what IsPartyLFG() tests and GetPartyLFGID()
        // returns, and every UI gate for the minimap eye and the Leave Dungeon entries
        // runs through it. It carries type 1 in all 8475 retail packets whose block is
        // populated -- never type 6 -- so it must be the RESOLVED dungeon. Ours is,
        // because LFGGroupStatus records the concrete dungeon CreateDungeonGroup ran.
        //
        // Slot B (lfgTail) carries the random category, type 6, and is 0 for a direct
        // queue. Worked example, capture-000044 seq 6287, block at payload offset 0x6E:
        //   00 00 80 3F | 01 | 00 | 88 00 00 01 | 00 00 04 | 03 01 00 06
        //   float=1.0   | b0 | b1 | A=0x01000088 | b2 b3 b4 | B=0x06000103
        // An earlier revision of this comment cited 03 01 00 06 as evidence for slot A.
        // Those bytes are slot B. The code was right and the citation was not.
        //
        // Sending isLfg with a zero A is WORSE than sending no block: the client copies
        // it, party+232 becomes 0, and IsPartyLFG() is then false -- indistinguishable
        // from having no LFG party at all.
        update.lfgDungeonEntry = sLFGMgr.GetGroupDungeonEntry(GetObjectGuid());
        update.lfgTail = sLFGMgr.GetGroupRandomDungeonEntry(GetObjectGuid());

        // b0 is the LFG state, and the client reads bit 0x02 of it as IsLFGComplete()
        // (sub_90261A: *(party+228) & 2). Retail flips it 1 -> 2 at DUNGEON_FINISHED
        // (capture-000720 seq 1074 -> 46476). We sent 0 always, so IsLFGComplete() was
        // permanently false and UIParent.lua:4176's `IsPartyLFG() and not IsLFGComplete()`
        // always fired the deserter warning on leaving.
        LFGState const lfgState = sLFGMgr.GetGroupLfgState(GetObjectGuid());
        update.lfgUnknownByte0 = (lfgState == LFG_STATE_FINISHED_DUNGEON) ? 2 : 1;

        // b4 tracks the member count, observed as n-1 in 5192 of the sampled rows.
        update.lfgUnknownByte4 = m_memberSlots.empty() ? 0 : uint8(m_memberSlots.size() - 1);
    }
    // Retail's LFG groups send groupType 0x0C, i.e. GROUPTYPE_LFD (0x08) plus 0x04.
    // Bit 0x04 is what the client returns from HasLFGRestrictions() (sub_9025EA reads
    // party+216 & 4). We stored and sent 0x08 alone, so every LFG group reported having
    // no restrictions. Set on the WIRE value only -- m_groupType is persisted and used
    // in server-side logic, and widening the stored enum would change both.
    update.groupType = uint8(m_groupType);
    if (update.isLfg)
    {
        update.groupType |= 0x04;
    }
    update.partyIndex = player->GetOriginalGroup() == this ? 0 :
        uint8(isBGGroup() || isLFGGroup());
    update.sequence = m_groupUpdateCounter;

    uint32 position = 0;
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        if (citr->group == recipient->group)
        {
            if (citr->guid == recipient->guid)
                update.groupPosition = int32(position);
            ++position;
        }

        Player* member = sObjectMgr.GetPlayer(citr->guid);
        MopPartyUpdatePackets::Member record;
        record.guid = citr->guid.GetRawValue();
        record.name = citr->name;
        record.status = member ? MEMBER_STATUS_ONLINE : MEMBER_STATUS_OFFLINE;
        if (isBGGroup())
            record.status |= MEMBER_STATUS_PVP;
        record.subgroup = citr->group;
        record.flags = uint8(GetFlags(*citr));
        // The roster has always carried a per-member role byte; nothing ever
        // filled it, so every member reported "no role" regardless of choice.
        record.roles = citr->roles;
        update.members.push_back(record);
    }

    WorldPacket data;
    if (MopPartyUpdatePackets::BuildPartyUpdate(data, update))
    {
        ++m_groupUpdateCounter;
        player->GetSession()->SendPacket(&data);
    }
}

void Group::SendRemovedUpdate(Player* player)
{
    if (!player || !player->GetSession())
        return;

    MopPartyUpdatePackets::PartyUpdate update;
    update.groupGuid = GetObjectGuid().GetRawValue();
    update.groupType = 0x10;
    update.partyIndex = uint8(isBGGroup() || isLFGGroup());
    update.groupPosition = -1;
    update.sequence = m_groupUpdateCounter;

    WorldPacket data;
    if (MopPartyUpdatePackets::BuildRemovedPartyUpdate(data, update))
    {
        ++m_groupUpdateCounter;
        player->GetSession()->SendPacket(&data);
    }
}

/**
 * @brief Sends updated party member stats to members who do not currently see the player.
 *
 * @param pPlayer The player whose stats changed.
 */
void Group::UpdatePlayerOutOfRange(Player* pPlayer)
{
    if (!pPlayer || !pPlayer->IsInWorld())
    {
        return;
    }

    if (pPlayer->GetGroupUpdateFlag() == GROUP_UPDATE_FLAG_NONE)
    {
        return;
    }

    WorldPacket data;
    pPlayer->GetSession()->BuildPartyMemberStatsChangedPacket(pPlayer, &data);

    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
        if (Player* player = itr->getSource())
            if (player != pPlayer && !player->HaveAtClient(pPlayer))
            {
                player->GetSession()->SendPacket(&data);
            }
}

/**
 * @brief Broadcasts a packet to group members with optional subgroup and ignore filters.
 *
 * @param packet The packet to broadcast.
 * @param ignorePlayersInBGRaid True to skip players whose active group differs from this one.
 * @param group The subgroup filter, or -1 for all members.
 * @param ignore A player GUID to exclude from delivery.
 */
void Group::BroadcastPacket(WorldPacket* packet, bool ignorePlayersInBGRaid, int group, ObjectGuid ignore)
{
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pl = itr->getSource();
        if (!pl || (ignore && pl->GetObjectGuid() == ignore) || (ignorePlayersInBGRaid && pl->GetGroup() != this))
        {
            continue;
        }

        if (pl->GetSession() && (group == -1 || itr->getSubGroup() == group))
        {
            pl->GetSession()->SendPacket(packet);
        }
    }
}

void Group::BroadcastAddonMessagePacket(WorldPacket* packet,
    std::string const& prefix, bool ignorePlayersInBGRaid, int group,
    ObjectGuid ignore)
{
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pl = itr->getSource();
        if (!pl || (ignore && pl->GetObjectGuid() == ignore) ||
            (ignorePlayersInBGRaid && pl->GetGroup() != this))
        {
            continue;
        }

        WorldSession* session = pl->GetSession();
        if (session && (group == -1 || itr->getSubGroup() == group) &&
            session->IsAddonRegistered(prefix))
        {
            session->SendPacket(packet);
        }
    }
}

/**
 * @brief Sends a ready-check packet to the leader and assistants.
 *
 * @param packet The ready-check packet to broadcast.
 */
void Group::BroadcastReadyCheck(WorldPacket* packet)
{
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pl = itr->getSource();
        if (pl && pl->GetSession())
            if (IsLeader(pl->GetObjectGuid()) || IsAssistant(pl->GetObjectGuid()))
            {
                pl->GetSession()->SendPacket(packet);
            }
    }
}

/**
 * @brief Marks offline members as not ready during a ready check.
 */
void Group::OfflineReadyCheck()
{
    if (!m_readyCheckActive)
        return;

    for (member_witerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        Player* pl = sObjectMgr.GetPlayer(citr->guid);
        if ((!pl || !pl->GetSession()) && !citr->readyCheckHasResponded)
        {
            WorldPacket data;
            MopReadyCheckPackets::BuildResponse(data,
                GetObjectGuid().GetRawValue(), citr->guid.GetRawValue(), false);
            BroadcastReadyCheck(&data);
            citr->readyCheckHasResponded = true;
        }
    }
}

bool Group::StartReadyCheck(uint8 partyIndex, ObjectGuid initiator)
{
    if (m_readyCheckActive || _getMemberCSlot(initiator) == m_memberSlots.end())
        return false;

    m_readyCheckActive = true;
    m_readyCheckPartyIndex = partyIndex;
    m_readyCheckInitiator = initiator;
    for (member_witerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
        itr->readyCheckHasResponded = false;
    return true;
}

bool Group::ReadyCheckMemberHasResponded(ObjectGuid guid)
{
    if (!m_readyCheckActive)
        return false;

    member_witerator itr = _getMemberWSlot(guid);
    if (itr == m_memberSlots.end() || itr->readyCheckHasResponded)
        return false;

    itr->readyCheckHasResponded = true;
    return true;
}

bool Group::ReadyCheckAllResponded() const
{
    if (!m_readyCheckActive)
        return false;

    for (member_citerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
        if (!itr->readyCheckHasResponded)
            return false;
    return true;
}

void Group::CompleteReadyCheck()
{
    if (!m_readyCheckActive)
        return;

    WorldPacket data;
    MopReadyCheckPackets::BuildCompleted(data,
        GetObjectGuid().GetRawValue(), m_readyCheckPartyIndex);
    BroadcastPacket(&data, false);

    if (Player* initiator = sObjectMgr.GetPlayer(m_readyCheckInitiator))
        initiator->SetReadyCheckTimer(0);

    m_readyCheckActive = false;
    m_readyCheckPartyIndex = 0;
    m_readyCheckInitiator.Clear();
    for (member_witerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
        itr->readyCheckHasResponded = false;
}
