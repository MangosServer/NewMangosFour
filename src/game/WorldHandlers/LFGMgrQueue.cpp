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

#include "DBCEnums.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "GameEventMgr.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Object.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "SharedDefines.h"
#include "WorldSession.h"

/**
 * @file LFGMgrQueue.cpp
 * @brief Cohesion split of LFGMgr.cpp -- queue join/leave and player/party access: JoinLFG/LeaveLFG, player-or-party data, join-result computation, player status getters/setters. Same LFGMgr class; no behaviour change. CMake file(GLOB) picks this file up automatically; LFGMgr.h is unchanged.
 */

void LFGMgr::JoinLFG(uint32 roles, std::set<uint32> dungeons, std::string comments, Player* plr)
{
    // Todo:
    //       - see if any of this code/information can be put into a generalized class for other use
    //       - look into splitting this into 2 fns- one for player case, one for group
    Group* pGroup = plr->GetGroup();
    ObjectGuid guid = (pGroup) ? pGroup->GetObjectGuid() : plr->GetObjectGuid();
    // Assigned only on the random-dungeon branch, but read unconditionally
    // further down, so it must not start indeterminate.
    uint32 randomDungeonID = 0; // used later if random dungeon has been chosen

    // Refuse a fresh queue while a proposal for this player is still open.
    //
    // The duplicate cleanup below is guarded on currentInfo, and TryFormGroup erases
    // m_playerData the moment a proposal is sent -- so a player sitting on an open
    // proposal window has no queue data, skipped that cleanup entirely, and got a
    // SECOND live entry. If the first proposal then completed, CreateDungeonGroup put
    // them in a dungeon group while they were still queued for another.
    LFGPlayerStatus const existingStatus = GetPlayerStatus(plr->GetObjectGuid());
    if (existingStatus.state == LFG_STATE_PROPOSAL)
    {
        partyForbidden noneForbidden;
        plr->GetSession()->SendLfgJoinResult(ERR_LFG_NO_LFG_OBJECT, existingStatus.state, noneForbidden);
        return;
    }

    LFGPlayers* currentInfo = GetPlayerOrPartyData(guid);

    // check if we actually have info on the player/group right now
    if (currentInfo)
    {
        bool groupCurrentlyInDungeon = pGroup && pGroup->isLFGGroup() && currentInfo->currentState != LFG_STATE_FINISHED_DUNGEON;

        // are they already queued?
        if (currentInfo->currentState == LFG_STATE_QUEUED)
        {
            // remove from that queue so they can later join this one
            queueSet::iterator qItr = m_queueSet.find(guid);
            if (qItr != m_queueSet.end())
            {
                m_queueSet.erase(qItr);
            }
            // note: do we need to send a packet telling them the current queue is over?
        }

        // are they already in a dungeon?
        if (groupCurrentlyInDungeon)
        {
            std::set<uint32> currentDungeon = currentInfo->dungeonList;

            dungeons.clear();
            dungeons.insert(*currentDungeon.begin()); // they should only have 1 dungeon in the map
        }
    }

    // used for upcoming checks
    bool isRandom  = false;
    bool isRaid    = false;
    bool isDungeon = false;

    LfgJoinResult result = GetJoinResult(plr);
    if (result == ERR_LFG_OK)
    {
        // additional checks on dungeon selection
        for (std::set<uint32>::iterator it = dungeons.begin(); it != dungeons.end(); ++it)
        {
            LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(*it);
            // The dungeon ids arrive from the client. LookupEntry returns NULL
            // for an unknown one, so dereferencing it unchecked is a remote
            // crash. That became reachable when this branch indexed
            // LfgDungeons.dbc by id: while it was indexed by row ordinal every
            // id below the record count found *something*, but the id space is
            // sparse, running to 774 with 432 holes.
            if (!dungeon)
            {
                result = ERR_LFG_INVALID_SLOT;
                break;
            }

            switch (dungeon->TypeID)
            {
                case LFG_TYPE_RANDOM_DUNGEON:
                    if (dungeons.size() > 1)
                    {
                        result = ERR_LFG_INVALID_SLOT;
                    }
                    else
                    {
                        isRandom = true;
                    }
                case LFG_TYPE_DUNGEON:
                case LFG_TYPE_HEROIC_DUNGEON:
                    if (isRaid)
                    {
                        result = ERR_LFG_MISMATCHED_SLOTS;
                    }
                    isDungeon = true;
                    break;
                case LFG_TYPE_RAID:
                    if (isDungeon)
                    {
                        result = ERR_LFG_MISMATCHED_SLOTS;
                    }
                    isRaid = true;
                    break;
                default: // one of the other types
                    result = ERR_LFG_INVALID_SLOT;
                    break;
            }

            // Refuse a slot whose tier this core cannot represent, instead of admitting it and
            // silently downgrading it later. LfgDungeons.dbc DifficultyID 7 (LFR), 8 (5-man
            // challenge), 11 and 12 (scenarios) and 14 (flexible) have no internal Difficulty, and
            // 77 of the 343 rows carry one of them.
            //
            // This is the gate that makes ToInternalDifficulty's negative return mean something.
            // CreateDungeonGroup runs long after the group has been built and its members pulled out
            // of their previous groups, so it is far too late to refuse there; all it can do is
            // substitute REGULAR_DIFFICULTY, which for an LFR row means a 25-player queue entering
            // the 10-normal tier of the same raid. Refusing at admission returns
            // ERR_LFG_INVALID_SLOT, which the client reports, and nothing is half-formed.
            if (result == ERR_LFG_OK && ToInternalDifficulty(dungeon->DifficultyID) < 0)
            {
                result = ERR_LFG_INVALID_SLOT;
            }
        }
    }

    // since our join result may have just changed, check it again
    if (result == ERR_LFG_OK)
    {
        if (isRandom)
        {
            // store the current dungeon id (replaced into the dungeon set later)
            randomDungeonID = *dungeons.begin();
            // fetch all dungeons with our groupID and add to set
            LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(*dungeons.begin());

            if (dungeon)
            {
                uint32 group = dungeon->Group_ID;

                for (uint32 id = 0; id < sLfgDungeonsStore.GetNumRows(); ++id)
                {
                    LfgDungeonsEntry const* dungeonList = sLfgDungeonsStore.LookupEntry(id);
                    if (dungeonList)
                    {
                        if (dungeonList->Group_ID == group)
                        {
                            dungeons.insert(dungeonList->ID); // adding to set
                        }
                    }
                }

                // Filter the expansion as well, so an untranslatable tier cannot ride into
                // roleCheck.dungeonList -- that list is handed back to the client and re-read on a
                // role-check rejoin.
                //
                // The numbers that were here before are withdrawn. They claimed Group_ID 0 matches
                // 273 rows, that 20 of those carry scenario tier 12, and that all 10 admissible
                // random rows have Group_ID 0. Re-measured against the shipped LfgDungeons.dbc, all
                // three are wrong: Group_ID 0 matches 79 rows, NONE of them carry tier 12, and no
                // admissible random row has Group_ID 0 at all -- the ten carry 1, 2, 3, 4, 5, 12,
                // 13, 33, 36 and 37, one per expansion tier. They were measured through a
                // LookupEntry that was returning the Nth ROW rather than the row with that ID,
                // because LfgDungeonsEntryfmt was missing its 'n' index marker, so every row read
                // belonged to a different dungeon.
                //
                // With that corrected, this filter removes nothing for any shipped random: each one
                // expands to a Group_ID whose members are ordinary dungeons carrying raw tier 1 or
                // 2, both translatable. It is kept as a guard rather than deleted, because the
                // expansion is driven by DBC content and a future row could carry a tier this core
                // cannot represent -- but it is a guard, not a load-bearing filter, and it should
                // not be cited as one.
                //
                // What actually stops an untranslatable tier reaching CreateDungeonGroup is the
                // admission check above. Both the party and solo paths below replace the expanded
                // set with randomDungeonID alone before the queued LFGPlayers state is built, and
                // SendDungeonProposal takes *dungeonList.begin() from that, so the proposal always
                // carries the random row -- which admission has already validated.
                //
                // Dropped, not refused: the expansion is a CANDIDATE list, so removing members this
                // core cannot run leaves random queueing working. No empty-set check follows,
                // because the admitted random row is itself translatable and always survives.
                for (std::set<uint32>::iterator it = dungeons.begin(); it != dungeons.end(); )
                {
                    LfgDungeonsEntry const* candidate = sLfgDungeonsStore.LookupEntry(*it);
                    if (!candidate || ToInternalDifficulty(candidate->DifficultyID) < 0)
                    {
                        it = dungeons.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            else
            {
                result = ERR_LFG_NO_LFG_OBJECT;
            }
        }
    }

    partyForbidden partyLockedDungeons;
    if (result == ERR_LFG_OK)
    {
        // do FindRandomDungeonsNotForPlayer for the plr or whole group
        if (pGroup)
        {
            for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                if (Player* pGroupPlr = itr->getSource())
                {
                    ObjectGuid plrGuid = pGroupPlr->GetObjectGuid();

                    dungeonForbidden lockedDungeons = FindRandomDungeonsNotForPlayer(pGroupPlr);
                    partyLockedDungeons[plrGuid] = lockedDungeons;

                    for (dungeonForbidden::iterator it = lockedDungeons.begin(); it != lockedDungeons.end(); ++it)
                    {
                        uint32 dungeonID = (it->first & 0x00FFFFFF);

                        std::set<uint32>::iterator setItr = dungeons.find(dungeonID);
                        if (setItr != dungeons.end())
                        {
                            dungeons.erase(*setItr);
                        }
                    }
                }
            }
        }
        else
        {
            dungeonForbidden lockedDungeons = FindRandomDungeonsNotForPlayer(plr);
            partyLockedDungeons[guid] = lockedDungeons;

            for (dungeonForbidden::iterator it = lockedDungeons.begin(); it != lockedDungeons.end(); ++it)
            {
                uint32 dungeonID = (it->first & 0x00FFFFFF);

                std::set<uint32>::iterator setItr = dungeons.find(dungeonID);
                if (setItr != dungeons.end())
                {
                    dungeons.erase(*setItr);
                }
            }
        }

        if (!dungeons.empty())
        {
            partyLockedDungeons.clear();
        }
        else
        {
            result = (pGroup) ? ERR_LFG_NO_SLOTS_PARTY : ERR_LFG_NO_SLOTS_PLAYER;
        }
    }

    // If our result is not ERR_LFG_OK, send join result now with err message
    if (result != ERR_LFG_OK)
    {
        plr->GetSession()->SendLfgJoinResult(result, LFG_STATE_NONE, partyLockedDungeons);
        return;
    }

    if (pGroup)
    {
        ObjectGuid leaderGuid = pGroup->GetLeaderGuid();

        LFGRoleCheck roleCheck;
        roleCheck.state = LFG_ROLECHECK_INITIALITING;
        roleCheck.dungeonList = dungeons;
        roleCheck.randomDungeonID = randomDungeonID;
        roleCheck.leaderGuidRaw = leaderGuid.GetRawValue();
        roleCheck.waitForRoleTime = time_t(time(NULL) + LFG_TIME_ROLECHECK);

        // place original dungeon ID back in the set
        if (isRandom)
        {
            dungeons.clear();
            dungeons.insert(randomDungeonID);
        }

        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            if (Player* pGroupPlr = itr->getSource())
            {
                LFGPlayerStatus overallStatus(LFG_STATE_NONE, LFG_UPDATE_JOIN, dungeons, comments);

                pGroupPlr->GetSession()->SendLfgUpdate(true, overallStatus);
                overallStatus.state = LFG_STATE_ROLECHECK;

                ObjectGuid plrGuid = pGroupPlr->GetObjectGuid();
                roleCheck.currentRoles[plrGuid] = 0;

                m_playerStatusMap[plrGuid] = overallStatus;
            }
        }

        // Stored AFTER the loop above, not before it. The stored copy used to be taken
        // while currentRoles was still empty, so the role check the rest of the system
        // saw listed nobody: PerformRoleCheck then found "everyone" had answered as soon
        // as the FIRST member replied, and a five-man queued on a one-entry role map.
        m_roleCheckMap[guid] = roleCheck;

        // used later if they enter the queue
        LFGPlayers groupInfo(LFG_STATE_NONE, dungeons, roleCheck.currentRoles, comments, false, time(NULL), 0, 0, 0);
        m_playerData[guid] = groupInfo;

        PerformRoleCheck(plr, pGroup, (uint8)roles);
    }
    else
    {
        // place original dungeon ID back in the set
        if (isRandom)
        {
            dungeons.clear();
            dungeons.insert(randomDungeonID);
        }

        // set up a role map and then an lfgplayer struct
        roleMap playerRole;
        playerRole[guid] = (uint8)roles;

        LFGPlayers playerInfo(LFG_STATE_QUEUED, dungeons, playerRole, comments, false, time(NULL), 0, 0, 0);
        m_playerData[guid] = playerInfo;

        // set up a status struct for client requests/updates
        LFGPlayerStatus plrStatus;
        plrStatus.updateType  = LFG_UPDATE_JOIN;
        plrStatus.state = LFG_STATE_NONE;
        plrStatus.dungeonList = dungeons;
        plrStatus.comment = comments;

        // Send information back to the client
        plr->GetSession()->SendLfgJoinResult(result, LFG_STATE_NONE, partyLockedDungeons);
        plr->GetSession()->SendLfgUpdate(false, plrStatus);

        plrStatus.state = LFG_STATE_QUEUED;
        m_playerStatusMap[guid] = plrStatus;
        AddToQueue(guid);
    }
}

void LFGMgr::LeaveLFG(Player* plr, bool isGroup)
{
    if (isGroup)
    {
        Group* pGroup = plr->GetGroup();
        ObjectGuid grpGuid = pGroup->GetObjectGuid();

        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            if (Player* pGroupPlr = itr->getSource())
            {
                ObjectGuid grpPlrGuid = pGroupPlr->GetObjectGuid();

                LFGPlayerStatus grpPlrStatus = GetPlayerStatus(grpPlrGuid);
                switch (grpPlrStatus.state)
                {
                    case LFG_STATE_PROPOSAL:
                    case LFG_STATE_QUEUED:
                        grpPlrStatus.updateType = LFG_UPDATE_LEAVE;
                        grpPlrStatus.state = LFG_STATE_NONE;
                        SendLfgUpdate(grpPlrGuid, grpPlrStatus, true);
                        break;
                    case LFG_STATE_ROLECHECK:
                        PerformRoleCheck(NULL, pGroup, 0);
                        break;
                    //todo: other state cases after they get implemented
                }

                m_playerData.erase(grpPlrGuid);
                m_playerStatusMap.erase(grpPlrGuid);
            }
        }

        m_queueSet.erase(grpGuid);
        m_playerData.erase(grpGuid);
    }
    else
    {
        ObjectGuid plrGuid = plr->GetObjectGuid();

        LFGPlayerStatus plrStatus = GetPlayerStatus(plrGuid);
        switch (plrStatus.state)
        {
            case LFG_STATE_PROPOSAL:
            case LFG_STATE_QUEUED:
                plrStatus.updateType = LFG_UPDATE_LEAVE;
                plrStatus.state = LFG_STATE_NONE;
                SendLfgUpdate(plrGuid, plrStatus, false);
                break;
            // do other states after being implemented, if applicable for a single plr
        }

        m_queueSet.erase(plrGuid);
        m_playerData.erase(plrGuid);
        m_playerStatusMap.erase(plrGuid);
    }

}

LFGPlayers* LFGMgr::GetPlayerOrPartyData(ObjectGuid guid)
{
    playerData::iterator it = m_playerData.find(guid);
    if (it != m_playerData.end())
    {
        return &(it->second);
    }
    else
    {
        return NULL;
    }
}

LFGProposal* LFGMgr::GetProposalData(uint32 proposalID)
{
    proposalMap::iterator it = m_proposalMap.find(proposalID);
    if (it != m_proposalMap.end())
    {
        return &(it->second);
    }
    else
    {
        return NULL;
    }
}

LfgJoinResult LFGMgr::GetJoinResult(Player* plr)
{
    // Initialised. `LfgJoinResult result;` was read uninitialised when a group had
    // members but every getSource() returned null.
    LfgJoinResult result = ERR_LFG_OK;
    Group* pGroup = plr->GetGroup();

    /* Reasons for not entering:
     *   Deserter spell
     *   Dungeon finder cooldown
     *   In a battleground
     *   In an arena
     *   Queued for battleground
     *   Too many members in group
     *   Group member disconnected
     *   Group member too low/high level
     *   Any group member cannot enter for x reason any other player can't
     */

    if (plr->HasAura(LFG_DESERTER_SPELL))
    {
        result = ERR_LFG_DESERTER_PLAYER;
    }
    else if (plr->InBattleGround() || plr->InBattleGroundQueue() || plr->InArena())
    {
        result = ERR_LFG_CANT_USE_DUNGEONS;
    }
    else if (plr->HasAura(LFG_COOLDOWN_SPELL))
    {
        result = ERR_LFG_RANDOM_COOLDOWN_PLAYER;
    }
    else if (plr->getLevel() < 15)
    {
        // The level test previously lived only in the group branch, so a solo player
        // below 15 was never checked at all.
        result = ERR_LFG_CANT_USE_DUNGEONS;
    }

    // Whatever the caller's own verdict is, it stands. The solo branch below used to
    // end in an unconditional `result = ERR_LFG_OK`, throwing away every check above
    // it: a solo player with Dungeon Deserter, on LFG cooldown, in a battleground, in
    // an arena or below level 15 was always admitted.
    if (result != ERR_LFG_OK)
    {
        return result;
    }

    if (pGroup)
    {
        if (pGroup->GetMembersCount() > 5)
        {
            result = ERR_LFG_TOO_MANY_MEMBERS;
        }
        else
        {
            uint8 currentMemberCount = 0;
            for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                if (Player* pGroupPlr = itr->getSource())
                {
                    // check if the group members are level 15+ to use finder
                    if (pGroupPlr->getLevel() < 15)
                    {
                        result = ERR_LFG_CANT_USE_DUNGEONS;
                    }
                    else if (pGroupPlr->HasAura(LFG_DESERTER_SPELL))
                    {
                        result = ERR_LFG_DESERTER_PARTY;
                    }
                    else if (pGroupPlr->InBattleGround() || pGroupPlr->InBattleGroundQueue() || pGroupPlr->InArena())
                    {
                        result = ERR_LFG_CANT_USE_DUNGEONS;
                    }
                    else if (pGroupPlr->HasAura(LFG_COOLDOWN_SPELL))
                    {
                        result = ERR_LFG_RANDOM_COOLDOWN_PARTY;
                    }
                    // No `else { result = ERR_LFG_OK; }` here. Assigning per member meant
                    // only the LAST iterated member's verdict survived, so a party
                    // containing one deserter was admitted whenever the last member
                    // happened to be clean.

                    ++currentMemberCount;
                }
            }

            if (result == ERR_LFG_OK && currentMemberCount != pGroup->GetMembersCount())
            {
                result = ERR_LFG_MEMBERS_NOT_PRESENT;
            }
        }
    }

    return result;
}

LFGPlayerStatus LFGMgr::GetPlayerStatus(ObjectGuid guid)
{
    LFGPlayerStatus status;

    playerStatusMap::iterator it = m_playerStatusMap.find(guid);
    if (it != m_playerStatusMap.end())
    {
        status = it->second;
    }

    return status;
}

bool LFGMgr::GetStatusPacketData(ObjectGuid queueGuid, ObjectGuid playerGuid, LFGStatusPacketData& data) const
{
    playerData::const_iterator queue = m_playerData.find(queueGuid);

    // A merged solo queuer has no entry of their own: MergeGroups folds them into the
    // absorbing entry and erases theirs. The direct lookup then missed, the caller was
    // handed a default-constructed struct, and their SMSG_LFG_UPDATE_STATUS went out
    // with zero roles, zero needed counts and a zero join time -- which is most of the
    // dungeon finder UI blank while the absorbing player's looked perfectly normal.
    //
    // So fall back to whichever entry actually LISTS this player.
    if (queue == m_playerData.end())
    {
        for (playerData::const_iterator it = m_playerData.begin(); it != m_playerData.end(); ++it)
        {
            if (it->second.currentRoles.find(playerGuid) != it->second.currentRoles.end())
            {
                queue = it;
                break;
            }
        }
    }

    if (queue == m_playerData.end())
        return false;

    LFGPlayers const& information = queue->second;
    roleMap::const_iterator role = information.currentRoles.find(playerGuid);
    if (role != information.currentRoles.end())
        data.roles = role->second;

    data.joinedTime = uint32(information.joinedTime);
    data.neededTanks = information.neededTanks;
    data.neededHealers = information.neededHealers;
    data.neededDps = information.neededDps;
    return true;
}

void LFGMgr::SetPlayerComment(ObjectGuid guid, std::string comment)
{
    LFGPlayerStatus status = GetPlayerStatus(guid);
    status.comment = comment;

    m_playerStatusMap[guid] = status;
}

void LFGMgr::SetPlayerState(ObjectGuid guid, LFGState state)
{
    LFGPlayerStatus status = GetPlayerStatus(guid);
    status.state = state;

    m_playerStatusMap[guid] = status;
}

void LFGMgr::SetPlayerUpdateType(ObjectGuid guid, LfgUpdateType updateType)
{
    LFGPlayerStatus status = GetPlayerStatus(guid);
    status.updateType = updateType;

    m_playerStatusMap[guid] = status;
}
