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

#ifndef MANGOSSERVER_GUILD_H
#define MANGOSSERVER_GUILD_H

#define WITHDRAW_MONEY_UNLIMITED    UI64LIT(0xFFFFFFFFFFFFFFFF)
#define WITHDRAW_SLOT_UNLIMITED     0xFFFFFFFF

#include "Common.h"
#include "Item.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <string>

class WorldPacket;

namespace MopGuildPackets
{
    struct EmblemDesign
    {
        uint64 vendorGuid = 0;
        uint32 emblemStyle = 0;
        uint32 emblemColor = 0;
        uint32 borderStyle = 0;
        uint32 borderColor = 0;
        uint32 backgroundColor = 0;
    };

    inline uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (index * 8));
    }

    inline uint64 ReadGuid(WorldPacket& in, uint8 const (&maskOrder)[8],
        uint8 const (&byteOrder)[8])
    {
        uint8 guidBytes[8] = {};
        for (uint8 index : maskOrder)
            guidBytes[index] = in.ReadBit();
        for (uint8 index : byteOrder)
            in.ReadByteSeq(guidBytes[index]);

        uint64 guid = 0;
        for (uint8 index = 0; index < 8; ++index)
            guid |= uint64(guidBytes[index]) << (index * 8);
        return guid;
    }

    inline uint64 ReadTabardVendorActivate(WorldPacket& in)
    {
        uint8 const maskOrder[] = { 2, 1, 4, 6, 3, 5, 0, 7 };
        uint8 const byteOrder[] = { 7, 6, 2, 5, 0, 1, 4, 3 };
        return ReadGuid(in, maskOrder, byteOrder);
    }

    inline void BuildTabardVendorActivate(WorldPacket& out, uint64 vendorGuid)
    {
        uint8 const maskOrder[] = { 1, 5, 0, 7, 4, 6, 3, 2 };
        uint8 const byteOrder[] = { 5, 4, 2, 3, 6, 0, 1, 7 };

        out.Initialize(SMSG_TABARD_VENDOR_ACTIVATE, 9);
        for (uint8 index : maskOrder)
            out.WriteBit(GuidByte(vendorGuid, index) != 0);
        out.FlushBits();
        for (uint8 index : byteOrder)
            out.WriteByteSeq(GuidByte(vendorGuid, index));
    }

    inline EmblemDesign ReadSaveGuildEmblem(WorldPacket& in)
    {
        EmblemDesign design;
        in >> design.borderStyle;
        in >> design.backgroundColor;
        in >> design.borderColor;
        in >> design.emblemColor;
        in >> design.emblemStyle;

        uint8 const maskOrder[] = { 0, 7, 4, 6, 5, 1, 2, 3 };
        uint8 const byteOrder[] = { 6, 2, 7, 5, 0, 4, 1, 3 };
        design.vendorGuid = ReadGuid(in, maskOrder, byteOrder);
        return design;
    }

    inline void BuildSaveGuildEmblemResult(WorldPacket& out, uint32 result)
    {
        out.Initialize(SMSG_SAVE_GUILD_EMBLEM, 4);
        out << result;
    }

    inline bool BuildGuildMotd(WorldPacket& out, std::string const& motd)
    {
        if (motd.size() >= (size_t(1) << 10))
            return false;

        out.WriteBits(motd.size(), 10);
        out.FlushBits();
        out.append(motd.data(), motd.size());
        return true;
    }

    template <size_t N>
    inline void WriteGuidMask(WorldPacket& out, uint64 guid,
        uint8 const (&order)[N])
    {
        for (uint8 index : order)
            out.WriteBit(GuidByte(guid, index) != 0);
    }

    template <size_t N>
    inline void WriteGuidBytes(WorldPacket& out, uint64 guid,
        uint8 const (&order)[N])
    {
        for (uint8 index : order)
            out.WriteByteSeq(GuidByte(guid, index));
    }

    inline bool FitsGuildEventName(std::string const& name)
    {
        return name.size() < (size_t(1) << 6);
    }

    inline bool BuildGuildMemberJoined(WorldPacket& out, uint64 memberGuid,
        std::string const& memberName, uint32 virtualRealm)
    {
        if (!FitsGuildEventName(memberName))
            return false;

        uint8 const firstMask[] = { 6, 1, 3 };
        uint8 const secondMask[] = { 7, 4, 2, 5, 0 };
        uint8 const firstBytes[] = { 2, 4, 1, 6, 5 };
        uint8 const secondBytes[] = { 3, 0 };
        uint8 const lastByte[] = { 7 };

        out.Initialize(SMSG_GUILD_EVENT_PLAYER_JOINED,
            2 + 8 + 4 + memberName.size());
        WriteGuidMask(out, memberGuid, firstMask);
        out.WriteBits(memberName.size(), 6);
        WriteGuidMask(out, memberGuid, secondMask);
        out.FlushBits();
        WriteGuidBytes(out, memberGuid, firstBytes);
        out << virtualRealm;
        WriteGuidBytes(out, memberGuid, secondBytes);
        out.append(memberName.data(), memberName.size());
        WriteGuidBytes(out, memberGuid, lastByte);
        return true;
    }

    inline bool BuildGuildPresenceChange(WorldPacket& out, uint64 playerGuid,
        std::string const& playerName, uint32 virtualRealm, bool loggedOn,
        bool mobile)
    {
        if (!FitsGuildEventName(playerName))
            return false;

        uint8 const firstMask[] = { 0, 6 };
        uint8 const secondMask[] = { 2, 5, 3 };
        uint8 const thirdMask[] = { 1, 7, 4 };
        uint8 const firstBytes[] = { 3, 2, 0 };
        uint8 const secondBytes[] = { 6 };
        uint8 const thirdBytes[] = { 4, 5, 7, 1 };

        out.Initialize(SMSG_GUILD_EVENT_PRESENCE_CHANGE,
            2 + 8 + 4 + playerName.size());
        WriteGuidMask(out, playerGuid, firstMask);
        out.WriteBit(mobile);
        WriteGuidMask(out, playerGuid, secondMask);
        out.WriteBits(playerName.size(), 6);
        WriteGuidMask(out, playerGuid, thirdMask);
        out.WriteBit(loggedOn);
        out.FlushBits();
        WriteGuidBytes(out, playerGuid, firstBytes);
        out << virtualRealm;
        WriteGuidBytes(out, playerGuid, secondBytes);
        out.append(playerName.data(), playerName.size());
        WriteGuidBytes(out, playerGuid, thirdBytes);
        return true;
    }

    inline void BuildGuildMemberRankUpdate(WorldPacket& out,
        uint64 issuerGuid, uint64 targetGuid, uint32 newRankId, bool promoted)
    {
        uint8 const targetMask1[] = { 5, 6 };
        uint8 const issuerMask1[] = { 0, 1 };
        uint8 const targetMask2[] = { 3 };
        uint8 const issuerMask2[] = { 4 };
        uint8 const targetMask3[] = { 2 };
        uint8 const issuerMask3[] = { 6, 3, 7 };
        uint8 const targetMask4[] = { 4, 0, 1 };
        uint8 const issuerMask4[] = { 2 };
        uint8 const targetMask5[] = { 7 };
        uint8 const issuerMask5[] = { 5 };

        out.Initialize(SMSG_GUILD_RANKS_UPDATE, 3 + 8 + 8 + 4);
        WriteGuidMask(out, targetGuid, targetMask1);
        WriteGuidMask(out, issuerGuid, issuerMask1);
        WriteGuidMask(out, targetGuid, targetMask2);
        WriteGuidMask(out, issuerGuid, issuerMask2);
        WriteGuidMask(out, targetGuid, targetMask3);
        WriteGuidMask(out, issuerGuid, issuerMask3);
        WriteGuidMask(out, targetGuid, targetMask4);
        WriteGuidMask(out, issuerGuid, issuerMask4);
        WriteGuidMask(out, targetGuid, targetMask5);
        out.WriteBit(promoted);
        WriteGuidMask(out, issuerGuid, issuerMask5);
        out.FlushBits();

        out.WriteByteSeq(GuidByte(targetGuid, 2));
        out.WriteByteSeq(GuidByte(issuerGuid, 1));
        out.WriteByteSeq(GuidByte(targetGuid, 6));
        out.WriteByteSeq(GuidByte(targetGuid, 1));
        out.WriteByteSeq(GuidByte(targetGuid, 5));
        out.WriteByteSeq(GuidByte(issuerGuid, 0));
        out << newRankId;
        out.WriteByteSeq(GuidByte(issuerGuid, 3));
        out.WriteByteSeq(GuidByte(issuerGuid, 7));
        out.WriteByteSeq(GuidByte(targetGuid, 7));
        out.WriteByteSeq(GuidByte(issuerGuid, 2));
        out.WriteByteSeq(GuidByte(targetGuid, 3));
        out.WriteByteSeq(GuidByte(targetGuid, 4));
        out.WriteByteSeq(GuidByte(issuerGuid, 6));
        out.WriteByteSeq(GuidByte(issuerGuid, 5));
        out.WriteByteSeq(GuidByte(targetGuid, 0));
        out.WriteByteSeq(GuidByte(issuerGuid, 4));
    }

    inline bool BuildGuildNewLeader(WorldPacket& out, uint64 oldLeaderGuid,
        std::string const& oldLeaderName, uint32 oldLeaderRealm,
        uint64 newLeaderGuid, std::string const& newLeaderName,
        uint32 newLeaderRealm, bool selfPromoted)
    {
        if (!FitsGuildEventName(oldLeaderName) ||
                !FitsGuildEventName(newLeaderName))
            return false;

        out.Initialize(SMSG_GUILD_EVENT_NEW_LEADER,
            4 + 16 + 8 + oldLeaderName.size() + newLeaderName.size());
        uint8 const newMask1[] = { 4, 2, 7 };
        uint8 const oldMask1[] = { 4 };
        uint8 const oldMask2[] = { 0 };
        uint8 const newMask2[] = { 6, 3 };
        uint8 const newMask3[] = { 1, 0 };
        uint8 const oldMask3[] = { 1, 7, 3, 6, 2 };
        uint8 const oldMask4[] = { 5 };
        uint8 const newMask4[] = { 5 };

        WriteGuidMask(out, newLeaderGuid, newMask1);
        WriteGuidMask(out, oldLeaderGuid, oldMask1);
        out.WriteBits(oldLeaderName.size(), 6);
        WriteGuidMask(out, oldLeaderGuid, oldMask2);
        WriteGuidMask(out, newLeaderGuid, newMask2);
        out.WriteBit(selfPromoted);
        WriteGuidMask(out, newLeaderGuid, newMask3);
        WriteGuidMask(out, oldLeaderGuid, oldMask3);
        out.WriteBits(newLeaderName.size(), 6);
        WriteGuidMask(out, oldLeaderGuid, oldMask4);
        WriteGuidMask(out, newLeaderGuid, newMask4);
        out.FlushBits();

        out.WriteByteSeq(GuidByte(newLeaderGuid, 5));
        out.WriteByteSeq(GuidByte(newLeaderGuid, 6));
        out.append(oldLeaderName.data(), oldLeaderName.size());
        out.append(newLeaderName.data(), newLeaderName.size());
        out.WriteByteSeq(GuidByte(newLeaderGuid, 3));
        out.WriteByteSeq(GuidByte(newLeaderGuid, 4));
        out << newLeaderRealm;
        out.WriteByteSeq(GuidByte(oldLeaderGuid, 6));
        out.WriteByteSeq(GuidByte(newLeaderGuid, 0));
        out.WriteByteSeq(GuidByte(oldLeaderGuid, 5));
        out.WriteByteSeq(GuidByte(newLeaderGuid, 2));
        out.WriteByteSeq(GuidByte(newLeaderGuid, 7));
        out.WriteByteSeq(GuidByte(oldLeaderGuid, 7));
        out.WriteByteSeq(GuidByte(oldLeaderGuid, 4));
        out << oldLeaderRealm;
        out.WriteByteSeq(GuidByte(newLeaderGuid, 1));
        out.WriteByteSeq(GuidByte(oldLeaderGuid, 2));
        out.WriteByteSeq(GuidByte(oldLeaderGuid, 1));
        out.WriteByteSeq(GuidByte(oldLeaderGuid, 3));
        out.WriteByteSeq(GuidByte(oldLeaderGuid, 0));
        return true;
    }

    inline void BuildGuildDisbanded(WorldPacket& out)
    {
        out.Initialize(SMSG_GUILD_EVENT_DISBANDED, 0);
    }

    inline bool BuildGuildPlayerLeft(WorldPacket& out, uint64 leaverGuid,
        std::string const& leaverName, uint32 leaverRealm,
        bool removedByMember, uint64 removerGuid,
        std::string const& removerName, uint32 removerRealm)
    {
        if (!FitsGuildEventName(leaverName) ||
                (removedByMember &&
                    (!removerGuid || !FitsGuildEventName(removerName))) ||
                (!removedByMember &&
                    (removerGuid || !removerName.empty() || removerRealm)))
            return false;

        bool const hasRemover = removedByMember;
        out.Initialize(SMSG_GUILD_EVENT_PLAYER_LEFT,
            4 + 8 + 4 + leaverName.size() +
            (hasRemover ? 8 + 4 + removerName.size() : 0));

        out.WriteBit(GuidByte(leaverGuid, 2) != 0);
        out.WriteBits(leaverName.size(), 6);
        uint8 const leaverMask1[] = { 6, 5 };
        WriteGuidMask(out, leaverGuid, leaverMask1);
        out.WriteBit(hasRemover);
        if (hasRemover)
        {
            out.WriteBit(false);                           // remover name present
            out.WriteBit(false);                           // removed, not self-leave
            out.WriteBits(removerName.size(), 6);
            uint8 const removerMask[] = { 1, 3, 4, 2, 5, 7, 6, 0 };
            WriteGuidMask(out, removerGuid, removerMask);
            out.WriteBit(false);                           // remover realm present
        }
        uint8 const leaverMask2[] = { 1, 0, 3, 4, 7 };
        WriteGuidMask(out, leaverGuid, leaverMask2);
        out.FlushBits();

        if (hasRemover)
        {
            uint8 const removerBytes[] = { 1, 3, 5, 2, 0, 4, 6, 7 };
            WriteGuidBytes(out, removerGuid, removerBytes);
            out.append(removerName.data(), removerName.size());
            out << removerRealm;
        }
        out.append(leaverName.data(), leaverName.size());
        out.WriteByteSeq(GuidByte(leaverGuid, 1));
        out << leaverRealm;
        uint8 const leaverBytes[] = { 0, 4, 2, 3, 6, 5, 7 };
        WriteGuidBytes(out, leaverGuid, leaverBytes);
        return true;
    }
}

class Item;

#define GUILD_RANK_NONE         0xFF

enum GuildDefaultRanks
{
    // these ranks can be modified, but they can not be deleted
    GR_GUILDMASTER  = 0,
    GR_OFFICER      = 1,
    GR_VETERAN      = 2,
    GR_MEMBER       = 3,
    GR_INITIATE     = 4,
    // When promoting member server does: rank--;!
    // When demoting member server does: rank++;!
};

enum GuildRankRights
{
    GR_RIGHT_EMPTY              = 0x00000040,
    GR_RIGHT_GCHATLISTEN        = 0x00000041,
    GR_RIGHT_GCHATSPEAK         = 0x00000042,
    GR_RIGHT_OFFCHATLISTEN      = 0x00000044,
    GR_RIGHT_OFFCHATSPEAK       = 0x00000048,
    GR_RIGHT_PROMOTE            = 0x000000C0,
    GR_RIGHT_DEMOTE             = 0x00000140,
    GR_RIGHT_INVITE             = 0x00000050,
    GR_RIGHT_REMOVE             = 0x00000060,
    GR_RIGHT_SETMOTD            = 0x00001040,
    GR_RIGHT_EPNOTE             = 0x00002040,
    GR_RIGHT_VIEWOFFNOTE        = 0x00004040,
    GR_RIGHT_EOFFNOTE           = 0x00008040,
    GR_RIGHT_MODIFY_GUILD_INFO  = 0x00010040,
    GR_RIGHT_WITHDRAW_GOLD_LOCK = 0x00020000,               // remove money withdraw capacity
    GR_RIGHT_WITHDRAW_REPAIR    = 0x00040000,               // withdraw for repair
    GR_RIGHT_WITHDRAW_GOLD      = 0x00080000,               // withdraw gold
    GR_RIGHT_CREATE_GUILD_EVENT = 0x00100000,               // wotlk
    GR_RIGHT_REQUIRES_AUTHENTICATOR = 0x00200000,
    GR_RIGHT_MODIFY_BANK_TABS       = 0x00400000,               // cata?
    GR_RIGHT_REMOVE_GUILD_EVENT     = 0x00800000,               // wotlk
    GR_RIGHT_ALL                    = 0x00DDF1FF,
};

enum Typecommand
{
    GUILD_CREATE_S  = 0x00,
    GUILD_INVITE_S  = 0x01,
    GUILD_QUIT_S    = 0x03,
    // 0x05?
    GUILD_FOUNDER_S = 0x0E,
    GUILD_UNK1      = 0x14,
    GUILD_UNK2      = 0x15,
};

enum CommandErrors
{
    ERR_PLAYER_NO_MORE_IN_GUILD     = 0x00, // no message/error
    ERR_GUILD_INTERNAL              = 0x01,
    ERR_ALREADY_IN_GUILD            = 0x02,
    ERR_ALREADY_IN_GUILD_S          = 0x03,
    ERR_INVITED_TO_GUILD            = 0x04,
    ERR_ALREADY_INVITED_TO_GUILD_S  = 0x05,
    ERR_GUILD_NAME_INVALID          = 0x06,
    ERR_GUILD_NAME_EXISTS_S         = 0x07,
    ERR_GUILD_LEADER_LEAVE          = 0x08, // for Typecommand 0x03
    ERR_GUILD_PERMISSIONS           = 0x08, // for another Typecommand
    ERR_GUILD_PLAYER_NOT_IN_GUILD   = 0x09,
    ERR_GUILD_PLAYER_NOT_IN_GUILD_S = 0x0A,
    ERR_GUILD_PLAYER_NOT_FOUND_S    = 0x0B,
    ERR_GUILD_NOT_ALLIED            = 0x0C,
    ERR_GUILD_RANK_TOO_HIGH_S       = 0x0D,
    ERR_GUILD_RANK_TOO_LOW_S        = 0x0E,
    ERR_GUILD_RANKS_LOCKED          = 0x11,
    ERR_GUILD_RANK_IN_USE           = 0x12,
    ERR_GUILD_IGNORING_YOU_S        = 0x13,
    ERR_GUILD_UNK1                  = 0x14,
    ERR_GUILD_WITHDRAW_LIMIT        = 0x19,
    ERR_GUILD_NOT_ENOUGH_MONEY      = 0x1A,
    ERR_GUILD_BANK_FULL             = 0x1C,
    ERR_GUILD_ITEM_NOT_FOUND            = 0x1D,
    ERR_GUILD_TOO_MUCH_MONEY            = 0x1F,
    ERR_GUILD_BANK_WRONG_TAB            = 0x20,
    ERR_RANK_REQUIRES_AUTHENTICATOR     = 0x22,
    ERR_GUILD_BANK_VOUCHER_FAILED       = 0x23,
    ERR_GUILD_TRIAL_ACCOUNT             = 0x24,
    ERR_GUILD_UNDELETABLE_DUE_TO_LEVEL  = 0x25,
    ERR_GUILD_MOVE_STARTING             = 0x26,
    ERR_GUILD_REP_TOO_LOW               = 0x27,
};

enum PetitionTurns
{
    PETITION_TURN_OK                    = 0,
    PETITION_TURN_ALREADY_IN_GUILD      = 2,
    PETITION_TURN_NEED_MORE_SIGNATURES  = 4,
    PETITION_TURN_GUILD_PERMISSIONS     = 11,
    PETITION_TURN_GUILD_NAME_INVALID    = 12,
};

enum PetitionSigns
{
    PETITION_SIGN_OK                = 0,
    PETITION_SIGN_ALREADY_SIGNED    = 1,
    PETITION_SIGN_ALREADY_IN_GUILD  = 2,
    PETITION_SIGN_CANT_SIGN_OWN     = 3,
    PETITION_SIGN_NOT_SAME_SERVER       = 5,
    PETITION_SIGN_PETITION_FULL         = 8,
    PETITION_SIGN_ALREADY_SIGNED_OTHER  = 10,
    PETITION_SIGN_RESTRICTED_ACCOUNT    = 11,
};

enum GuildBankRights
{
    GUILD_BANK_RIGHT_VIEW_TAB       = 0x01,
    GUILD_BANK_RIGHT_PUT_ITEM       = 0x02,
    GUILD_BANK_RIGHT_UPDATE_TEXT    = 0x04,

    GUILD_BANK_RIGHT_DEPOSIT_ITEM   = GUILD_BANK_RIGHT_VIEW_TAB | GUILD_BANK_RIGHT_PUT_ITEM,
    GUILD_BANK_RIGHT_FULL           = 0xFF,
};

enum GuildBankEventLogTypes
{
    GUILD_BANK_LOG_DEPOSIT_ITEM     = 1,
    GUILD_BANK_LOG_WITHDRAW_ITEM    = 2,
    GUILD_BANK_LOG_MOVE_ITEM        = 3,
    GUILD_BANK_LOG_DEPOSIT_MONEY    = 4,
    GUILD_BANK_LOG_WITHDRAW_MONEY   = 5,
    GUILD_BANK_LOG_REPAIR_MONEY     = 6,
    GUILD_BANK_LOG_MOVE_ITEM2       = 7,
    GUILD_BANK_LOG_UNK1             = 8,
    GUILD_BANK_LOG_BUY_SLOT             = 9,
    GUILD_BANK_LOG_CASH_FLOW_DEPOSIT    = 10,
};

enum GuildEventLogTypes
{
    GUILD_EVENT_LOG_INVITE_PLAYER     = 1,
    GUILD_EVENT_LOG_JOIN_GUILD        = 2,
    GUILD_EVENT_LOG_PROMOTE_PLAYER    = 3,
    GUILD_EVENT_LOG_DEMOTE_PLAYER     = 4,
    GUILD_EVENT_LOG_UNINVITE_PLAYER   = 5,
    GUILD_EVENT_LOG_LEAVE_GUILD       = 6,
};

enum GuildEmblem
{
    ERR_GUILDEMBLEM_SUCCESS               = 0,
    ERR_GUILDEMBLEM_INVALID_TABARD_COLORS = 1,
    ERR_GUILDEMBLEM_NOGUILD               = 2,
    ERR_GUILDEMBLEM_NOTGUILDMASTER        = 3,
    ERR_GUILDEMBLEM_NOTENOUGHMONEY        = 4,
    ERR_GUILDEMBLEM_INVALIDVENDOR         = 5
};

enum GuildMemberFlags
{
    GUILDMEMBER_STATUS_NONE      = 0x0000,
    GUILDMEMBER_STATUS_ONLINE    = 0x0001,
    GUILDMEMBER_STATUS_AFK       = 0x0002,
    GUILDMEMBER_STATUS_DND       = 0x0004,
    GUILDMEMBER_STATUS_MOBILE    = 0x0008,
};

inline uint64 GetGuildBankTabPrice(uint8 Index)
{
    switch (Index)
    {
        case 0: return 100;
        case 1: return 250;
        case 2: return 500;
        case 3: return 1000;
        case 4: return 2500;
        case 5: return 5000;
        default:
            return 0;
    }
}

struct GuildEventLogEntry
{
    uint8  EventType;
    uint32 PlayerGuid1;
    uint32 PlayerGuid2;
    uint8  NewRank;
    uint64 TimeStamp;

    void WriteData(WorldPacket& data, ByteBuffer& buffer);
};

struct GuildBankEventLogEntry
{
    uint8  EventType;
    uint32 PlayerGuid;
    uint32 ItemOrMoney;
    uint8  ItemStackCount;
    uint8  DestTabId;
    uint64 TimeStamp;

    bool isMoneyEvent() const
    {
        return EventType == GUILD_BANK_LOG_DEPOSIT_MONEY ||
               EventType == GUILD_BANK_LOG_WITHDRAW_MONEY ||
               EventType == GUILD_BANK_LOG_REPAIR_MONEY;
    }

    void WriteData(WorldPacket& data, ByteBuffer& buffer);
};

struct GuildBankTab
{
    GuildBankTab() { memset(Slots, 0, GUILD_BANK_MAX_SLOTS * sizeof(Item*)); }

    Item* Slots[GUILD_BANK_MAX_SLOTS];
    std::string Name;
    std::string Icon;
    std::string Text;
};

struct GuildItemPosCount
{
    GuildItemPosCount(uint8 _slot, uint32 _count) : Slot(_slot), Count(_count) {}

    bool isContainedIn(std::vector<GuildItemPosCount> const& vec) const;

    uint8 Slot;
    uint32 Count;
};
typedef std::vector<GuildItemPosCount> GuildItemPosCountVec;

struct MemberSlot
{
    void SetMemberStats(Player* player);
    void UpdateLogoutTime();
    void SetPNOTE(std::string pnote);
    void SetOFFNOTE(std::string offnote);
    void ChangeRank(uint32 newRank);

    ObjectGuid guid;
    uint32 accountId;
    std::string Name;
    uint32 RankId;
    uint8 Level;
    uint8 Class;
    uint32 ZoneId;
    uint64 LogoutTime;
    std::string Pnote;
    std::string OFFnote;
    uint32 BankResetTimeMoney;
    uint32 BankRemMoney;
    uint32 BankResetTimeTab[GUILD_BANK_MAX_TABS];
    uint32 BankRemSlotsTab[GUILD_BANK_MAX_TABS];
};

struct RankInfo
{
    RankInfo(const std::string& _name, uint32 _rights, uint32 _money) : Name(_name), Rights(_rights), BankMoneyPerDay(_money)
    {
        for (uint8 i = 0; i < GUILD_BANK_MAX_TABS; ++i)
        {
            TabRight[i] = 0;
            TabSlotPerDay[i] = 0;
        }
    }

    std::string Name;
    uint32 Rights;
    uint32 BankMoneyPerDay;
    uint32 TabRight[GUILD_BANK_MAX_TABS];
    uint32 TabSlotPerDay[GUILD_BANK_MAX_TABS];
};

class Guild
{
    public:
        Guild();
        ~Guild();

        bool Create(Player* leader, std::string gname);
        void CreateDefaultGuildRanks(int locale_idx);
        void Disband();

        void DeleteGuildBankItems(bool alsoInDB = false);
        typedef std::unordered_map<uint32, MemberSlot> MemberList;
        typedef std::vector<RankInfo> RankList;

        uint32 GetId() const { return m_Id; }
        uint32 GetLevel() const { return m_Level; }
        ObjectGuid GetObjectGuid() const { return ObjectGuid(HIGHGUID_GUILD, 0, m_Id); }
        ObjectGuid GetLeaderGuid() const { return m_LeaderGuid; }
        std::string const& GetName() const { return m_Name; }
        std::string const& GetMOTD() const { return MOTD; }
        std::string const& GetGINFO() const { return GINFO; }

        time_t GetCreatedDate() const { return m_CreatedDate; }

        uint32 GetEmblemStyle() const { return m_EmblemStyle; }
        uint32 GetEmblemColor() const { return m_EmblemColor; }
        uint32 GetBorderStyle() const { return m_BorderStyle; }
        uint32 GetBorderColor() const { return m_BorderColor; }
        uint32 GetBackgroundColor() const { return m_BackgroundColor; }

        void SetLeader(ObjectGuid guid);
        bool AddMember(ObjectGuid plGuid, uint32 plRank);
        bool DelMember(ObjectGuid guid, bool isDisbanding = false);
        bool ChangeMemberRank(ObjectGuid guid, uint8 newRank);
        // lowest rank is the count of ranks - 1 (the highest rank_id in table)
        uint32 GetLowestRank() const { return m_Ranks.size() - 1; }

        void SetMOTD(std::string motd);
        void SetGINFO(std::string ginfo);
        void SetEmblem(uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor, uint32 backgroundColor);

        uint32 GetMemberSize() const { return members.size(); }
        uint32 GetAccountsNumber();

        bool LoadGuildFromDB(QueryResult* guildDataResult);
        bool CheckGuildStructure();
        bool LoadRanksFromDB(QueryResult* guildRanksResult);
        bool LoadMembersFromDB(QueryResult* guildMembersResult);

        void BroadcastToGuild(WorldSession* session, const std::string& msg, uint32 language = LANG_UNIVERSAL);
        void BroadcastAddonToGuild(WorldSession* session, const std::string& msg, const std::string& prefix);
        void BroadcastToOfficers(WorldSession* session, const std::string& msg, uint32 language = LANG_UNIVERSAL);
        void BroadcastAddonToOfficers(WorldSession* session, const std::string& msg, const std::string& prefix);
        void BroadcastPacketToRank(WorldPacket* packet, uint32 rankId);
        void BroadcastPacket(WorldPacket* packet);
        // for calendar
        void MassInviteToEvent(WorldSession* session, uint32 minLevel, uint32 maxLevel, uint32 minRank);

        void BroadcastMotd(std::string const& motd);
        void BroadcastMemberJoined(ObjectGuid guid, std::string const& name);
        void BroadcastMemberPresence(ObjectGuid guid, std::string const& name, bool loggedOn);
        void BroadcastMemberRankUpdate(ObjectGuid issuerGuid, ObjectGuid targetGuid,
            uint32 newRankId, bool promoted);
        void BroadcastNewLeader(ObjectGuid oldLeaderGuid, std::string const& oldLeaderName,
            ObjectGuid newLeaderGuid, std::string const& newLeaderName, bool selfPromoted = false);
        void BroadcastMemberLeft(ObjectGuid guid, std::string const& name);
        void BroadcastMemberRemoved(ObjectGuid guid, std::string const& name,
            ObjectGuid removerGuid, std::string const& removerName);
        void BroadcastDisbanded();

        template<class Do>
        void BroadcastWorker(Do& _do, Player* except = NULL)
        {
            for (MemberList::iterator itr = members.begin(); itr != members.end(); ++itr)
                if (Player* player = sObjectAccessor.FindPlayer(ObjectGuid(HIGHGUID_PLAYER, itr->first)))
                    if (player != except)
                    {
                        _do(player);
                    }
        }

        void CreateRank(std::string name, uint32 rights);
        void DelRank(uint32 rankId);
        void SwitchRank(uint32 rankId, bool up);
        std::string GetRankName(uint32 rankId);
        uint32 GetRankRights(uint32 rankId);
        uint32 GetRanksSize() const { return m_Ranks.size(); }

        void SetRankName(uint32 rankId, std::string name);
        void SetRankRights(uint32 rankId, uint32 rights);
        bool HasRankRight(uint32 rankId, uint32 right)
        {
            return ((GetRankRights(rankId) & right) != GR_RIGHT_EMPTY) ? true : false;
        }

        bool HasMembersWithRank(uint32 rankId) const
        {
            for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
                if (itr->second.RankId == rankId)
                {
                    return true;
                }

            return false;
        }

        int32 GetRank(ObjectGuid guid)
        {
            MemberSlot* slot = GetMemberSlot(guid);
            return slot ? slot->RankId : -1;
        }

        MemberSlot* GetMemberSlot(ObjectGuid guid)
        {
            MemberList::iterator itr = members.find(guid.GetCounter());
            return itr != members.end() ? &itr->second : NULL;
        }

        MemberSlot* GetMemberSlot(const std::string& name)
        {
            for (MemberList::iterator itr = members.begin(); itr != members.end(); ++itr)
                if (itr->second.Name == name)
                {
                    return &itr->second;
                }

            return NULL;
        }

        void Roster(WorldSession* session = NULL);          // NULL = broadcast
        void Query(WorldSession* session);
        void QueryRanks(WorldSession* session);

        // Guild EventLog
        void   LoadGuildEventLogFromDB();
        void   DisplayGuildEventLog(WorldSession* session);
        void   LogGuildEvent(uint8 EventType, ObjectGuid playerGuid1, ObjectGuid playerGuid2 = ObjectGuid(), uint8 newRank = 0);

        // ** Guild bank **
        // Content & item deposit/withdraw
        void   DisplayGuildBankContent(WorldSession* session, uint8 TabId);
        void   DisplayGuildBankMoneyUpdate(WorldSession* session);

        void   SwapItems(Player* pl, uint8 BankTab, uint8 BankTabSlot, uint8 BankTabDst, uint8 BankTabSlotDst, uint32 SplitedAmount);
        void   MoveFromBankToChar(Player* pl, uint8 BankTab, uint8 BankTabSlot, uint8 PlayerBag, uint8 PlayerSlot, uint32 SplitedAmount);
        void   MoveFromCharToBank(Player* pl, uint8 PlayerBag, uint8 PlayerSlot, uint8 BankTab, uint8 BankTabSlot, uint32 SplitedAmount);

        // Tabs
        void   DisplayGuildBankTabsInfo(WorldSession* session);
        void   CreateNewBankTab();
        void   SetGuildBankTabText(uint8 TabId, std::string text);
        void   SendGuildBankTabText(WorldSession* session, uint8 TabId);
        void   SetGuildBankTabInfo(uint8 TabId, std::string name, std::string icon);
        uint8  GetPurchasedTabs() const { return m_TabListMap.size(); }
        uint32 GetBankRights(uint32 rankId, uint8 TabId) const;
        bool   IsMemberHaveRights(uint32 LowGuid, uint8 TabId, uint32 rights) const;
        bool   CanMemberViewTab(uint32 LowGuid, uint8 TabId) const;
        // Load
        void   LoadGuildBankFromDB();
        // Money deposit/withdraw
        void   SendMoneyInfo(WorldSession* session, uint32 LowGuid);
        bool   MemberMoneyWithdraw(uint64 amount, uint32 LowGuid);
        uint64 GetGuildBankMoney() { return m_GuildBankMoney; }
        void   SetBankMoney(int64 money);
        // per days
        bool   MemberItemWithdraw(uint8 TabId, uint32 LowGuid);
        uint32 GetMemberSlotWithdrawRem(uint32 LowGuid, uint8 TabId);
        uint64 GetMemberMoneyWithdrawRem(uint32 LowGuid);
        void   SetBankMoneyPerDay(uint32 rankId, uint32 money);
        void   SetBankRightsAndSlots(uint32 rankId, uint8 TabId, uint32 right, uint32 SlotPerDay, bool db);
        uint32 GetBankMoneyPerDay(uint32 rankId);
        uint32 GetBankSlotPerDay(uint32 rankId, uint8 TabId);
        // rights per day
        bool   LoadBankRightsFromDB(QueryResult* guildBankTabRightsResult);
        // Guild Bank Event Logs
        void   LoadGuildBankEventLogFromDB();
        void   DisplayGuildBankLogs(WorldSession* session, uint8 TabId);
        void   LogBankEvent(uint8 EventType, uint8 TabId, uint32 PlayerGuidLow, uint32 ItemOrMoney, uint8 ItemStackCount = 0, uint8 DestTabId = 0);
        bool   AddGBankItemToDB(uint32 GuildId, uint32 BankTab , uint32 BankTabSlot , uint32 GUIDLow, uint32 Entry);

    protected:
        void AddRank(const std::string& name, uint32 rights, uint32 money);

        uint32 m_Id;
        uint32 m_Level;
        std::string m_Name;
        ObjectGuid m_LeaderGuid;
        std::string MOTD;
        std::string GINFO;
        time_t m_CreatedDate;

        uint32 m_EmblemStyle;
        uint32 m_EmblemColor;
        uint32 m_BorderStyle;
        uint32 m_BorderColor;
        uint32 m_BackgroundColor;
        uint32 m_accountsNumber;                            // 0 used as marker for need lazy calculation at request

        RankList m_Ranks;

        MemberList members;

        typedef std::vector<GuildBankTab*> TabListMap;
        TabListMap m_TabListMap;

        /** These are actually ordered lists. The first element is the oldest entry.*/
        typedef std::list<GuildEventLogEntry> GuildEventLog;
        typedef std::list<GuildBankEventLogEntry> GuildBankEventLog;
        GuildEventLog m_GuildEventLog;
        GuildBankEventLog m_GuildBankEventLog_Money;
        GuildBankEventLog m_GuildBankEventLog_Item[GUILD_BANK_MAX_TABS];

        uint32 m_GuildEventLogNextGuid;
        uint32 m_GuildBankEventLogNextGuid_Money;
        uint32 m_GuildBankEventLogNextGuid_Item[GUILD_BANK_MAX_TABS];

        uint64 m_GuildBankMoney;

    private:
        void UpdateAccountsNumber() { m_accountsNumber = 0;}// mark for lazy calculation at request in GetAccountsNumber
        void _ChangeRank(ObjectGuid guid, MemberSlot* slot, uint32 newRank);

        // used only from high level Swap/Move functions
        Item*  GetItem(uint8 TabId, uint8 SlotId);
        InventoryResult CanStoreItem(uint8 tab, uint8 slot, GuildItemPosCountVec& dest, uint32 count, Item* pItem, bool swap = false) const;
        Item*  StoreItem(uint8 tab, GuildItemPosCountVec const& pos, Item* pItem);
        void   RemoveItem(uint8 tab, uint8 slot);
        void   DisplayGuildBankContentUpdate(uint8 TabId, int32 slot1, int32 slot2 = -1);
        void   DisplayGuildBankContentUpdate(uint8 TabId, GuildItemPosCountVec const& slots);

        // internal common parts for CanStore/StoreItem functions
        void AppendDisplayGuildBankSlot(WorldPacket& data, ByteBuffer& buffer, GuildBankTab const* tab, int32 slot);
        InventoryResult _CanStoreItem_InSpecificSlot(uint8 tab, uint8 slot, GuildItemPosCountVec& dest, uint32& count, bool swap, Item* pSrcItem) const;
        InventoryResult _CanStoreItem_InTab(uint8 tab, GuildItemPosCountVec& dest, uint32& count, bool merge, Item* pSrcItem, uint8 skip_slot) const;
        Item* _StoreItem(uint8 tab, uint8 slot, Item* pItem, uint32 count, bool clone);
};
#endif
