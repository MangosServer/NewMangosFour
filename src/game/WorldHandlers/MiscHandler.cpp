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
 * @file MiscHandler.cpp
 * @brief Miscellaneous opcode handlers
 *
 * This file handles miscellaneous opcodes that don't fit into
 * other specific handler categories:
 *
 * - CMSG_NAME_QUERY: Query character name by GUID
 * - CMSG_PING: Client ping/pong
 * - CMSG_LOGOUT_REQUEST: Logout request
 * - CMSG_LOGOUT_CANCEL: Cancel logout
 * - CMSG_ZONE_UPDATE: Zone update
 * - CMSG_SET_ACTIONBAR_TOGGLES: Set action bar toggles
 * - CMSG_VIOLENCE_LEVEL: Set the client-only violence rendering level
 * - CMSG_SET_ACTIONBAR_TEXT: Set action bar text
 * - CMSG_MOVE_TIME_SKIPPED: Movement time skipped
 * - CMSG_MOVE_FALL_RESET: Fall reset
 * - CMSG_WORLD_STATE_UI_TIMER: UI timer
 * - CMSG_NEXT_CINEMATIC_CAMERA: Cinematic camera
 * - CMSG_COMPLETE_CINEMATIC: Complete cinematic
 * - CMSG_SET_FACTION_AT_WAR: Set faction at war
 * - CMSG_SET_WATCHED_FACTION: Set watched faction
 * - CMSG_TOGGLE_PVP: Toggle PVP flag
 * - CMSG_SET_PLAYER_DECLARED_NAME: Set player name
 */

#include "Common.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Database/DatabaseImpl.h"
#include "WorldPacket.h"
#include "MopFarSightPackets.h"
#include "MopLogoutPackets.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "World.h"
#include "CinematicFlyover.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "LFGMgr.h"
#include "WorldSession.h"
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "UpdateData.h"
#include "LootMgr.h"
#include "Chat.h"
#include "ScriptMgr.h"
#include "zlib.h"
#include "ObjectAccessor.h"
#include "Object.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Guild.h"
#include "Pet.h"
#include "SocialMgr.h"
#include "DBCEnums.h"
#include <algorithm>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

void WorldSession::HandleRequestCemeteryListOpcode(WorldPacket& /*recv_data*/)
{
    std::vector<uint32> const cemeteryIds =
        sObjectMgr.GetGraveYardIds(GetPlayer()->GetZoneId(), GetPlayer()->GetTeam(), MopDeathPackets::CEMETERY_LIST_MAX);

    WorldPacket data;
    MopDeathPackets::BuildCemeteryListResponse(data, cemeteryIds, false);
    SendPacket(&data);
}

void WorldSession::HandleRepopRequestOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REPOP_REQUEST");

    recv_data.read_skip<uint8>();

    if (GetPlayer()->IsAlive() || GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        return;
    }

    if (GetPlayer()->HasAuraType(SPELL_AURA_PREVENT_RESURRECTION))
    {
        return;
    }

    // the world update order is sessions, players, creatures
    // the netcode runs in parallel with all of these
    // creatures can kill players
    // so if the server is lagging enough the player can
    // release spirit after he's killed but before he is updated
    if (GetPlayer()->GetDeathState() == JUST_DIED)
    {
        DEBUG_LOG("HandleRepopRequestOpcode: got request after player %s(%d) was killed and before he was updated", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow());
        GetPlayer()->KillPlayer();
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetPlayer()->GetEluna())
    {
        e->OnRepop(GetPlayer());
    }
#endif /* ENABLE_ELUNA */

    // this is spirit release confirm?
    GetPlayer()->RemovePet(PET_SAVE_REAGENTS);
    GetPlayer()->BuildPlayerRepop();
    GetPlayer()->RepopAtGraveyard();
}

/**
 * @brief Handles a /who query and sends matching player results.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleWhoOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_WHO");
    // recv_data.hexlike();

    uint32 clientcount = 0;

    uint32 level_min, level_max, racemask, classmask, zones_count, str_count;
    uint32 zoneids[10];                                     // 10 is client limit
    std::string player_name, guild_name;

    recv_data >> level_min;                                 // maximal player level, default 0
    recv_data >> level_max;                                 // minimal player level, default 100 (MAX_LEVEL)
    recv_data >> player_name;                               // player name, case sensitive...

    recv_data >> guild_name;                                // guild name, case sensitive...

    recv_data >> racemask;                                  // race mask
    recv_data >> classmask;                                 // class mask
    recv_data >> zones_count;                               // zones count, client limit=10 (2.0.10)

    if (zones_count > 10)
    {
        return;                                             // can't be received from real client or broken packet
    }

    for (uint32 i = 0; i < zones_count; ++i)
    {
        uint32 temp;
        recv_data >> temp;                                  // zone id, 0 if zone is unknown...
        zoneids[i] = temp;
        DEBUG_LOG("Zone %u: %u", i, zoneids[i]);
    }

    recv_data >> str_count;                                 // user entered strings count, client limit=4 (checked on 2.0.10)

    if (str_count > 4)
    {
        return;                                             // can't be received from real client or broken packet
    }

    DEBUG_LOG("Minlvl %u, maxlvl %u, name %s, guild %s, racemask %u, classmask %u, zones %u, strings %u", level_min, level_max, player_name.c_str(), guild_name.c_str(), racemask, classmask, zones_count, str_count);

    std::wstring str[4];                                    // 4 is client limit
    for (uint32 i = 0; i < str_count; ++i)
    {
        std::string temp;
        recv_data >> temp;                                  // user entered string, it used as universal search pattern(guild+player name)?

        if (!Utf8toWStr(temp, str[i]))
        {
            continue;
        }

        wstrToLower(str[i]);

        DEBUG_LOG("String %u: %s", i, temp.c_str());
    }

    std::wstring wplayer_name;
    std::wstring wguild_name;
    if (!(Utf8toWStr(player_name, wplayer_name) && Utf8toWStr(guild_name, wguild_name)))
    {
        return;
    }

    wstrToLower(wplayer_name);
    wstrToLower(wguild_name);

    // client send in case not set max level value 100 but mangos support 255 max level,
    // update it to show GMs with characters after 100 level
    if (level_max >= MAX_LEVEL)
    {
        level_max = STRONG_MAX_LEVEL;
    }

    Team team = _player->GetTeam();
    uint32 security = GetSecurity();
    bool allowTwoSideWhoList = sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST);
    AccountTypes gmLevelInWhoList = (AccountTypes)sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST);

    uint32 matchcount = 0;
    uint32 displaycount = 0;

    WorldPacket data(SMSG_WHO, 50);                         // guess size
    data << uint32(clientcount);                            // clientcount place holder, listed count
    data << uint32(clientcount);                            // clientcount place holder, online count

    uint32 count = 0;
    sObjectAccessor.DoForAllPlayers([&](Player* pl)->void
    {
        ++count;

        if (clientcount == 50)
        {
            return;
        }

        if (security == SEC_PLAYER)
        {
            // player can see member of other team only if CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST
            if (pl->GetTeam() != team && !allowTwoSideWhoList)
            {
                return;
            }

            // player can see MODERATOR, GAME MASTER, ADMINISTRATOR only if CONFIG_GM_IN_WHO_LIST
            if (pl->GetSession()->GetSecurity() > gmLevelInWhoList)
            {
                return;
            }
        }

        // do not process players which are not in world
        if (!pl->IsInWorld())
        {
            return;
        }

        // check if target is globally visible for player
        if (!pl->IsVisibleGloballyFor(_player))
        {
            return;
        }

        // check if target's level is in level range
        uint32 lvl = pl->getLevel();
        if (lvl < level_min || lvl > level_max)
        {
            return;
        }

        // check if class matches classmask
        uint32 class_ = pl->getClass();
        if (!(classmask & (1 << class_)))
        {
            return;
        }

        // check if race matches racemask
        uint32 race = pl->getRace();
        if (!(racemask & (1 << race)))
        {
            return;
        }

        uint32 pzoneid = pl->GetZoneId();
        uint8 gender = pl->getGender();

        bool z_show = true;
        for (uint32 i = 0; i < zones_count; ++i)
        {
            if (zoneids[i] == pzoneid)
            {
                z_show = true;
                break;
            }

            z_show = false;
        }

        if (!z_show)
        {
            return;
        }

        std::string pname = pl->GetName();
        std::wstring wpname;
        if (!Utf8toWStr(pname, wpname))
        {
            return;
        }

        wstrToLower(wpname);

        if (!(wplayer_name.empty() || wpname.find(wplayer_name) != std::wstring::npos))
        {
            return;
        }

        std::string gname = sGuildMgr.GetGuildNameById(pl->GetGuildId());
        std::wstring wgname;
        if (!Utf8toWStr(gname, wgname))
        {
            return;
        }

        wstrToLower(wgname);

        if (!(wguild_name.empty() || wgname.find(wguild_name) != std::wstring::npos))
        {
            return;
        }

        std::string aname;
        if (AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(pzoneid))
        {
            aname = areaEntry->AreaName_lang[GetSessionDbcLocale()];
        }

        bool s_show = true;
        for (uint32 i = 0; i < str_count; ++i)
        {
            if (!str[i].empty())
            {
                if (wgname.find(str[i]) != std::wstring::npos ||
                    wpname.find(str[i]) != std::wstring::npos ||
                    Utf8FitTo(aname, str[i]))
                {
                    s_show = true;
                    break;
                }
                s_show = false;
            }
        }

        if (!s_show)
        {
            return;
        }

        data << pname;                                      // player name
        data << gname;                                      // guild name
        data << uint32(lvl);                                // player level
        data << uint32(class_);                             // player class
        data << uint32(race);                               // player race
        data << uint8(gender);                              // player gender
        data << uint32(pzoneid);                            // player zone id

        ++clientcount;
    });

    data.put(0, clientcount);                               // insert right count, listed count
    data.put(4, count > 50 ? count : clientcount);          // insert right count, online count


    SendPacket(&data);
    DEBUG_LOG("WORLD: Send SMSG_WHO Message");
}

/**
 * @brief Starts the logout flow and validates whether logout is allowed.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleLogoutRequestOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LOGOUT_REQUEST, security %u", GetSecurity());

    if (ObjectGuid lootGuid = GetPlayer()->GetLootGuid())
    {
        DoLootRelease(lootGuid);
    }

    bool instantLogout = (GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) || GetPlayer()->IsTaxiFlying() || GetSecurity() >= (AccountTypes)sWorld.getConfig(CONFIG_UINT32_INSTANT_LOGOUT));

    bool canLogoutInCombat = GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);

    uint8 reason = 0;
    if (GetPlayer()->IsInCombat() && !canLogoutInCombat)
    {
        reason = 1;
    }
    else if (GetPlayer()->m_movementInfo.HasMovementFlag(MovementFlags(MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR)))
    {
        reason = 3;                                         // is jumping or falling
    }
    else if (GetPlayer()->duel || GetPlayer()->HasAura(9454)) // is dueling or frozen by GM via freeze command
    {
        reason = 2;                                         // FIXME - Need the correct value
    }

    WorldPacket data;
    MopLogoutPackets::BuildResponse(data, reason, instantLogout != 0);
    SendPacket(&data);

    if (reason)
    {
        LogoutRequest(0);
        return;
    }

    // instant logout in taverns/cities or on taxi or for admins, gm's, mod's if its enabled in mangosd.conf
    if (instantLogout)
    {
        LogoutPlayer(true);
        return;
    }

    // not set flags if player can't free move to prevent lost state at logout cancel
    if (GetPlayer()->CanFreeMove())
    {
        float height = GetPlayer()->GetMap()->GetHeight(GetPlayer()->GetPhaseMask(), GetPlayer()->GetPositionX(), GetPlayer()->GetPositionY(), GetPlayer()->GetPositionZ());
        if ((GetPlayer()->GetPositionZ() < height + 0.1f) && !(GetPlayer()->IsInWater()))
        {
            GetPlayer()->SetStandState(UNIT_STAND_STATE_SIT);
        }

        GetPlayer()->SetRoot(true);
        GetPlayer()->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
    }

    LogoutRequest(time(NULL));
}

/**
 * @brief Acknowledges the client logout opcode.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandlePlayerLogoutOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_PLAYER_LOGOUT Message");
}

/**
 * @brief Cancels a pending logout request.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleLogoutCancelOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_LOGOUT_CANCEL Message");

    LogoutRequest(0);

    WorldPacket data;
    MopLogoutPackets::BuildCancelAck(data);
    SendPacket(&data);

    // not remove flags if can't free move - its not set in Logout request code.
    if (GetPlayer()->CanFreeMove())
    {
        //!we can move again
        GetPlayer()->SetRoot(false);

        //! Stand Up
        GetPlayer()->SetStandState(UNIT_STAND_STATE_STAND);

        //! DISABLE_ROTATE
        GetPlayer()->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_STUNNED);
    }

    DEBUG_LOG("WORLD: sent SMSG_LOGOUT_CANCEL_ACK Message");
}

/**
 * @brief Toggles or explicitly sets the player's PvP flag.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleTogglePvP(WorldPacket& recv_data)
{
    // this opcode can be used in two ways: Either set explicit new status or toggle old status
    if (recv_data.size() == 1)
    {
        bool newPvPStatus;
        recv_data >> newPvPStatus;
        GetPlayer()->ApplyModFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP, newPvPStatus);
        GetPlayer()->ApplyModFlag(PLAYER_FLAGS, PLAYER_FLAGS_PVP_TIMER, !newPvPStatus);
    }
    else
    {
        GetPlayer()->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP);
        GetPlayer()->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_PVP_TIMER);
    }

    if (GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP))
    {
        if (!GetPlayer()->IsPvP() || GetPlayer()->pvpInfo.endTimer != 0)
        {
            GetPlayer()->UpdatePvP(true, true);
        }
    }
    else
    {
        if (!GetPlayer()->pvpInfo.inHostileArea && GetPlayer()->IsPvP())
        {
            GetPlayer()->pvpInfo.endTimer = time(NULL);     // start toggle-off
        }
    }
}

/**
 * @brief Updates the player's cached zone and area.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleZoneUpdateOpcode(WorldPacket& recv_data)
{
    uint32 newZone;
    recv_data >> newZone;

    DETAIL_LOG("WORLD: Received opcode CMSG_ZONEUPDATE: newzone is %u", newZone);

    // use server side data
    uint32 newzone, newarea;
    GetPlayer()->GetZoneAndAreaId(newzone, newarea);
    GetPlayer()->UpdateZone(newzone, newarea);
}

/**
 * @brief Sets the player's current target selection.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetTargetOpcode(WorldPacket& recv_data)
{
    // When this packet send?
    ObjectGuid guid ;
    recv_data >> guid;

    _player->SetTargetGuid(guid);

    // update reputation list if need
    Unit* unit = sObjectAccessor.GetUnit(*_player, guid);   // can select group members at diff maps
    if (!unit)
    {
        return;
    }

    if (FactionTemplateEntry const* factionTemplateEntry = sFactionTemplateStore.LookupEntry(unit->getFaction()))
    {
        _player->GetReputationMgr().SetVisible(factionTemplateEntry);
    }
}

/**
 * @brief Sets the player's selected object guid.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetSelectionOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    guid[7] = recv_data.ReadBit();
    guid[6] = recv_data.ReadBit();
    guid[5] = recv_data.ReadBit();
    guid[4] = recv_data.ReadBit();
    guid[3] = recv_data.ReadBit();
    guid[2] = recv_data.ReadBit();
    guid[1] = recv_data.ReadBit();
    guid[0] = recv_data.ReadBit();

    recv_data.ReadByteSeq(guid[0]);
    recv_data.ReadByteSeq(guid[7]);
    recv_data.ReadByteSeq(guid[3]);
    recv_data.ReadByteSeq(guid[5]);
    recv_data.ReadByteSeq(guid[1]);
    recv_data.ReadByteSeq(guid[4]);
    recv_data.ReadByteSeq(guid[6]);
    recv_data.ReadByteSeq(guid[2]);

    _player->SetSelectionGuid(guid);

    // update reputation list if need
    Unit* unit = sObjectAccessor.GetUnit(*_player, guid);   // can select group members at diff maps
    if (!unit)
    {
        return;
    }

    if (FactionTemplateEntry const* factionTemplateEntry = sFactionTemplateStore.LookupEntry(unit->getFaction()))
    {
        _player->GetReputationMgr().SetVisible(factionTemplateEntry);
    }
}

/**
 * @brief Changes the player's stand state animation.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleStandStateChangeOpcode(WorldPacket& recv_data)
{
    // DEBUG_LOG("WORLD: Received opcode CMSG_STANDSTATECHANGE"); -- too many spam in log at lags/debug stop
    uint32 animstate;
    recv_data >> animstate;

    _player->SetStandState(animstate);
}

/**
 * @brief Stores a bug report or suggestion from the client.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleBugOpcode(WorldPacket& recv_data)
{
    uint32 suggestion, contentlen, typelen;
    std::string content, type;

    recv_data >> suggestion >> contentlen >> content;

    recv_data >> typelen >> type;

    if (suggestion == 0)
    {
        DEBUG_LOG("WORLD: Received opcode CMSG_BUG [Bug Report]");
    }
    else
    {
        DEBUG_LOG("WORLD: Received opcode CMSG_BUG [Suggestion]");
    }

    DEBUG_LOG("%s", type.c_str());
    DEBUG_LOG("%s", content.c_str());

    CharacterDatabase.escape_string(type);
    CharacterDatabase.escape_string(content);
    CharacterDatabase.PExecute("INSERT INTO `bugreport` (`type`,`content`) VALUES('%s', '%s')", type.c_str(), content.c_str());
}

/**
 * @brief Attempts to reclaim the player's corpse.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleReclaimCorpseOpcode(WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_RECLAIM_CORPSE");

    ObjectGuid corpseGuid;
    if (!MopDeathPackets::ParseReclaimCorpseRequest(recv_data, corpseGuid))
    {
        DEBUG_LOG("WORLD: Ignoring malformed CMSG_RECLAIM_CORPSE");
        return;
    }

    if (GetPlayer()->IsAlive())
    {
        return;
    }

    // do not allow corpse reclaim in arena
    if (GetPlayer()->InArena())
    {
        return;
    }

    // body not released yet
    if (!GetPlayer()->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        return;
    }

    Corpse* corpse = GetPlayer()->GetCorpse();

    if (!corpse)
    {
        return;
    }

    // prevent resurrect before 30-sec delay after body release not finished
    if (corpse->GetGhostTime() + GetPlayer()->GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP) > time(NULL))
    {
        return;
    }

    if (!corpse->IsWithinDistInMap(GetPlayer(), CORPSE_RECLAIM_RADIUS, true))
    {
        return;
    }

    // resurrect
    GetPlayer()->ResurrectPlayer(GetPlayer()->InBattleGround() ? 1.0f : 0.5f);

    // spawn bones
    GetPlayer()->SpawnCorpseBones();
}

/**
 * @brief Accepts or rejects a pending resurrection request.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleResurrectResponseOpcode(WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_RESURRECT_RESPONSE");

    MopDeathPackets::ResurrectResponse response;
    if (!MopDeathPackets::ParseResurrectResponse(recv_data, response))
    {
        DEBUG_LOG("WORLD: Ignoring malformed CMSG_RESURRECT_RESPONSE");
        return;
    }

    if (response.response != 0)
    {
        GetPlayer()->clearResurrectRequestData();
        return;
    }

    if (GetPlayer()->IsAlive())
    {
        return;
    }

    if (!GetPlayer()->isRessurectRequestedBy(response.resurrectorGuid))
    {
        return;
    }

    GetPlayer()->ResurectUsingRequestData();                // will call spawncorpsebones
}

void WorldSession::HandleReturnToGraveyard(WorldPacket& /*recvPacket*/)
{
    Player* pPlayer = GetPlayer();
    if (pPlayer->IsAlive() || !pPlayer->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        return;
    }

    Corpse* corpse = pPlayer->GetCorpse();
    WorldSafeLocsEntry const* ClosestGrave = NULL;

    // Special handle for battleground maps
    if (BattleGround* bg = pPlayer->GetBattleGround())
    {
        ClosestGrave = bg->GetClosestGraveYard(pPlayer);
    }
    else
    {
        if (!corpse)
        {
            return;
        }
        ClosestGrave = sObjectMgr.GetClosestGraveYard(corpse->GetPositionX(), corpse->GetPositionY(), corpse->GetPositionZ(), corpse->GetMapId(), pPlayer->GetTeam());
    }

    // if no grave found, stay at the current location
    // and don't show spirit healer location
    if (ClosestGrave)
    {
        bool updateVisibility = pPlayer->IsInWorld() && corpse && corpse->GetMapId() == ClosestGrave->Continent;
        pPlayer->TeleportTo(ClosestGrave->Continent, ClosestGrave->Pos_X, ClosestGrave->Pos_Y, ClosestGrave->Pos_Z, pPlayer->GetOrientation());
        if (pPlayer->IsDead())                                       // not send if alive, because it used in TeleportTo()
        {
            WorldPacket data;
            MopDeathPackets::BuildDeathReleaseLocation(data,
                ClosestGrave->Continent, ClosestGrave->Pos_X,
                ClosestGrave->Pos_Y, ClosestGrave->Pos_Z);  // show spirit healer position on minimap
            pPlayer->GetSession()->SendPacket(&data);
        }

        if (updateVisibility && pPlayer->IsInWorld())
        {
            pPlayer->UpdateVisibilityAndView();
        }
    }
}

/**
 * @brief Processes an area trigger activation.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleAreaTriggerOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_AREATRIGGER");

    MopAreaTriggerPackets::Request request;
    if (!MopAreaTriggerPackets::ParseRequest(recv_data, request))
    {
        DEBUG_LOG("WORLD: Ignoring malformed CMSG_AREATRIGGER");
        return;
    }

    // The 18414 client reports both edges. Existing area-trigger gameplay is
    // enter-only, so a valid leave report must not replay quests or teleports.
    if (!request.entered)
    {
        return;
    }

    uint32 const Trigger_ID = request.triggerId;
    DEBUG_LOG("Trigger ID: %u", Trigger_ID);
    Player* player = GetPlayer();

    if (player->IsTaxiFlying())
    {
        DEBUG_LOG("Player '%s' (GUID: %u) in flight, ignore Area Trigger ID: %u", player->GetName(), player->GetGUIDLow(), Trigger_ID);
        return;
    }

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
    if (!atEntry)
    {
        DEBUG_LOG("Player '%s' (GUID: %u) send unknown (by DBC) Area Trigger ID: %u", player->GetName(), player->GetGUIDLow(), Trigger_ID);
        return;
    }

    // delta is safe radius
    const float delta = 5.0f;

    // check if player in the range of areatrigger
    if (!IsPointInAreaTriggerZone(atEntry, player->GetMapId(), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), delta))
    {
        DEBUG_LOG("Player '%s' (GUID: %u) too far, ignore Area Trigger ID: %u", player->GetName(), player->GetGUIDLow(), Trigger_ID);
        return;
    }

    if (sScriptMgr.OnAreaTrigger(player, atEntry))
    {
        return;
    }

    uint32 quest_id = sObjectMgr.GetQuestForAreaTrigger(Trigger_ID);
    if (quest_id && player->IsAlive() && player->IsActiveQuest(quest_id))
    {
        Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);
        if (pQuest)
        {
            if (player->GetQuestStatus(quest_id) == QUEST_STATUS_INCOMPLETE)
            {
                player->AreaExploredOrEventHappens(quest_id);
            }
        }
    }

    // enter to tavern, not overwrite city rest
    if (sObjectMgr.IsTavernAreaTrigger(Trigger_ID))
    {
        // set resting flag we are in the inn
        if (player->GetRestType() != REST_TYPE_IN_CITY)
        {
            player->SetRestType(REST_TYPE_IN_TAVERN, Trigger_ID);
        }
        return;
    }

    if (BattleGround* bg = player->GetBattleGround())
    {
        if (bg->HandleAreaTrigger(player, Trigger_ID))
        {
            return;
        }
    }
    else if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
    {
        if (outdoorPvP->HandleAreaTrigger(player, Trigger_ID))
        {
            return;
        }
    }

    // NULL if all values default (non teleport trigger)
    AreaTrigger const* at = sObjectMgr.GetAreaTrigger(Trigger_ID);
    if (!at)
    {
        return;
    }

    MapEntry const* targetMapEntry = sMapStore.LookupEntry(at->target_mapId);
    if (!targetMapEntry)
    {
        return;
    }

    // ghost resurrected at enter attempt to dungeon with corpse (including fail enter cases)
    if (!player->IsAlive() && targetMapEntry->IsDungeon())
    {
        int32 corpseMapId = 0;
        if (Corpse* corpse = player->GetCorpse())
        {
            corpseMapId = corpse->GetMapId();
        }

        // check back way from corpse to entrance
        uint32 instance_map = corpseMapId;
        do
        {
            // most often fast case
            if (instance_map == targetMapEntry->ID)
            {
                break;
            }

            InstanceTemplate const* instance = ObjectMgr::GetInstanceTemplate(instance_map);
            instance_map = instance ? instance->parent : 0;
        }
        while (instance_map);

        // corpse not in dungeon or some linked deep dungeons
        if (!instance_map)
        {
            WorldPacket data;
            MopAreaTriggerPackets::BuildNoCorpse(data);
            player->GetSession()->SendPacket(&data);
            return;
        }

        // need find areatrigger to inner dungeon for landing point
        if (at->target_mapId != corpseMapId)
        {
            if (AreaTrigger const* corpseAt = sObjectMgr.GetMapEntranceTrigger(corpseMapId))
            {
                at = corpseAt;
                targetMapEntry = sMapStore.LookupEntry(at->target_mapId);
                if (!targetMapEntry)
                {
                    return;
                }
            }
        }

        // now we can resurrect player, and then check teleport requirements
        player->ResurrectPlayer(0.5f);
        player->SpawnCorpseBones();
    }

    // Leaving a dungeon finder run on foot returns the player to where they QUEUED, not to
    // the dungeon's doorstep -- but only while they are still in the group.
    //
    // The rule is conditioned on group membership rather than on how the player leaves:
    //   still in the LFD group and you exit  -> back to where you started
    //   left or were kicked, then you exit   -> dropped outside the entrance
    //
    // Source is developer testimony from the getMangos community (2026-08-06) rather than a
    // capture: the corpus contains no build-18414 episode of an on-foot exit from an LFG
    // instance, so this cannot be settled from the wire. It is consistent with what IS
    // proven, which is the same rule reached the other way -- in capture-000044 the
    // CMSG_LFG_TELEPORT exits land on the player's pre-queue position on a DIFFERENT
    // continent from the dungeon (map 974 Timeless Isle, while The Slave Pens' own entrance
    // is map 530), and two exits in the same run are byte-identical while different queue
    // episodes differ. Both paths returning to one recorded point is the coherent reading.
    //
    // It also matches what was observed live on 2026-08-06 20:12:07: two players walked out
    // of Shadowfang Keep to the trigger's fixed target after their group had already
    // disbanded, which is exactly the second branch.
    //
    // IsPlayerInLfgDungeon covers the whole condition -- it requires an LFG group, a live
    // group status, and the player actually standing on that run's map -- so a player who
    // has left the group, or whose run has ended, falls through to the ordinary trigger.
    if (targetMapEntry->ID != player->GetMapId() && sLFGMgr.IsPlayerInLfgDungeon(player))
    {
        player->TeleportToBGEntryPoint();
        return;
    }

    // teleport player (trigger requirement will be checked on TeleportTo)
    player->TeleportTo(at->target_mapId, at->target_X, at->target_Y, at->target_Z, at->target_Orientation, TELE_TO_NOT_LEAVE_TRANSPORT, at);
}

/**
 * @brief Consumes an account-data update packet.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleUpdateAccountData(WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_UPDATE_ACCOUNT_DATA");

    // MoP 5.4.8.18414 layout, proved by the direct client writer sub_669815:
    //   u32 decompressedSize, u32 timestamp, u32 compressedSize, then <compressedSize> zlib bytes,
    //   then the account-data type in the high 3 bits of the FINAL byte. The pre-MoP handler read the
    //   type FIRST as a full u32 (wrong order), and the opcode carried a stale value and was never
    //   registered -- so the client's upload only showed up as "not handled opcode UNKNOWN (0x0068)".
    if (recv_data.size() < 13)                              // 3x u32 header + at least the trailing type byte
    {
        return;
    }

    uint32 decompressedSize, timestamp, compressedSize;
    recv_data >> decompressedSize >> timestamp >> compressedSize;

    // The account-data type is bit-packed into the FINAL byte, and the zlib body is consumed by pointer
    // -- so the byte cursor never advances to the end on its own. Capture the blob pointer + remaining
    // length, read the type from the tail byte, then consume the whole packet up front so the dispatcher
    // does not flag spurious "unprocessed tail data". WriteBits/ReadBits are MSB-first in this ByteBuffer,
    // so a byte-aligned 3-bit field lands in bits 7-5 -- shift down by 5 (masking the LOW bits would read
    // 0 for every non-zero slot, e.g. type 4 == 0x80).
    uint8 const* compressed = recv_data.contents() + recv_data.rpos();
    uint32 available = uint32(recv_data.size() - recv_data.rpos());
    uint8 type = (recv_data.contents()[recv_data.size() - 1] >> 5) & 0x07;   // 3-bit slot in bits 7-5 (MSB-first)
    recv_data.rpos(recv_data.size());

    if (type >= NUM_ACCOUNT_DATA_TYPES)
    {
        return;
    }

    if (compressedSize != available - 1)
    {
        sLog.outError("UAD: compressed size %u does not leave exactly one trailing type byte (available %u)",
            compressedSize, available);
        return;
    }

    if (decompressedSize == 0 && compressedSize != 0)
    {
        sLog.outError("UAD: clear for account-data type %u carried a compressed body", type);
        return;
    }

    if (decompressedSize == 0)                              // client cleared this slot
    {
        SetAccountData(AccountDataType(type), 0, "");
        DETAIL_LOG("Account data updated by client: type %u cleared", type);
        return;
    }

    if (decompressedSize > 0xFFFF)
    {
        sLog.outError("UAD: bad account-data sizes (decompressed %u, compressed %u)", decompressedSize, compressedSize);
        return;
    }

    ByteBuffer dest;
    dest.resize(decompressedSize);

    uLongf realSize = decompressedSize;
    int const zResult = uncompress(const_cast<uint8*>(dest.contents()), &realSize,
                                  const_cast<uint8*>(compressed), compressedSize);
    if (zResult != Z_OK || realSize != decompressedSize)
    {
        sLog.outError("UAD: failed to decompress account data exactly (type %u, zlib %d, expected %u, actual %lu)",
            type, zResult, decompressedSize, static_cast<unsigned long>(realSize));
        return;
    }

    std::string adata(reinterpret_cast<char const*>(dest.contents()), decompressedSize);
    SetAccountData(AccountDataType(type), timestamp, adata);

    DETAIL_LOG("Account data updated by client: type %u, %u bytes (ts %u)", type, decompressedSize, timestamp);

    // The 18414 client completes account-data uploads locally immediately after sending 0x0068.
    // It installs no pending reply, so this protocol has no server completion ACK.
}

/**
 * @brief Acknowledges an account-data request packet.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleRequestAccountData(WorldPacket& recv_data)
{
    DETAIL_LOG("WORLD: Received opcode CMSG_REQUEST_ACCOUNT_DATA");

    // The request is a single byte carrying the 3-bit account-data type in bits 7-5 (WriteBits/ReadBits
    // are MSB-first). Confirmed by live capture 2026-07-17: the client sends 0x00 -> type 0, 0x40 -> type
    // 2, 0x80 -> type 4 (a low-bits mask would collapse every non-zero slot to 0).
    uint32 type = 0;
    if (recv_data.size() >= 1)
    {
        type = (recv_data.contents()[recv_data.size() - 1] >> 5) & 0x07;
    }
    recv_data.rpos(recv_data.size());                      // consume fully (no unprocessed-tail warning)

    DEBUG_LOG("RAD: type %u", type);

    if (type >= NUM_ACCOUNT_DATA_TYPES)
    {
        sLog.outError("RAD: out-of-range account-data type %u (max %u); ignoring", type, NUM_ACCOUNT_DATA_TYPES - 1);
        return;
    }

    AccountData* adata = GetAccountData(AccountDataType(type));

    uint32 size = adata->Data.size();

    uLongf destSize = compressBound(size);

    ByteBuffer dest;
    dest.resize(destSize);

    if (size && compress(const_cast<uint8*>(dest.contents()), &destSize, (uint8*)adata->Data.c_str(), size) != Z_OK)
    {
        DEBUG_LOG("RAD: Failed to compress account data");
        return;
    }

    dest.resize(destSize);

    // SMSG_UPDATE_ACCOUNT_DATA reply, MoP bit-packed. The player guid is empty for the pre-character
    // global-cache phase (_player null) and the logged-in character's guid for the in-world
    // per-character phase (SendAccountDataTimes(PER_CHARACTER_CACHE_MASK) is issued in the login path)
    // so the client associates the data with that character. Layout: a 3-bit type, an 8-bit guid mask,
    // FlushBits, the low guid bytes, u32 decompressed size, u32 compressed size, the zlib blob, the
    // high guid bytes, u32 unix time. For an empty guid the mask is 8 zero bits and no guid bytes are
    // written -- reproducing the 45-byte reply confirmed accepted by the live 18414 client 2026-07-17.
    // The direct 18414 reader sub_6F1A32 proves the non-empty per-character GUID mask and byte order.
    uint64 guid = _player ? _player->GetObjectGuid().GetRawValue() : uint64(0);
    static uint8 guidMaskOrder[8]  = { 5, 1, 3, 7, 0, 4, 2, 6 };
    static uint8 guidBytesPre[3]   = { 3, 1, 5 };
    static uint8 guidBytesPost[5]  = { 7, 4, 0, 6, 2 };

    WorldPacket data(SMSG_UPDATE_ACCOUNT_DATA, 2 + 8 + 4 + 4 + destSize + 4);
    data.WriteBits(type, 3);                               // account-data type (0-7)
    data.WriteGuidMask(guid, guidMaskOrder, 8);            // 8-bit guid mask (all zero if no player)
    data.FlushBits();
    data.WriteGuidBytes(guid, guidBytesPre, 3, 0);         // low guid bytes (none if no player)
    data << uint32(size);                                  // decompressed length
    data << uint32(destSize);                              // compressed length
    data.append(dest);                                     // compressed data
    data.WriteGuidBytes(guid, guidBytesPost, 5, 0);        // high guid bytes (none if no player)
    data << uint32(adata->Time);                           // unix time
    SendPacket(&data);
}

/**
 * @brief Updates one action button assignment.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetActionButtonOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_ACTION_BUTTON");
    // 18414 sends the button plainly, then a PACKED eight-byte value: the action
    // in the full low 32 bits and the type in byte 7. The inherited read took a
    // raw uint32 and split it at bit 24, which is neither the right width nor the
    // right boundary. See MopCompactPackets::ReadSetActionButton.
    uint8 button;
    uint32 action = 0;
    uint8 type = 0;
    MopCompactPackets::ReadSetActionButton(recv_data, button, action, type);

    DETAIL_LOG("BUTTON: %u ACTION: %u TYPE: %u", button, action, type);

    // The wire carries a 32-bit action and storage holds 24, but nothing is
    // needed here: IsActionButtonDataValid already rejects anything at or above
    // MAX_ACTION_BUTTON_ACTION_VALUE, and addActionButton runs it for the active
    // spec. A guard was briefly added at this level and was pure duplication.

    if (!action && !type)
    {
        DETAIL_LOG("MISC: Remove action from button %u", button);
        GetPlayer()->removeActionButton(GetPlayer()->GetActiveSpec(), button);
    }
    else
    {
        // The client's own type predicates all test type & 0xF0, so dispatch on
        // the high nibble. Switching on the exact byte would send a legitimate
        // low-nibble modifier to the error branch.
        switch (type & 0xF0)
        {
            case ACTION_BUTTON_MACRO:                       // and CMACRO, whose
                                                            // low nibble masks off
                DETAIL_LOG("MISC: Added Macro %u into button %u", action, button);
                break;
            case ACTION_BUTTON_EQSET:
                DETAIL_LOG("MISC: Added EquipmentSet %u into button %u", action, button);
                break;
            case ACTION_BUTTON_SPELL:
                DETAIL_LOG("MISC: Added Spell %u into button %u", action, button);
                break;
            case ACTION_BUTTON_ITEM:
                DETAIL_LOG("MISC: Added Item %u into button %u", action, button);
                break;
            case ACTION_BUTTON_EXPANDABLE:
                DETAIL_LOG("MISC: Added Expandable action %u into button %u", action, button);
                break;
            default:
                sLog.outError("MISC: Unknown action button type %u for action %u into button %u", type, action, button);
                return;
        }
        GetPlayer()->addActionButton(GetPlayer()->m_activeSpec, button, action, type);
    }
}

/**
 * @brief Acknowledges cinematic completion.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCompleteCinematic(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_COMPLETE_CINEMATIC");

    // Stop cinematic flyover if present; DK may hold an early
    // visibility lease before the flyover becomes active.
    Player* player = GetPlayer();
    if (!player)
    {
        return;
    }

    if (CinematicFlyover* flyover = player->GetCinematicFlyover())
    {
        flyover->Stop();
    }

    // The flyover has stopped, so DK intro-deferred state can now be applied (the
    // player is in-world at the intro spawn): area-exploration discovery/XP and
    // the hostile-area PvP flag. Both are no-ops for races.
    player->CheckAreaExploreAndOutdoor();
    player->ApplyDeferredIntroPvP();
}

/**
 * @brief Advances the client's cinematic camera.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleNextCinematicCamera(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_NEXT_CINEMATIC_CAMERA");

    // The client sends this when it enters the cinematic. Begin the flyover now
    // (summon body + bind camera) so farsight binds in sync with the client's
    // cinematic rather than during the login control window. Begin() is guarded.
    Player* player = GetPlayer();
    if (!player)
    {
        return;
    }

    if (CinematicFlyover* flyover = player->GetCinematicFlyover())
    {
        flyover->Begin();
    }
}

void WorldSession::HandleMoveTimeSkippedOpcode(WorldPacket& recv_data)
{
    /*  WorldSession::Update( WorldTimer::getMSTime() );*/
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_TIME_SKIPPED");

    MopControlPackets::MoveTimeSkippedRequest const request =
        MopControlPackets::ReadMoveTimeSkipped(recv_data);
    (void)request;

    /*
        ObjectGuid guid;
        uint32 time_skipped;
        recv_data >> guid;
        recv_data >> time_skipped;
        DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_TIME_SKIPPED");

        /// TODO
        must be need use in mangos
        We substract server Lags to move time ( AntiLags )
        for exmaple
        {
            GetPlayer()->ModifyLastMoveTime( -int32(time_skipped) );
        }
    */
}

/**
 * @brief Consumes a feather-fall movement acknowledgement.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleFeatherFallAck(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_FEATHER_FALL_ACK");

    // not used
    recv_data.rfinish();                                    // prevent warnings spam
    /*
        bitsream packet
    */
}

/**
 * @brief Consumes a movement unroot acknowledgement.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleMoveUnRootAck(WorldPacket& recv_data)
{
    MovementInfo movementInfo;
    recv_data >> movementInfo;

    if (recv_data.rpos() != recv_data.size() ||
        !VerifyMovementInfo(movementInfo, movementInfo.GetGuid()))
    {
        return;
    }

    DEBUG_LOG("CMSG_FORCE_MOVE_UNROOT_ACK: mover %s counter %u",
        movementInfo.GetGuid().GetString().c_str(), movementInfo.GetMovementCounter());
}

/**
 * @brief Consumes a movement root acknowledgement.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleMoveRootAck(WorldPacket& recv_data)
{
    MovementInfo movementInfo;
    recv_data >> movementInfo;

    if (recv_data.rpos() != recv_data.size() ||
        !VerifyMovementInfo(movementInfo, movementInfo.GetGuid()))
    {
        return;
    }

    DEBUG_LOG("CMSG_FORCE_MOVE_ROOT_ACK: mover %s counter %u",
        movementInfo.GetGuid().GetString().c_str(), movementInfo.GetMovementCounter());

    if (!m_waitingForTransferRootAck)
    {
        return;
    }

    if (!_player->IsBeingTeleportedFar() ||
        movementInfo.GetMovementCounter() != m_pendingTransferRootCounter)
    {
        DEBUG_LOG("WORLD: ignoring mismatched transfer CMSG_FORCE_MOVE_ROOT_ACK counter %u (expected %u).",
                  movementInfo.GetMovementCounter(), m_pendingTransferRootCounter);
        return;
    }

    m_waitingForTransferRootAck = false;
    m_pendingTransferRootCounter = 0;

    // The 18414 suspend response is hard-routed by the client to connection
    // channel 1. Four does not yet establish that secondary instance
    // connection, so gating this single-socket transport seam on the response
    // leaves the client permanently waiting. The root acknowledgement still
    // provides the required teardown boundary before NEW_WORLD.
    WorldLocation const& loc = _player->GetTeleportDest();
    WorldPacket data(SMSG_NEW_WORLD, 20);
    MopWorldEntryPackets::BuildNewWorld(data, loc.mapid, loc.coord_x,
                                        loc.coord_y, loc.coord_z, loc.orientation);
    SendPacket(&data);
    _player->SendSavedInstances();
}

/**
 * @brief Updates the player's action-bar toggle byte.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetActionBarTogglesOpcode(WorldPacket& recv_data)
{
    uint8 ActionBar;

    recv_data >> ActionBar;

    if (!GetPlayer())                                       // ignore until not logged (check needed because STATUS_AUTHED)
    {
        if (ActionBar != 0)
        {
            sLog.outError("WorldSession::HandleSetActionBarToggles in not logged state with value: %u, ignored", uint32(ActionBar));
        }
        return;
    }

    GetPlayer()->SetByteValue(PLAYER_FIELD_BYTES, 2, ActionBar);
}

/**
 * @brief Consumes the client's violence rendering preference.
 *
 * This setting changes only client-side presentation, so the server has no
 * state to update after consuming the binary-verified one-byte body.
 */
void WorldSession::HandleViolenceLevelOpcode(WorldPacket& recv_data)
{
    recv_data.read_skip<uint8>();
}

/**
 * @brief Sends the player's total and current-level played time.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandlePlayedTime(WorldPacket& recv_data)
{
    bool const displayEvent = MopQueryPackets::ReadPlayedTimeRequest(recv_data);

    WorldPacket data(SMSG_PLAYED_TIME, 4 + 4 + 1);
    MopQueryPackets::BuildPlayedTimeResponse(data,
        uint32(_player->GetTotalPlayedTime()),
        uint32(_player->GetLevelPlayedTime()), displayEvent);
    SendPacket(&data);
}

/**
 * @brief Begins inspecting another nearby friendly player.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleInspectOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    if (!MopInspectPackets::ParseRequest(recv_data, guid))
        return;

    DEBUG_LOG("Inspected guid is %s", guid.GetString().c_str());

    Player* plr = sObjectMgr.GetPlayer(guid);
    if (!plr)                                               // wrong player
    {
        return;
    }

    if (!_player->IsWithinDistInMap(plr, INSPECT_DISTANCE, false))
    {
        return;
    }

    if (_player->IsHostileTo(plr))
    {
        return;
    }

    MopInspectPackets::Response response;
    response.targetGuid = plr->GetObjectGuid();

    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = plr->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        MopInspectPackets::Item inspectItem;
        inspectItem.creatorGuid = item->GetGuidValue(ITEM_FIELD_CREATOR);
        inspectItem.randomPropertyId = int16(item->GetItemRandomPropertyId());
        inspectItem.suffixFactor = item->GetItemSuffixFactor();
        // This core has no 18414 dynamic-item-modifier backend. The client
        // reader accepts the truthful zero-length blob emitted here.
        for (uint8 enchantSlot = 0; enchantSlot < MAX_ENCHANTMENT_SLOT;
            ++enchantSlot)
        {
            uint32 const enchantmentId =
                item->GetEnchantmentId(EnchantmentSlot(enchantSlot));
            if (enchantmentId)
                inspectItem.enchantments.push_back(
                    { enchantmentId, enchantSlot });
        }
        inspectItem.entry = item->GetEntry();
        inspectItem.slot = slot;
        response.items.push_back(inspectItem);
    }

    if (sWorld.getConfig(CONFIG_BOOL_TALENTS_INSPECTING) || _player->isGameMaster())
    {
        for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
            response.glyphs.push_back(
                uint16(plr->m_glyphMgr.GetGlyph(plr->GetActiveSpec(), slot)));

        // Use the native 5.4.8 update field. The legacy TalentTab identifier
        // is a different namespace and must not be substituted here.
        response.specializationId =
            plr->GetUInt32Value(PLAYER_FIELD_CURRENT_SPEC_ID);

        PlayerTalentMap const& talents = plr->m_talents[plr->GetActiveSpec()];
        for (PlayerTalentMap::const_iterator itr = talents.begin();
            itr != talents.end(); ++itr)
        {
            if (itr->second.state != PLAYERSPELL_REMOVED &&
                itr->second.talentEntry)
            {
                response.talents.push_back(
                    uint16(itr->second.talentEntry->TalentID));
            }
        }
        std::sort(response.talents.begin(), response.talents.end());
    }

    if (Guild* guild = sGuildMgr.GetGuildById(plr->GetGuildId()))
    {
        response.hasGuild = true;
        response.guild.guid = guild->GetObjectGuid();
        response.guild.memberCount = guild->GetMemberSize();
        // Guild experience is not modelled by this core; zero is the truthful
        // value for the otherwise fully represented 18414 guild block.
        response.guild.experience = 0;
        response.guild.level = guild->GetLevel();
    }

    WorldPacket data;
    if (!MopInspectPackets::BuildResponse(data, response))
    {
        sLog.outError("Inspect: could not serialize result for %s",
            guid.GetString().c_str());
        return;
    }
    SendPacket(&data);
}

/**
 * @brief Sends honor statistics for an inspected player.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleInspectHonorStatsOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data.ReadGuidMask<1, 5, 7, 3, 2, 4, 0, 6>(guid);
    recv_data.ReadGuidBytes<4, 7, 0, 5, 1, 6, 2, 3>(guid);

    Player* player = sObjectMgr.GetPlayer(guid);
    if (!player)
    {
        sLog.outError("InspectHonorStats: WTF, player not found...");
        return;
    }

    if (!_player->IsWithinDistInMap(player, INSPECT_DISTANCE, false))
    {
        return;
    }

    if (_player->IsHostileTo(player))
    {
        return;
    }

    WorldPacket data(SMSG_INSPECT_HONOR_STATS, 18);
    data.WriteGuidMask<4, 3, 6, 2, 5, 0, 7, 1>(player->GetObjectGuid());
    data << uint8(0);                                                   // rank
    data << uint16(player->GetUInt16Value(PLAYER_FIELD_KILLS, 1));      // yesterday kills
    data << uint16(player->GetUInt16Value(PLAYER_FIELD_KILLS, 0));      // today kills
    data.WriteGuidBytes<2, 0, 6, 3, 4, 1, 5>(player->GetObjectGuid());
    data << uint32(player->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS));
    data.WriteGuidBytes<7>(player->GetObjectGuid());

    SendPacket(&data);
}

/**
 * @brief Teleports an administrator to explicit world coordinates.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleWorldTeleportOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_WORLD_TELEPORT from %s", GetPlayer()->GetGuidStr().c_str());

    // write in client console: worldport 469 452 6454 2536 180 or /console worldport 469 452 6454 2536 180
    // Received opcode CMSG_WORLD_TELEPORT
    // Time is ***, map=469, x=452.000000, y=6454.000000, z=2536.000000, orient=3.141593

    uint32 time;
    uint32 mapid;
    float PositionX;
    float PositionY;
    float PositionZ;
    float Orientation;

    recv_data >> time;                                      // time in m.sec.
    recv_data >> mapid;
    recv_data >> PositionX;
    recv_data >> PositionY;
    recv_data >> PositionZ;
    recv_data >> Orientation;                               // o (3.141593 = 180 degrees)

    // DEBUG_LOG("Received opcode CMSG_WORLD_TELEPORT");

    if (GetPlayer()->IsTaxiFlying())
    {
        DEBUG_LOG("Player '%s' (GUID: %u) in flight, ignore worldport command.", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow());
        return;
    }

    DEBUG_LOG("Time %u sec, map=%u, x=%f, y=%f, z=%f, orient=%f", time / 1000, mapid, PositionX, PositionY, PositionZ, Orientation);

    if (GetSecurity() >= SEC_ADMINISTRATOR)
    {
        GetPlayer()->TeleportTo(mapid, PositionX, PositionY, PositionZ, Orientation);
    }
    else
    {
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
    }
}

/**
 * @brief Sends account identity information for a named player.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleWhoisOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_WHOIS");
    std::string charname;
    recv_data >> charname;

    if (GetSecurity() < SEC_ADMINISTRATOR)
    {
        SendNotification(LANG_YOU_NOT_HAVE_PERMISSION);
        return;
    }

    if (charname.empty() || !normalizePlayerName(charname))
    {
        SendNotification(LANG_NEED_CHARACTER_NAME);
        return;
    }

    Player* plr = sObjectMgr.GetPlayer(charname.c_str());

    if (!plr)
    {
        SendNotification(LANG_PLAYER_NOT_EXIST_OR_OFFLINE, charname.c_str());
        return;
    }

    uint32 accid = plr->GetSession()->GetAccountId();

    QueryResult* result = LoginDatabase.PQuery("SELECT `username`,`email`,`last_ip` FROM `account` WHERE `id`=%u", accid);
    if (!result)
    {
        SendNotification(LANG_ACCOUNT_FOR_PLAYER_NOT_FOUND, charname.c_str());
        return;
    }

    Field* fields = result->Fetch();
    std::string acc = fields[0].GetCppString();
    if (acc.empty())
    {
        acc = "Unknown";
    }
    std::string email = fields[1].GetCppString();
    if (email.empty())
    {
        email = "Unknown";
    }
    std::string lastip = fields[2].GetCppString();
    if (lastip.empty())
    {
        lastip = "Unknown";
    }

    std::string msg = charname + "'s " + "account is " + acc + ", e-mail: " + email + ", last ip: " + lastip;

    WorldPacket data(SMSG_WHOIS, msg.size() + 1);
    data << msg;
    _player->GetSession()->SendPacket(&data);

    delete result;

    DEBUG_LOG("Received whois command from player %s for character %s", GetPlayer()->GetName(), charname.c_str());
}

void WorldSession::HandleComplainOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_COMPLAIN");
    recv_data.hexlike();

    // 18414 body, derived from the client writer at Wow.exe sub_66BD81 (the
    // packet class is built by sub_6668BE, vtable off_D6386C, slot 1). The
    // inherited reader below was the pre-MoP one: a direct `>> spam_type >>
    // spammerGuid`, which cannot parse a bit-packed body at all.
    //
    // Shape: a byte-aligned spam type, then one bit stream, then the packed
    // GUID bytes, then the optional trailing scalars. The client has exactly
    // two senders - sub_9A7D76 reports an inbox item (type 0) and sub_CD85CE
    // reports chat spam (type 1). Type 2 exists in the writer but no caller in
    // this build reaches it, so it is refused rather than guessed at: a wrong
    // parse desynchronises the whole stream.
    //
    // NOT byte-verified. Neither this opcode nor SMSG_COMPLAIN_RESULT appears
    // anywhere in the 18414 sniff corpus, so gate 3 is unmet and CMSG_COMPLAIN
    // is deliberately left unregistered in Opcodes.cpp. This reader exists so a
    // single live capture can confirm it and flip the switch.
    uint8 spam_type;                                        // 0 - mail, 1 - chat
    ObjectGuid spammerGuid;
    uint32 unk1 = 0;                                        // writer field this+0x1C
    uint32 unk2 = 0;                                        // mail index, or chat this+0x38
    uint32 unk3 = 0;                                        // writer field this+0x18
    uint32 unk4 = 0;                                        // chat this+0x3C
    std::string description = "";

    recv_data >> spam_type;
    if (spam_type > 1)
    {
        sLog.outError("CMSG_COMPLAIN: unsupported spam type %u from account %u; refusing to parse",
            spam_type, GetAccountId());
        return;
    }

    // Each "absent" bit is set when the matching field is zero, so the field is
    // omitted from the tail.
    bool const field1CAbsent = recv_data.ReadBit();
    bool const field18Absent = recv_data.ReadBit();
    recv_data.ReadBit();                                    // whole GUID is zero; redundant with the mask

    bool field38Absent = true;
    bool field3CAbsent = true;
    uint32 messageLength = 0;
    if (spam_type == 1)
    {
        field38Absent = recv_data.ReadBit();
        messageLength = recv_data.ReadBits(8);
        field3CAbsent = recv_data.ReadBit();
    }

    recv_data.ReadGuidMask<4, 5, 6, 7, 3, 1, 2, 0>(spammerGuid);

    bool mailIndexAbsent = true;
    if (spam_type == 0)
    {
        mailIndexAbsent = recv_data.ReadBit();
    }

    recv_data.ReadGuidBytes<0, 1, 4, 3, 6, 5, 2, 7>(spammerGuid);

    if (spam_type == 1)
    {
        // The writer emits 0x3C before 0x38, which is the reverse of the order
        // their presence bits were written in.
        if (!field3CAbsent)
        {
            recv_data >> unk4;
        }
        if (!field38Absent)
        {
            recv_data >> unk2;
        }
        description = recv_data.ReadString(messageLength);
    }
    else if (!mailIndexAbsent)
    {
        recv_data >> unk2;                                  // mail index, writer field this+0xBF8
    }

    if (!field1CAbsent)
    {
        recv_data >> unk1;
    }
    if (!field18Absent)
    {
        recv_data >> unk3;
    }

    // NOTE: all chat messages from this spammer automatically ignored by spam reporter until logout in case chat spam.
    // if it's mail spam - ALL mails from this spammer automatically removed by client

    // Complaint Received message.
    //
    // STALE: this is the pre-MoP single-uint8 body, not an 18414 one, and it has
    // never been verified. It is inert today behind a double gate -- CMSG_COMPLAIN
    // is unregistered so this handler is unreachable, and SMSG_COMPLAIN_RESULT is
    // not admitted by IsEnterWorldConverted so the packet would be dropped anyway.
    // The client also has no reader for 0x128F at all: the binary carries seven
    // immediates for 0x319 and none for 0x128F. Re-derive or delete this reply
    // before either gate is opened. CalendarHandler.cpp sends the same opcode with
    // a different, equally stale two-uint8 body; both need settling together.
    WorldPacket data(SMSG_COMPLAIN_RESULT, 1);
    data << uint8(0);
    SendPacket(&data);

    DEBUG_LOG("REPORT SPAM: type %u, spammer %s, unk1 %u, unk2 %u, unk3 %u, "
              "unk4 %u, message %s",
              spam_type, spammerGuid.GetString().c_str(),
              unk1, unk2, unk3, unk4, description.c_str());
}

void WorldSession::HandleRealmSplitOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REALM_SPLIT");

    uint32 unk;
    std::string split_date = "01/01/01";
    recv_data >> unk;

    WorldPacket data(SMSG_REALM_SPLIT, 4 + 4 + split_date.size() + 1);
    data << unk;
    data << uint32(0x00000000);                             // realm split state
    // split states:
    // 0x0 realm normal
    // 0x1 realm split
    // 0x2 realm split pending
    data << split_date;
    SendPacket(&data);
    // DEBUG_LOG("response sent %u", unk);
}

/**
 * @brief Enables or disables farsight camera mode.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleFarSightOpcode(WorldPacket& recv_data)
{
    bool enable = false;
    if (!MopFarSightPackets::ReadRequest(recv_data, enable))
        return;

    DEBUG_LOG("WORLD: Received opcode CMSG_FAR_SIGHT");

    if (!enable)
    {
        DEBUG_LOG("Removed FarSight from %s", _player->GetGuidStr().c_str());
        _player->GetCamera().ResetView(false);
        return;
    }

    // Resolve the far-sight object only when enabling. The inherited handler
    // looked it up before the switch, so a disable silently did nothing whenever
    // the object had already gone out of scope -- which is exactly when a client
    // most needs its view reset.
    WorldObject* obj = _player->GetMap()->GetWorldObject(_player->GetFarSightGuid());
    if (!obj)
    {
        return;
    }

    DEBUG_LOG("Added FarSight %s to %s", _player->GetFarSightGuid().GetString().c_str(), _player->GetGuidStr().c_str());
    _player->GetCamera().SetView(obj, false);
}

void WorldSession::HandleSetTitleOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_TITLE");

    int32 title;
    recv_data >> title;

    // -1 at none
    if (title > 0 && title < MAX_TITLE_INDEX)
    {
        if (!GetPlayer()->HasTitle(title))
        {
            return;
        }
    }
    else
    {
        title = 0;
    }

    GetPlayer()->SetUInt32Value(PLAYER_CHOSEN_TITLE, title);
}

void WorldSession::HandleTimeSyncResp(WorldPacket& recv_data)
{
    uint32 counter, clientTicks;
    recv_data >> counter >> clientTicks;

    DEBUG_LOG("WORLD: Received opcode CMSG_TIME_SYNC_RESP: counter %u, client ticks %u, time since last sync %u", counter, clientTicks, clientTicks - _player->m_timeSyncClient);

    if (counter != _player->m_timeSyncCounter - 1)
    {
        DEBUG_LOG(" WORLD: Opcode CMSG_TIME_SYNC_RESP -- Wrong time sync counter from %s (cheater?)", _player->GetGuidStr().c_str());
    }

    uint32 ourTicks = clientTicks + (GameTime::GetGameTimeMS() - _player->m_timeSyncServer);

    // diff should be small
    DEBUG_LOG(" WORLD: Opcode CMSG_TIME_SYNC_RESP -- Our ticks: %u, diff %u, latency %u", ourTicks, ourTicks - clientTicks, GetLatency());

    _player->m_timeSyncClient = clientTicks;
}

void WorldSession::HandleTimeSyncResponseFailed(WorldPacket& recv_data)
{
    uint32 const counter =
        MopWorldEntryPackets::ReadTimeSyncResponseFailed(recv_data);
    DEBUG_LOG("WORLD: Received opcode CMSG_TIME_SYNC_RESPONSE_FAILED: counter %u", counter);
}

void WorldSession::HandleTimeSyncResponseDropped(WorldPacket& recv_data)
{
    MopWorldEntryPackets::TimeSyncResponseDroppedReport const report =
        MopWorldEntryPackets::ReadTimeSyncResponseDropped(recv_data);
    DEBUG_LOG("WORLD: Received opcode CMSG_TIME_SYNC_RESPONSE_DROPPED: first value %u, second value %u",
        report.first, report.second);
}

void WorldSession::HandleDiscardedTimeSyncAcks(WorldPacket& recv_data)
{
    MopWorldEntryPackets::DiscardedTimeSyncAcksReport const report =
        MopWorldEntryPackets::ReadDiscardedTimeSyncAcks(recv_data);
    if (report.hasValue)
        DEBUG_LOG("WORLD: Received opcode CMSG_DISCARDED_TIME_SYNC_ACKS: value %u", report.value);
    else
        DEBUG_LOG("WORLD: Received opcode CMSG_DISCARDED_TIME_SYNC_ACKS: no value");
}

/**
 * @brief Cancels the player's mount aura when allowed.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCancelMountAuraOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CANCEL_MOUNT_AURA");

    // If player is not mounted, so go out :)
    if (!_player->IsMounted())                              // not blizz like; no any messages on blizz
    {
        ChatHandler(this).SendSysMessage(LANG_CHAR_NON_MOUNTED);
        return;
    }

    if (_player->IsTaxiFlying())                            // not blizz like; no any messages on blizz
    {
        ChatHandler(this).SendSysMessage(LANG_YOU_IN_FLIGHT);
        return;
    }

    _player->Unmount(_player->HasAuraType(SPELL_AURA_MOUNTED));
    _player->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
}

void WorldSession::HandleMoveSetCanFlyAckOpcode(WorldPacket& recv_data)
{
    // fly mode on/off
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_SET_CAN_FLY_ACK");

    MovementInfo movementInfo;
    recv_data >> movementInfo;

    if (_player->GetMover()->GetObjectGuid() != movementInfo.GetGuid())
    {
        DEBUG_LOG("WorldSession::HandleMoveSetCanFlyAckOpcode: player %s, mover %s, received %s, ignored",
                  _player->GetGuidStr().c_str(), _player->GetMover()->GetGuidStr().c_str(), movementInfo.GetGuid().GetString().c_str());
        return;
    }

    // Only the player's OWN acknowledgement is accepted. Player::SetCanFly is
    // the only sender of the counter-bearing SMSG_MOVE_SET_CAN_FLY, and it sends
    // it to the controlling session about that session's player; a creature is
    // told with the counter-less spline form and has no acknowledgement to make.
    // So a mover that is not the player is either a controlled unit that was
    // never sent this opcode, or a forgery.
    //
    // This matters because the line below imports the client's entire movement
    // flag word onto the authoritative mover with no filtering. Landing that on
    // a vehicle or charmed creature feeds client-authored flags into
    // creature-only pathfinding and the vehicle board/unboard state machine.
    // Restricting it to self does not remove the trust, which ordinary movement
    // packets already extend, but it does stop that trust being redirected onto
    // a unit the client does not own.
    if (_player->GetMover() != _player)
    {
        DEBUG_LOG("WorldSession::HandleMoveSetCanFlyAckOpcode: %s acknowledged for a "
                  "non-self mover %s, ignored",
                  _player->GetGuidStr().c_str(), _player->GetMover()->GetGuidStr().c_str());
        return;
    }

    _player->m_movementInfo.SetMovementFlags(movementInfo.GetMovementFlags());
}

/**
 * @brief Placeholder handler for client pet-info requests.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleRequestPetInfoOpcode(WorldPacket & /*recv_data */)
{
    /*
        DEBUG_LOG("WORLD: Received opcode CMSG_REQUEST_PET_INFO");
        recv_data.hexlike();
    */
}

/**
 * @brief Records client taxi benchmark mode changes.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetTaxiBenchmarkOpcode(WorldPacket& recv_data)
{
    uint8 mode;
    recv_data >> mode;

    DEBUG_LOG("Client used \"/timetest %d\" command", mode);
}

void WorldSession::HandleQueryInspectAchievementsOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid.ReadAsPacked();

    Player* player = sObjectMgr.GetPlayer(guid);
    if (!player)
    {
        return;
    }

    if (!_player->IsWithinDistInMap(player, INSPECT_DISTANCE, false))
    {
        return;
    }

    if (_player->IsHostileTo(player))
    {
        return;
    }

    player->GetAchievementMgr().SendRespondInspectAchievements(_player);
}

void WorldSession::HandleUITimeRequestOpcode(WorldPacket& /*recv_data*/)
{
    // empty opcode
    DEBUG_LOG("WORLD: Received opcode SMSG_UI_TIME");

    WorldPacket data(SMSG_UI_TIME, 4);
    data << uint32(time(NULL));
    SendPacket(&data);
}

void WorldSession::HandleReadyForAccountDataTimesOpcode(WorldPacket& /*recv_data*/)
{
    // empty opcode
    DEBUG_LOG("WORLD: Received opcode CMSG_READY_FOR_ACCOUNT_DATA_TIMES");

    SendAccountDataTimes(GLOBAL_CACHE_MASK);

    // Retail sends SMSG_SET_TIME_ZONE_INFORMATION twice per session -- once here in the
    // account-data phase and once at world entry -- and the two occurrences are ordered
    // differently. Here it is immediately after SMSG_ACCOUNT_DATA_TIMES and before the character
    // list: capture-000019 seq 22 ACCOUNT_DATA_TIMES -> 23 SET_TIME_ZONE_INFORMATION -> 24
    // ENUM_CHARACTERS_RESULT. At world entry it instead follows the MOTD (see HandlePlayerLogin).
    //
    // An earlier revision of this comment claimed every corpus instance sits immediately after
    // ACCOUNT_DATA_TIMES. That is true of this occurrence only; the world-entry one does not, and
    // the total covers both. Character select previously had no send at all -- the login path's
    // send was the only one -- which is what this fixes.
    //
    // 817 observations at build 18414 (catalogue 47A3C991). An earlier revision said 843, which
    // was the previous catalogue folding the adjacent build 18291 in; 817 + 23 = 840 of that.
    //
    // Retail's payload is the zone name twice ("Europe/ParisEurope/Paris" in the captures), which
    // is the shape this builder already emits; ours is shorter only because Etc/UTC is shorter.
    // Every one of the 817 is exactly 26 bytes -- min == max -- which is 2 + 12 + 12 for that one
    // name. That uniformity is a property of the capture set, not of the packet: these are all
    // one region's servers. The field is length-prefixed, so our 16 bytes (2 + 7 + 7) is a valid
    // encoding of a shorter name rather than a truncation, and a live client accepts it.
    WorldPacket tz(SMSG_SET_TIME_ZONE_INFORMATION, 2 + 2 * 7);
    MopWorldEntryPackets::BuildSetTimeZoneInformation(tz, "Etc/UTC");
    SendPacket(&tz);
}

/**
 * @brief Answers the character-select store query with an empty purchase list.
 *
 * The shipped UI's C_PurchaseAPI.GetPurchaseList writes an empty 0x18B2 and waits. Retail
 * normally answers: across build 18414 the corpus holds 425 requests and 409 responses. The
 * sixteen unmatched requests are not accounted for -- they may be capture or session boundaries
 * -- so "normally" is as strong as the counts support, not "always".
 *
 * The body length is not a sample: all 425 requests are exactly zero bytes and all 409 responses
 * are exactly seven (reported min == max == 7 over the whole population), so the seven-byte reply
 * below matches every observation there is.
 *
 * Counts are from catalogue 47A3C991, and earlier revisions of this comment said 434/420. That
 * was not a miscount -- it was the previous catalogue merging the adjacent build 18291 into
 * 18414. Splitting them gives 409 + 11 = 420, exactly the old figure. Build separation matters
 * more than it looks here: opcode 0x023A is this 7-byte SMSG in 18414/18291, but in builds
 * 17359/17371/17399 the same number is a CMSG carrying 37-86 bytes -- a different message
 * entirely. Re-derive against a stated catalogue generation, never across builds.
 *
 * We have no Store backend, so the reply we send is the one a player who has purchased nothing
 * receives - an empty list - rather than dropping the request, which leaves the client waiting.
 * Note that this emulates a successful zero-purchase account; it is not an observed "server with
 * no Store" state, which the corpus does not contain.
 *
 * The client does decode the body: it reads a 19-bit list count followed by a uint32 result
 * (Wow.exe.c sub at 1046525/1046565), so seven zero bytes mean count=0, result=0 and the handler
 * marks the purchase list ready and empty. The constant is therefore both the observed bytes and
 * a correct encoding, which is why it is safe to send without a Store.
 *
 * ORDERING CAVEAT: retail answers this after the character list, not before --
 * capture-000019 seq 24 ENUM_CHARACTERS_RESULT then 25 BATTLE_PAY_GET_PURCHASE_LIST_RESPONSE (7
 * bytes), even though the client sent the two requests in the opposite order at seq 20/21. We
 * reply inline while SMSG_CHAR_ENUM waits on an async DB query, so ours lands first. With
 * count=0 there is nothing the early reply can lose, so this is left as a transcript-level
 * divergence; revisit if a real Store is ever implemented.
 */
void WorldSession::HandleBattlePayGetPurchaseListOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BATTLE_PAY_GET_PURCHASE_LIST");

    WorldPacket data(SMSG_BATTLE_PAY_GET_PURCHASE_LIST_RESPONSE, 7);
    for (uint8 i = 0; i < 7; ++i)
    {
        data << uint8(0);
    }
    SendPacket(&data);
}

void WorldSession::HandleHearthandResurrect(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_HEARTH_AND_RESURRECT");

    AreaTableEntry const* atEntry = sAreaStore.LookupEntry(_player->GetAreaId());
    if (!atEntry || !(atEntry->Flags & AREA_FLAG_CAN_HEARTH_AND_RES))
    {
        return;
    }

    // Can't use in flight
    if (_player->IsTaxiFlying())
    {
        return;
    }

    // Send Everytime
    _player->BuildPlayerRepop();
    _player->ResurrectPlayer(100);
    _player->TeleportToHomebind();
}

void WorldSession::HandleRequestHotfix(WorldPacket& recv_data)
{
    MopHotfixPackets::HotfixRequest request;
    if (!MopHotfixPackets::ReadHotfixRequest(recv_data, request))
    {
        sLog.outError("CMSG_REQUEST_HOTFIX: malformed request from account %u", GetAccountId());
        recv_data.rfinish();
        return;
    }

    for (MopHotfixPackets::HotfixRecord const& record : request.records)
    {
        switch (request.type)
        {
            case DB2_REPLY_ITEM:
                SendItemDb2Reply(record.entry);
                break;
            case DB2_REPLY_SPARSE:
                SendItemSparseDb2Reply(record.entry);
                break;
            case DB2_REPLY_BROADCAST_TEXT:
                SendBroadcastTextDb2Reply(record.entry);
                break;
            case DB2_REPLY_BATTLE_PET_EFFECT_PROPERTIES:
                SendBattlePetEffectPropertiesDb2Reply(record.entry);
                break;
            default:
                sLog.outError("CMSG_REQUEST_HOTFIX: Received unknown hotfix type: %u", request.type);
                recv_data.rfinish();
                return;
        }
    }
}

void WorldSession::HandleObjectUpdateFailedOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guid;

    guid[2] = recvPacket.ReadBit();
    guid[3] = recvPacket.ReadBit();
    guid[5] = recvPacket.ReadBit();
    guid[0] = recvPacket.ReadBit();
    guid[4] = recvPacket.ReadBit();
    guid[7] = recvPacket.ReadBit();
    guid[6] = recvPacket.ReadBit();
    guid[1] = recvPacket.ReadBit();

    recvPacket.ReadByteSeq(guid[1]);
    recvPacket.ReadByteSeq(guid[2]);
    recvPacket.ReadByteSeq(guid[5]);
    recvPacket.ReadByteSeq(guid[0]);
    recvPacket.ReadByteSeq(guid[3]);
    recvPacket.ReadByteSeq(guid[4]);
    recvPacket.ReadByteSeq(guid[6]);
    recvPacket.ReadByteSeq(guid[7]);


    DEBUG_LOG("WORLD: Received CMSG_OBJECT_UPDATE_FAILED from %s (%u) guid: %s", GetPlayerName(), GetAccountId(), guid.GetString().c_str());
    if (_player->IsInWorld())
    {
        if (WorldObject* obj = _player->GetMap()->GetWorldObject(guid))
        {
            obj->SendCreateUpdateToPlayer(_player);
        }
    }
    else
    {
        sLog.outError("WorldSession::HandleObjectUpdateFailedOpcode: received from player not in map");
    }
}
