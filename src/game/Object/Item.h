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
 * @file Item.h
 * @brief Item class definition and inventory/equipment structures.
 *
 * This file defines the Item class which represents items in player inventories,
 * equipped items, bank items, and world-drop loot items.
 *
 * Key functionality includes:
 * - Item creation and deletion
 * - Item ownership and container management
 * - Equipment slot validation
 * - Enchantment and modification storage
 * - Item locking and trade safety
 * - Item expiration and decay
 * - Inventory space management
 * - Durability tracking
 * - Gem socket management
 * - Item-bound state tracking (soul-bound, account-bound, etc.)
 *
 * The file also contains item-related enumerations and structures including
 * InventoryResult for error codes, ItemSetEffect for set bonuses, and various
 * utility functions for item validation.
 *
 * @see Item for the main item implementation
 * @see ItemPrototype for item template data
 * @see Bag for container-specific item implementation
 */

#ifndef MANGOSSERVER_ITEM_H
#define MANGOSSERVER_ITEM_H

#include "Common.h"
#include "Object.h"
#include "LootMgr.h"
#include "ItemPrototype.h"
#include "WorldPacket.h"

#include <array>

namespace MopItemPackets
{
    inline uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (8 * index));
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

    // MoP inventory requests carry compressed position records after their
    // actionable fields. They describe the client's affected slots; the
    // server parses them for wire integrity and derives gameplay from its own
    // authoritative inventory state.
    struct InventoryPositionUpdate
    {
        uint8 bag = 0;
        uint8 slot = 0;
    };

    struct SwapInvItemRequest
    {
        uint8 sourceSlot = 0;
        uint8 destinationSlot = 0;
        std::array<InventoryPositionUpdate, 2> updates;
    };

    struct SwapItemRequest
    {
        uint8 sourceBag = 0;
        uint8 sourceSlot = 0;
        uint8 destinationBag = 0;
        uint8 destinationSlot = 0;
        std::array<InventoryPositionUpdate, 2> updates;
    };

    struct AutoEquipItemRequest
    {
        uint8 cursorBag = 0;
        uint8 cursorSlot = 0;
        InventoryPositionUpdate update;
    };

    struct AutoStoreBagItemRequest
    {
        uint8 sourceBag = 0;
        uint8 sourceSlot = 0;
        uint8 destinationBag = 0;
    };

    struct SplitItemRequest
    {
        uint8 sourceBag = 0;
        uint8 sourceSlot = 0;
        uint8 destinationBag = 0;
        uint8 destinationSlot = 0;
        uint32 count = 0;
    };

    // Drain malformed requests so the session cannot accidentally reuse an
    // unread tail after a structural validation failure.
    inline bool RejectRequest(WorldPacket& in)
    {
        in.rfinish();
        return false;
    }

    // Decode the base-inventory swap body. Its optional update bytes are
    // slot-then-bag, which intentionally differs from CMSG_SWAP_ITEM.
    inline bool ParseSwapInvItem(WorldPacket& in, SwapInvItemRequest& request)
    {
        if (in.size() - in.rpos() < 3)
            return RejectRequest(in);

        SwapInvItemRequest parsed;
        in >> parsed.sourceSlot >> parsed.destinationSlot;

        uint8 const bitByte = in[in.rpos()];
        if ((bitByte & 0x03) != 0 || in.ReadBits(2) != 2)
            return RejectRequest(in);

        std::array<bool, 2> slotIsZero;
        std::array<bool, 2> bagIsZero;
        for (size_t i = 0; i < parsed.updates.size(); ++i)
        {
            slotIsZero[i] = in.ReadBit();
            bagIsZero[i] = in.ReadBit();
        }

        size_t byteCount = 0;
        for (size_t i = 0; i < parsed.updates.size(); ++i)
            byteCount += !slotIsZero[i] + !bagIsZero[i];
        if (in.size() - in.rpos() != byteCount)
            return RejectRequest(in);

        for (size_t i = 0; i < parsed.updates.size(); ++i)
        {
            if (!slotIsZero[i])
                in >> parsed.updates[i].slot;
            if (!bagIsZero[i])
                in >> parsed.updates[i].bag;
        }

        request = parsed;
        return true;
    }

    // Decode a cross-container swap. The four leading fields and the optional
    // bag-then-slot update bytes follow the 18414 writer's non-legacy order.
    inline bool ParseSwapItem(WorldPacket& in, SwapItemRequest& request)
    {
        if (in.size() - in.rpos() < 5)
            return RejectRequest(in);

        SwapItemRequest parsed;
        in >> parsed.sourceSlot >> parsed.destinationBag >>
            parsed.sourceBag >> parsed.destinationSlot;

        uint8 const bitByte = in[in.rpos()];
        if ((bitByte & 0x03) != 0 || in.ReadBits(2) != 2)
            return RejectRequest(in);

        std::array<bool, 2> slotIsZero;
        std::array<bool, 2> bagIsZero;
        for (size_t i = 0; i < parsed.updates.size(); ++i)
        {
            slotIsZero[i] = in.ReadBit();
            bagIsZero[i] = in.ReadBit();
        }

        size_t byteCount = 0;
        for (size_t i = 0; i < parsed.updates.size(); ++i)
            byteCount += !slotIsZero[i] + !bagIsZero[i];
        if (in.size() - in.rpos() != byteCount)
            return RejectRequest(in);

        for (size_t i = 0; i < parsed.updates.size(); ++i)
        {
            if (!bagIsZero[i])
                in >> parsed.updates[i].bag;
            if (!slotIsZero[i])
                in >> parsed.updates[i].slot;
        }

        request = parsed;
        return true;
    }

    // Decode cursor slot/bag plus the mandatory one-record compressed update
    // block produced by AutoEquipCursorItem.
    inline bool ParseAutoEquipItem(WorldPacket& in,
        AutoEquipItemRequest& request)
    {
        if (in.size() - in.rpos() < 3)
            return RejectRequest(in);

        AutoEquipItemRequest parsed;
        in >> parsed.cursorSlot >> parsed.cursorBag;

        uint8 const bitByte = in[in.rpos()];
        if ((bitByte & 0x0F) != 0 || in.ReadBits(2) != 1)
            return RejectRequest(in);

        bool const bagIsZero = in.ReadBit();
        bool const slotIsZero = in.ReadBit();
        size_t const byteCount = !slotIsZero + !bagIsZero;
        if (in.size() - in.rpos() != byteCount)
            return RejectRequest(in);

        if (!slotIsZero)
            in >> parsed.update.slot;
        if (!bagIsZero)
            in >> parsed.update.bag;

        request = parsed;
        return true;
    }

    // Decode source slot/bag and destination bag. The final zero byte is the
    // client's two-bit zero update count plus alignment padding.
    inline bool ParseAutoStoreBagItem(WorldPacket& in,
        AutoStoreBagItemRequest& request)
    {
        if (in.size() - in.rpos() != 4)
            return RejectRequest(in);

        AutoStoreBagItemRequest parsed;
        in >> parsed.sourceSlot >> parsed.sourceBag >> parsed.destinationBag;
        if (in[in.rpos()] != 0 || in.ReadBits(2) != 0)
            return RejectRequest(in);

        request = parsed;
        return true;
    }

    // Decode the split count between source bag and destination fields. The
    // final zero byte is a two-bit auxiliary count that is invariant on the
    // live split-stack construction path.
    inline bool ParseSplitItem(WorldPacket& in, SplitItemRequest& request)
    {
        if (in.size() - in.rpos() != 9)
            return RejectRequest(in);

        SplitItemRequest parsed;
        in >> parsed.sourceBag >> parsed.count >> parsed.destinationBag >>
            parsed.sourceSlot >> parsed.destinationSlot;
        if (parsed.count == 0 || in[in.rpos()] != 0 || in.ReadBits(2) != 0)
            return RejectRequest(in);

        request = parsed;
        return true;
    }

    // Logical inputs for the common inventory-error response. itemGuid is the
    // primary client object and itemGuid2 is the secondary involved object.
    // The two bind-confirm GUID roles are the strongest labels recoverable
    // from the client handler; their higher-level gameplay names are unknown.
    struct InventoryChangeFailure
    {
        uint8 result = 0;
        uint64 itemGuid = 0;
        uint64 itemGuid2 = 0;
        uint8 bagSubclass = 0;
        uint32 resultParameter = 0;
        uint64 bindConfirmPendingGuid = 0;
        uint64 bindConfirmHelperGuid = 0;
    };

    // Serialize the 18414 packed-GUID response consumed by CGGameUI. The
    // result-dependent tail is a required level (1/87), item-limit category
    // (84/85/89), or the bind-confirm parameter and two extra packed GUIDs
    // (81). Result zero still carries the four-byte base body.
    inline void BuildInventoryChangeFailure(WorldPacket& out,
        InventoryChangeFailure const& failure)
    {
        uint64 const g1 = failure.itemGuid;
        uint64 const g2 = failure.itemGuid2;
        out.Initialize(SMSG_INVENTORY_CHANGE_FAILURE, 48);

        out.WriteBit(GuidByte(g2, 4) != 0);
        out.WriteBit(GuidByte(g1, 3) != 0);
        out.WriteBit(GuidByte(g2, 6) != 0);
        out.WriteBit(GuidByte(g2, 2) != 0);
        out.WriteBit(GuidByte(g1, 4) != 0);
        out.WriteBit(GuidByte(g2, 5) != 0);
        out.WriteBit(GuidByte(g1, 1) != 0);
        out.WriteBit(GuidByte(g1, 6) != 0);
        out.WriteBit(GuidByte(g2, 0) != 0);
        out.WriteBit(GuidByte(g2, 3) != 0);
        out.WriteBit(GuidByte(g2, 1) != 0);
        out.WriteBit(GuidByte(g1, 2) != 0);
        out.WriteBit(GuidByte(g1, 0) != 0);
        out.WriteBit(GuidByte(g1, 5) != 0);
        out.WriteBit(GuidByte(g1, 7) != 0);
        out.WriteBit(GuidByte(g2, 7) != 0);
        out.FlushBits();

        out.WriteByteSeq(GuidByte(g2, 0));
        out << failure.bagSubclass;
        out.WriteByteSeq(GuidByte(g2, 6));
        out.WriteByteSeq(GuidByte(g1, 4));
        out.WriteByteSeq(GuidByte(g1, 0));
        out.WriteByteSeq(GuidByte(g1, 7));
        out.WriteByteSeq(GuidByte(g1, 3));
        out.WriteByteSeq(GuidByte(g2, 1));
        out.WriteByteSeq(GuidByte(g2, 5));
        out.WriteByteSeq(GuidByte(g1, 5));
        out.WriteByteSeq(GuidByte(g2, 7));
        out.WriteByteSeq(GuidByte(g2, 2));
        out.WriteByteSeq(GuidByte(g1, 1));
        out.WriteByteSeq(GuidByte(g1, 6));
        out.WriteByteSeq(GuidByte(g1, 2));
        out.WriteByteSeq(GuidByte(g2, 3));
        out.WriteByteSeq(GuidByte(g2, 4));
        out << failure.result;

        if (failure.result == 84 || failure.result == 85 ||
                failure.result == 89)
        {
            out << failure.resultParameter;
        }
        else if (failure.result == 81)
        {
            uint64 const g3 = failure.bindConfirmPendingGuid;
            uint64 const g4 = failure.bindConfirmHelperGuid;
            out << failure.resultParameter;

            out.WriteBit(GuidByte(g3, 5) != 0);
            out.WriteBit(GuidByte(g4, 2) != 0);
            out.WriteBit(GuidByte(g4, 0) != 0);
            out.WriteBit(GuidByte(g4, 3) != 0);
            out.WriteBit(GuidByte(g3, 3) != 0);
            out.WriteBit(GuidByte(g4, 5) != 0);
            out.WriteBit(GuidByte(g3, 0) != 0);
            out.WriteBit(GuidByte(g4, 6) != 0);
            out.WriteBit(GuidByte(g4, 7) != 0);
            out.WriteBit(GuidByte(g3, 1) != 0);
            out.WriteBit(GuidByte(g4, 1) != 0);
            out.WriteBit(GuidByte(g3, 4) != 0);
            out.WriteBit(GuidByte(g3, 2) != 0);
            out.WriteBit(GuidByte(g3, 7) != 0);
            out.WriteBit(GuidByte(g4, 4) != 0);
            out.WriteBit(GuidByte(g3, 6) != 0);
            out.FlushBits();

            out.WriteByteSeq(GuidByte(g4, 7));
            out.WriteByteSeq(GuidByte(g3, 3));
            out.WriteByteSeq(GuidByte(g3, 1));
            out.WriteByteSeq(GuidByte(g3, 4));
            out.WriteByteSeq(GuidByte(g4, 1));
            out.WriteByteSeq(GuidByte(g3, 7));
            out.WriteByteSeq(GuidByte(g4, 6));
            out.WriteByteSeq(GuidByte(g4, 2));
            out.WriteByteSeq(GuidByte(g3, 6));
            out.WriteByteSeq(GuidByte(g4, 0));
            out.WriteByteSeq(GuidByte(g3, 2));
            out.WriteByteSeq(GuidByte(g4, 4));
            out.WriteByteSeq(GuidByte(g3, 0));
            out.WriteByteSeq(GuidByte(g3, 5));
            out.WriteByteSeq(GuidByte(g4, 5));
            out.WriteByteSeq(GuidByte(g4, 3));
        }
        else if (failure.result == 1 || failure.result == 87)
        {
            out << failure.resultParameter;
        }
    }

    struct VendorItemRecord
    {
        int32 leftInStock = -1;
        uint32 price = 0;
        uint32 type = 0;
        int32 maxDurability = 0;
        uint32 displayId = 0;
        uint32 buyCount = 0;
        uint32 itemId = 0;
        bool hasExtendedCost = false;
        uint32 extendedCost = 0;
        uint32 upgradeId = 0;
        bool hasCondition = false;
        uint32 condition = 0;
        uint32 slot = 0;
        bool doNotFilter = false;
    };

    struct VendorList
    {
        ObjectGuid vendorGuid;
        uint8 reason = 0;
        std::vector<VendorItemRecord> items;
    };

    inline ObjectGuid ReadListInventory(WorldPacket& in)
    {
        ObjectGuid guid;
        in.ReadGuidMask<6, 7, 3, 1, 2, 0, 4, 5>(guid);
        in.ReadGuidBytes<0, 7, 1, 6, 4, 3, 5, 2>(guid);
        return guid;
    }

    inline void BuildVendorList(WorldPacket& out, VendorList const& list)
    {
        size_t const itemCount = list.items.size() < 0x3FFFF
            ? list.items.size() : 0x3FFFF;
        uint64 const vendorGuid = list.vendorGuid.GetRawValue();
        uint8 const maskA[] = { 5, 7, 1, 3, 6 };
        uint8 const maskB[] = { 4, 0, 2 };
        uint8 const bytes[] = { 3, 7, 0, 6, 2, 1, 4, 5 };

        out.Initialize(SMSG_LIST_INVENTORY, 13 + itemCount * 44);
        WriteGuidMask(out, vendorGuid, maskA);
        out.WriteBits(uint32(itemCount), 18);
        for (size_t i = 0; i < itemCount; ++i)
        {
            VendorItemRecord const& item = list.items[i];
            out.WriteBit(item.doNotFilter);
            out.WriteBit(!item.hasExtendedCost);
            out.WriteBit(!item.hasCondition);
        }
        WriteGuidMask(out, vendorGuid, maskB);
        out.FlushBits();

        out << list.reason;
        for (size_t i = 0; i < itemCount; ++i)
        {
            VendorItemRecord const& item = list.items[i];
            out << item.leftInStock;
            out << item.price;
            out << item.type;
            out << item.maxDurability;
            out << item.displayId;
            out << item.buyCount;
            out << item.itemId;
            if (item.hasExtendedCost)
                out << item.extendedCost;
            out << item.upgradeId;
            if (item.hasCondition)
                out << item.condition;
            out << item.slot;
        }
        WriteGuidBytes(out, vendorGuid, bytes);
    }

    inline void BuildItemTimeUpdate(WorldPacket& out, uint64 itemGuid,
        uint32 duration)
    {
        uint8 const maskOrder[] = { 5, 3, 4, 1, 2, 6, 0, 7 };
        uint8 const byteOrder[] = { 2, 6, 7, 4, 0, 3, 5, 1 };
        WriteGuidMask(out, itemGuid, maskOrder);
        out.FlushBits();
        WriteGuidBytes(out, itemGuid, byteOrder);
        out << duration;
    }

    inline void BuildItemEnchantTimeUpdate(WorldPacket& out,
        uint64 playerGuid, uint64 itemGuid, uint32 slot, uint32 duration)
    {
        uint8 const itemMaskA[] = { 4, 0 };
        uint8 const playerMaskA[] = { 3 };
        uint8 const itemMaskB[] = { 3 };
        uint8 const playerMaskB[] = { 2, 6, 7 };
        uint8 const itemMaskC[] = { 1 };
        uint8 const playerMaskC[] = { 4 };
        uint8 const itemMaskD[] = { 6, 5 };
        uint8 const playerMaskD[] = { 0 };
        uint8 const itemMaskE[] = { 2 };
        uint8 const playerMaskE[] = { 5, 1 };
        uint8 const itemMaskF[] = { 7 };

        WriteGuidMask(out, itemGuid, itemMaskA);
        WriteGuidMask(out, playerGuid, playerMaskA);
        WriteGuidMask(out, itemGuid, itemMaskB);
        WriteGuidMask(out, playerGuid, playerMaskB);
        WriteGuidMask(out, itemGuid, itemMaskC);
        WriteGuidMask(out, playerGuid, playerMaskC);
        WriteGuidMask(out, itemGuid, itemMaskD);
        WriteGuidMask(out, playerGuid, playerMaskD);
        WriteGuidMask(out, itemGuid, itemMaskE);
        WriteGuidMask(out, playerGuid, playerMaskE);
        WriteGuidMask(out, itemGuid, itemMaskF);
        out.FlushBits();

        out << slot;
        uint8 const playerBytesA[] = { 4, 2 };
        uint8 const itemBytesA[] = { 5, 4 };
        uint8 const playerBytesB[] = { 6 };
        uint8 const itemBytesB[] = { 1 };
        uint8 const playerBytesC[] = { 0, 1 };
        uint8 const itemBytesC[] = { 6, 2 };
        uint8 const playerBytesD[] = { 7 };
        uint8 const itemBytesD[] = { 0, 3, 7 };
        uint8 const playerBytesE[] = { 3, 5 };
        WriteGuidBytes(out, playerGuid, playerBytesA);
        WriteGuidBytes(out, itemGuid, itemBytesA);
        WriteGuidBytes(out, playerGuid, playerBytesB);
        WriteGuidBytes(out, itemGuid, itemBytesB);
        WriteGuidBytes(out, playerGuid, playerBytesC);
        WriteGuidBytes(out, itemGuid, itemBytesC);
        WriteGuidBytes(out, playerGuid, playerBytesD);
        WriteGuidBytes(out, itemGuid, itemBytesD);
        WriteGuidBytes(out, playerGuid, playerBytesE);
        out << duration;
    }
}

struct SpellEntry;
class Bag;
class Field;
class QueryResult;
class Unit;

/// @brief Item set effect structure.
///
/// Stores the spells granted by item set bonuses when a player equips
/// a certain number of items from a specific item set.
struct ItemSetEffect
{
    uint32 setid;
    uint32 item_count;
    SpellEntry const* spells[8];
};

/// @brief Item inventory/equipment result enumeration.
///
/// Error codes returned when attempting to equip, move, or use items.
/// Maps to client-side error messages and UI feedback.
enum InventoryResult
{
    EQUIP_ERR_OK                                 = 0,
    EQUIP_ERR_CANT_EQUIP_LEVEL_I                 = 1,       // ERR_CANT_EQUIP_LEVEL_I
    EQUIP_ERR_CANT_EQUIP_SKILL                   = 2,       // ERR_CANT_EQUIP_SKILL
    EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT             = 3,       // ERR_WRONG_SLOT
    EQUIP_ERR_BAG_FULL                           = 4,       // ERR_BAG_FULL
    EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG        = 5,       // ERR_BAG_IN_BAG
    EQUIP_ERR_CANT_TRADE_EQUIP_BAGS              = 6,       // ERR_TRADE_EQUIPPED_BAG
    EQUIP_ERR_ONLY_AMMO_CAN_GO_HERE              = 7,       // ERR_AMMO_ONLY
    EQUIP_ERR_NO_REQUIRED_PROFICIENCY            = 8,       // ERR_PROFICIENCY_NEEDED
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE        = 9,       // ERR_NO_SLOT_AVAILABLE
    EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM        = 10,      // ERR_CANT_EQUIP_EVER
    EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM2       = 11,      // ERR_CANT_EQUIP_EVER
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE2       = 12,      // ERR_NO_SLOT_AVAILABLE
    EQUIP_ERR_CANT_EQUIP_WITH_TWOHANDED          = 13,      // ERR_2HANDED_EQUIPPED
    EQUIP_ERR_CANT_DUAL_WIELD                    = 14,      // ERR_2HSKILLNOTFOUND
    EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG            = 15,      // ERR_WRONG_BAG_TYPE
    EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2           = 16,      // ERR_WRONG_BAG_TYPE
    EQUIP_ERR_CANT_CARRY_MORE_OF_THIS            = 17,      // ERR_ITEM_MAX_COUNT
    EQUIP_ERR_NO_EQUIPMENT_SLOT_AVAILABLE3       = 18,      // ERR_NO_SLOT_AVAILABLE
    EQUIP_ERR_ITEM_CANT_STACK                    = 19,      // ERR_CANT_STACK
    EQUIP_ERR_ITEM_CANT_BE_EQUIPPED              = 20,      // ERR_NOT_EQUIPPABLE
    EQUIP_ERR_ITEMS_CANT_BE_SWAPPED              = 21,      // ERR_CANT_SWAP
    EQUIP_ERR_SLOT_IS_EMPTY                      = 22,      // ERR_SLOT_EMPTY
    EQUIP_ERR_ITEM_NOT_FOUND                     = 23,      // ERR_ITEM_NOT_FOUND
    EQUIP_ERR_CANT_DROP_SOULBOUND                = 24,      // ERR_DROP_BOUND_ITEM
    EQUIP_ERR_OUT_OF_RANGE                       = 25,      // ERR_OUT_OF_RANGE
    EQUIP_ERR_TRIED_TO_SPLIT_MORE_THAN_COUNT     = 26,      // ERR_TOO_FEW_TO_SPLIT
    EQUIP_ERR_COULDNT_SPLIT_ITEMS                = 27,      // ERR_SPLIT_FAILED
    EQUIP_ERR_MISSING_REAGENT                    = 28,      // ERR_SPELL_FAILED_REAGENTS_GENERIC
    EQUIP_ERR_NOT_ENOUGH_MONEY                   = 29,      // ERR_NOT_ENOUGH_MONEY
    EQUIP_ERR_NOT_A_BAG                          = 30,      // ERR_NOT_A_BAG
    EQUIP_ERR_CAN_ONLY_DO_WITH_EMPTY_BAGS        = 31,      // ERR_DESTROY_NONEMPTY_BAG
    EQUIP_ERR_DONT_OWN_THAT_ITEM                 = 32,      // ERR_NOT_OWNER
    EQUIP_ERR_CAN_EQUIP_ONLY1_QUIVER             = 33,      // ERR_ONLY_ONE_QUIVER
    EQUIP_ERR_MUST_PURCHASE_THAT_BAG_SLOT        = 34,      // ERR_NO_BANK_SLOT
    EQUIP_ERR_TOO_FAR_AWAY_FROM_BANK             = 35,      // ERR_NO_BANK_HERE
    EQUIP_ERR_ITEM_LOCKED                        = 36,      // ERR_ITEM_LOCKED
    EQUIP_ERR_YOU_ARE_STUNNED                    = 37,      // ERR_GENERIC_STUNNED
    EQUIP_ERR_YOU_ARE_DEAD                       = 38,      // ERR_PLAYER_DEAD
    EQUIP_ERR_CANT_DO_RIGHT_NOW                  = 39,      // ERR_CLIENT_LOCKED_OUT
    EQUIP_ERR_INT_BAG_ERROR                      = 40,      // ERR_INTERNAL_BAG_ERROR
    EQUIP_ERR_CAN_EQUIP_ONLY1_BOLT               = 41,      // ERR_ONLY_ONE_BOLT
    EQUIP_ERR_CAN_EQUIP_ONLY1_AMMOPOUCH          = 42,      // ERR_ONLY_ONE_AMMO
    EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED          = 43,      // ERR_CANT_WRAP_STACKABLE
    EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED           = 44,      // ERR_CANT_WRAP_EQUIPPED
    EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED            = 45,      // ERR_CANT_WRAP_WRAPPED
    EQUIP_ERR_BOUND_CANT_BE_WRAPPED              = 46,      // ERR_CANT_WRAP_BOUND
    EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED             = 47,      // ERR_CANT_WRAP_UNIQUE
    EQUIP_ERR_BAGS_CANT_BE_WRAPPED               = 48,      // ERR_CANT_WRAP_BAGS
    EQUIP_ERR_ALREADY_LOOTED                     = 49,      // ERR_LOOT_GONE
    EQUIP_ERR_INVENTORY_FULL                     = 50,      // ERR_INV_FULL
    EQUIP_ERR_BANK_FULL                          = 51,      // ERR_BAG_FULL
    EQUIP_ERR_ITEM_IS_CURRENTLY_SOLD_OUT         = 52,      // ERR_VENDOR_SOLD_OUT
    EQUIP_ERR_BAG_FULL3                          = 53,      // ERR_BAG_FULL
    EQUIP_ERR_ITEM_NOT_FOUND2                    = 54,      // ERR_ITEM_NOT_FOUND
    EQUIP_ERR_ITEM_CANT_STACK2                   = 55,      // ERR_CANT_STACK
    EQUIP_ERR_BAG_FULL4                          = 56,      // ERR_BAG_FULL
    EQUIP_ERR_ITEM_SOLD_OUT                      = 57,      // ERR_VENDOR_SOLD_OUT
    EQUIP_ERR_OBJECT_IS_BUSY                     = 58,      // ERR_OBJECT_IS_BUSY
    EQUIP_ERR_NONE                               = 59,      // ERR_CANT_BE_DISENCHANTED
    EQUIP_ERR_NOT_IN_COMBAT                      = 60,      // ERR_NOT_IN_COMBAT
    EQUIP_ERR_NOT_WHILE_DISARMED                 = 61,      // ERR_NOT_WHILE_DISARMED
    EQUIP_ERR_BAG_FULL6                          = 62,      // ERR_BAG_FULL
    EQUIP_ERR_CANT_EQUIP_RANK                    = 63,      // ERR_CANT_EQUIP_RANK
    EQUIP_ERR_CANT_EQUIP_REPUTATION              = 64,      // ERR_CANT_EQUIP_REPUTATION
    EQUIP_ERR_TOO_MANY_SPECIAL_BAGS              = 65,      // ERR_TOO_MANY_SPECIAL_BAGS
    EQUIP_ERR_LOOT_CANT_LOOT_THAT_NOW            = 66,      // ERR_LOOT_CANT_LOOT_THAT_NOW
    EQUIP_ERR_ITEM_UNIQUE_EQUIPABLE              = 67,      // ERR_ITEM_UNIQUE_EQUIPPABLE
    EQUIP_ERR_VENDOR_MISSING_TURNINS             = 68,      // ERR_VENDOR_MISSING_TURNINS
    EQUIP_ERR_NOT_ENOUGH_HONOR_POINTS            = 69,      // ERR_NOT_ENOUGH_HONOR_POINTS
    EQUIP_ERR_NOT_ENOUGH_ARENA_POINTS            = 70,      // ERR_NOT_ENOUGH_ARENA_POINTS
    EQUIP_ERR_ITEM_MAX_COUNT_SOCKETED            = 71,      // ERR_ITEM_MAX_COUNT_SOCKETED
    EQUIP_ERR_MAIL_BOUND_ITEM                    = 72,      // ERR_MAIL_BOUND_ITEM
    EQUIP_ERR_NO_SPLIT_WHILE_PROSPECTING         = 73,      // ERR_INTERNAL_BAG_ERROR
    EQUIP_ERR_BAG_FULL7                          = 74,      // ERR_BAG_FULL
    EQUIP_ERR_ITEM_MAX_COUNT_EQUIPPED_SOCKETED   = 75,      // ERR_ITEM_MAX_COUNT_EQUIPPED_SOCKETED
    EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED    = 76,      // ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED
    EQUIP_ERR_TOO_MUCH_GOLD                      = 77,      // ERR_TOO_MUCH_GOLD
    EQUIP_ERR_NOT_DURING_ARENA_MATCH             = 78,      // ERR_NOT_DURING_ARENA_MATCH
    EQUIP_ERR_CANNOT_TRADE_THAT                  = 79,      // ERR_TRADE_BOUND_ITEM
    EQUIP_ERR_PERSONAL_ARENA_RATING_TOO_LOW      = 80,      // ERR_CANT_EQUIP_RATING
    EQUIP_ERR_EVENT_AUTOEQUIP_BIND_CONFIRM       = 81,      // EQUIP_ERR_OK, EVENT_AUTOEQUIP_BIND_CONFIRM
    EQUIP_ERR_ARTEFACTS_ONLY_FOR_OWN_CHARACTERS  = 82,      // ERR_NOT_SAME_ACCOUNT
    EQUIP_ERR_OK2                                = 83,      // EQUIP_ERR_OK
    EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS = 84,       // You can only carry %d %s
    EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_SOCKETED_EXCEEDED_IS  = 85,   // You can only equip %d |4item:items in the %s category
    EQUIP_ERR_SCALING_STAT_ITEM_LEVEL_EXCEEDED   = 86,      // Your level is too high to use that item
    EQUIP_ERR_PURCHASE_LEVEL_TOO_LOW             = 87,      // You must reach level %d to purchase that item.
    EQUIP_ERR_CANT_EQUIP_NEED_TALENT             = 88,      // You do not have the required talent to equip that
    EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_EQUIPPED_EXCEEDED_IS = 89,    // You can only equip %d |4item:items in the %s category
    EQUIP_ERR_SHAPESHIFT_FORM_CANNOT_EQUIP       = 90,      // Cannot equip item in this form
    EQUIP_ERR_ITEM_INVENTORY_FULL_SATCHEL        = 91,      // Your inventory is full. Your satchel has been delivered to your mailbox.
    EQUIP_ERR_SCALING_STAT_ITEM_LEVEL_TOO_LOW    = 92,      // Your level is too low to use that item
    EQUIP_ERR_CANT_BUY_QUANTITY                  = 93,      // You can't buy the specified quantity of that item.
};

enum BuyResult
{
    BUY_ERR_CANT_FIND_ITEM                      = 0,
    BUY_ERR_ITEM_ALREADY_SOLD                   = 1,
    BUY_ERR_NOT_ENOUGHT_MONEY                   = 2,
    BUY_ERR_SELLER_DONT_LIKE_YOU                = 4,
    BUY_ERR_DISTANCE_TOO_FAR                    = 5,
    BUY_ERR_ITEM_SOLD_OUT                       = 7,
    BUY_ERR_CANT_CARRY_MORE                     = 8,
    BUY_ERR_RANK_REQUIRE                        = 11,
    BUY_ERR_REPUTATION_REQUIRE                  = 12
};

enum SellResult
{
    SELL_ERR_CANT_FIND_ITEM                      = 1,
    SELL_ERR_CANT_SELL_ITEM                      = 2,       // merchant doesn't like that item
    SELL_ERR_CANT_FIND_VENDOR                    = 3,       // merchant doesn't like you
    SELL_ERR_YOU_DONT_OWN_THAT_ITEM              = 4,       // you don't own that item
    SELL_ERR_UNK                                 = 5,       // nothing appears...
    SELL_ERR_ONLY_EMPTY_BAG                      = 6        // can only do with empty bags
};

// -1 from client enchantment slot number
enum EnchantmentSlot
{
    PERM_ENCHANTMENT_SLOT           = 0,
    TEMP_ENCHANTMENT_SLOT           = 1,
    SOCK_ENCHANTMENT_SLOT           = 2,
    SOCK_ENCHANTMENT_SLOT_2         = 3,
    SOCK_ENCHANTMENT_SLOT_3         = 4,
    BONUS_ENCHANTMENT_SLOT          = 5,
    PRISMATIC_ENCHANTMENT_SLOT      = 6,                    // added at apply special permanent enchantment
    //                              = 7,
    REFORGE_ENCHANTMENT_SLOT        = 8,
    TRANSMOGRIFY_ENCHANTMENT_SLOT   = 9,
    MAX_INSPECTED_ENCHANTMENT_SLOT  = 10,

    PROP_ENCHANTMENT_SLOT_0         = 10,                   // used with RandomSuffix
    PROP_ENCHANTMENT_SLOT_1         = 11,                   // used with RandomSuffix
    PROP_ENCHANTMENT_SLOT_2         = 12,                   // used with RandomSuffix and RandomProperty
    PROP_ENCHANTMENT_SLOT_3         = 13,                   // used with RandomProperty
    PROP_ENCHANTMENT_SLOT_4         = 14,                   // used with RandomProperty
    MAX_ENCHANTMENT_SLOT            = 15,
};

#define MAX_VISIBLE_ITEM_OFFSET       2                     // 2 fields per visible item (entry+enchantment)

#define MAX_GEM_SOCKETS               MAX_ITEM_PROTO_SOCKETS// (BONUS_ENCHANTMENT_SLOT-SOCK_ENCHANTMENT_SLOT) and item proto size, equal value expected

enum EnchantmentOffset
{
    ENCHANTMENT_ID_OFFSET       = 0,
    ENCHANTMENT_DURATION_OFFSET = 1,
    ENCHANTMENT_CHARGES_OFFSET  = 2                         // now here not only charges, but something new in wotlk
};

#define MAX_ENCHANTMENT_OFFSET    3

enum EnchantmentSlotMask
{
    ENCHANTMENT_CAN_SOULBOUND  = 0x01,
    ENCHANTMENT_UNK1           = 0x02,
    ENCHANTMENT_UNK2           = 0x04,
    ENCHANTMENT_UNK3           = 0x08
};

enum ItemUpdateState
{
    ITEM_UNCHANGED                               = 0,
    ITEM_CHANGED                                 = 1,
    ITEM_NEW                                     = 2,
    ITEM_REMOVED                                 = 3
};

enum ItemLootUpdateState
{
    ITEM_LOOT_NONE                                = 0,      // loot not generated
    ITEM_LOOT_TEMPORARY                           = 1,      // generated loot is temporary (will deleted at loot window close)
    ITEM_LOOT_UNCHANGED                           = 2,
    ITEM_LOOT_CHANGED                             = 3,
    ITEM_LOOT_NEW                                 = 4,
    ITEM_LOOT_REMOVED                             = 5
};

// masks for ITEM_FIELD_FLAGS field
enum ItemDynFlags
{
    ITEM_DYNFLAG_BINDED                       = 0x00000001, // set in game at binding
    ITEM_DYNFLAG_UNK1                         = 0x00000002,
    ITEM_DYNFLAG_UNLOCKED                     = 0x00000004, // have meaning only for item with proto->LockId, if not set show as "Locked, req. lockpicking N"
    ITEM_DYNFLAG_WRAPPED                      = 0x00000008, // mark item as wrapped into wrapper container
    ITEM_DYNFLAG_UNK4                         = 0x00000010, // can't repeat old note: appears red icon (like when item durability==0)
    ITEM_DYNFLAG_UNK5                         = 0x00000020,
    ITEM_DYNFLAG_UNK6                         = 0x00000040, // ? old note: usable
    ITEM_DYNFLAG_UNK7                         = 0x00000080,
    ITEM_DYNFLAG_UNK8                         = 0x00000100,
    ITEM_DYNFLAG_READABLE                     = 0x00000200, // can be open for read, it or item proto pagetText make show "Right click to read"
    ITEM_DYNFLAG_UNK10                        = 0x00000400,
    ITEM_DYNFLAG_UNK11                        = 0x00000800,
    ITEM_DYNFLAG_UNK12                        = 0x00001000,
    ITEM_DYNFLAG_UNK13                        = 0x00002000,
    ITEM_DYNFLAG_UNK14                        = 0x00004000,
    ITEM_DYNFLAG_UNK15                        = 0x00008000,
    ITEM_DYNFLAG_UNK16                        = 0x00010000,
    ITEM_DYNFLAG_UNK17                        = 0x00020000,
    ITEM_DYNFLAG_UNK18                        = 0x00040000,
    ITEM_DYNFLAG_UNK19                        = 0x00080000,
    ITEM_DYNFLAG_UNK20                        = 0x00100000,
    ITEM_DYNFLAG_UNK21                        = 0x00200000,
    ITEM_DYNFLAG_UNK22                        = 0x00400000,
    ITEM_DYNFLAG_UNK23                        = 0x00800000,
    ITEM_DYNFLAG_UNK24                        = 0x01000000,
    ITEM_DYNFLAG_UNK25                        = 0x02000000,
    ITEM_DYNFLAG_UNK26                        = 0x04000000,
    ITEM_DYNFLAG_UNK27                        = 0x08000000,
    ITEM_DYNFLAG_UNK28                        = 0x10000000,
    ITEM_DYNFLAG_UNK29                        = 0x20000000,
    ITEM_DYNFLAG_UNK30                        = 0x40000000,
    ITEM_DYNFLAG_UNK31                        = 0x80000000
};

enum ItemRequiredTargetType
{
    ITEM_TARGET_TYPE_CREATURE   = 1,
    ITEM_TARGET_TYPE_DEAD       = 2
};

#define MAX_ITEM_REQ_TARGET_TYPE 2

struct ItemRequiredTarget
{
    ItemRequiredTarget(ItemRequiredTargetType uiType, uint32 uiTargetEntry) : m_uiType(uiType), m_uiTargetEntry(uiTargetEntry) {}
    ItemRequiredTargetType m_uiType;
    uint32 m_uiTargetEntry;

    // helpers
    bool IsFitToRequirements(Unit* pUnitTarget) const;
};

/**
 * Checks whether an item template can be placed inside a bag template.
 */
bool ItemCanGoIntoBag(ItemPrototype const* proto, ItemPrototype const* pBagProto);

class Item : public Object
{
    public:
        static Item* CreateItem(uint32 item, uint32 count, Player const* player = NULL, uint32 randomPropertyId = 0);
        Item* CloneItem(uint32 count, Player const* player = NULL) const;

        Item();
        ~Item();

        virtual bool Create(uint32 guidlow, uint32 itemid, Player const* owner);

        ItemPrototype const* GetProto() const;

        ObjectGuid const& GetOwnerGuid() const { return GetGuidValue(ITEM_FIELD_OWNER); }
        void SetOwnerGuid(ObjectGuid guid) { SetGuidValue(ITEM_FIELD_OWNER, guid); }
        Player* GetOwner()const;

        void SetBinding(bool val) { ApplyModFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED, val); }
        bool IsSoulBound() const { return HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED); }
        bool IsBoundAccountWide() const { return GetProto()->Flags & ITEM_FLAG_BOA; }
        bool IsBindedNotWith(Player const* player) const;
        bool IsBoundByEnchant() const;
        virtual void SaveToDB();
        virtual bool LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid = ObjectGuid());
        virtual void DeleteFromDB();
        void DeleteFromInventoryDB();
        void LoadLootFromDB(Field* fields);

        Bag* ToBag() { if (IsBag()) return reinterpret_cast<Bag*>(this); else return NULL; }
        const Bag* ToBag() const { if (IsBag()) return reinterpret_cast<const Bag*>(this); else return NULL; }

        bool IsLocked() const { return !HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_UNLOCKED); }
        bool IsBag() const { return GetProto()->InventoryType == INVTYPE_BAG; }
        bool IsCurrencyToken() const { return GetProto()->IsCurrencyToken(); }
        bool IsNotEmptyBag() const;
        bool IsBroken() const { return GetUInt32Value(ITEM_FIELD_MAXDURABILITY) > 0 && GetUInt32Value(ITEM_FIELD_DURABILITY) == 0; }
        bool CanBeTraded(bool mail = false) const;
        void SetInTrade(bool b = true) { mb_in_trade = b; }
        bool IsInTrade() const { return mb_in_trade; }

        bool IsFitToSpellRequirements(SpellEntry const* spellInfo) const;
        bool IsTargetValidForItemUse(Unit* pUnitTarget);
        bool IsLimitedToAnotherMapOrZone(uint32 cur_mapId, uint32 cur_zoneId) const;
        bool GemsFitSockets() const;

        uint32 GetCount() const { return GetUInt32Value(ITEM_FIELD_STACK_COUNT); }
        void SetCount(uint32 value) { SetUInt32Value(ITEM_FIELD_STACK_COUNT, value); }
        uint32 GetMaxStackCount() const { return GetProto()->GetMaxStackSize(); }
        uint8 GetGemCountWithID(uint32 GemID) const;
        uint8 GetGemCountWithLimitCategory(uint32 limitCategory) const;
        InventoryResult CanBeMergedPartlyWith(ItemPrototype const* proto) const;

        uint8 GetSlot() const {return m_slot;}
        Bag* GetContainer() { return m_container; }
        uint8 GetBagSlot() const;
        void SetSlot(uint8 slot) {m_slot = slot;}
        uint16 GetPos() const { return uint16(GetBagSlot()) << 8 | GetSlot(); }
        void SetContainer(Bag* container) { m_container = container; }

        bool IsInBag() const { return m_container != NULL; }
        bool IsEquipped() const;

        uint32 GetSkill();

        // RandomPropertyId (signed but stored as unsigned)
        int32 GetItemRandomPropertyId() const { return GetInt32Value(ITEM_FIELD_RANDOM_PROPERTIES_ID); }
        uint32 GetItemSuffixFactor() const { return GetUInt32Value(ITEM_FIELD_PROPERTY_SEED); }
        void SetItemRandomProperties(int32 randomPropId);
        bool UpdateItemSuffixFactor();
        static int32 GenerateItemRandomPropertyId(uint32 item_id);
        void SetEnchantment(EnchantmentSlot slot, uint32 id, uint32 duration, uint32 charges, ObjectGuid casterGuid = ObjectGuid());
        void SetEnchantmentDuration(EnchantmentSlot slot, uint32 duration);
        void SetEnchantmentCharges(EnchantmentSlot slot, uint32 charges);
        void ClearEnchantment(EnchantmentSlot slot);
        uint32 GetEnchantmentId(EnchantmentSlot slot)       const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_ID_OFFSET);}
        uint32 GetEnchantmentDuration(EnchantmentSlot slot) const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_DURATION_OFFSET);}
        uint32 GetEnchantmentCharges(EnchantmentSlot slot)  const { return GetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1 + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_CHARGES_OFFSET);}
        void SendUpdateSockets();

        std::string const& GetText() const { return m_text; }
        void SetText(std::string const& text) { m_text = text; }

        void SendTimeUpdate(Player* owner);
        void UpdateDuration(Player* owner, uint32 diff);

        // spell charges (signed but stored as unsigned)
        int32 GetSpellCharges(uint8 index/*0..5*/ = 0) const { return GetInt32Value(ITEM_FIELD_SPELL_CHARGES + index); }
        void SetSpellCharges(uint8 index/*0..5*/, int32 value) { SetInt32Value(ITEM_FIELD_SPELL_CHARGES + index, value); }
        bool HasMaxCharges() const;
        void RestoreCharges();

        Loot loot;

        void SetLootState(ItemLootUpdateState state);
        bool HasGeneratedLoot() const { return m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_REMOVED; }
        bool HasTemporaryLoot() const { return m_lootState == ITEM_LOOT_TEMPORARY; }

        bool HasSavedLoot() const { return m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_NEW && m_lootState != ITEM_LOOT_TEMPORARY; }

        // Update States
        ItemUpdateState GetState() const { return uState; }
        void SetState(ItemUpdateState state, Player* forplayer = NULL);
        void AddToUpdateQueueOf(Player* player);
        void RemoveFromUpdateQueueOf(Player* player);
        bool IsInUpdateQueue() const { return uQueuePos != -1; }
        uint16 GetQueuePos() const { return uQueuePos; }
        void FSetState(ItemUpdateState state)               // forced
        {
            uState = state;
        }

        bool HasQuest(uint32 quest_id) const override { return GetProto()->StartQuest == quest_id; }
        bool HasInvolvedQuest(uint32 /*quest_id*/) const override { return false; }
        bool IsPotion() const { return GetProto()->IsPotion(); }
        bool IsConjuredConsumable() const { return GetProto()->IsConjuredConsumable(); }
        uint32 GetScriptId() const;

        void AddToClientUpdateList() override;
        void RemoveFromClientUpdateList() override;
        void BuildUpdateData(UpdateDataMapType& update_players) override;

        // Reforge
        static uint32 GetSpecialPrice(ItemPrototype const* proto, uint32 minimumPrice = 10000);
        uint32 GetSpecialPrice(uint32 minimumPrice = 10000) const { return Item::GetSpecialPrice(GetProto(), minimumPrice); }
        int32 GetReforgableStat(ItemModType statType) const;
    private:
        std::string m_text;
        uint8 m_slot;
        Bag* m_container;
        ItemUpdateState uState;
        int16 uQueuePos;
        bool mb_in_trade;                                   // true if item is currently in trade-window
        ItemLootUpdateState m_lootState;
};

#endif
