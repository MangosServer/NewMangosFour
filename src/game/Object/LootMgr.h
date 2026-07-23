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

#ifndef MANGOS_LOOTMGR_H
#define MANGOS_LOOTMGR_H

#include "ItemEnchantmentMgr.h"
#include "ByteBuffer.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "Utilities/LinkedReference/RefManager.h"
#include "WorldPacket.h"

#include <array>
#include <map>
#include <vector>

class Player;
class LootStore;
class WorldObject;

#define MAX_NR_LOOT_ITEMS 16
// note: the client can not show more than 16 items total
#define MAX_NR_QUEST_ITEMS 32
// unrelated to the number of quest items shown, just for reserve

enum PermissionTypes
{
    ALL_PERMISSION    = 0,
    GROUP_PERMISSION  = 1,
    MASTER_PERMISSION = 2,
    OWNER_PERMISSION  = 3,                                  // for single player only loots
    NONE_PERMISSION   = 4
};

enum LootItemType
{
    LOOT_ITEM_TYPE_ITEM         = 0,
    LOOT_ITEM_TYPE_CURRENCY     = 1,
    LOOTITEM_TYPE_CONDITIONNAL  = 3,
    LOOTITEM_TYPE_CURRENCY      = 4,
};

enum LootType
{
    LOOT_CORPSE                 = 1,
    LOOT_PICKPOCKETING          = 2,
    LOOT_FISHING                = 3,
    LOOT_DISENCHANTING          = 4,
    // ignored always by client
    LOOT_SKINNING               = 6,                        // unsupported by client, sending LOOT_PICKPOCKETING instead
    LOOT_PROSPECTING            = 7,                        // unsupported by client, sending LOOT_PICKPOCKETING instead
    LOOT_MILLING                = 8,

    LOOT_FISHINGHOLE            = 20,                       // unsupported by client, sending LOOT_FISHING instead
    LOOT_FISHING_FAIL           = 21,                       // unsupported by client, sending LOOT_FISHING instead
    LOOT_INSIGNIA               = 22,                        // unsupported by client, sending LOOT_CORPSE instead
    LOOT_MAIL                   = 23,
    LOOT_SPELL                  = 24,
};

enum LootSlotType
{
    LOOT_SLOT_NORMAL  = 0,                                  // can be looted
    LOOT_SLOT_VIEW    = 1,                                  // can be only view (ignore any loot attempts)
    LOOT_SLOT_MASTER  = 2,                                  // can be looted only master (error message)
    LOOT_SLOT_REQS    = 3,                                  // can't be looted (error message about missing reqs)
    LOOT_SLOT_OWNER   = 4,                                  // ignore binding confirmation and etc, for single player looting
    MAX_LOOT_SLOT_TYPE                                      // custom, use for mark skipped from show items
};

namespace MopLootPackets
{
    constexpr size_t MAX_TAKE_ENTRIES = 50;
    constexpr size_t MAX_VISIBLE_ENTRIES = 255;
    constexpr size_t MAX_SITU_BYTES = 64;

    struct LootTakeEntry
    {
        uint64 lootGuid = 0;
        uint8 lootListId = 0;
    };

    struct LootItem
    {
        int32 randomSuffix = 0;
        uint32 count = 0;
        uint32 itemId = 0;
        std::vector<uint8> situ;
        int32 randomPropertyId = 0;
        uint32 displayInfoId = 0;
        uint8 slotType = 0;
        uint8 lootListId = 0;
        uint8 optionalByte = 0;
        uint8 unknown = 0;
        bool canTradeToTapList = false;
        bool hasOptionalByte = false;
        bool hasLootListId = true;
    };

    struct LootCurrency
    {
        uint32 amount = 0;
        uint32 currencyId = 0;
        uint8 slotType = 0;
        uint8 lootListId = 0;
    };

    struct LootResponse
    {
        uint64 sourceGuid = 0;
        uint64 lootGuid = 0;
        uint32 money = 0;
        uint8 lootMethod = 3;
        uint8 lootType = 0;
        uint8 failureReason = 0;
        uint8 lootThreshold = 0;
        bool hasLootMethod = false;
        bool hasLootType = false;
        bool hasFailureReason = false;
        bool hasLootThreshold = false;
        bool success = true;
        bool isAreaOfEffectLooting = false;
        std::vector<LootItem> items;
        std::vector<LootCurrency> currencies;
    };

    inline uint8 GuidByte(uint64 guid, size_t index)
    {
        return uint8(guid >> (index * 8));
    }

    inline bool RejectRequest(WorldPacket& in)
    {
        in.rfinish();
        return false;
    }

    inline size_t PresentByteCount(uint8 mask)
    {
        size_t count = 0;
        for (; mask != 0; mask >>= 1)
        {
            count += mask & 1;
        }
        return count;
    }

    inline bool ParseSingleGuidRequest(WorldPacket& in,
        std::array<uint8, 8> const& maskOrder,
        std::array<uint8, 8> const& byteOrder, uint64& guid)
    {
        if (in.size() - in.rpos() < 1)
        {
            return RejectRequest(in);
        }

        uint8 const mask = in[in.rpos()];
        if (in.size() - in.rpos() != 1 + PresentByteCount(mask))
        {
            return RejectRequest(in);
        }

        uint8 guidBytes[8] = {};
        for (uint8 index : maskOrder)
        {
            guidBytes[index] = in.ReadBit();
        }
        for (uint8 index : byteOrder)
        {
            in.ReadByteSeq(guidBytes[index]);
        }

        uint64 parsed = 0;
        for (size_t index = 0; index < 8; ++index)
        {
            parsed |= uint64(guidBytes[index]) << (index * 8);
        }
        guid = parsed;
        return in.rpos() == in.size();
    }

    // Both requests contain one GUID, but 18414 deliberately uses different
    // mask and XOR-byte permutations for opening and releasing a loot source.
    inline bool ParseLootRequest(WorldPacket& in, uint64& guid)
    {
        return ParseSingleGuidRequest(in,
            {{ 4, 5, 2, 7, 0, 1, 3, 6 }},
            {{ 3, 5, 0, 6, 4, 1, 7, 2 }}, guid);
    }

    inline bool ParseLootReleaseRequest(WorldPacket& in, uint64& guid)
    {
        return ParseSingleGuidRequest(in,
            {{ 7, 4, 2, 3, 0, 5, 6, 1 }},
            {{ 0, 6, 4, 2, 5, 3, 7, 1 }}, guid);
    }

    inline bool ParseLootMoneyRequest(WorldPacket& in)
    {
        if (in.rpos() != in.size())
        {
            return RejectRequest(in);
        }
        return true;
    }

    inline bool ParseAutostoreLootItemRequest(WorldPacket& in,
        std::vector<LootTakeEntry>& entries)
    {
        if (in.size() - in.rpos() < 3)
        {
            return RejectRequest(in);
        }

        uint32 const count = in.ReadBits(23);
        if (count == 0 || count > MAX_TAKE_ENTRIES)
        {
            return RejectRequest(in);
        }

        // The 23-bit count leaves one bit in its last byte. Every entry adds
        // exactly eight mask bits, so the final low bit is invariant padding.
        size_t const maskEnd = in.rpos() + count;
        if (maskEnd > in.size() || (in[maskEnd - 1] & 0x01) != 0)
        {
            return RejectRequest(in);
        }

        std::vector<std::array<uint8, 8> > guidBytes(count);
        size_t bytePhaseSize = count; // one raw loot-list slot per entry
        uint8 const maskOrder[] = { 2, 7, 0, 6, 5, 3, 1, 4 };
        for (uint32 entry = 0; entry < count; ++entry)
        {
            for (uint8 index : maskOrder)
            {
                guidBytes[entry][index] = in.ReadBit();
                bytePhaseSize += guidBytes[entry][index] != 0;
            }
        }
        in.ResetBitReader();

        if (in.size() - in.rpos() != bytePhaseSize)
        {
            return RejectRequest(in);
        }

        std::vector<LootTakeEntry> parsed(count);
        uint8 const byteOrder[] = { 0, 4, 1, 7, 6, 5, 3, 2 };
        for (uint32 entry = 0; entry < count; ++entry)
        {
            for (uint8 index : byteOrder)
            {
                in.ReadByteSeq(guidBytes[entry][index]);
            }

            for (size_t index = 0; index < 8; ++index)
            {
                parsed[entry].lootGuid |=
                    uint64(guidBytes[entry][index]) << (index * 8);
            }
            in >> parsed[entry].lootListId;
        }

        entries.swap(parsed);
        return in.rpos() == in.size();
    }

    inline void WriteGuidMask(WorldPacket& out, uint64 guid,
        std::initializer_list<size_t> order)
    {
        for (size_t index : order)
        {
            out.WriteBit(GuidByte(guid, index) != 0);
        }
    }

    inline void WriteGuidBytes(WorldPacket& out, uint64 guid,
        std::initializer_list<size_t> order)
    {
        for (size_t index : order)
        {
            out.WriteByteSeq(GuidByte(guid, index));
        }
    }

    inline bool BuildLootResponse(WorldPacket& out,
        LootResponse const& response)
    {
        if (response.items.size() > MAX_VISIBLE_ENTRIES ||
            response.currencies.size() > MAX_VISIBLE_ENTRIES)
        {
            return false;
        }

        for (LootItem const& item : response.items)
        {
            if (item.slotType > 7 || item.unknown > 3 ||
                item.situ.size() > MAX_SITU_BYTES)
            {
                return false;
            }
        }
        for (LootCurrency const& currency : response.currencies)
        {
            if (currency.slotType > 7)
            {
                return false;
            }
        }

        out.Initialize(SMSG_LOOT_RESPONSE, 64);

        // The client reads every item bit record before any item bytes, then
        // every currency bit record before currency bytes. Keep these phases
        // separate even though ordinary loot uses small byte-sized lists.
        out.WriteBit(!response.hasLootMethod);
        out.WriteBit(!response.hasLootType);
        out.WriteBit(GuidByte(response.sourceGuid, 4) != 0);
        out.WriteBits(uint32(response.currencies.size()), 20);
        WriteGuidMask(out, response.lootGuid, { 2, 3, 7, 1 });
        WriteGuidMask(out, response.sourceGuid, { 6, 7 });
        out.WriteBit(response.money == 0);
        out.WriteBit(response.success);
        out.WriteBit(!response.hasFailureReason);
        out.WriteBit(response.isAreaOfEffectLooting);
        out.WriteBit(GuidByte(response.sourceGuid, 5) != 0);
        out.WriteBit(GuidByte(response.lootGuid, 6) != 0);
        out.WriteBits(uint32(response.items.size()), 19);
        out.WriteBit(GuidByte(response.lootGuid, 0) != 0);

        for (LootItem const& item : response.items)
        {
            out.WriteBits(item.slotType, 3);
            out.WriteBit(item.canTradeToTapList);
            out.WriteBit(!item.hasOptionalByte);
            out.WriteBit(!item.hasLootListId);
            out.WriteBits(item.unknown, 2);
        }

        WriteGuidMask(out, response.sourceGuid, { 1, 0 });
        for (LootCurrency const& currency : response.currencies)
        {
            out.WriteBits(currency.slotType, 3);
        }
        WriteGuidMask(out, response.lootGuid, { 5 });
        WriteGuidMask(out, response.sourceGuid, { 3 });
        WriteGuidMask(out, response.lootGuid, { 4 });
        out.WriteBit(!response.hasLootThreshold);
        WriteGuidMask(out, response.sourceGuid, { 2 });
        out.FlushBits();

        if (response.hasLootThreshold)
        {
            out << response.lootThreshold;
        }

        for (LootItem const& item : response.items)
        {
            out << item.randomSuffix << item.count << item.itemId;
            out << uint32(item.situ.size());
            if (!item.situ.empty())
            {
                out.append(item.situ.data(), item.situ.size());
            }
            if (item.hasOptionalByte)
            {
                out << item.optionalByte;
            }
            out << item.randomPropertyId;
            if (item.hasLootListId)
            {
                out << item.lootListId;
            }
            out << item.displayInfoId;
        }

        WriteGuidBytes(out, response.lootGuid, { 2 });
        if (response.money != 0)
        {
            out << response.money;
        }
        WriteGuidBytes(out, response.lootGuid, { 7 });
        WriteGuidBytes(out, response.sourceGuid, { 5 });
        WriteGuidBytes(out, response.lootGuid, { 3, 4 });
        if (response.hasLootMethod)
        {
            out << response.lootMethod;
        }
        if (response.hasLootType)
        {
            out << response.lootType;
        }
        WriteGuidBytes(out, response.sourceGuid, { 4 });
        WriteGuidBytes(out, response.lootGuid, { 5 });

        for (LootCurrency const& currency : response.currencies)
        {
            out << currency.amount << currency.lootListId <<
                currency.currencyId;
        }

        WriteGuidBytes(out, response.sourceGuid, { 2, 3 });
        if (response.hasFailureReason)
        {
            out << response.failureReason;
        }
        WriteGuidBytes(out, response.lootGuid, { 1 });
        WriteGuidBytes(out, response.sourceGuid, { 0 });
        WriteGuidBytes(out, response.lootGuid, { 0 });
        WriteGuidBytes(out, response.sourceGuid, { 6, 7, 1 });
        WriteGuidBytes(out, response.lootGuid, { 6 });
        return true;
    }

    inline void BuildLootReleaseResponse(WorldPacket& out, uint64 sourceGuid,
        uint64 lootGuid)
    {
        out.Initialize(SMSG_LOOT_RELEASE_RESPONSE, 18);
        // Source and loot-view identities are distinct client fields. The
        // current single-target gameplay layer intentionally supplies the same
        // internal GUID for both; future area loot need not.
        WriteGuidMask(out, lootGuid, { 0, 7, 5 });
        WriteGuidMask(out, sourceGuid, { 0 });
        WriteGuidMask(out, lootGuid, { 4, 6 });
        WriteGuidMask(out, sourceGuid, { 1 });
        WriteGuidMask(out, lootGuid, { 2 });
        WriteGuidMask(out, sourceGuid, { 5 });
        WriteGuidMask(out, lootGuid, { 3 });
        WriteGuidMask(out, sourceGuid, { 3, 2, 4 });
        WriteGuidMask(out, lootGuid, { 1 });
        WriteGuidMask(out, sourceGuid, { 6, 7 });
        out.FlushBits();

        WriteGuidBytes(out, sourceGuid, { 1 });
        WriteGuidBytes(out, lootGuid, { 1 });
        WriteGuidBytes(out, sourceGuid, { 2, 5 });
        WriteGuidBytes(out, lootGuid, { 5, 7, 3 });
        WriteGuidBytes(out, sourceGuid, { 0 });
        WriteGuidBytes(out, lootGuid, { 2, 0 });
        WriteGuidBytes(out, sourceGuid, { 3, 6 });
        WriteGuidBytes(out, lootGuid, { 6 });
        WriteGuidBytes(out, sourceGuid, { 4 });
        WriteGuidBytes(out, lootGuid, { 4 });
        WriteGuidBytes(out, sourceGuid, { 7 });
    }

    inline void BuildLootMoneyNotify(WorldPacket& out, uint32 amount,
        bool soleLoot)
    {
        out.Initialize(SMSG_LOOT_MONEY_NOTIFY, 5);
        // One means "You loot"; zero means the group-split "Your share" path.
        out.WriteBit(soleLoot);
        out.FlushBits();
        out << amount;
    }

    inline void BuildLootClearMoney(WorldPacket& out, uint64 lootGuid)
    {
        out.Initialize(SMSG_LOOT_CLEAR_MONEY, 9);
        WriteGuidMask(out, lootGuid, { 6, 0, 4, 1, 3, 5, 2, 7 });
        out.FlushBits();
        WriteGuidBytes(out, lootGuid, { 0, 4, 2, 7, 1, 5, 3, 6 });
    }

    inline void BuildLootRemoved(WorldPacket& out, uint64 sourceGuid,
        uint64 lootGuid, uint8 lootListId)
    {
        out.Initialize(SMSG_LOOT_REMOVED, 19);
        // The second GUID is the active client loot-view lookup key; the slot
        // is the raw ID previously assigned in SMSG_LOOT_RESPONSE.
        WriteGuidMask(out, sourceGuid, { 7, 0, 2 });
        WriteGuidMask(out, lootGuid, { 0, 1, 2, 7, 6, 5 });
        WriteGuidMask(out, sourceGuid, { 1, 5, 6 });
        WriteGuidMask(out, lootGuid, { 3, 4 });
        WriteGuidMask(out, sourceGuid, { 3, 4 });
        out.FlushBits();

        WriteGuidBytes(out, lootGuid, { 1 });
        WriteGuidBytes(out, sourceGuid, { 7 });
        WriteGuidBytes(out, lootGuid, { 7, 0 });
        WriteGuidBytes(out, sourceGuid, { 6, 2 });
        WriteGuidBytes(out, lootGuid, { 5, 3, 2 });
        WriteGuidBytes(out, sourceGuid, { 0, 5, 1 });
        out << lootListId;
        WriteGuidBytes(out, lootGuid, { 6 });
        WriteGuidBytes(out, sourceGuid, { 3, 4 });
        WriteGuidBytes(out, lootGuid, { 4 });
    }
}

struct LootStoreItem
{
    uint32  itemid;                                         // id of the item
    uint8   type;                                           // 0 = item, 1 = currency
    float   chance;                                         // always positive, chance to drop for both quest and non-quest items, chance to be used for refs
    int32   mincountOrRef;                                  // mincount for drop items (positive) or minus referenced TemplateleId (negative)
    uint8   maxcount;                                       // max drop count for the item (mincountOrRef positive) or Ref multiplicator (mincountOrRef negative)
    uint8   group       : 7;
    bool    needs_quest : 1;                                // quest drop (negative ChanceOrQuestChance in DB)
    uint16  conditionId : 16;                               // additional loot condition Id

    // Constructor, converting ChanceOrQuestChance -> (chance, needs_quest)
    // displayid is filled in IsValid() which must be called after
    LootStoreItem(uint32 _itemid, uint8 _type, float _chanceOrQuestChance, int8 _group, uint16 _conditionId, int32 _mincountOrRef, uint32 _maxcount)
        : itemid(_itemid), type(_type), chance(fabs(_chanceOrQuestChance)), mincountOrRef(_mincountOrRef),
          group(_group), needs_quest(_chanceOrQuestChance < 0), maxcount(_maxcount), conditionId(_conditionId)
    {}

    bool Roll(bool rate) const;                             // Checks if the entry takes it's chance (at loot generation)
    bool IsValid(LootStore const& store, uint32 entry) const;
    // Checks correctness of values
};

struct LootItem
{
    uint32  itemid;
    uint8   type;                                           // 0 = item, 1 = currency
    uint32  randomSuffix;
    int32   randomPropertyId;
    uint32  count;
    uint16  conditionId       : 16;                         // allow compiler pack structure
    bool    currency          : 1;
    bool    is_looted         : 1;
    bool    is_blocked        : 1;
    bool    freeforall        : 1;                          // free for all
    bool    is_underthreshold : 1;
    bool    is_counted        : 1;
    bool    needs_quest       : 1;                          // quest drop

    // Constructor, copies most fields from LootStoreItem, generates random count and random suffixes/properties
    // Should be called for non-reference LootStoreItem entries only (mincountOrRef > 0)
    explicit LootItem(LootStoreItem const& li);

    LootItem(uint32 itemid_, uint8 type_, uint32 count_, uint32 randomSuffix_ = 0, int32 randomPropertyId_ = 0);

    // Basic checks for player/item compatibility - if false no chance to see the item in the loot
    bool AllowedForPlayer(Player const* player, WorldObject const* lootTarget) const;
    LootSlotType GetSlotTypeForSharedLoot(PermissionTypes permission, Player* viewer, WorldObject const* lootTarget, bool condition_ok = false) const;
};

typedef std::vector<LootItem> LootItemList;

struct QuestItem
{
    uint8   index;                                          // position in quest_items;
    bool    is_looted;

    QuestItem()
        : index(0), is_looted(false) {}

    QuestItem(uint8 _index, bool _islooted = false)
        : index(_index), is_looted(_islooted) {}
};

struct Loot;
class LootTemplate;

typedef std::vector<QuestItem> QuestItemList;
typedef std::map<uint32, QuestItemList*> QuestItemMap;
typedef std::vector<LootStoreItem> LootStoreItemList;
typedef std::unordered_map<uint32, LootTemplate*> LootTemplateMap;

typedef std::set<uint32> LootIdSet;

class LootStore
{
    public:
        explicit LootStore(char const* name, char const* entryName, bool ratesAllowed)
            : m_name(name), m_entryName(entryName), m_ratesAllowed(ratesAllowed) {}
        virtual ~LootStore() { Clear(); }

        void Verify() const;

        void LoadAndCollectLootIds(LootIdSet& ids_set);
        void CheckLootRefs(LootIdSet* ref_set = NULL) const;// check existence reference and remove it from ref_set
        void ReportUnusedIds(LootIdSet const& ids_set) const;
        void ReportNotExistedId(uint32 id) const;

        bool HaveLootFor(uint32 loot_id) const { return m_LootTemplates.find(loot_id) != m_LootTemplates.end(); }
        bool HaveQuestLootFor(uint32 loot_id) const;
        bool HaveQuestLootForPlayer(uint32 loot_id, Player* player) const;
        bool HaveConditionalLootFor(uint32 loot_id) const;
        bool HaveConditionalLootForPlayer(uint32 loot_id, Player* player, WorldObject const* lootTarget) const;

        LootTemplate const* GetLootFor(uint32 loot_id) const;

        char const* GetName() const { return m_name; }
        char const* GetEntryName() const { return m_entryName; }
        bool IsRatesAllowed() const { return m_ratesAllowed; }
    protected:
        void LoadLootTable();
        void Clear();
    private:
        LootTemplateMap m_LootTemplates;
        char const* m_name;
        char const* m_entryName;
        bool m_ratesAllowed;
};

class LootTemplate
{
        class LootGroup;                                   // A set of loot definitions for items (refs are not allowed inside)
        typedef std::vector<LootGroup> LootGroups;

    public:
        // Adds an entry to the group (at loading stage)
        void AddEntry(LootStoreItem& item);
        // Rolls for every item in the template and adds the rolled items the the loot
        void Process(Loot& loot, LootStore const& store, bool rate, uint8 GroupId = 0) const;

        // True if template includes at least 1 quest drop entry
        bool HasQuestDrop(LootTemplateMap const& store, uint8 GroupId = 0) const;
        // True if template includes at least 1 quest drop for an active quest of the player
        bool HasQuestDropForPlayer(LootTemplateMap const& store, Player const* player, uint8 GroupId = 0) const;
        bool HasConditionalDrop(LootTemplateMap const& store, uint8 groupId = 0) const;
        bool HasConditionalDropForPlayer(LootTemplateMap const& store, Player const* player, WorldObject const* lootTarget, uint8 groupId = 0) const;

        // Checks integrity of the template
        void Verify(LootStore const& store, uint32 Id) const;
        void CheckLootRefs(LootIdSet* ref_set) const;
    private:
        LootStoreItemList Entries;                          // not grouped only
        LootGroups        Groups;                           // groups have own (optimised) processing, grouped entries go there
};

//=====================================================

class LootValidatorRef :  public Reference<Loot, LootValidatorRef>
{
    public:
        LootValidatorRef() {}
        void targetObjectDestroyLink() override {}
        void sourceObjectDestroyLink() override {}
};

//=====================================================

class LootValidatorRefManager : public RefManager<Loot, LootValidatorRef>
{
    public:
        typedef LinkedListHead::Iterator< LootValidatorRef > iterator;

        LootValidatorRef* getFirst() { return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getFirst(); }
        LootValidatorRef* getLast() { return (LootValidatorRef*)RefManager<Loot, LootValidatorRef>::getLast(); }

        iterator begin() { return iterator(getFirst()); }
        iterator end() { return iterator(NULL); }
        iterator rbegin() { return iterator(getLast()); }
        iterator rend() { return iterator(NULL); }
};

//=====================================================
struct LootView;

/**
 * Builds the complete 5.4.8 loot-window response for one viewer.
 */
bool BuildMopLootResponse(WorldPacket& out, LootView const& view,
    ObjectGuid sourceGuid, LootType lootType);

struct Loot
{
        friend bool BuildMopLootResponse(WorldPacket& out,
            LootView const& view, ObjectGuid sourceGuid, LootType lootType);

        QuestItemMap const& GetPlayerCurrencies() const { return m_playerCurrencies; }
        QuestItemMap const& GetPlayerQuestItems() const { return m_playerQuestItems; }
        QuestItemMap const& GetPlayerFFAItems() const { return m_playerFFAItems; }
        QuestItemMap const& GetPlayerNonQuestNonFFANonCurrencyConditionalItems() const { return m_playerNonQuestNonFFANonCurrencyConditionalItems; }

        LootItemList items;
        uint32 gold;
        uint8 unlootedCount;
        LootType loot_type;                                 // required for achievement system

        Loot(WorldObject const* lootTarget, uint32 _gold = 0) : gold(_gold), unlootedCount(0), loot_type(LOOT_CORPSE), m_lootTarget(lootTarget) {}
        ~Loot() { clear(); }

        // if loot becomes invalid this reference is used to inform the listener
        void addLootValidatorRef(LootValidatorRef* pLootValidatorRef)
        {
            m_LootValidatorRefManager.insertFirst(pLootValidatorRef);
        }

        // void clear();
        void clear()
        {
            for (QuestItemMap::const_iterator itr = m_playerCurrencies.begin(); itr != m_playerCurrencies.end(); ++itr)
            {
                delete itr->second;
            }
            m_playerCurrencies.clear();

            for (QuestItemMap::const_iterator itr = m_playerQuestItems.begin(); itr != m_playerQuestItems.end(); ++itr)
            {
                delete itr->second;
            }
            m_playerQuestItems.clear();

            for (QuestItemMap::const_iterator itr = m_playerFFAItems.begin(); itr != m_playerFFAItems.end(); ++itr)
            {
                delete itr->second;
            }
            m_playerFFAItems.clear();

            for (QuestItemMap::const_iterator itr = m_playerNonQuestNonFFANonCurrencyConditionalItems.begin(); itr != m_playerNonQuestNonFFANonCurrencyConditionalItems.end(); ++itr)
            {
                delete itr->second;
            }
            m_playerNonQuestNonFFANonCurrencyConditionalItems.clear();

            m_playersLooting.clear();
            items.clear();
            m_questItems.clear();
            gold = 0;
            unlootedCount = 0;
            m_LootValidatorRefManager.clearReferences();
        }

        bool empty() const { return items.empty() && gold == 0; }
        bool isLooted() const { return gold == 0 && unlootedCount == 0; }

        void NotifyItemRemoved(uint8 lootIndex);
        void NotifyQuestItemRemoved(uint8 questIndex);
        void NotifyMoneyRemoved();
        void AddLooter(ObjectGuid guid) { m_playersLooting.insert(guid); }
        void RemoveLooter(ObjectGuid guid) { m_playersLooting.erase(guid); }

        void generateMoneyLoot(uint32 minAmount, uint32 maxAmount);
        bool FillLoot(uint32 loot_id, LootStore const& store, Player* loot_owner, bool personal, bool noEmptyError = false);

        // Inserts the item into the loot (called by LootTemplate processors)
        void AddItem(LootStoreItem const& item);

        LootItem* LootItemInSlot(uint32 lootslot, Player* player, QuestItem** qitem = NULL, QuestItem** ffaitem = NULL, QuestItem** conditem = NULL, QuestItem** currency = NULL);
        uint32 GetMaxSlotInLootFor(Player* player) const;

        WorldObject const* GetLootTarget() const { return m_lootTarget; }

    private:
        void FillNotNormalLootFor(Player* player);
        QuestItemList* FillCurrencyLoot(Player* player);
        QuestItemList* FillFFALoot(Player* player);
        QuestItemList* FillQuestLoot(Player* player);
        QuestItemList* FillNonQuestNonFFANonCurrencyConditionalLoot(Player* player);

        LootItemList m_questItems;

        GuidSet m_playersLooting;

        QuestItemMap m_playerCurrencies;
        QuestItemMap m_playerQuestItems;
        QuestItemMap m_playerFFAItems;
        QuestItemMap m_playerNonQuestNonFFANonCurrencyConditionalItems;

        // All rolls are registered here. They need to know, when the loot is not valid anymore
        LootValidatorRefManager m_LootValidatorRefManager;

        // What is looted
        WorldObject const* m_lootTarget;
};

struct LootView
{
    Loot& loot;
    Player* viewer;
    PermissionTypes permission;
    LootView(Loot& _loot, Player* _viewer, PermissionTypes _permission = ALL_PERMISSION)
        : loot(_loot), viewer(_viewer), permission(_permission) {}
};

extern LootStore LootTemplates_Creature;
extern LootStore LootTemplates_Fishing;
extern LootStore LootTemplates_Gameobject;
extern LootStore LootTemplates_Item;
extern LootStore LootTemplates_Mail;
extern LootStore LootTemplates_Milling;
extern LootStore LootTemplates_Pickpocketing;
extern LootStore LootTemplates_Skinning;
extern LootStore LootTemplates_Disenchant;
extern LootStore LootTemplates_Prospecting;
extern LootStore LootTemplates_Spell;

/**
 * Loads creature loot templates.
 */
void LoadLootTemplates_Creature();

/**
 * Loads fishing loot templates.
 */
void LoadLootTemplates_Fishing();

/**
 * Loads gameobject loot templates.
 */
void LoadLootTemplates_Gameobject();

/**
 * Loads item loot templates.
 */
void LoadLootTemplates_Item();

/**
 * Loads mail loot templates.
 */
void LoadLootTemplates_Mail();

/**
 * Loads milling loot templates.
 */
void LoadLootTemplates_Milling();

/**
 * Loads pickpocketing loot templates.
 */
void LoadLootTemplates_Pickpocketing();

/**
 * Loads skinning loot templates.
 */
void LoadLootTemplates_Skinning();

/**
 * Loads disenchant loot templates.
 */
void LoadLootTemplates_Disenchant();
void LoadLootTemplates_Prospecting();

void LoadLootTemplates_Spell();

/**
 * Loads reference loot templates used by other loot tables.
 */
void LoadLootTemplates_Reference();

inline void LoadLootTables()
{
    LoadLootTemplates_Creature();
    LoadLootTemplates_Fishing();
    LoadLootTemplates_Gameobject();
    LoadLootTemplates_Item();
    LoadLootTemplates_Mail();
    LoadLootTemplates_Milling();
    LoadLootTemplates_Pickpocketing();
    LoadLootTemplates_Skinning();
    LoadLootTemplates_Disenchant();
    LoadLootTemplates_Prospecting();
    LoadLootTemplates_Spell();

    LoadLootTemplates_Reference();
}

#endif
