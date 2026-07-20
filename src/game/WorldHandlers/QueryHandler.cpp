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

/**
 * @file QueryHandler.cpp
 * @brief Query opcode handlers for game data lookups
 *
 * This file handles query opcodes for requesting game data:
 * - CMSG_NAME_QUERY: Query character name by GUID
 * - CMSG_ITEM_QUERY: Query item info
 * - CMSG_GAMEOBJECT_QUERY: Query gameobject info
 * - CMSG_CREATURE_QUERY: Query creature info
 * - CMSG_PAGE_TEXT_QUERY: Query page text
 * - CMSG_QUERY_TIME: Query server time
 *
 * Query responses include name, display ID, and other metadata
 * for the requested object type.
 */

#include "Common.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Database/DatabaseImpl.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "UpdateMask.h"
#include "NPCHandler.h"
#include "Pet.h"
#include "MapManager.h"
#include "SQLStorages.h"
#include "Server/MopQueryPackets.h"

/**
 * @brief Sends an in-memory name query response for a player.
 *
 * @param p The player being queried.
 */
void WorldSession::SendNameQueryOpcode(Player* p)
{
    if (!p)
    {
        return;
    }

    MopQueryPackets::NameQueryResponse response;
    response.guid = p->GetObjectGuid().GetRawValue();
    response.result = 0;
    response.realmId = realmID;
    response.accountId = 0;
    response.level = 0;
    response.classId = uint8(p->getClass());
    response.race = uint8(p->getRace());
    response.gender = uint8(p->getGender());
    response.displayGuid = response.guid;
    response.name = p->GetName();
    if (DeclinedName const* names = p->GetDeclinedNames())
    {
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
            response.declinedNames[i] = names->name[i];
    }

    WorldPacket data(SMSG_NAME_QUERY_RESPONSE, 128);
    MopQueryPackets::BuildNameQueryResponse(data, response);
    SendPacket(&data);
}

/**
 * @brief Starts an asynchronous database lookup for a player name query.
 *
 * @param guid The queried player guid.
 */
void WorldSession::SendNameQueryOpcodeFromDB(ObjectGuid guid)
{
    CharacterDatabase.AsyncPQuery(&WorldSession::SendNameQueryOpcodeFromDBCallBack,
                                  GetAccountId(), guid.GetRawValue(),
                                  !sWorld.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED) ?
                                  //   ------- Query Without Declined Names --------
                                  //       0       1       2       3         4
                                  "SELECT `guid`, `name`, `race`, `gender`, `class` "
                                  "FROM `characters` WHERE `guid` = '%u'"
                                  :
                                  //   --------- Query With Declined Names ---------
                                  //                    0       1       2       3         4
                                  "SELECT `characters`.`guid`, `name`, `race`, `gender`, `class`, "
                                  //   5        6         7             8               9
                                  "`genitive`, `dative`, `accusative`, `instrumental`, `prepositional` "
                                  "FROM `characters` LEFT JOIN `character_declinedname` ON `characters`.`guid` = `character_declinedname`.`guid` WHERE `characters`.`guid` = '%u'",
                                  guid.GetCounter());
}

/**
 * @brief Completes an asynchronous player name query from the database.
 *
 * @param result The database result.
 * @param accountId The requesting account id.
 * @param requestedGuid The original queried GUID.
 */
void WorldSession::SendNameQueryOpcodeFromDBCallBack(QueryResult* result,
    uint32 accountId, uint64 requestedGuid)
{
    WorldSession* session = sWorld.FindSession(accountId);
    if (!session)
    {
        delete result;
        return;
    }

    MopQueryPackets::NameQueryResponse response;
    response.guid = requestedGuid;
    if (result)
    {
        Field* fields = result->Fetch();
        response.result = 0;
        response.realmId = realmID;
        response.accountId = 0;
        response.level = 0;
        response.name = fields[1].GetCppString();
        if (response.name.empty())
            response.name = session->GetMangosString(LANG_NON_EXIST_CHARACTER);
        else
        {
            response.race = fields[2].GetUInt8();
            response.gender = fields[3].GetUInt8();
            response.classId = fields[4].GetUInt8();
        }
        response.displayGuid = requestedGuid;

        if (sWorld.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED) &&
            !fields[5].GetCppString().empty())
        {
            for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
                response.declinedNames[i] = fields[i + 5].GetCppString();
        }
    }
    else
        response.result = 1;

    WorldPacket data(SMSG_NAME_QUERY_RESPONSE, 128);
    MopQueryPackets::BuildNameQueryResponse(data, response);
    session->SendPacket(&data);
    delete result;
}

/**
 * @brief Handles a client request to query another player's name.
 *
 * @param recv_data The incoming name query packet.
 */
void WorldSession::HandleNameQueryOpcode(WorldPacket& recv_data)
{
    MopQueryPackets::NameQueryRequest const request =
        MopQueryPackets::ReadNameQueryRequest(recv_data);
    Player* pChar = sObjectMgr.GetPlayer(ObjectGuid(request.guid));

    if (pChar)
    {
        SendNameQueryOpcode(pChar);
    }
    else
    {
        SendNameQueryOpcodeFromDB(ObjectGuid(request.guid));
    }
}

/**
 * @brief Handles a client request for the current server time.
 *
 * @param recv_data The unused incoming packet.
 */
void WorldSession::HandleQueryTimeOpcode(WorldPacket & /*recv_data*/)
{
    SendQueryTimeResponse();
}

/// Only _static_ data send in this packet !!!
void WorldSession::HandleCreatureQueryOpcode(WorldPacket& recv_data)
{
    uint32 entry;
    recv_data >> entry;

    MopQueryPackets::CreatureQueryResponse response;
    response.entry = entry;
    if (CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(entry))
    {
        response.hasData = true;
        char const* name = ci->Name;
        char const* subName = ci->SubName;
        int const localeIndex = GetSessionDbLocaleIndex();
        if (localeIndex >= 0)
            sObjectMgr.GetCreatureLocaleStrings(entry, localeIndex, &name, &subName);

        DETAIL_LOG("WORLD: CMSG_CREATURE_QUERY '%s' - Entry: %u.", ci->Name, entry);
        response.name = name ? name : "";
        response.subName = subName ? subName : "";
        response.iconName = ci->IconName ? ci->IconName : "";
        response.creatureType = ci->CreatureType;
        response.family = ci->Family;
        response.rank = ci->Rank;
        response.expansion = uint32(ci->Expansion);
        response.movementTemplateId = ci->MovementTemplateId;
        response.creatureTypeFlags = ci->CreatureTypeFlags;
        response.creatureTypeFlags2 = 0;
        response.modelIds = {{ ci->ModelId[0], ci->ModelId[1], ci->ModelId[2], ci->ModelId[3] }};
        response.killCredits = {{ ci->KillCredit[0], ci->KillCredit[1] }};
        response.healthMultiplier = ci->HealthMultiplier;
        response.powerMultiplier = ci->PowerMultiplier;
        response.racialLeader = ci->RacialLeader != 0;
        for (size_t i = 0; i < response.questItems.size(); ++i)
            response.questItems[i] = ci->QuestItems[i];
    }
    else
    {
        DEBUG_LOG("WORLD: CMSG_CREATURE_QUERY - Entry: %u NO CREATURE INFO!", entry);
    }

    WorldPacket data(SMSG_CREATURE_QUERY_RESPONSE, 128);
    MopQueryPackets::BuildCreatureQueryResponse(data, response);
    SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_CREATURE_QUERY_RESPONSE");
}

/// Only _static_ data send in this packet !!!
void WorldSession::HandleGameObjectQueryOpcode(WorldPacket& recv_data)
{
    MopQueryPackets::GameObjectQueryRequest const request =
        MopQueryPackets::ReadGameObjectQueryRequest(recv_data);
    MopQueryPackets::GameObjectQueryResponse response;
    response.entry = request.entry;

    if (GameObjectInfo const* info = ObjectMgr::GetGameObjectInfo(request.entry))
    {
        response.hasData = true;
        char const* name = info->name;
        char const* castBarCaption = info->castBarCaption;
        int const localeIndex = GetSessionDbLocaleIndex();
        if (localeIndex >= 0)
        {
            GameObjectLocale const* gl = sObjectMgr.GetGameObjectLocale(request.entry);
            if (gl)
            {
                if (gl->Name.size() > size_t(localeIndex) && !gl->Name[localeIndex].empty())
                    name = gl->Name[localeIndex].c_str();
                if (gl->CastBarCaption.size() > size_t(localeIndex) && !gl->CastBarCaption[localeIndex].empty())
                    castBarCaption = gl->CastBarCaption[localeIndex].c_str();
            }
        }

        response.names[0] = name ? name : "";
        response.iconName = info->IconName ? info->IconName : "";
        response.castBarCaption = castBarCaption ? castBarCaption : "";
        response.unknownString = info->unk1 ? info->unk1 : "";
        response.type = info->type;
        response.displayId = info->displayId;
        for (size_t i = 0; i < response.data.size(); ++i)
            response.data[i] = info->raw.data[i];
        response.size = info->size;
        for (uint32 questItem : info->questItems)
            response.questItems.push_back(questItem);

        DETAIL_LOG("WORLD: CMSG_GAMEOBJECT_QUERY '%s' - Entry: %u.",
                   response.names[0].c_str(), request.entry);
    }
    else
    {
        ObjectGuid const guid(request.guid);
        DEBUG_LOG("WORLD: CMSG_GAMEOBJECT_QUERY - Guid: %s Entry: %u Missing gameobject info!",
                  guid.GetString().c_str(), request.entry);
    }

    WorldPacket data(SMSG_GAMEOBJECT_QUERY_RESPONSE, 200);
    MopQueryPackets::BuildGameObjectQueryResponse(data, response);
    SendPacket(&data);
    DEBUG_LOG("WORLD: Sent SMSG_GAMEOBJECT_QUERY_RESPONSE");
}

/**
 * @brief Handles a corpse query and returns corpse or graveyard location data.
 *
 * @param recv_data The unused incoming packet.
 */
void WorldSession::HandleCorpseQueryOpcode(WorldPacket & /*recv_data*/)
{
    DETAIL_LOG("WORLD: Received opcode MSG_CORPSE_QUERY");

    Corpse* corpse = GetPlayer()->GetCorpse();

    if (!corpse)
    {
        WorldPacket data(MSG_CORPSE_QUERY, 1);
        data << uint8(0);                                   // corpse not found
        SendPacket(&data);
        return;
    }

    uint32 corpsemapid = corpse->GetMapId();
    float x = corpse->GetPositionX();
    float y = corpse->GetPositionY();
    float z = corpse->GetPositionZ();
    int32 mapid = corpsemapid;

    // if corpse at different map
    if (corpsemapid != _player->GetMapId())
    {
        // search entrance map for proper show entrance
        if (MapEntry const* corpseMapEntry = sMapStore.LookupEntry(corpsemapid))
        {
            if (corpseMapEntry->IsDungeon() && corpseMapEntry->CorpseMapID >= 0)
            {
                // if corpse map have entrance
                if (TerrainInfo const* entranceMap = sTerrainMgr.LoadTerrain(corpseMapEntry->CorpseMapID))
                {
                    mapid = corpseMapEntry->CorpseMapID;
                    x = corpseMapEntry->ghost_entrance_x;
                    y = corpseMapEntry->ghost_entrance_y;
                    z = entranceMap->GetHeightStatic(x, y, MAX_HEIGHT);
                }
            }
        }
    }

    WorldPacket data(MSG_CORPSE_QUERY, 1 + (6 * 4));
    data << uint8(1);                                       // corpse found
    data << int32(mapid);
    data << float(x);
    data << float(y);
    data << float(z);
    data << uint32(corpsemapid);
    data << uint32(0);                                      // unknown
    SendPacket(&data);
}

/**
 * @brief Handles an NPC text query and sends localized gossip text options.
 *
 * @param recv_data The incoming NPC text query packet.
 */
void WorldSession::HandleNpcTextQueryOpcode(WorldPacket& recv_data)
{
    uint32 textID;
    ObjectGuid guid;

    recv_data >> textID;
    recv_data >> guid;

    DETAIL_LOG("WORLD: CMSG_NPC_TEXT_QUERY ID '%u'", textID);

    _player->SetTargetGuid(guid);

    GossipText const* pGossip = sObjectMgr.GetGossipText(textID);

    WorldPacket data(SMSG_NPC_TEXT_UPDATE, 100);            // guess size
    data << textID;

    if (!pGossip)
    {
        for (uint32 i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            data << float(0);
            data << "Greetings $N";
            data << "Greetings $N";
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
        }
    }
    else
    {
        std::string Text_0[MAX_GOSSIP_TEXT_OPTIONS], Text_1[MAX_GOSSIP_TEXT_OPTIONS];
        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            Text_0[i] = pGossip->Options[i].Text_0;
            Text_1[i] = pGossip->Options[i].Text_1;
        }

        int loc_idx = GetSessionDbLocaleIndex();

        sObjectMgr.GetNpcTextLocaleStringsAll(textID, loc_idx, &Text_0, &Text_1);

        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            data << pGossip->Options[i].Probability;

            if (Text_0[i].empty())
            {
                data << Text_1[i];
            }
            else
            {
                data << Text_0[i];
            }

            if (Text_1[i].empty())
            {
                data << Text_0[i];
            }
            else
            {
                data << Text_1[i];
            }

            data << pGossip->Options[i].Language;

            for (int j = 0; j < 3; ++j)
            {
                data << pGossip->Options[i].Emotes[j]._Delay;
                data << pGossip->Options[i].Emotes[j]._Emote;
            }
        }
    }

    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_NPC_TEXT_UPDATE");
}

/**
 * @brief Handles an item page text query and sends all linked pages.
 *
 * @param recv_data The incoming page text query packet.
 */
void WorldSession::HandlePageTextQueryOpcode(WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_PAGE_TEXT_QUERY");
    recv_data.hexlike();

    uint32 pageID;
    recv_data >> pageID;
    recv_data.read_skip<uint64>();                          // guid

    while (pageID)
    {
        PageText const* pPage = sPageTextStore.LookupEntry<PageText>(pageID);
        // guess size
        WorldPacket data(SMSG_PAGE_TEXT_QUERY_RESPONSE, 50);
        data << pageID;

        if (!pPage)
        {
            data << "Item page missing.";
            data << uint32(0);
            pageID = 0;
        }
        else
        {
            std::string Text = pPage->Text;

            int loc_idx = GetSessionDbLocaleIndex();
            if (loc_idx >= 0)
            {
                PageTextLocale const* pl = sObjectMgr.GetPageTextLocale(pageID);
                if (pl)
                {
                    if (pl->Text.size() > size_t(loc_idx) && !pl->Text[loc_idx].empty())
                    {
                        Text = pl->Text[loc_idx];
                    }
                }
            }

            data << Text;
            data << uint32(pPage->Next_Page);
            pageID = pPage->Next_Page;
        }
        SendPacket(&data);

        DEBUG_LOG("WORLD: Sent SMSG_PAGE_TEXT_QUERY_RESPONSE");
    }
}

void WorldSession::HandleCorpseMapPositionQueryOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Recv CMSG_CORPSE_MAP_POSITION_QUERY");

    uint32 unk;
    recv_data >> unk;

    WorldPacket data(SMSG_CORPSE_TRANSPORT_QUERY, 4 + 4 + 4 + 4);
    data << float(0);
    data << float(0);
    data << float(0);
    data << float(0);
    SendPacket(&data);
}

void WorldSession::HandleQueryQuestsCompletedOpcode(WorldPacket& /*recv_data */)
{
    uint32 count = 0;

    WorldPacket data(SMSG_ALL_QUESTS_COMPLETED, 4 + 4 * count);
    data << uint32(count);

    for (QuestStatusMap::const_iterator itr = _player->getQuestStatusMap().begin(); itr != _player->getQuestStatusMap().end(); ++itr)
    {
        if (itr->second.m_rewarded)
        {
            data << uint32(itr->first);
            ++count;
        }
    }
    data.put<uint32>(0, count);
    SendPacket(&data);
}

void WorldSession::HandleQuestPOIQueryOpcode(WorldPacket& recv_data)
{
    uint32 count;
    recv_data >> count;                                     // quest count, max=25

    if (count > MAX_QUEST_LOG_SIZE)
    {
        recv_data.rpos(recv_data.wpos());                   // set to end to avoid warnings spam
        return;
    }

    WorldPacket data(SMSG_QUEST_POI_QUERY_RESPONSE, 4 + (4 + 4)*count);
    data << uint32(count);                                  // count

    for (uint32 i = 0; i < count; ++i)
    {
        uint32 questId;
        recv_data >> questId;                               // quest id

        bool questOk = false;

        uint16 questSlot = _player->FindQuestSlot(questId);

        if (questSlot != MAX_QUEST_LOG_SIZE)
        {
            questOk = _player->GetQuestSlotQuestId(questSlot) == questId;
        }

        if (questOk)
        {
            QuestPOIVector const* POI = sObjectMgr.GetQuestPOIVector(questId);

            if (POI)
            {
                data << uint32(questId);                    // quest ID
                data << uint32(POI->size());                // POI count

                for (QuestPOIVector::const_iterator itr = POI->begin(); itr != POI->end(); ++itr)
                {
                    data << uint32(itr->PoiId);             // POI index
                    data << int32(itr->ObjectiveIndex);     // objective index
                    data << uint32(itr->MapId);             // mapid
                    data << uint32(itr->MapAreaId);         // world map area id
                    data << uint32(itr->FloorId);           // floor id
                    data << uint32(itr->Unk3);              // unknown
                    data << uint32(itr->Unk4);              // unknown
                    data << uint32(itr->points.size());     // POI points count

                    for (std::vector<QuestPOIPoint>::const_iterator itr2 = itr->points.begin(); itr2 != itr->points.end(); ++itr2)
                    {
                        data << int32(itr2->x);             // POI point x
                        data << int32(itr2->y);             // POI point y
                    }
                }
            }
            else
            {
                data << uint32(questId);                    // quest ID
                data << uint32(0);                          // POI count
            }
        }
        else
        {
            data << uint32(questId);                        // quest ID
            data << uint32(0);                              // POI count
        }
    }

    SendPacket(&data);
}

/**
 * @brief Sends the current server time to the client.
 */
void WorldSession::SendQueryTimeResponse()
{
    WorldPacket data(SMSG_QUERY_TIME_RESPONSE, 4 + 4);
    uint32 const serverTime = uint32(time(NULL));
    uint32 const secondsUntilReset = uint32(sWorld.GetNextDailyQuestsResetTime() - time(NULL));
    MopQueryPackets::BuildQueryTimeResponse(data, serverTime, secondsUntilReset);
    SendPacket(&data);
}
