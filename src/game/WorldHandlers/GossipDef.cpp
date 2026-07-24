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
 * @file GossipDef.cpp
 * @brief NPC gossip menu implementation
 *
 * This file implements the gossip menu system used by NPCs to present
 * interactive options to players. Features:
 *
 * - Gossip menu creation and management
 * - Menu item addition with icons and actions
 * - Quest gossip integration
 * - Point-of-interest (POI) handling
 * - Player choice processing
 * - Scripted gossip options
 *
 * Gossip menus are loaded from the `gossip_menu` database tables
 * and can be extended with scripts.
 *
 * @see GossipMenu for the menu class
 * @see GossipMenuData for menu data
 */

#include "GossipDef.h"
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Formulas.h"

// Constructor for GossipMenu, initializes the session and reserves space for menu items
GossipMenu::GossipMenu(WorldSession* session) : m_session(session)
{
    m_gItems.reserve(16); // can be set for max from most often sizes to speedup push_back and less memory use
    m_gMenuId = 0;
}

// Destructor for GossipMenu, clears the menu items
GossipMenu::~GossipMenu()
{
    ClearMenu();
}

// Adds a menu item to the gossip menu
void GossipMenu::AddMenuItem(uint8 Icon, const std::string& Message, uint32 dtSender, uint32 dtAction, const std::string& BoxMessage, uint32 BoxMoney, bool Coded)
{
    MANGOS_ASSERT(m_gItems.size() <= GOSSIP_MAX_MENU_ITEMS);

    GossipMenuItem gItem;

    gItem.m_gIcon       = Icon;
    gItem.m_gMessage    = Message;
    gItem.m_gCoded      = Coded;
    gItem.m_gSender     = dtSender;
    gItem.m_gOptionId   = dtAction;
    gItem.m_gBoxMessage = BoxMessage;
    gItem.m_gBoxMoney   = BoxMoney;

    m_gItems.push_back(gItem);
}

// Adds gossip menu item data
void GossipMenu::AddMenuItemData(int32 action_menu, uint32 action_poi, uint32 action_script)
{
    GossipMenuItemData pItemData{};

    pItemData.m_gAction_menu    = action_menu;
    pItemData.m_gAction_poi     = action_poi;
    pItemData.m_gAction_script  = action_script;

    m_gItemsData.push_back(pItemData);
}

// Overloaded method to add a menu item with fewer parameters
void GossipMenu::AddMenuItem(uint8 Icon, const std::string& Message, bool Coded)
{
    AddMenuItem(Icon, Message, 0, 0, "", 0, Coded);
}

// Overloaded method to add a menu item with a C-style string message
void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""), Coded);
}

void GossipMenu::AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, char const* BoxMessage, uint32 BoxMoney, bool Coded)
{
    AddMenuItem(Icon, std::string(Message ? Message : ""), dtSender, dtAction, std::string(BoxMessage ? BoxMessage : ""), BoxMoney, Coded);
}

void GossipMenu::AddMenuItem(uint8 Icon, int32 itemText, uint32 dtSender, uint32 dtAction, int32 boxText, uint32 BoxMoney, bool Coded)
{
    uint32 loc_idx = m_session->GetSessionDbLocaleIndex();

    char const* item_text = itemText ? sObjectMgr.GetMangosString(itemText, loc_idx) : "";
    char const* box_text = boxText ? sObjectMgr.GetMangosString(boxText, loc_idx) : "";

    AddMenuItem(Icon, std::string(item_text), dtSender, dtAction, std::string(box_text), BoxMoney, Coded);
}

// Returns the sender ID of a menu item
uint32 GossipMenu::MenuItemSender(unsigned int ItemId) const
{
    if (ItemId >= m_gItems.size())
    {
        return 0;
    }

    return m_gItems[ItemId].m_gSender;
}

// Returns the action ID of a menu item
uint32 GossipMenu::MenuItemAction(unsigned int ItemId) const
{
    if (ItemId >= m_gItems.size())
    {
        return 0;
    }

    return m_gItems[ItemId].m_gOptionId;
}

// Returns whether a menu item is coded
bool GossipMenu::MenuItemCoded(unsigned int ItemId) const
{
    if (ItemId >= m_gItems.size())
    {
        return 0;
    }

    return m_gItems[ItemId].m_gCoded;
}

// Clears all menu items
void GossipMenu::ClearMenu()
{
    m_gItems.clear();
    m_gItemsData.clear();
    m_gMenuId = 0;
}

// Constructor for PlayerMenu, initializes the gossip menu
PlayerMenu::PlayerMenu(WorldSession* session) : mGossipMenu(session)
{
}

// Destructor for PlayerMenu, clears all menus
PlayerMenu::~PlayerMenu()
{
    ClearMenus();
}

// Clears all menus in the player menu
void PlayerMenu::ClearMenus()
{
    mGossipMenu.ClearMenu();
    mQuestMenu.ClearMenu();
}

// Returns the sender ID of a gossip option
uint32 PlayerMenu::GossipOptionSender(unsigned int Selection) const
{
    return mGossipMenu.MenuItemSender(Selection);
}

// Returns the action ID of a gossip option
uint32 PlayerMenu::GossipOptionAction(unsigned int Selection) const
{
    return mGossipMenu.MenuItemAction(Selection);
}

// Returns whether a gossip option is coded
bool PlayerMenu::GossipOptionCoded(unsigned int Selection) const
{
    return mGossipMenu.MenuItemCoded(Selection);
}

// Sends the gossip menu to the player
void PlayerMenu::SendGossipMenu(uint32 TitleTextId, ObjectGuid objectGuid)
{
    MopGossipPackets::Message message;
    message.sourceGuid = objectGuid;
    message.menuId = mGossipMenu.GetMenuId();
    message.titleTextId = TitleTextId;

    for (uint32 iI = 0; iI < mGossipMenu.MenuItemCount(); ++iI)
    {
        GossipMenuItem const& gItem = mGossipMenu.GetItem(iI);
        MopGossipPackets::GossipItem item;
        item.optionId = iI;
        item.icon = gItem.m_gIcon;
        item.coded = gItem.m_gCoded;
        item.boxMoney = gItem.m_gBoxMoney;
        item.message = gItem.m_gMessage;
        item.boxMessage = gItem.m_gBoxMessage;
        message.gossipItems.push_back(item);
    }

    for (uint32 iI = 0; iI < mQuestMenu.MenuItemCount(); ++iI)
    {
        QuestMenuItem const& qItem = mQuestMenu.GetItem(iI);
        uint32 questID = qItem.m_qId;
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(questID);

        int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
        std::string title = pQuest->GetTitle();
        sObjectMgr.GetQuestLocaleStrings(questID, loc_idx, &title);

        MopGossipPackets::QuestItem item;
        item.questId = questID;
        item.icon = qItem.m_qIcon;
        item.level = pQuest->GetQuestLevel();
        item.flags = pQuest->GetQuestFlags();
        item.repeatable = pQuest->IsRepeatable();
        item.title = title;
        message.quests.push_back(item);
    }

    WorldPacket data;
    MopGossipPackets::BuildMessage(data, message);
    GetMenuSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_MESSAGE from %s", objectGuid.GetString().c_str());
}

// Closes the gossip menu
void PlayerMenu::CloseGossip() const
{
    WorldPacket data(SMSG_GOSSIP_COMPLETE, 0);
    GetMenuSession()->SendPacket(&data);

    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_COMPLETE");
}

// Sends a point of interest to the player (outdated method)
void PlayerMenu::SendPointOfInterest(float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, char const* locName) const
{
    WorldPacket data(SMSG_GOSSIP_POI, (4 + 4 + 4 + 4 + 4 + 10)); // guess size
    data << uint32(Flags);
    data << float(X);
    data << float(Y);
    data << uint32(Icon);
    data << uint32(Data);
    data << locName;

    GetMenuSession()->SendPacket(&data);
    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_POI");
}

// Sends a point of interest to the player by ID
void PlayerMenu::SendPointOfInterest(uint32 poi_id) const
{
    PointOfInterest const* poi = sObjectMgr.GetPointOfInterest(poi_id);
    if (!poi)
    {
        sLog.outErrorDb("Requested send nonexistent POI (Id: %u), ignore.", poi_id);
        return;
    }

    std::string icon_name = poi->icon_name;

    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        if (PointOfInterestLocale const* pl = sObjectMgr.GetPointOfInterestLocale(poi_id))
        {
            if (pl->IconName.size() > size_t(loc_idx) && !pl->IconName[loc_idx].empty())
            {
                icon_name = pl->IconName[loc_idx];
            }
        }
    }

    WorldPacket data(SMSG_GOSSIP_POI, (4 + 4 + 4 + 4 + 4 + 10)); // guess size
    data << uint32(poi->flags);
    data << float(poi->x);
    data << float(poi->y);
    data << uint32(poi->icon);
    data << uint32(poi->data);
    data << icon_name;

    GetMenuSession()->SendPacket(&data);
    // DEBUG_LOG("WORLD: Sent SMSG_GOSSIP_POI");
}

// Sends a talking message to the player by text ID
void PlayerMenu::SendTalking(uint32 textID) const
{
    GossipText const* pGossip = sObjectMgr.GetGossipText(textID);
    MopNpcTextPackets::Response const response =
        MopNpcTextPackets::MakeResponse(textID, pGossip);

    WorldPacket data;
    MopNpcTextPackets::BuildResponse(data, response);
    GetMenuSession()->SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_NPC_TEXT_UPDATE ID '%u' mapped=%u",
        textID, response.found ? 1u : 0u);
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/

// Constructor for QuestMenu, reserves space for quest items
QuestMenu::QuestMenu()
{
    m_qItems.reserve(16); // can be set for max from most often sizes to speedup push_back and less memory use
}

// Destructor for QuestMenu, clears the menu items
QuestMenu::~QuestMenu()
{
    ClearMenu();
}

// Adds a quest menu item to the quest menu
void QuestMenu::AddMenuItem(uint32 QuestId, uint8 Icon)
{
    Quest const* qinfo = sObjectMgr.GetQuestTemplate(QuestId);
    if (!qinfo)
    {
        return;
    }

    MANGOS_ASSERT(m_qItems.size() <= GOSSIP_MAX_MENU_ITEMS);

    QuestMenuItem qItem{};

    qItem.m_qId   = QuestId;
    qItem.m_qIcon = Icon;

    m_qItems.push_back(qItem);
}

// Checks if the quest menu has a specific item
bool QuestMenu::HasItem(uint32 questid) const
{
    for (QuestMenuItemList::const_iterator i = m_qItems.begin(); i != m_qItems.end(); ++i)
    {
        if (i->m_qId == questid)
        {
            return true;
        }
    }
    return false;
}

// Clears all quest menu items
void QuestMenu::ClearMenu()
{
    m_qItems.clear();
}

// Sends the quest giver quest list to the player
void PlayerMenu::SendQuestGiverQuestList(QEmote eEmote, const std::string& Title, ObjectGuid npcGUID)
{
    MopQuestGiverPackets::QuestList list;
    list.questGiverGuid = npcGUID;
    list.emote = eEmote._Emote;
    list.emoteDelay = eEmote._Delay;
    list.greeting = Title;

    for (uint32 index = 0; index < mQuestMenu.MenuItemCount(); ++index)
    {
        QuestMenuItem const& qmi = mQuestMenu.GetItem(index);
        if (Quest const* pQuest = sObjectMgr.GetQuestTemplate(qmi.m_qId))
        {
            int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
            std::string title = pQuest->GetTitle();
            sObjectMgr.GetQuestLocaleStrings(qmi.m_qId, loc_idx, &title);

            MopQuestGiverPackets::QuestListEntry entry;
            entry.questId = qmi.m_qId;
            entry.icon = qmi.m_qIcon;
            entry.level = pQuest->GetQuestLevel();
            entry.flags = pQuest->GetQuestFlags();
            // The current quest schema predates the second MoP flag word.
            entry.flags2 = 0;
            entry.repeatable = pQuest->IsRepeatable();
            entry.title = title;
            list.quests.push_back(entry);
        }
    }

    WorldPacket data;
    if (!MopQuestGiverPackets::BuildQuestList(data, list))
    {
        sLog.outError("WORLD: SMSG_QUESTGIVER_QUEST_LIST exceeds the 18414 payload bounds");
        return;
    }
    GetMenuSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_LIST NPC Guid = %s", npcGUID.GetString().c_str());
}

void PlayerMenu::SendQuestGiverStatus(uint32 questStatus, ObjectGuid npcGUID)
{
    WorldPacket data;
    MopQuestStatusPackets::BuildStatus(data, questStatus, npcGUID);

    GetMenuSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_STATUS for %s", npcGUID.GetString().c_str());
}

// Sends the quest giver quest details to the player
void PlayerMenu::SendQuestGiverQuestDetails(Quest const* pQuest, ObjectGuid guid, bool /*ActivateAccept*/) const
{
    // Retrieve the quest title, details, and objectives
    std::string Title      = pQuest->GetTitle();
    std::string Details    = pQuest->GetDetails();
    std::string Objectives = pQuest->GetObjectives();
    std::string PortraitGiverName = pQuest->GetPortraitGiverName();
    std::string PortraitGiverText = pQuest->GetPortraitGiverText();
    std::string PortraitTurnInName = pQuest->GetPortraitTurnInName();
    std::string PortraitTurnInText = pQuest->GetPortraitTurnInText();

    // Get the locale index for the session
    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        // Retrieve localized quest strings if available
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->Details.size() > (size_t)loc_idx && !ql->Details[loc_idx].empty())
            {
                Details = ql->Details[loc_idx];
            }
            if (ql->Objectives.size() > (size_t)loc_idx && !ql->Objectives[loc_idx].empty())
            {
                Objectives = ql->Objectives[loc_idx];
            }
            if (ql->PortraitGiverName.size() > (size_t)loc_idx && !ql->PortraitGiverName[loc_idx].empty())
            {
                PortraitGiverName = ql->PortraitGiverName[loc_idx];
            }
            if (ql->PortraitGiverText.size() > (size_t)loc_idx && !ql->PortraitGiverText[loc_idx].empty())
            {
                PortraitGiverText = ql->PortraitGiverText[loc_idx];
            }
            if (ql->PortraitTurnInName.size() > (size_t)loc_idx && !ql->PortraitTurnInName[loc_idx].empty())
            {
                PortraitTurnInName = ql->PortraitTurnInName[loc_idx];
            }
            if (ql->PortraitTurnInText.size() > (size_t)loc_idx && !ql->PortraitTurnInText[loc_idx].empty())
            {
                PortraitTurnInText = ql->PortraitTurnInText[loc_idx];
            }
        }
    }

    MopQuestGiverPackets::QuestDetails details;
    details.dividerGuid = GetMenuSession()->GetPlayer()->GetDividerGuid();
    details.questGiverGuid = guid;
    details.questId = pQuest->GetQuestId();
    details.suggestedPlayers = pQuest->GetSuggestedPlayers();
    details.bonusTalents = pQuest->GetBonusTalents();
    details.questGiverPortrait = pQuest->GetPortraitGiver();
    details.questTurnInPortrait = pQuest->GetPortraitTurnIn();
    details.questFlags = pQuest->GetQuestFlags();
    // The binary proves these two fixed slots, but the legacy Four quest
    // schema has no Flags2 or reward-package fields to source from.
    details.questFlags2 = 0;
    details.rewardPackageItemId = 0;
    details.rewardChoiceItemCount = pQuest->GetRewChoiceItemsCount();
    details.rewardItemCount = pQuest->GetRewItemsCount();
    details.rewardSkillId = pQuest->GetRewSkill();
    details.rewardSkillPoints = pQuest->GetRewSkillValue();
    details.rewardXp = pQuest->XPValue(GetMenuSession()->GetPlayer());
    details.rewardSpell = pQuest->GetRewSpell();
    // This u32 position is directly proven, but its exact client-side reward
    // name is not. Preserve the value formerly sent as the cast reward spell.
    details.rewardSpellCastOrUnknown = pQuest->GetRewSpellCast();
    details.characterTitleId = pQuest->GetCharTitleId();
    details.rewardOrRequiredMoney =
        GetMenuSession()->GetPlayer()->getLevel() >=
        sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL)
        ? pQuest->GetRewMoneyMaxLevel()
        : uint32(pQuest->GetRewOrReqMoney());
    // The 18414 reader treats this bit as auto-launch state. The legacy
    // ActivateAccept argument is an accept-button hint and is not proven to
    // represent the same state, so keep the wire bit clear.
    details.questDetailsAcceptFlag = false;

    details.questTitle = Title;
    details.questDetails = Details;
    details.questObjectives = Objectives;
    details.questGiverTextWindow = PortraitGiverText;
    details.questGiverTargetName = PortraitGiverName;
    details.questTurnTextWindow = PortraitTurnInText;
    details.questTurnTargetName = PortraitTurnInName;

    for (size_t index = 0; index < QUEST_REWARD_CHOICES_COUNT; ++index)
    {
        details.rewardChoiceItemIds[index] =
            pQuest->RewChoiceItemId[index];
        details.rewardChoiceItemCounts[index] =
            pQuest->RewChoiceItemCount[index];
        if (ItemPrototype const* item =
            ObjectMgr::GetItemPrototype(pQuest->RewChoiceItemId[index]))
        {
            details.rewardChoiceItemDisplayIds[index] =
                item->DisplayInfoID;
        }
    }
    for (size_t index = 0; index < QUEST_REWARDS_COUNT; ++index)
    {
        details.rewardItemIds[index] = pQuest->RewItemId[index];
        details.rewardItemCounts[index] = pQuest->RewItemCount[index];
        if (ItemPrototype const* item =
            ObjectMgr::GetItemPrototype(pQuest->RewItemId[index]))
        {
            details.rewardItemDisplayIds[index] = item->DisplayInfoID;
        }
    }
    for (size_t index = 0; index < QUEST_REWARD_CURRENCY_COUNT; ++index)
    {
        details.rewardCurrencyIds[index] = pQuest->RewCurrencyId[index];
        details.rewardCurrencyCounts[index] =
            pQuest->RewCurrencyCount[index];
    }
    for (size_t index = 0; index < QUEST_REPUTATIONS_COUNT; ++index)
    {
        details.rewardFactionIds[index] = pQuest->RewRepFaction[index];
        details.rewardFactionValueIds[index] =
            uint32(pQuest->RewRepValueId[index]);
        details.rewardFactionValueOverrides[index] =
            uint32(pQuest->RewRepValue[index]);
    }
    for (size_t index = 0; index < QUEST_EMOTE_COUNT; ++index)
    {
        MopQuestGiverPackets::QuestEmote emote;
        emote.delay = pQuest->DetailsEmoteDelay[index];
        emote.emote = pQuest->DetailsEmote[index];
        details.emotes.push_back(emote);
    }

    // 18414 objective records require a stable objective ID and type. The
    // legacy schema retains only target/count arrays, so fabricating records
    // would be less correct than sending the proven zero-count form while
    // retaining the user-facing objective text.
    WorldPacket data;
    if (!MopQuestGiverPackets::BuildQuestDetails(data, details))
    {
        sLog.outError("WORLD: SMSG_QUESTGIVER_QUEST_DETAILS exceeds the 18414 payload bounds");
        return;
    }

    // Send the packet to the player
    GetMenuSession()->SendPacket(&data);

    // Log the sent packet
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_QUEST_DETAILS - for %s of %s, questid = %u", GetMenuSession()->GetPlayer()->GetGuidStr().c_str(), guid.GetString().c_str(), pQuest->GetQuestId());
}

// Sends the quest query response to the player
void PlayerMenu::SendQuestQueryResponse(uint32 questId,
    Quest const* pQuest) const
{
    WorldPacket data;
    if (!pQuest)
    {
        MopQuestQueryPackets::BuildAbsentResponse(data, questId);
        GetMenuSession()->SendPacket(&data);
        return;
    }

    std::string Title, Details, Objectives, EndText, CompletedText;
    std::string PortraitGiverText, PortraitGiverName;
    std::string PortraitTurnInText, PortraitTurnInName;
    Title = pQuest->GetTitle();
    Details = pQuest->GetDetails();
    Objectives = pQuest->GetObjectives();
    EndText = pQuest->GetEndText();
    CompletedText = pQuest->GetCompletedText();
    PortraitGiverName = pQuest->GetPortraitGiverName();
    PortraitGiverText = pQuest->GetPortraitGiverText();
    PortraitTurnInName = pQuest->GetPortraitTurnInName();
    PortraitTurnInText = pQuest->GetPortraitTurnInText();

    // Get the locale index for the session
    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        // Retrieve localized quest strings if available
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->Details.size() > (size_t)loc_idx && !ql->Details[loc_idx].empty())
            {
                Details = ql->Details[loc_idx];
            }
            if (ql->Objectives.size() > (size_t)loc_idx && !ql->Objectives[loc_idx].empty())
            {
                Objectives = ql->Objectives[loc_idx];
            }
            if (ql->EndText.size() > (size_t)loc_idx && !ql->EndText[loc_idx].empty())
            {
                EndText = ql->EndText[loc_idx];
            }
            if (ql->CompletedText.size() > (size_t)loc_idx && !ql->CompletedText[loc_idx].empty())
            {
                CompletedText = ql->CompletedText[loc_idx];
            }
            if (ql->PortraitGiverName.size() > (size_t)loc_idx && !ql->PortraitGiverName[loc_idx].empty())
            {
                PortraitGiverName = ql->PortraitGiverName[loc_idx];
            }
            if (ql->PortraitGiverText.size() > (size_t)loc_idx && !ql->PortraitGiverText[loc_idx].empty())
            {
                PortraitGiverText = ql->PortraitGiverText[loc_idx];
            }
            if (ql->PortraitTurnInName.size() > (size_t)loc_idx && !ql->PortraitTurnInName[loc_idx].empty())
            {
                PortraitTurnInName = ql->PortraitTurnInName[loc_idx];
            }
            if (ql->PortraitTurnInText.size() > (size_t)loc_idx && !ql->PortraitTurnInText[loc_idx].empty())
            {
                PortraitTurnInText = ql->PortraitTurnInText[loc_idx];
            }
        }
    }

    MopQuestQueryPackets::Response response;
    response.questId = pQuest->GetQuestId();
    response.characterTitleId = pQuest->GetCharTitleId();
    response.pointY = pQuest->GetPointY();
    response.soundTurnIn = pQuest->GetSoundTurnInId();
    response.rewardMoneyMaxLevel = pQuest->GetRewMoneyMaxLevel();
    response.rewardHonorAddition = pQuest->GetRewHonorAddition();
    response.suggestedPlayers = pQuest->GetSuggestedPlayers();
    response.repObjectiveFaction = pQuest->GetRepObjectiveFaction();
    response.minLevel = int32(pQuest->GetMinLevel());
    response.pointOpt = pQuest->GetPointOpt();
    response.questLevel = pQuest->GetQuestLevel();
    response.rewardXpId = pQuest->GetRewXPId();
    response.rewardSpellCast = pQuest->GetRewSpellCast();
    response.rewardSkillPoints = pQuest->GetRewSkillValue();
    response.questType = pQuest->GetType();
    response.playersSlain = pQuest->GetPlayersSlain();
    response.pointMapId = pQuest->GetPointMapId();
    response.nextQuestInChain = pQuest->GetNextQuestInChain();
    response.pointX = pQuest->GetPointX();
    response.soundAccept = pQuest->GetSoundAcceptId();
    response.rewardHonorMultiplier = pQuest->GetRewHonorMultiplier();
    response.requiredSpell = pQuest->GetReqSpellLearned();
    response.zoneOrSort = pQuest->GetZoneOrSort();
    response.bonusTalents = pQuest->GetBonusTalents();
    response.rewardSpell = pQuest->GetRewSpell();
    response.rewardSkillId = pQuest->GetRewSkill();
    response.questFlags = pQuest->GetQuestFlags();
    response.questMethod = pQuest->GetQuestMethod();
    response.sourceItemId = pQuest->GetSrcItemId();
    response.title = Title;
    response.details = Details;
    response.objectivesText = Objectives;
    response.endText = EndText;
    response.completedText = CompletedText;
    response.portraitGiverText = PortraitGiverText;
    response.portraitGiverName = PortraitGiverName;
    response.portraitTurnInText = PortraitTurnInText;
    response.portraitTurnInName = PortraitTurnInName;

    // Preserve the existing quest backend policy: the query cache receives
    // the same configured rewards as the quest-details window.
    response.rewardMoney = pQuest->GetRewOrReqMoney();
    for (size_t index = 0; index < QUEST_SOURCE_ITEM_IDS_COUNT; ++index)
    {
        response.requiredSourceItemIds[index] =
            pQuest->ReqSourceId[index];
        response.requiredSourceItemCounts[index] =
            pQuest->ReqSourceCount[index];
    }
    for (size_t index = 0; index < QUEST_REWARDS_COUNT; ++index)
    {
        response.rewardItemIds[index] = pQuest->RewItemId[index];
        response.rewardItemCounts[index] = pQuest->RewItemCount[index];
    }
    for (size_t index = 0; index < QUEST_REWARD_CHOICES_COUNT; ++index)
    {
        response.rewardChoiceItemIds[index] =
            pQuest->RewChoiceItemId[index];
        response.rewardChoiceItemCounts[index] =
            pQuest->RewChoiceItemCount[index];
    }
    for (size_t index = 0; index < QUEST_REWARD_CURRENCY_COUNT; ++index)
    {
        response.rewardCurrencyIds[index] =
            pQuest->RewCurrencyId[index];
        response.rewardCurrencyCounts[index] =
            pQuest->RewCurrencyCount[index];
    }
    for (size_t index = 0; index < QUEST_REPUTATIONS_COUNT; ++index)
    {
        response.rewardFactionIds[index] =
            pQuest->RewRepFaction[index];
        response.rewardFactionValueIds[index] =
            uint32(pQuest->RewRepValueId[index]);
        response.rewardFactionValueOverrides[index] = 0;
    }

    // The legacy quest schema has no stable 5.4.8 QuestObjective IDs.
    // Sending synthetic IDs would poison the client cache and break later
    // objective updates, so retain the directly valid zero-count form.
    if (!MopQuestQueryPackets::BuildResponse(data, response))
    {
        sLog.outError("WORLD: SMSG_QUEST_QUERY_RESPONSE exceeds the "
            "18414 payload bounds");
        return;
    }

    GetMenuSession()->SendPacket(&data);

    // Log the sent packet
    DEBUG_LOG("WORLD: Sent SMSG_QUEST_QUERY_RESPONSE questid=%u",
        pQuest->GetQuestId());
}

/**
 * @brief Sends the quest reward offer window for a completed quest.
 *
 * @param pQuest The quest being rewarded.
 * @param npcGUID The quest giver guid.
 * @param EnableNext True if the client should allow automatic progression to the next quest.
 */
void PlayerMenu::SendQuestGiverOfferReward(Quest const* pQuest, ObjectGuid npcGUID, bool EnableNext) const
{
    std::string Title = pQuest->GetTitle();
    std::string OfferRewardText = pQuest->GetOfferRewardText();
    std::string PortraitGiverName = pQuest->GetPortraitGiverName();
    std::string PortraitGiverText = pQuest->GetPortraitGiverText();
    std::string PortraitTurnInName = pQuest->GetPortraitTurnInName();
    std::string PortraitTurnInText = pQuest->GetPortraitTurnInText();

    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->OfferRewardText.size() > (size_t)loc_idx && !ql->OfferRewardText[loc_idx].empty())
            {
                OfferRewardText = ql->OfferRewardText[loc_idx];
            }
            if (ql->PortraitGiverName.size() > (size_t)loc_idx && !ql->PortraitGiverName[loc_idx].empty())
            {
                PortraitGiverName = ql->PortraitGiverName[loc_idx];
            }
            if (ql->PortraitGiverText.size() > (size_t)loc_idx && !ql->PortraitGiverText[loc_idx].empty())
            {
                PortraitGiverText = ql->PortraitGiverText[loc_idx];
            }
            if (ql->PortraitTurnInName.size() > (size_t)loc_idx && !ql->PortraitTurnInName[loc_idx].empty())
            {
                PortraitTurnInName = ql->PortraitTurnInName[loc_idx];
            }
            if (ql->PortraitTurnInText.size() > (size_t)loc_idx && !ql->PortraitTurnInText[loc_idx].empty())
            {
                PortraitTurnInText = ql->PortraitTurnInText[loc_idx];
            }
        }
    }

    MopQuestGiverPackets::QuestRewardOffer response;
    response.questGiverGuid = npcGUID;
    response.questId = pQuest->GetQuestId();
    response.suggestedPlayers = pQuest->GetSuggestedPlayers();
    response.bonusTalents = pQuest->GetBonusTalents();
    response.rewardSkillId = pQuest->GetRewSkill();
    response.rewardXp = pQuest->XPValue(GetMenuSession()->GetPlayer());
    response.rewardSpell = pQuest->GetRewSpell();
    response.rewardSpellCast = pQuest->GetRewSpellCast();
    response.questFlags = pQuest->GetQuestFlags();
    response.characterTitleId = pQuest->GetCharTitleId();
    response.rewardItemCount = pQuest->GetRewItemsCount();
    response.rewardChoiceItemCount = pQuest->GetRewChoiceItemsCount();
    response.rewardSkillPoints = pQuest->GetRewSkillValue();
    response.questGiverPortrait = pQuest->GetPortraitGiver();
    response.questTurnInPortrait = pQuest->GetPortraitTurnIn();
    response.autoLaunch = EnableNext;
    response.questTitle = Title;
    response.offerRewardText = OfferRewardText;
    response.questGiverTextWindow = PortraitGiverText;
    response.questGiverTargetName = PortraitGiverName;
    response.questTurnTextWindow = PortraitTurnInText;
    response.questTurnTargetName = PortraitTurnInName;

    for (size_t index = 0; index < QUEST_REWARD_CHOICES_COUNT; ++index)
    {
        response.rewardChoiceItemIds[index] =
            pQuest->RewChoiceItemId[index];
        response.rewardChoiceItemCounts[index] =
            pQuest->RewChoiceItemCount[index];
        if (ItemPrototype const* item = ObjectMgr::GetItemPrototype(
                pQuest->RewChoiceItemId[index]))
        {
            response.rewardChoiceItemDisplayIds[index] =
                item->DisplayInfoID;
        }
    }
    for (size_t index = 0; index < QUEST_REWARDS_COUNT; ++index)
    {
        response.rewardItemIds[index] = pQuest->RewItemId[index];
        response.rewardItemCounts[index] = pQuest->RewItemCount[index];
        if (ItemPrototype const* item = ObjectMgr::GetItemPrototype(
                pQuest->RewItemId[index]))
        {
            response.rewardItemDisplayIds[index] = item->DisplayInfoID;
        }
    }
    for (size_t index = 0; index < QUEST_REWARD_CURRENCY_COUNT; ++index)
    {
        response.rewardCurrencyIds[index] =
            pQuest->RewCurrencyId[index];
        response.rewardCurrencyCounts[index] =
            pQuest->RewCurrencyCount[index];
    }
    for (size_t index = 0; index < QUEST_REPUTATIONS_COUNT; ++index)
    {
        response.rewardFactionIds[index] = pQuest->RewRepFaction[index];
        response.rewardFactionValueIds[index] =
            uint32(pQuest->RewRepValueId[index]);
        // The current quest schema has no separate 18414 override field.
        response.rewardFactionValueOverrides[index] = 0;
    }

    if (GetMenuSession()->GetPlayer()->getLevel() >=
        sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        response.rewardMoney = pQuest->GetRewMoneyMaxLevel();
        response.rewardXp = 0;
    }
    else
    {
        response.rewardMoney = uint32(std::max(
            pQuest->GetRewOrReqMoney(), int32(0)));
    }

    for (uint32 i = 0; i < QUEST_EMOTE_COUNT; ++i)
    {
        if (pQuest->OfferRewardEmote[i] <= 0)
        {
            break;
        }
        MopQuestGiverPackets::QuestEmote emote;
        emote.delay = pQuest->OfferRewardEmoteDelay[i];
        emote.emote = pQuest->OfferRewardEmote[i];
        response.emotes.push_back(emote);
    }

    // Flags2, reward-package ID, reputation mask, and quest-taker entry are
    // explicit zeroes because the legacy quest backend cannot authoritatively
    // supply their 18414 meanings.
    WorldPacket data;
    if (!MopQuestGiverPackets::BuildQuestOfferReward(data, response))
    {
        sLog.outError("WORLD: SMSG_QUESTGIVER_OFFER_REWARD exceeds the "
            "18414 payload bounds");
        return;
    }
    GetMenuSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_OFFER_REWARD NPCGuid = %s, questid = %u", npcGUID.GetString().c_str(), pQuest->GetQuestId());
}

/**
 * @brief Sends the quest item request window for a quest turn-in.
 *
 * @param pQuest The quest being turned in.
 * @param npcGUID The quest giver guid.
 * @param Completable True if the player currently meets the quest completion requirements.
 * @param AutoLaunch True when the client should automatically advance the
 * quest frame.
 */
void PlayerMenu::SendQuestGiverRequestItems(Quest const* pQuest,
    ObjectGuid npcGUID, bool Completable, bool AutoLaunch) const
{
    if (!pQuest->GetReqItemsCount() && !pQuest->GetReqCurrencyCount() && Completable)
    {
        SendQuestGiverOfferReward(pQuest, npcGUID, true);
        return;
    }

    std::string Title = pQuest->GetTitle();
    std::string RequestItemsText = pQuest->GetRequestItemsText();

    int loc_idx = GetMenuSession()->GetSessionDbLocaleIndex();
    if (loc_idx >= 0)
    {
        if (QuestLocale const* ql = sObjectMgr.GetQuestLocale(pQuest->GetQuestId()))
        {
            if (ql->Title.size() > (size_t)loc_idx && !ql->Title[loc_idx].empty())
            {
                Title = ql->Title[loc_idx];
            }
            if (ql->RequestItemsText.size() > (size_t)loc_idx && !ql->RequestItemsText[loc_idx].empty())
            {
                RequestItemsText = ql->RequestItemsText[loc_idx];
            }
        }
    }

    MopQuestGiverPackets::QuestRequestItems response;
    response.questGiverGuid = npcGUID;
    response.suggestedPlayers = pQuest->GetSuggestedPlayers();
    response.questFlags = pQuest->GetQuestFlags();
    response.statusFlags = Completable ? 0x5F : 0x5B;
    response.requiredMoney = pQuest->GetRewOrReqMoney() < 0 ?
        uint32(-pQuest->GetRewOrReqMoney()) : 0;
    response.emote = Completable ? pQuest->GetCompleteEmote() :
        pQuest->GetIncompleteEmote();
    response.questId = pQuest->GetQuestId();
    response.autoLaunch = AutoLaunch;
    response.title = Title;
    response.requestText = RequestItemsText;

    for (size_t index = 0; index < QUEST_ITEM_OBJECTIVES_COUNT; ++index)
    {
        response.requiredItemIds[index] = pQuest->ReqItemId[index];
        response.requiredItemCounts[index] = pQuest->ReqItemCount[index];
        if (ItemPrototype const* item =
            ObjectMgr::GetItemPrototype(pQuest->ReqItemId[index]))
        {
            response.requiredItemDisplayIds[index] = item->DisplayInfoID;
        }
    }
    for (size_t index = 0; index < QUEST_REQUIRED_CURRENCY_COUNT; ++index)
    {
        response.requiredCurrencyIds[index] =
            pQuest->ReqCurrencyId[index];
        response.requiredCurrencyCounts[index] =
            pQuest->ReqCurrencyCount[index];
    }

    // The legacy schema has no QuestFlags2 and no separate completion-emote
    // delay. Both remain zero rather than inheriting fork-only guesses.
    WorldPacket data;
    if (!MopQuestGiverPackets::BuildQuestRequestItems(data, response))
    {
        sLog.outError("WORLD: SMSG_QUESTGIVER_REQUEST_ITEMS exceeds the "
            "18414 payload bounds");
        return;
    }
    GetMenuSession()->SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_QUESTGIVER_REQUEST_ITEMS NPCGuid = %s, questid = %u", npcGUID.GetString().c_str(), pQuest->GetQuestId());
}
