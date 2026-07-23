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

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include "Item.h"
#include "Spell.h"
#include "SocialMgr.h"
#include "Language.h"
#include "DBCStores.h"

/**
 * @brief Sends a trade status packet to the client.
 *
 * @param status The trade status payload to send.
 */
void WorldSession::SendTradeStatus(TradeStatus status)
{
    MopTradePackets::StatusData statusData;
    SendTradeStatus(status, statusData);
}

void WorldSession::SendTradeStatus(TradeStatus status,
    MopTradePackets::StatusData const& statusData)
{
    WorldPacket data;
    if (!MopTradePackets::BuildStatus(data, status, statusData))
        return;
    SendPacket(&data);
}

/**
 * @brief Handles the client notification that a trade request was ignored.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleIgnoreTradeOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Ignore Trade %u", _player->GetGUIDLow());
    // recvPacket.print_storage();
}

/**
 * @brief Handles the client notification that the target is busy trading.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleBusyTradeOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Busy Trade %u", _player->GetGUIDLow());
    // recvPacket.print_storage();
}

/**
 * @brief Sends the current trade window contents to one side of the trade.
 *
 * @param trader_state True to send the trader's view, false for the player's own view.
 */
void WorldSession::SendUpdateTrade(bool trader_state /*= true*/)
{
    TradeData* my_trade = _player->GetTradeData();
    if (!my_trade)
    {
        return;
    }

    TradeData* view_trade = trader_state ? my_trade->GetTraderData() : my_trade;
    if (!view_trade)
    {
        return;
    }

    MopTradePackets::UpdateData update;
    update.transaction = 0;
    update.currency = 0;
    update.spell = view_trade->GetSpell();
    update.side = trader_state ? 1 : 0;
    update.money = view_trade->GetMoney();
    update.slotCountA = TRADE_SLOT_COUNT;
    update.sequence = 0;
    update.slotCountB = TRADE_SLOT_COUNT;

    for (uint8 i = 0; i < TRADE_SLOT_COUNT; ++i)
    {
        if (Item* item = view_trade->GetItem(TradeSlots(i)))
        {
            MopTradePackets::ItemData record;
            record.notWrapped =
                !item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
            record.creatorGuid =
                item->GetGuidValue(ITEM_FIELD_CREATOR).GetRawValue();
            record.giftCreatorGuid =
                item->GetGuidValue(ITEM_FIELD_GIFTCREATOR).GetRawValue();
            record.hasLock = item->GetProto()->LockID &&
                !item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_UNLOCKED);
            record.maxDurability =
                item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
            record.unknown0 = 0;
            record.reforge =
                item->GetEnchantmentId(REFORGE_ENCHANTMENT_SLOT);
            record.permanentEnchant =
                item->GetEnchantmentId(PERM_ENCHANTMENT_SLOT);
            record.durability =
                item->GetUInt32Value(ITEM_FIELD_DURABILITY);
            for (uint32 enchantSlot = 0; enchantSlot < MAX_GEM_SOCKETS;
                    ++enchantSlot)
            {
                record.gemEnchant[enchantSlot] = item->GetEnchantmentId(
                    EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + enchantSlot));
            }
            record.randomProperty = uint32(item->GetItemRandomPropertyId());
            record.spellCharges = uint32(item->GetSpellCharges());
            record.suffixFactor = item->GetItemSuffixFactor();
            record.slot = i;
            record.itemId = item->GetProto()->ItemId;
            record.stackCount = item->GetCount();
            update.items.push_back(record);
        }
    }

    WorldPacket data;
    if (!MopTradePackets::BuildUpdate(data, update))
        return;
    SendPacket(&data);
}

//==============================================================
// transfer the items to the players

void WorldSession::moveItems(Item* myItems[], Item* hisItems[])
{
    Player* trader = _player->GetTrader();
    if (!trader)
    {
        return;
    }

    for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
    {
        ItemPosCountVec traderDst;
        ItemPosCountVec playerDst;
        bool traderCanTrade = (myItems[i] == NULL || trader->CanStoreItem(NULL_BAG, NULL_SLOT, traderDst, myItems[i], false) == EQUIP_ERR_OK);
        bool playerCanTrade = (hisItems[i] == NULL || _player->CanStoreItem(NULL_BAG, NULL_SLOT, playerDst, hisItems[i], false) == EQUIP_ERR_OK);
        if (traderCanTrade && playerCanTrade)
        {
            // Ok, if trade item exists and can be stored
            // If we trade in both directions we had to check, if the trade will work before we actually do it
            // A roll back is not possible after we stored it
            if (myItems[i])
            {
                // logging
                DEBUG_LOG("partner storing: %s", myItems[i]->GetGuidStr().c_str());
                if (_player->GetSession()->GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
                {
                    sLog.outCommand(_player->GetSession()->GetAccountId(), "GM %s (Account: %u) trade: %s (Entry: %d Count: %u) to player: %s (Account: %u)",
                                    _player->GetName(), _player->GetSession()->GetAccountId(),
                                    myItems[i]->GetProto()->Name1, myItems[i]->GetEntry(), myItems[i]->GetCount(),
                                    trader->GetName(), trader->GetSession()->GetAccountId());
                }

                // store
                trader->MoveItemToInventory(traderDst, myItems[i], true, true);
            }

            if (hisItems[i])
            {
                // logging
                DEBUG_LOG("player storing: %s", hisItems[i]->GetGuidStr().c_str());
                if (trader->GetSession()->GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
                {
                    sLog.outCommand(trader->GetSession()->GetAccountId(), "GM %s (Account: %u) trade: %s (Entry: %d Count: %u) to player: %s (Account: %u)",
                                    trader->GetName(), trader->GetSession()->GetAccountId(),
                                    hisItems[i]->GetProto()->Name1, hisItems[i]->GetEntry(), hisItems[i]->GetCount(),
                                    _player->GetName(), _player->GetSession()->GetAccountId());
                }

                // store
                _player->MoveItemToInventory(playerDst, hisItems[i], true, true);
            }
        }
        else
        {
            // in case of fatal error log error message
            // return the already removed items to the original owner
            if (myItems[i])
            {
                if (!traderCanTrade)
                {
                    sLog.outError("trader can't store item: %s", myItems[i]->GetGuidStr().c_str());
                }
                if (_player->CanStoreItem(NULL_BAG, NULL_SLOT, playerDst, myItems[i], false) == EQUIP_ERR_OK)
                {
                    _player->MoveItemToInventory(playerDst, myItems[i], true, true);
                }
                else
                {
                    sLog.outError("player can't take item back: %s", myItems[i]->GetGuidStr().c_str());
                }
            }
            // return the already removed items to the original owner
            if (hisItems[i])
            {
                if (!playerCanTrade)
                {
                    sLog.outError("player can't store item: %s", hisItems[i]->GetGuidStr().c_str());
                }
                if (trader->CanStoreItem(NULL_BAG, NULL_SLOT, traderDst, hisItems[i], false) == EQUIP_ERR_OK)
                {
                    trader->MoveItemToInventory(traderDst, hisItems[i], true, true);
                }
                else
                {
                    sLog.outError("trader can't take item back: %s", hisItems[i]->GetGuidStr().c_str());
                }
            }
        }
    }
}

//==============================================================
static void setAcceptTradeMode(TradeData* myTrade, TradeData* hisTrade, Item** myItems, Item** hisItems)
{
    myTrade->SetInAcceptProcess(true);
    hisTrade->SetInAcceptProcess(true);

    // store items in local list and set 'in-trade' flag
    for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
    {
        if (Item* item = myTrade->GetItem(TradeSlots(i)))
        {
            DEBUG_LOG("player trade %s bag: %u slot: %u", item->GetGuidStr().c_str(), item->GetBagSlot(), item->GetSlot());
            // Can return NULL
            myItems[i] = item;
            myItems[i]->SetInTrade();
        }

        if (Item* item = hisTrade->GetItem(TradeSlots(i)))
        {
            DEBUG_LOG("partner trade %s bag: %u slot: %u", item->GetGuidStr().c_str(), item->GetBagSlot(), item->GetSlot());
            hisItems[i] = item;
            hisItems[i]->SetInTrade();
        }
    }
}

/**
 * @brief Clears the accept-in-progress state on both trade objects.
 *
 * @param myTrade The initiating player's trade data.
 * @param hisTrade The target player's trade data.
 */
static void clearAcceptTradeMode(TradeData* myTrade, TradeData* hisTrade)
{
    myTrade->SetInAcceptProcess(false);
    hisTrade->SetInAcceptProcess(false);
}

/**
 * @brief Clears the in-trade flag on cached traded items.
 *
 * @param myItems The initiating player's cached items.
 * @param hisItems The target player's cached items.
 */
static void clearAcceptTradeMode(Item** myItems, Item** hisItems)
{
    // clear 'in-trade' flag
    for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
    {
        if (myItems[i])
        {
            myItems[i]->SetInTrade(false);
        }
        if (hisItems[i])
        {
            hisItems[i]->SetInTrade(false);
        }
    }
}

/**
 * @brief Finalizes a trade when one side accepts the current offer.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleAcceptTradeOpcode(WorldPacket& recvPacket)
{
    recvPacket.read_skip<uint32>();                         // 7, amount traded slots ?

    TradeData* my_trade = _player->m_trade;
    if (!my_trade)
    {
        return;
    }

    Player* trader = my_trade->GetTrader();
    if (!trader)
    {
        return;
    }

    TradeData* his_trade = trader->m_trade;
    if (!his_trade)
    {
        return;
    }

    Item* myItems[TRADE_SLOT_TRADED_COUNT]  = { NULL, NULL, NULL, NULL, NULL, NULL };
    Item* hisItems[TRADE_SLOT_TRADED_COUNT] = { NULL, NULL, NULL, NULL, NULL, NULL };

    // set before checks to properly undo at problems (it already set in to client)
    my_trade->SetAccepted(true);

    if (!_player->IsWithinDistInMap(trader, TRADE_DISTANCE, false))
    {
        SendTradeStatus(TRADE_STATUS_TOO_FAR_AWAY);
        my_trade->SetAccepted(false);
        return;
    }

    // not accept case incorrect money amount
    if (my_trade->GetMoney() > _player->GetMoney())
    {
        SendNotification(LANG_NOT_ENOUGH_GOLD);
        my_trade->SetAccepted(false, true);
        return;
    }

    // not accept case incorrect money amount
    if (his_trade->GetMoney() > trader->GetMoney())
    {
        trader->GetSession()->SendNotification(LANG_NOT_ENOUGH_GOLD);
        his_trade->SetAccepted(false, true);
        return;
    }

    // not accept if some items now can't be trade (cheating)
    for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
    {
        if (Item* item = my_trade->GetItem(TradeSlots(i)))
        {
            if (!item->CanBeTraded())
            {
                SendTradeStatus(TRADE_STATUS_CANCELLED);
                return;
            }
        }

        if (Item* item  = his_trade->GetItem(TradeSlots(i)))
        {
            if (!item->CanBeTraded())
            {
                SendTradeStatus(TRADE_STATUS_CANCELLED);
                return;
            }
        }
    }

    if (his_trade->IsAccepted())
    {
        setAcceptTradeMode(my_trade, his_trade, myItems, hisItems);

        Spell* my_spell = NULL;
        SpellCastTargets my_targets;

        Spell* his_spell = NULL;
        SpellCastTargets his_targets;

        // not accept if spell can't be casted now (cheating)
        if (uint32 my_spell_id = my_trade->GetSpell())
        {
            SpellEntry const* spellEntry = sSpellStore.LookupEntry(my_spell_id);
            Item* castItem = my_trade->GetSpellCastItem();

            if (!spellEntry || !his_trade->GetItem(TRADE_SLOT_NONTRADED) ||
                    (my_trade->HasSpellCastItem() && !castItem))
            {
                clearAcceptTradeMode(my_trade, his_trade);
                clearAcceptTradeMode(myItems, hisItems);

                my_trade->SetSpell(0);
                return;
            }

            my_spell = new Spell(_player, spellEntry, true);
            my_spell->m_CastItem = castItem;
            my_targets.setTradeItemTarget(_player);
            my_spell->m_targets = my_targets;

            SpellCastResult res = my_spell->CheckCast(true);
            if (res != SPELL_CAST_OK)
            {
                my_spell->SendCastResult(res);

                clearAcceptTradeMode(my_trade, his_trade);
                clearAcceptTradeMode(myItems, hisItems);

                delete my_spell;
                my_trade->SetSpell(0);
                return;
            }
        }

        // not accept if spell can't be casted now (cheating)
        if (uint32 his_spell_id = his_trade->GetSpell())
        {
            SpellEntry const* spellEntry = sSpellStore.LookupEntry(his_spell_id);
            Item* castItem = his_trade->GetSpellCastItem();

            if (!spellEntry || !my_trade->GetItem(TRADE_SLOT_NONTRADED) ||
                    (his_trade->HasSpellCastItem() && !castItem))
            {
                delete my_spell;
                his_trade->SetSpell(0);

                clearAcceptTradeMode(my_trade, his_trade);
                clearAcceptTradeMode(myItems, hisItems);
                return;
            }

            his_spell = new Spell(trader, spellEntry, true);
            his_spell->m_CastItem = castItem;
            his_targets.setTradeItemTarget(trader);
            his_spell->m_targets = his_targets;

            SpellCastResult res = his_spell->CheckCast(true);
            if (res != SPELL_CAST_OK)
            {
                his_spell->SendCastResult(res);

                clearAcceptTradeMode(my_trade, his_trade);
                clearAcceptTradeMode(myItems, hisItems);

                delete my_spell;
                delete his_spell;

                his_trade->SetSpell(0);
                return;
            }
        }

        // inform partner client
        trader->GetSession()->SendTradeStatus(TRADE_STATUS_ACCEPTED);

        // test if item will fit in each inventory
        bool hisCanCompleteTrade = (trader->CanStoreItems(myItems, TRADE_SLOT_TRADED_COUNT) == EQUIP_ERR_OK);
        bool myCanCompleteTrade = (_player->CanStoreItems(hisItems, TRADE_SLOT_TRADED_COUNT) == EQUIP_ERR_OK);

        clearAcceptTradeMode(myItems, hisItems);

        // in case of missing space report error
        if (!myCanCompleteTrade)
        {
            clearAcceptTradeMode(my_trade, his_trade);

            SendNotification(LANG_NOT_FREE_TRADE_SLOTS);
            trader->GetSession()->SendNotification(LANG_NOT_PARTNER_FREE_TRADE_SLOTS);
            my_trade->SetAccepted(false);
            his_trade->SetAccepted(false);
            return;
        }
        else if (!hisCanCompleteTrade)
        {
            clearAcceptTradeMode(my_trade, his_trade);

            SendNotification(LANG_NOT_PARTNER_FREE_TRADE_SLOTS);
            trader->GetSession()->SendNotification(LANG_NOT_FREE_TRADE_SLOTS);
            my_trade->SetAccepted(false);
            his_trade->SetAccepted(false);
            return;
        }

        // execute trade: 1. remove
        for (int i = 0; i < TRADE_SLOT_TRADED_COUNT; ++i)
        {
            if (Item* item = myItems[i])
            {
                item->SetGuidValue(ITEM_FIELD_GIFTCREATOR, _player->GetObjectGuid());
                _player->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
            }
            if (Item* item = hisItems[i])
            {
                item->SetGuidValue(ITEM_FIELD_GIFTCREATOR, trader->GetObjectGuid());
                trader->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
            }
        }

        // execute trade: 2. store
        moveItems(myItems, hisItems);

        // logging money
        if (sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
        {
            if (_player->GetSession()->GetSecurity() > SEC_PLAYER && my_trade->GetMoney() > 0)
            {
                sLog.outCommand(_player->GetSession()->GetAccountId(), "GM %s (Account: %u) give money (Amount: " UI64FMTD ") to player: %s (Account: %u)",
                                _player->GetName(), _player->GetSession()->GetAccountId(),
                                my_trade->GetMoney(),
                                trader->GetName(), trader->GetSession()->GetAccountId());
            }
            if (trader->GetSession()->GetSecurity() > SEC_PLAYER && his_trade->GetMoney() > 0)
            {
                sLog.outCommand(trader->GetSession()->GetAccountId(), "GM %s (Account: %u) give money (Amount: " UI64FMTD ") to player: %s (Account: %u)",
                                trader->GetName(), trader->GetSession()->GetAccountId(),
                                his_trade->GetMoney(),
                                _player->GetName(), _player->GetSession()->GetAccountId());
            }
        }

        // update money
        _player->ModifyMoney(-int64(my_trade->GetMoney()));
        _player->ModifyMoney(his_trade->GetMoney());
        trader->ModifyMoney(-int64(his_trade->GetMoney()));
        trader->ModifyMoney(my_trade->GetMoney());

        if (my_spell)
        {
            my_spell->SpellStart(&my_targets);
        }

        if (his_spell)
        {
            his_spell->SpellStart(&his_targets);
        }

        // cleanup
        clearAcceptTradeMode(my_trade, his_trade);
        delete _player->m_trade;
        _player->m_trade = NULL;
        delete trader->m_trade;
        trader->m_trade = NULL;

        // desynchronized with the other saves here (SaveInventoryAndGoldToDB() not have own transaction guards)
        CharacterDatabase.BeginTransaction();
        _player->SaveInventoryAndGoldToDB();
        trader->SaveInventoryAndGoldToDB();
        CharacterDatabase.CommitTransaction();

        trader->GetSession()->SendTradeStatus(TRADE_STATUS_COMPLETE);
        SendTradeStatus(TRADE_STATUS_COMPLETE);
    }
    else
    {
        trader->GetSession()->SendTradeStatus(TRADE_STATUS_ACCEPTED);
    }
}

/**
 * @brief Clears the local accepted state for the active trade.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleUnacceptTradeOpcode(WorldPacket& /*recvPacket*/)
{
    TradeData* my_trade = _player->m_trade;
    if (!my_trade)
    {
        return;
    }

    my_trade->SetAccepted(false, true);
}

/**
 * @brief Opens the trade window for both participants.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleBeginTradeOpcode(WorldPacket& /*recvPacket*/)
{
    TradeData* my_trade = _player->m_trade;
    if (!my_trade)
    {
        return;
    }

    Player* trader = my_trade->GetTrader();
    if (!trader || !trader->GetSession())
    {
        return;
    }

    trader->GetSession()->SendTradeStatus(TRADE_STATUS_INITIATED);
    SendTradeStatus(TRADE_STATUS_INITIATED);
}

/**
 * @brief Sends a trade canceled status to the client.
 */
void WorldSession::SendCancelTrade()
{
    if (m_playerRecentlyLogout)
    {
        return;
    }

    SendTradeStatus(TRADE_STATUS_CANCELLED);
}

/**
 * @brief Cancels the current trade session.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleCancelTradeOpcode(WorldPacket& /*recvPacket*/)
{
    // sent also after LOGOUT COMPLETE
    if (_player)                                            // needed because STATUS_LOGGEDIN_OR_RECENTLY_LOGGOUT
    {
        _player->TradeCancel(true);
    }
}

/**
 * @brief Starts a trade request with another player.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleInitiateTradeOpcode(WorldPacket& recvPacket)
{
    ObjectGuid otherGuid;
    recvPacket.ReadGuidMask<0, 3, 5, 1, 4, 6, 7, 2>(otherGuid);
    recvPacket.ReadGuidBytes<7, 4, 3, 5, 1, 2, 6, 0>(otherGuid);

    if (GetPlayer()->m_trade)
    {
        return;
    }

    if (!GetPlayer()->IsAlive())
    {
        SendTradeStatus(TRADE_STATUS_DEAD);
        return;
    }

    if (GetPlayer()->hasUnitState(UNIT_STAT_STUNNED))
    {
        SendTradeStatus(TRADE_STATUS_STUNNED);
        return;
    }

    if (isLogingOut())
    {
        SendTradeStatus(TRADE_STATUS_LOGGING_OUT);
        return;
    }

    if (GetPlayer()->IsTaxiFlying())
    {
        SendTradeStatus(TRADE_STATUS_TOO_FAR_AWAY);
        return;
    }

    Player* pOther = sObjectAccessor.FindPlayer(otherGuid);

    if (!pOther)
    {
        SendTradeStatus(TRADE_STATUS_PLAYER_NOT_FOUND);
        return;
    }

    if (pOther == GetPlayer() || pOther->m_trade)
    {
        SendTradeStatus(TRADE_STATUS_PLAYER_BUSY);
        return;
    }

    if (!pOther->IsAlive())
    {
        SendTradeStatus(TRADE_STATUS_TARGET_DEAD);
        return;
    }

    if (pOther->IsTaxiFlying())
    {
        SendTradeStatus(TRADE_STATUS_TOO_FAR_AWAY);
        return;
    }

    if (pOther->hasUnitState(UNIT_STAT_STUNNED))
    {
        SendTradeStatus(TRADE_STATUS_TARGET_STUNNED);
        return;
    }

    if (pOther->GetSession()->isLogingOut())
    {
        SendTradeStatus(TRADE_STATUS_TARGET_LOGGING_OUT);
        return;
    }

    if (pOther->GetSocial()->HasIgnore(GetPlayer()->GetObjectGuid()))
    {
        SendTradeStatus(TRADE_STATUS_PLAYER_IGNORED);
        return;
    }

    if (pOther->GetTeam() != _player->GetTeam())
    {
        SendTradeStatus(TRADE_STATUS_WRONG_FACTION);
        return;
    }

    if (!pOther->IsWithinDistInMap(_player, 10.0f, false))
    {
        SendTradeStatus(TRADE_STATUS_TOO_FAR_AWAY);
        return;
    }

    // OK start trade
    _player->m_trade = new TradeData(_player, pOther);
    pOther->m_trade = new TradeData(pOther, _player);

    MopTradePackets::StatusData proposalData;
    proposalData.guid = _player->GetObjectGuid().GetRawValue();
    pOther->GetSession()->SendTradeStatus(
        TRADE_STATUS_PROPOSED, proposalData);
}

/**
 * @brief Updates the gold amount offered in the current trade.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleSetTradeGoldOpcode(WorldPacket& recvPacket)
{
    uint64 gold;

    recvPacket >> gold;

    TradeData* my_trade = _player->GetTradeData();
    if (!my_trade)
    {
        return;
    }

    // gold can be incorrect, but this is checked at trade finished.
    my_trade->SetMoney(gold);
}

/**
 * @brief Assigns an inventory item to a trade slot.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleSetTradeItemOpcode(WorldPacket& recvPacket)
{
    // send update
    uint8 tradeSlot;
    uint8 bag;
    uint8 slot;

    recvPacket >> slot;
    recvPacket >> tradeSlot;
    recvPacket >> bag;

    TradeData* my_trade = _player->m_trade;
    if (!my_trade)
    {
        return;
    }

    // invalid slot number
    if (tradeSlot >= TRADE_SLOT_COUNT)
    {
        SendTradeStatus(TRADE_STATUS_CANCELLED);
        return;
    }

    // check cheating, can't fail with correct client operations
    Item* item = _player->GetItemByPos(bag, slot);
    if (!item || (tradeSlot != TRADE_SLOT_NONTRADED && !item->CanBeTraded()))
    {
        SendTradeStatus(TRADE_STATUS_CANCELLED);
        return;
    }

    // prevent place single item into many trade slots using cheating and client bugs
    if (my_trade->HasItem(item->GetObjectGuid()))
    {
        // cheating attempt
        SendTradeStatus(TRADE_STATUS_CANCELLED);
        return;
    }

    my_trade->SetItem(TradeSlots(tradeSlot), item);
}

/**
 * @brief Clears an item slot from the current trade offer.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleClearTradeItemOpcode(WorldPacket& recvPacket)
{
    uint8 tradeSlot;
    recvPacket >> tradeSlot;

    TradeData* my_trade = _player->m_trade;
    if (!my_trade)
    {
        return;
    }

    // invalid slot number
    if (tradeSlot >= TRADE_SLOT_COUNT)
    {
        return;
    }

    my_trade->SetItem(TradeSlots(tradeSlot), NULL);
}
