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

#include <sstream>
#include <vector>

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
 * @file LFGMgrProposal.cpp
 * @brief Cohesion split of LFGMgr.cpp -- role check, dungeon proposal and in-dungeon flow: PerformRoleCheck, proposal send/update/decline, dungeon group create, teleport, boss-kill, kick/vote and LFG packet senders. Same LFGMgr class; no behaviour change. CMake file(GLOB) picks this file up automatically; LFGMgr.h is unchanged.
 */

// called each time a player selects their role
void LFGMgr::PerformRoleCheck(Player* pPlayer, Group* pGroup, uint8 roles)
{
    ObjectGuid groupGuid = pGroup->GetObjectGuid();
    ObjectGuid plrGuid = pPlayer ? pPlayer->GetObjectGuid() : ObjectGuid();

    roleCheckMap::iterator it = m_roleCheckMap.find(groupGuid);
    if (it == m_roleCheckMap.end())
    {
        return; // no role check map found
    }

    // A REFERENCE, not a copy. This was `LFGRoleCheck roleCheck = it->second;`, so
    // every `roleCheck.currentRoles[plrGuid] = roles` below landed in a temporary that
    // was discarded on return -- no member's answer was ever recorded, and a party of
    // two or more could never complete its role check no matter what anyone clicked.
    LFGRoleCheck& roleCheck = it->second;
    bool roleChosen = roleCheck.state != LFG_ROLECHECK_DEFAULT && plrGuid;

    if (!plrGuid)
    {
        roleCheck.state = LFG_ROLECHECK_ABORTED;  // aborted if anyone cancels during role check
    }
    else if (!(roles & (PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER | PLAYER_ROLE_DAMAGE)))
    {
        // The mask must name at least one real role. Testing `roles < PLAYER_ROLE_TANK`
        // only rejected 0 and a bare LEADER bit; it accepted any unknown high bit as a
        // valid answer, which then matched no role anywhere downstream.
        roleCheck.state = LFG_ROLECHECK_NO_ROLE;
    }
    else
    {
        roleCheck.currentRoles[plrGuid] = roles;

        bool allRolesChosen = true;
        for (roleMap::iterator rItr = roleCheck.currentRoles.begin(); rItr != roleCheck.currentRoles.end(); ++rItr)
        {
            if (rItr->second == PLAYER_ROLE_NONE)
            {
                allRolesChosen = false;
                break;
            }
        }

        if (allRolesChosen) // meaning that everyone confirmed their roles
        {
            roleCheck.state = ValidateGroupRoles(roleCheck.currentRoles, roleCheck.dungeonList) ? LFG_ROLECHECK_FINISHED : LFG_ROLECHECK_MISSING_ROLE;
        }
    }

    std::set<uint32> dungeonBuff;
    if (roleCheck.randomDungeonID)
    {
        dungeonBuff.insert(roleCheck.randomDungeonID);
    }
    else
    {
        dungeonBuff = roleCheck.dungeonList;
    }

    partyForbidden nullForbidden;

    for (roleMap::iterator itr = roleCheck.currentRoles.begin(); itr != roleCheck.currentRoles.end(); ++itr)
    {
        ObjectGuid guidBuff = itr->first;
        if (roleChosen)
        {
            SendRoleChosen(guidBuff, plrGuid, roles); // send SMSG_LFG_ROLE_CHOSEN to each player
        }

        // send SMSG_LFG_ROLE_CHECK_UPDATE
        SendRoleCheckUpdate(guidBuff, roleCheck);

        switch (roleCheck.state)
        {
            case LFG_ROLECHECK_INITIALITING:
                continue;
            case LFG_ROLECHECK_FINISHED:
                // set current plr's state to queued. then set their role in that struct
                // then send lfgupdate packet with UPDATETYPE_ADDED_TO_QUEUE
                SetPlayerState(guidBuff, LFG_STATE_QUEUED);
                SetPlayerUpdateType(guidBuff, LFG_UPDATE_ADDED_TO_QUEUE);
                SendLfgUpdate(guidBuff, GetPlayerStatus(guidBuff), true);
                break;
            default:
                if (roleCheck.leaderGuidRaw == guidBuff.GetRawValue())
                {
                    SendLfgJoinResult(guidBuff, ERR_LFG_ROLE_CHECK_FAILED, uint8(roleCheck.state), nullForbidden);
                }
                SetPlayerUpdateType(guidBuff, LFG_UPDATE_ROLECHECK_FAILED);
                SendLfgUpdate(guidBuff, GetPlayerStatus(guidBuff), true);
                break;
        }
    }

    if (roleCheck.state == LFG_ROLECHECK_FINISHED)
    {
        LFGPlayers* queueInfo = GetPlayerOrPartyData(groupGuid);
        if (!queueInfo)
        {
            m_roleCheckMap.erase(groupGuid);
            return;
        }

        queueInfo->currentState = LFG_STATE_QUEUED;
        queueInfo->currentRoles = roleCheck.currentRoles;
        queueInfo->joinedTime   = time(NULL);

        m_playerData[groupGuid] = *queueInfo;

        AddToQueue(groupGuid);

        // The check is resolved; leaving it in the map makes RemoveOldRoleChecks expire
        // an already-queued party and tear its queue entry back down.
        m_roleCheckMap.erase(groupGuid);
    }
    else if (roleCheck.state != LFG_ROLECHECK_INITIALITING)
    {
        // todo: add players back to individual queues if applicable
        roleCheck.state = LFG_ROLECHECK_NO_ROLE;

        for (roleMap::iterator roleMapItr = roleCheck.currentRoles.begin(); roleMapItr != roleCheck.currentRoles.end(); ++roleMapItr)
        {
            ObjectGuid plrGuid = roleMapItr->first;

            SetPlayerState(plrGuid, LFG_STATE_NONE);

            SendRoleCheckUpdate(plrGuid, roleCheck);                 // role check failed
            SendLfgUpdate(plrGuid, GetPlayerStatus(plrGuid), true);  // not in lfg system anymore
        }
        m_roleCheckMap.erase(groupGuid);
    }
}

bool LFGMgr::ValidateGroupRoles(roleMap groupMap, std::set<uint32> const& dungeonList)
{
    if (groupMap.empty()) // sanity check
    {
        return false;
    }

    // This used to assert only that every member had picked exactly one of tank/healer/
    // damage, which failed two ways at once: a member who ticked tank AND damage matched
    // no case and sank the whole party's role check, while a party of five tanks passed
    // it and then jammed the queue because no dungeon has five tank slots.
    //
    // Asking whether the party can be assigned to the dungeon's actual role counts covers
    // both, and covers scenarios and raid finder, whose compositions are not 1/1/3.
    return RolesAreValidForDungeons(groupMap, dungeonList);
}

/**
 * @brief The dungeon a proposal should actually put the group into.
 *
 * A normal queue names a real dungeon and this returns it unchanged. A RANDOM queue names a
 * category, and a category is not a place: all 12 TypeID 6 rows in LfgDungeons.dbc carry MapID
 * 0 or 0xFFFFFFFF. Proposing one sent the group to a plain teleport failure, or -- for the four
 * carrying 0 -- silently to Eastern Kingdoms.
 *
 * The category row is excluded from its own expansion. Group_ID 33, behind Random Hour of
 * Twilight Heroic, has exactly ONE member and that member is the category row itself, so
 * without the exclusion that random would still propose an unrunnable row.
 *
 * Untranslatable tiers are excluded for the same reason JoinLFG refuses them at admission: a
 * row whose DifficultyID has no internal mode cannot be entered at the tier it claims.
 *
 * @return a concrete dungeon id, or 0 when nothing behind the selection is runnable.
 */
static uint32 PickConcreteDungeon(uint32 queuedDungeonId, std::set<uint32> const& candidates)
{
    LfgDungeonsEntry const* queued = sLfgDungeonsStore.LookupEntry(queuedDungeonId);
    if (!queued)
    {
        return 0;
    }

    if (queued->TypeID != LFG_TYPE_RANDOM_DUNGEON)
    {
        return queuedDungeonId;                             // already a real dungeon
    }

    for (std::set<uint32>::const_iterator it = candidates.begin(); it != candidates.end(); ++it)
    {
        if (*it == queuedDungeonId)
        {
            continue;                                       // the category cannot host itself
        }

        LfgDungeonsEntry const* candidate = sLfgDungeonsStore.LookupEntry(*it);
        if (!candidate || candidate->TypeID == LFG_TYPE_RANDOM_DUNGEON)
        {
            continue;
        }

        if (ToInternalDifficulty(candidate->DifficultyID) < 0)
        {
            continue;
        }

        return candidate->ID;
    }

    return 0;
}

//todo: remove from queue, update queue average settings
void LFGMgr::SendDungeonProposal(ObjectGuid queueGuid, LFGPlayers* lfgGroup)
{
    ++m_proposalId; // increment number to make a new proposal id

    std::set<uint32>::iterator dItr = lfgGroup->dungeonList.begin();

    // note: group create function's parameters are leader guid & leader name
    LFGProposal newProposal;
    newProposal.id = m_proposalId;
    newProposal.state = LFG_PROPOSAL_INITIATING;
    newProposal.encounters = 0; // todo: check if group has already started a dungeon and are looking for another plr
    newProposal.currentRoles = lfgGroup->currentRoles;
    newProposal.dungeonID = *dItr;

    // The dungeon the group is actually put into.
    //
    // For a normal queue that is the queued row. For a RANDOM one it cannot be: every TypeID 6
    // row in LfgDungeons.dbc carries MapID 0 or 0xFFFFFFFF, so proposing the category itself
    // teleports the group nowhere -- 4 of the 12 silently to Eastern Kingdoms and the other 8 to
    // a plain failure. A concrete member of the expansion is chosen instead, while dungeonID
    // keeps naming the random entry for the proposal packet and the reward lookup.
    newProposal.concreteDungeonID = PickConcreteDungeon(*dItr, lfgGroup->candidateDungeons);
    if (!newProposal.concreteDungeonID)
    {
        // Nothing runnable behind the category. Do not build a proposal that cannot complete:
        // the group would be formed, torn out of its previous groups and then left standing.
        sLog.outError("LFG SendDungeonProposal: random dungeon %u expanded to no runnable "
                      "member; refusing to propose.", *dItr);
        return;
    }

    newProposal.isNew = true;
    newProposal.joinedQueue = lfgGroup->joinedTime;
    newProposal.createdTime = time(NULL);

    // Which queue entry this came from, so a failure can put the survivors back. Passed
    // in rather than recovered by scanning m_playerData for a matching address: the
    // caller already knows the key, and identifying a map entry by the address of its
    // value is the kind of thing that quietly stops working the first time anyone copies
    // the struct.
    newProposal.queueGuid = queueGuid;

    {
        std::ostringstream avail;
        for (std::set<uint32>::const_iterator it = lfgGroup->dungeonList.begin();
             it != lfgGroup->dungeonList.end(); ++it)
        {
            avail << (it == lfgGroup->dungeonList.begin() ? "" : ",") << *it;
        }
        DEBUG_LOG("LFG SendDungeonProposal: entry dungeons={%s} -> chose %u (entry 0x%08X)",
                  avail.str().c_str(), newProposal.dungeonID, GetDungeonEntry(newProposal.dungeonID));
    }

    bool premadeGroup = IsProposalSameGroup(newProposal);

    // iterate through role map just so get everyone's guid
    for (roleMap::iterator it = lfgGroup->currentRoles.begin(); it != lfgGroup->currentRoles.end(); ++it)
    {
        ObjectGuid plrGuid = it->first;
        SetPlayerState(plrGuid, LFG_STATE_PROPOSAL);

        Player* pPlayer = sObjectAccessor.FindPlayer(plrGuid);
        if (!pPlayer)
        {
            continue;
        }

        if (Group* pGroup = pPlayer->GetGroup())
        {
            ObjectGuid grpGuid = pGroup->GetObjectGuid();

            SetPlayerUpdateType(plrGuid, LFG_UPDATE_PROPOSAL_BEGIN);

            if (premadeGroup && pGroup->IsLeader(plrGuid))
            {
                newProposal.groupLeaderGuid = plrGuid.GetRawValue();
            }

            if (premadeGroup && !newProposal.groupRawGuid)
            {
                newProposal.groupRawGuid = grpGuid.GetRawValue();
            }

            newProposal.groups[plrGuid] = grpGuid;

            SendLfgUpdate(plrGuid, GetPlayerStatus(plrGuid), true);
        }
        else
        {
            newProposal.groups[plrGuid] = ObjectGuid();

            //SetPlayerUpdateType(plrGuid, LFG_UPDATE_GROUP_FOUND);
            //SendLfgUpdate(plrGuid, GetPlayerStatus(plrGuid), false);

            SetPlayerUpdateType(plrGuid, LFG_UPDATE_PROPOSAL_BEGIN);
            SendLfgUpdate(plrGuid, GetPlayerStatus(plrGuid), false);
        }

        newProposal.answers[plrGuid] = LFG_ANSWER_PENDING;
    }

    // Sent only once the proposal is COMPLETE.
    //
    // This used to sit inside the loop above, which is still filling `groups` and
    // `answers`. Since the packet serialises those maps, every recipient except the last
    // one received an opening proposal that omitted the members added after them -- so
    // the ready popup showed an incomplete group until somebody answered.
    for (roleMap::const_iterator it = lfgGroup->currentRoles.begin();
         it != lfgGroup->currentRoles.end(); ++it)
    {
        if (Player* pMember = sObjectAccessor.FindPlayer(it->first))
        {
            pMember->GetSession()->SendLfgProposalUpdate(newProposal);
        }
    }

    // then if group guid is set, call Group::SetAsLfgGroup()
    if (premadeGroup)
    {
        Player* pGroupLeader = sObjectAccessor.FindPlayer(ObjectGuid(newProposal.groupLeaderGuid));

        if (pGroupLeader)
        {
            Group* pGroup = pGroupLeader->GetGroup();
            if (pGroup)
            {
                pGroup->SetAsLfgGroup();
            }
            else
            {
                // Log an error: group not found for group leader
                // In the future, we should determine the right actions for this scenario.
            }
        }
        else
        {
            // Log an error: group leader not found
            // In the future, we should determine the right actions for this scenario.
        }
    }

    // also save the proposal
    m_proposalMap[newProposal.id] = newProposal;
}

bool LFGMgr::IsProposalSameGroup(LFGProposal const& proposal)
{
    // True only when EVERY member is in the SAME existing group.
    //
    // This used to skip ungrouped players entirely, so a two-man party matched with
    // three solo queuers returned true -- the proposal was then treated as a premade
    // and CreateDungeonGroup reused the party's group without ever adding the solos.
    // It also returned true when nobody was grouped at all, because isSameGroup started
    // true and had no way to become false.
    bool firstLoop = true;
    bool isSameGroup = true;
    bool anyGrouped = false;

    ObjectGuid priorGroupGuid;

    // when this is called we don't have the groups part filled, so iterate via role map
    for (roleMap::const_iterator it = proposal.currentRoles.begin(); it != proposal.currentRoles.end(); ++it)
    {
        ObjectGuid plrGuid = it->first;

        Player* pPlayer = sObjectAccessor.FindPlayer(plrGuid);
        // A queued player who logged out, or who is mid-teleport, is not found.
        // This runs BEFORE the offline-skip loop in SendDungeonProposal, so one
        // absent member would crash the whole proposal.
        if (!pPlayer)
        {
            continue;
        }

        Group* pGroup = pPlayer->GetGroup();
        if (!pGroup)
        {
            return false;   // an ungrouped member means this is not one existing group
        }

        anyGrouped = true;
        ObjectGuid grpGuid = pGroup->GetObjectGuid();

        if (firstLoop)
        {
            priorGroupGuid = grpGuid;
            firstLoop = false;
        }
        else if (grpGuid != priorGroupGuid)
        {
            isSameGroup = false;
        }
    }

    return anyGrouped && isSameGroup;
}

// From a CMSG_LFG_PROPOSAL_RESPONSE call
/// A decline cancels the proposal, but it does NOT eject everyone.
///
/// The client states all three outcomes plainly:
///   ERR_LFG_PROPOSAL_FAILED         "Someone has declined the invite. You have been
///                                    returned to the front of the queue."
///   ERR_LFG_PROPOSAL_DECLINED_SELF  "You have been removed from the queue because you
///                                    did not accept the invitation."
///   ERR_LFG_PROPOSAL_DECLINED_PARTY "...because someone in your party did not accept."
///
/// So the decliner leaves, their premade leaves with them, and everyone else is
/// requeued. An earlier version of this removed everyone, which is why the queue entry
/// is now kept alive for the lifetime of the proposal -- there has to be something left
/// to put people back into.
void LFGMgr::DeclineProposal(ObjectGuid plrGuid, LFGProposal* proposal)
{
    std::set<ObjectGuid> culprits;
    culprits.insert(plrGuid);

    // A premade is removed alongside the member who declined for it.
    playerGroupMap::const_iterator declinerGroup = proposal->groups.find(plrGuid);
    if (declinerGroup != proposal->groups.end() && declinerGroup->second)
    {
        for (playerGroupMap::const_iterator it = proposal->groups.begin();
             it != proposal->groups.end(); ++it)
        {
            if (it->second == declinerGroup->second)
            {
                culprits.insert(it->first);
            }
        }
    }

    CancelProposal(proposal->id, culprits);
}

void LFGMgr::ProposalUpdate(uint32 proposalID, ObjectGuid plrGuid, bool accepted)
{
    //note: create a group here if it doesn't exist and everyone accepted proposal
    LFGProposal* proposal = GetProposalData(proposalID);

    if (!proposal)
    {
        return;
    }

    // Only a participant may answer.
    //
    // m_proposalId is a plain incrementing counter, so an id is trivially guessable.
    // Without this check, writing to proposal->answers INSERTED the caller, and a
    // `false` answer from any logged-in player cancelled a group they had nothing to do
    // with -- clearing the real members out of the queue.
    if (proposal->answers.find(plrGuid) == proposal->answers.end())
    {
        sLog.outError("LFG: %s answered proposal %u they are not part of.",
                      plrGuid.GetString().c_str(), proposalID);
        return;
    }

    bool allOkay = true; // true if everyone answered LFG_ANSWER_AGREE

    // Update answer map to given value
    LFGProposalAnswer plrAnswer = (LFGProposalAnswer)accepted;
    proposal->answers[plrGuid] = plrAnswer;

    if (plrAnswer == LFG_ANSWER_DENY)
    {
        DeclineProposal(plrGuid, proposal);
        return;
    }

    for (proposalAnswerMap::iterator it = proposal->answers.begin(); it != proposal->answers.end(); ++it)
    {
        if (it->second != LFG_ANSWER_AGREE)
        {
            allOkay = false;
        }
    }

    // if !allOkay, send proposal updates to all
    if (!allOkay)
    {
        for (proposalAnswerMap::iterator itr = proposal->answers.begin(); itr != proposal->answers.end(); ++itr)
        {
            ObjectGuid proposalPlrGuid  = itr->first;
            Player* pProposalPlayer = sObjectAccessor.FindPlayer(proposalPlrGuid);
            if (pProposalPlayer)
            {
                pProposalPlayer->GetSession()->SendLfgProposalUpdate(*proposal);
            }
        }

        return;
    }

    // at this point everyone's good to join the dungeon!

    time_t joinedTime = time(NULL);
    bool sendProposalUpdate = proposal->state != LFG_PROPOSAL_SUCCESS;

    // now update the proposal's state to successful and inform the players
    proposal->state = LFG_PROPOSAL_SUCCESS;
    for (roleMap::iterator rItr = proposal->currentRoles.begin(); rItr != proposal->currentRoles.end(); ++rItr)
    {
        // get the player's role
        uint8 proposalPlrRole   = rItr->second;
        proposalPlrRole &= ~PLAYER_ROLE_LEADER;

        ObjectGuid proposalPlrGuid  = rItr->first;
        Player* pProposalPlayer = sObjectAccessor.FindPlayer(proposalPlrGuid);
        if (!pProposalPlayer)
        {
            // Accepted, then logged out before the last answer arrived. allOkay still
            // passed because their answer was already AGREE, and skipping them here
            // built a SHORT group and teleported it while groupStatus recorded a role
            // for someone who was never added. Cancel instead: the absent member is the
            // culprit and everyone else goes back to the queue.
            std::set<ObjectGuid> absent;
            absent.insert(proposalPlrGuid);
            CancelProposal(proposal->id, absent);
            return;
        }

        if (sendProposalUpdate)
        {
            pProposalPlayer->GetSession()->SendLfgProposalUpdate(*proposal);
        }

        // amount of time spent in queue
        int32 timeWaited = joinedTime - proposal->joinedQueue;

        // tell the lfg system to update the average wait times on the next tick
        UpdateWaitMap(LFGRoles(proposalPlrRole), proposal->dungeonID, timeWaited);

        // send some updates to the player, depending on group status
        LFGPlayerStatus proposalPlrStatus = GetPlayerStatus(proposalPlrGuid);
        proposalPlrStatus.updateType = LFG_UPDATE_GROUP_FOUND;

        if (pProposalPlayer->GetGroup())
        {
            SendLfgUpdate(proposalPlrGuid, proposalPlrStatus, true);
            RemoveFromQueue(pProposalPlayer->GetGroup()->GetObjectGuid()); // not the best way to handle this
        }
        else
        {
            SendLfgUpdate(proposalPlrGuid, proposalPlrStatus, false);
            RemoveFromQueue(proposalPlrGuid);
        }

        proposalPlrStatus.updateType = LFG_UPDATE_LEAVE;
        SendLfgUpdate(proposalPlrGuid, proposalPlrStatus, false);
        SendLfgUpdate(proposalPlrGuid, proposalPlrStatus, true);
    }

    CreateDungeonGroup(proposal);

    // Tear the queue entry down. TryFormGroup deliberately KEEPS it alive for the
    // lifetime of the proposal so a decline or timeout can put the survivors back --
    // but on success nobody put it back, so it sat in m_playerData forever with
    // currentState LFG_STATE_PROPOSAL, and every member's stored status stayed at
    // LFG_STATE_PROPOSAL too. JoinLFG refuses that state, so a player who successfully
    // entered a dungeon could never queue again until relog. Observed live: five
    // rejected CMSG_LFG_JOIN attempts after one successful proposal.
    ObjectGuid const queueGuid = proposal->queueGuid;
    for (roleMap::const_iterator it = proposal->currentRoles.begin();
         it != proposal->currentRoles.end(); ++it)
    {
        // They are in the dungeon now, not queued. TeleportToDungeon sets this too for
        // the players it actually moves, but a member whose teleport was denied must not
        // be left reading LFG_STATE_PROPOSAL either.
        SetPlayerState(it->first, LFG_STATE_IN_DUNGEON);
    }

    m_queueSet.erase(queueGuid);
    m_playerData.erase(queueGuid);

    m_proposalMap.erase(proposal->id);
}

bool LFGMgr::HasLeaderFlag(roleMap const& roles)
{
    for (roleMap::const_iterator it = roles.begin(); it != roles.end(); ++it)
    {
        if (it->second & PLAYER_ROLE_LEADER)
        {
            return true;
        }
    }
    return false;
}

void LFGMgr::CreateDungeonGroup(LFGProposal* proposal)
{
    if (!proposal)
    {
        return;
    }

    // Rewritten. The previous version had four independent defects on this one path:
    //
    //  - The leader search looped over every role-flagged member calling Group::Create
    //    with no break, so two merged premades carrying two LEADER bits ran Create
    //    twice on one object. Each call does its own GenerateGroupLowGuid plus an
    //    INSERT INTO groups in its own transaction, orphaning the first group id and
    //    stranding that id's group_member rows.
    //  - If a leader bit was set but every leader-flagged player was offline, Create
    //    never ran while AddMember still did -- building a group with id 0 and an empty
    //    leader guid, which was then inserted into m_groupSet.
    //  - The existing-group branch called no AddMember at all, so the commonest LFD
    //    composition (one premade plus solo queuers) dequeued the solos, told them a
    //    group was found, and never put them in one.
    //  - Nothing registered the group with ObjectMgr, so GetGroupById could not find
    //    it, it leaked at shutdown, and the boot path called RemoveGroup on a group
    //    that had never been added.
    //
    // Resolve the leader ONCE, up front, and require them to be online.
    ObjectGuid leaderGuid;

    // With `.debug dungeon` active a game master leads the dungeon regardless of who
    // carries the LEADER bit, so the operator always has control of the group they are
    // testing. Checked first, so it wins outright.
    if (m_debugMode != LFG_DEBUG_OFF)
    {
        for (roleMap::const_iterator it = proposal->currentRoles.begin();
             it != proposal->currentRoles.end(); ++it)
        {
            Player* pPlayer = sObjectAccessor.FindPlayer(it->first);
            if (pPlayer && pPlayer->GetSession() &&
                pPlayer->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
            {
                leaderGuid = it->first;
                break;
            }
        }
    }

    for (roleMap::const_iterator it = proposal->currentRoles.begin();
         !leaderGuid && it != proposal->currentRoles.end(); ++it)
    {
        if ((it->second & PLAYER_ROLE_LEADER) && sObjectAccessor.FindPlayer(it->first))
        {
            leaderGuid = it->first;
            break;
        }
    }

    // Looked up BEFORE anything is created. This used to sit after group creation, so
    // an unknown dungeon id returned having already new'd a Group, run Create (a group
    // id plus an INSERT INTO groups) and registered it with ObjectMgr -- leaking the
    // object and stranding its rows, with the proposal also left in m_proposalMap.
    // The CONCRETE dungeon: proposal->dungeonID may be a random category, which has no map.
    // Older proposals predating the split carry 0 here, so fall back rather than refuse.
    uint32 const runDungeonId = proposal->concreteDungeonID ? proposal->concreteDungeonID
                                                            : proposal->dungeonID;
    LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(runDungeonId);
    if (!dungeon)
    {
        return;
    }

    Group* pGroup = nullptr;

    if (proposal->groupRawGuid)
    {
        // Reuse the premade group the proposal was built around.
        Player* pGroupLeader = sObjectAccessor.FindPlayer(ObjectGuid(proposal->groupLeaderGuid));
        if (pGroupLeader)
        {
            pGroup = pGroupLeader->GetGroup();
        }

        // The stored leader may have logged out between proposal and acceptance. Fall
        // back to any online member still in that same group.
        if (!pGroup)
        {
            for (playerGroupMap::const_iterator it = proposal->groups.begin();
                 it != proposal->groups.end(); ++it)
            {
                if (it->second.GetRawValue() != proposal->groupRawGuid)
                {
                    continue;
                }

                if (Player* pMember = sObjectAccessor.FindPlayer(it->first))
                {
                    pGroup = pMember->GetGroup();
                    if (pGroup)
                    {
                        break;
                    }
                }
            }
        }
    }

    if (!pGroup)
    {
        // No group to reuse: build one. The leader is whoever carries the LEADER bit
        // and is online, else the first online member.
        if (!leaderGuid)
        {
            for (playerGroupMap::const_iterator it = proposal->groups.begin();
                 it != proposal->groups.end(); ++it)
            {
                if (sObjectAccessor.FindPlayer(it->first))
                {
                    leaderGuid = it->first;
                    break;
                }
            }
        }

        Player* pLeader = sObjectAccessor.FindPlayer(leaderGuid);
        if (!pLeader)
        {
            return;     // everyone went offline; nothing to build
        }

        // Detach from any prior group BEFORE creating, so Create does not run against a
        // player their old group still lists.
        //
        // Player::RemoveFromGroup, not Group::RemoveMember directly: pulling a member
        // out of a two-man group makes RemoveMember Disband it, and Disband does not
        // delete the object or unregister it. The helper is the codebase's own
        // convention for exactly this and handles RemoveGroup plus delete.
        for (playerGroupMap::const_iterator it = proposal->groups.begin();
             it != proposal->groups.end(); ++it)
        {
            Player* pMember = sObjectAccessor.FindPlayer(it->first);
            if (pMember && pMember->GetGroup())
            {
                Player::RemoveFromGroup(pMember->GetGroup(), it->first);
            }
        }

        pGroup = new Group();
        if (!pGroup->Create(pLeader->GetObjectGuid(), pLeader->GetName()))
        {
            delete pGroup;
            return;
        }

        pGroup->SetAsLfgGroup();

        // A dungeon whose composition exceeds a party must be a RAID before anyone is
        // added. Group::IsFull caps a normal party at MAX_GROUP_SIZE, and AddMember just
        // returns false past that -- so a raid-finder proposal (2/6/17 = 25) silently
        // completed as a five-man while the other twenty were told a group had been
        // found, never added, and never teleported.
        if (dungeon->Count_tank + dungeon->Count_healer + dungeon->Count_damage > MAX_GROUP_SIZE)
        {
            pGroup->ConvertToRaid();
        }

        sObjectMgr.AddGroup(pGroup);
    }

    // Everyone in the proposal who is not already in this group joins it. That covers
    // both paths: a freshly created group needs every non-leader added, and a reused
    // premade needs the solo queuers that were matched into it.
    ObjectGuid const groupGuid = pGroup->GetObjectGuid();
    for (playerGroupMap::const_iterator it = proposal->groups.begin();
         it != proposal->groups.end(); ++it)
    {
        Player* pMember = sObjectAccessor.FindPlayer(it->first);
        if (!pMember || pGroup->IsMember(it->first))
        {
            continue;
        }

        if (Group* existing = pMember->GetGroup())
        {
            Player::RemoveFromGroup(existing, it->first);
        }

        if (!pGroup->AddMember(it->first, pMember->GetName()))
        {
            // Ignoring this return is how the raid case failed silently. Say so.
            sLog.outError("LFG: could not add %s to dungeon group %u (full at %u members).",
                          it->first.GetString().c_str(), pGroup->GetId(),
                          pGroup->GetMembersCount());
        }
    }

    // `dungeon` is the lookup made at the top of this function, before any group was
    // created -- it is not re-fetched here.
    //
    // LfgDungeons.dbc carries a RAW client DifficultyID. Casting it straight to
    // Difficulty made LFG normal (id 1) select internal mode 1 -- HEROIC -- and LFG
    // heroic (id 2) select mode 2, CHALLENGE. That value does not stay in the session:
    // Group::SetDungeonDifficulty persists it to `groups`.`difficulty` and, through the
    // members, to `characters`.`dungeon_difficulty`, so a single LFG run left every
    // member's saved difficulty wrong.
    int32 const dungeonMode = ToInternalDifficulty(dungeon->DifficultyID);

    // Raids and dungeons keep their difficulty in DIFFERENT fields, and the two setters do not
    // persist symmetrically:
    //   SetDungeonDifficulty -> `groups`.`difficulty` and every member's
    //                           `characters`.`dungeon_difficulty`
    //   SetRaidDifficulty    -> `groups`.`raiddifficulty` only, plus in-memory
    //                           Player::m_raidDifficulty
    //
    // There is deliberately no `characters`.`raid_difficulty` claim here: that column does not
    // exist in the schema. The raid tier reaches a character through Player::_LoadGroup at the
    // next login, not through a column of its own. An earlier revision of this comment asserted
    // the symmetric pair and was wrong about half of it.
    //
    // Sending a raid through the dungeon setter therefore stores the raid tier in the dungeon slot
    // and leaves the raid slot untouched. 61 of the 343 LfgDungeons rows are TypeID 2, and the
    // tiers they carry translate to internal 0..3 -- client 3 and 9 to 0, 4 to 1, 5 to 2, 6 to 3.
    // Internal 3 is 25-player heroic, which no 5-man tier corresponds to, so a group could end up
    // holding a dungeon difficulty outside the range dungeon difficulties represent.
    bool const isRaid = (dungeon->TypeID == LFG_TYPE_RAID);

    if (dungeonMode < 0)
    {
        // UNREACHABLE from the queue: JoinLFG refuses any slot whose DifficultyID has no internal
        // mode with ERR_LFG_INVALID_SLOT, and also filters the random-dungeon group expansion, which
        // otherwise smuggles 20 scenario rows in behind a valid random selection. Together those are
        // where an unsupported tier is actually rejected.
        // By the time execution arrives here the group exists and its members have already been
        // pulled out of their previous groups, so refusing is no longer an option -- all that is
        // left is to avoid persisting a mode the core cannot read.
        //
        // So this is a safety net, not a policy, and it is deliberately loud: if it ever fires, a
        // proposal reached here by some path that does not go through JoinLFG's admission check, and
        // the group is about to be teleported into REGULAR_DIFFICULTY of a map it did not queue for.
        // LFR (7), 5-man challenge (8), scenarios (11, 12) and flexible (14) have no internal
        // equivalent; 77 of the 343 LfgDungeons rows are one of those, 4 of them raids.
        sLog.outError("LFGMgr::CreateDungeonGroup: dungeon %u has client DifficultyID %u with no internal mode. "
                      "JoinLFG should have refused this slot; falling back to regular difficulty.",
                      dungeon->ID, dungeon->DifficultyID);

        if (isRaid)
        {
            pGroup->SetRaidDifficulty(REGULAR_DIFFICULTY);
        }
        else
        {
            pGroup->SetDungeonDifficulty(REGULAR_DIFFICULTY);
        }
    }
    else if (isRaid)
    {
        pGroup->SetRaidDifficulty(Difficulty(dungeonMode));
    }
    else
    {
        pGroup->SetDungeonDifficulty(Difficulty(dungeonMode));
    }

    // Add group to our group set and group map, then teleport to the dungeon.
    // groupGuid is the one taken above; do not shadow it.
    LFGGroupStatus groupStatus(LFG_STATE_IN_DUNGEON, dungeon->ID, proposal->currentRoles, pGroup->GetLeaderGuid());

    m_groupSet.insert(groupGuid);
    m_groupStatusMap[groupGuid] = groupStatus;

    TeleportToDungeon(dungeon->ID, pGroup);

    pGroup->SendUpdate();
}

void LFGMgr::TeleportToDungeon(uint32 dungeonID, Group* pGroup)
{
    // if the group's leader is already in the dungeon, teleport anyone not in dungeon to them
    // if nobody is in the dungeon, teleport all to beginning of dungeon (sObjectMgr.GetMapEntranceTrigger(mapid [not dungeonid]))
    LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(dungeonID);
    if (!dungeon || !pGroup)
    {
        return;
    }

    uint32 mapID = (uint32)dungeon->MapID;
    float x, y, z, o;
    LFGTeleportError err = LFG_TELEPORTERROR_OK;

    Player* pGroupLeader = sObjectAccessor.FindPlayer(pGroup->GetLeaderGuid());

    if (pGroupLeader && pGroupLeader->GetMapId() == mapID) // Already in the dungeon
    {
        // set teleport location to that of the group leader
        x = pGroupLeader->GetPositionX();
        y = pGroupLeader->GetPositionY();
        z = pGroupLeader->GetPositionZ();
        o = pGroupLeader->GetOrientation();
    }
    else
    {
        if (AreaTrigger const* at = sObjectMgr.GetMapEntranceTrigger(mapID))
        {
            x = at->target_X;
            y = at->target_Y;
            z = at->target_Z;
            o = at->target_Orientation;
        }
        else
        {
            sLog.outError("LFG TeleportToDungeon: no map entrance trigger for map %u "
                          "(dungeon %u) -- areatrigger_teleport has no row targeting it",
                          mapID, dungeonID);
            err = LFG_TELEPORTERROR_INVALID_LOCATION;
        }
    }

    dungeonForbidden lockedDungeons;
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        if (Player* pGroupPlr = itr->getSource())
        {
            // further checks: player is dead, in vehicle, in battleground, on taxi, etc
            LFGTeleportError plrErr = LFG_TELEPORTERROR_OK;

            if (pGroupPlr->IsDead())
            {
                plrErr = LFG_TELEPORTERROR_PLAYER_DEAD;
            }
            if (pGroupPlr->IsFalling())
            {
                plrErr = LFG_TELEPORTERROR_FALLING;
            }
            if (pGroupPlr->GetVehicleInfo())
            {
                plrErr = LFG_TELEPORTERROR_IN_VEHICLE;
            }
            // Same reasoning as the guard in TeleportPlayer: a member who is fighting
            // is not moved. This list already refused dead, falling and in-vehicle and
            // simply had no combat case.
            if (pGroupPlr->IsInCombat())
            {
                plrErr = LFG_TELEPORTERROR_IN_COMBAT;
            }

            lockedDungeons = FindRandomDungeonsNotForPlayer(pGroupPlr);
            if (lockedDungeons.find(dungeon->Entry()) != lockedDungeons.end())
            {
                plrErr = LFG_TELEPORTERROR_INVALID_LOCATION;
            }

            if (err == LFG_TELEPORTERROR_OK && plrErr == LFG_TELEPORTERROR_OK && pGroupPlr->GetMapId() != mapID)
            {
                if (pGroupPlr->GetMap() && !pGroupPlr->GetMap()->IsDungeon() && !pGroupPlr->GetMap()->IsRaid() && !pGroupPlr->InBattleGround())
                {
                    pGroupPlr->SetBattleGroundEntryPoint(); // store current position and such
                }

                if (!pGroupPlr->TeleportTo(mapID, x, y, z, o))
                {
                    plrErr = LFG_TELEPORTERROR_INVALID_LOCATION;
                }
            }

            if (err != LFG_TELEPORTERROR_OK)
            {
                sLog.outError("LFG TeleportToDungeon: %s DENIED, dungeon %u map %u, group error %u",
                              pGroupPlr->GetName(), dungeonID, mapID, uint32(err));
                pGroupPlr->GetSession()->SendLfgTeleportError(err);
            }
            else if (plrErr != LFG_TELEPORTERROR_OK)
            {
                sLog.outError("LFG TeleportToDungeon: %s DENIED, dungeon %u map %u, player error %u",
                              pGroupPlr->GetName(), dungeonID, mapID, uint32(plrErr));
                pGroupPlr->GetSession()->SendLfgTeleportError(plrErr);
            }
            else
            {
                SetPlayerState(pGroupPlr->GetObjectGuid(), LFG_STATE_IN_DUNGEON);
            }
        }
    }
}

void LFGMgr::TeleportPlayer(Player* pPlayer, bool out)
{
    // Fetch necessary data first
    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
    {
        pPlayer->GetSession()->SendLfgTeleportError((uint8)LFG_TELEPORTERROR_INVALID_LOCATION);
        return;
    }

    LFGGroupStatus* status = GetGroupStatus(pGroup->GetObjectGuid());
    if (!status)
    {
        pPlayer->GetSession()->SendLfgTeleportError((uint8)LFG_TELEPORTERROR_INVALID_LOCATION);
        return;
    }

    // Never move a player who is fighting, in EITHER direction.
    //
    // Without this the dropdown was an instant combat escape -- pull a pack, teleport
    // out, and the fight is simply over -- and Leave Instance Group yanked the player
    // out mid-pull, leaving the rest of the group in a fight they did not choose to
    // take alone. The client agrees this is refusable: it ships the message for it
    // (ERR_PARTY_LFG_TELEPORT_IN_COMBAT, "You cannot teleport out of the dungeon while
    // in combat.").
    //
    // Deliberately covers `in` as well. Teleporting INTO a dungeon while fighting
    // something outside it strands the mob and drops the player into an instance still
    // flagged in combat.
    //
    // This guard sits in TeleportPlayer rather than at the call sites so that the
    // dropdown (CMSG_LFG_TELEPORT) and the leave path (CMSG_GROUP_DISBAND) are both
    // covered by one check that cannot be forgotten by a third caller.
    if (pPlayer->IsInCombat())
    {
        DEBUG_LOG("LFG TeleportPlayer: %s refused (%s) -- in combat",
                  pPlayer->GetName(), out ? "out" : "in");
        pPlayer->GetSession()->SendLfgTeleportError((uint8)LFG_TELEPORTERROR_IN_COMBAT);
        return;
    }

    // Get dungeon info and then teleport the player out if applicable
    if (out)
    {
        LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(status->dungeonID);
        if (dungeon && pPlayer->GetMapId() == dungeon->MapID)
        {
            pPlayer->TeleportToBGEntryPoint();
        }
        return;
    }

    // Teleport back IN.
    //
    // This branch did not exist: TeleportPlayer only ever handled `out`, so the dropdown's
    // "Teleport to dungeon" resolved the group and the status and then fell off the end of
    // the function doing nothing. Observed live -- a player who ported out could not get
    // back, which is worse than not offering the option at all.
    //
    // TeleportToDungeon is the same routine the proposal uses on group creation. It moves
    // only members whose map is not already the dungeon's, so calling it for the whole
    // group moves exactly the one player who left, and it carries the dead / falling /
    // in-vehicle checks and the SMSG_LFG_TELEPORT_DENIED replies with it. It also prefers
    // the group leader's position when the leader is already inside, which is what puts a
    // returning player back with the group rather than at the entrance.
    TeleportToDungeon(status->dungeonID, pGroup);
}

LFGGroupStatus* LFGMgr::GetGroupStatus(ObjectGuid guid)
{
    groupStatusMap::iterator it = m_groupStatusMap.find(guid);
    if (it != m_groupStatusMap.end())
    {
        return &(it->second);
    }
    else
    {
        return NULL;
    }
}

/// Legacy per-player decline teardown.
///
/// No longer on the decline path: ProposalUpdate routes declines through CancelProposal,
/// which implements the three outcomes the client actually describes (decliner out,
/// their premade out, everyone else requeued). Kept because the boot/kick flow still
/// references this shape, but it must not be called for a proposal response.
void LFGMgr::ProposalDeclined(ObjectGuid guid, LFGProposal* proposal)
{
    Player* pPlayer = sObjectAccessor.FindPlayer(guid);

    if (!pPlayer)
    {
        return;
    }

    bool leaveGroupLFG = false;

    for (roleMap::iterator it = proposal->currentRoles.begin(); it != proposal->currentRoles.end(); ++it)
    {
        ObjectGuid groupPlrGuid = it->first;

        // update each player with a LFG_UPDATE_PROPOSAL_DECLINED
        SetPlayerUpdateType(groupPlrGuid, LFG_UPDATE_PROPOSAL_DECLINED);

        Player* pGroupPlayer = sObjectAccessor.FindPlayer(groupPlrGuid);
        if (!pGroupPlayer)
        {
            continue;
        }
        Group* pGroup = pGroupPlayer->GetGroup();

        // if player was in a premade group and declined, remove the group.
        if (groupPlrGuid == guid)
        {
            //LeaveLFG(pGroupPlayer, true);
            if (pGroup && (pGroup->GetObjectGuid().GetRawValue() == proposal->groupRawGuid))
            {
                leaveGroupLFG = true;
            }

            SendLfgUpdate(groupPlrGuid, GetPlayerStatus(groupPlrGuid), false);
        }
        else
        {
            if (proposal->groupRawGuid)
            {
                SendLfgUpdate(groupPlrGuid, GetPlayerStatus(groupPlrGuid), true);
            }
            else
            {
                SendLfgUpdate(groupPlrGuid, GetPlayerStatus(groupPlrGuid), false);
            }
        }
    }

    // The proposal is erased by ProposalUpdate, which owns it -- erasing here destroyed
    // the object our caller still holds a pointer to. Nor is there any point pruning the
    // decliner out of currentRoles/answers/groups any more: the whole proposal is torn
    // down either way, and pruning was exactly what let the survivors read as unanimous.
    LeaveLFG(pPlayer, leaveGroupLFG);
}

void LFGMgr::UpdateWaitMap(LFGRoles role, uint32 dungeonID, time_t waitTime)
{
    if (!role || !dungeonID || !waitTime)
    {
        return;
    }

    switch (role)
    {
        case PLAYER_ROLE_TANK:
        {
            waitTimeMap::iterator it = m_tankWaitTime.find(dungeonID);
            if (it != m_tankWaitTime.end())
            {
                LFGWait wait = it->second;

                wait.previousTime = wait.time;
                wait.time = waitTime;
                wait.doAverage = true;

                m_tankWaitTime[dungeonID] = wait;
            }
        }
            break;
        case PLAYER_ROLE_HEALER:
        {
            waitTimeMap::iterator hIt = m_healerWaitTime.find(dungeonID);
            if (hIt != m_healerWaitTime.end())
            {
                LFGWait wait = hIt->second;

                wait.previousTime = wait.time;
                wait.time = waitTime;
                wait.doAverage = true;

                m_healerWaitTime[dungeonID] = wait;
            }
        }
            break;
        case PLAYER_ROLE_DAMAGE:
        {
            waitTimeMap::iterator dIt = m_dpsWaitTime.find(dungeonID);
            if (dIt != m_dpsWaitTime.end())
            {
                LFGWait wait = dIt->second;

                wait.previousTime = wait.time;
                wait.time = waitTime;
                wait.doAverage = true;

                m_dpsWaitTime[dungeonID] = wait;
            }
        }
            break;
        default:
        {
            waitTimeMap::iterator aIt = m_avgWaitTime.find(dungeonID);
            if (aIt != m_avgWaitTime.end())
            {
                LFGWait wait = aIt->second;

                wait.previousTime = wait.time;
                wait.time = waitTime;
                wait.doAverage = true;

                m_avgWaitTime[dungeonID] = wait;
            }
        }
            break;
    }

}

void LFGMgr::HandleBossKilled(Player* pPlayer)
{
    Group* pGroup = pPlayer->GetGroup();
    if (!pGroup)
    {
        return;
    }

    ObjectGuid groupGuid = pGroup->GetObjectGuid();
    LFGGroupStatus* status = GetGroupStatus(groupGuid);
    if (!status)
    {
        return;
    }

    // set each player's lfgstate to LFG_STATE_FINISHED_DUNGEON
    // fetch reward info, and if it's the first dungeon of the day (per player),
    //    give them 2x the xp (or 1x if it's not the first), and the reward item
    //    (special case for 2nd wotlk heroic and +). If no room in inventory, send
    //    via ingame mail.
    status->state = LFG_STATE_FINISHED_DUNGEON;

    DungeonTypes type = GetDungeonType(status->dungeonID);
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next()) //todo: check if we will need to use mail or not
    {
        if (Player* pGroupPlr = itr->getSource())
        {
            SetPlayerState(pGroupPlr->GetObjectGuid(), LFG_STATE_FINISHED_DUNGEON);

            // check if player did a random dungeon
            uint32 randomDungeonId = 0;
            LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(status->dungeonID);
            // A stored dungeon id that no longer resolves -- stale group state,
            // or a DBC that changed under a saved group -- would crash the
            // reward path rather than simply pay nothing.
            if (dungeon && (dungeon->TypeID == LFG_TYPE_RANDOM_DUNGEON || IsSeasonal(dungeon->Flags)))
            {
                randomDungeonId = dungeon->ID;
            }

            // get rewards
            uint32 groupPlrLevel = pGroupPlr->getLevel();
            const DungeonFinderRewards* rewards = sObjectMgr.GetDungeonFinderRewards(groupPlrLevel); // Fetch base xp/money reward
            if (!rewards)
            {
                // Unconditionally dereferenced below. dungeonfinder_rewards ships 66
                // rows covering levels 15-80, so every level 81-90 character -- i.e.
                // every MoP-relevant one -- crashed the world server on a tracked boss
                // kill. No row means no base reward, not a crash.
                continue;
            }

            ItemRewards itemRewards = GetDungeonItemRewards(status->dungeonID, type);                // fetch item reward

            int32 multiplier;                                                                        // base reward modifier
            bool hasDoneDaily = HasPlayerDoneDaily(pGroupPlr->GetGUIDLow(), type);                                 // first dungeon of the day?
            (hasDoneDaily) ? multiplier = 1 : multiplier = 2;

            uint32 xpReward = multiplier*rewards->baseXPReward;                                      // player's xp reward
            uint32 moneyReward = uint32(multiplier*rewards->baseMonetaryReward);                              // player's money reward

            uint32 itemReward = 0;                                                                   // reward item
            uint32 itemAmount = 0;                                                                   // amount of item
            if (hasDoneDaily && (type == DUNGEON_WOTLK_HEROIC))
            {
                itemReward = WOTLK_SPECIAL_HEROIC_ITEM;
                itemAmount = WOTLK_SPECIAL_HEROIC_AMNT;
            }
            else if (!hasDoneDaily)
            {
                itemReward = itemRewards.itemId;
                itemAmount = itemRewards.itemAmount;
            }

            // and then fill a structure corresponding to SMSG_LFG_PLAYER_REWARD and
            // send one of these to each player
            LFGRewards reward(randomDungeonId, status->dungeonID, hasDoneDaily, moneyReward, xpReward, itemReward, itemAmount);
            pGroupPlr->GetSession()->SendLfgRewards(reward);
        }
    }

    // now we can remove the group from our maps
    m_groupStatusMap.erase(groupGuid);
    m_groupSet.erase(groupGuid);
}

void LFGMgr::AttemptToKickPlayer(Group* pGroup, ObjectGuid guid, ObjectGuid kicker, std::string reason)
{
    ObjectGuid groupGuid = pGroup->GetObjectGuid();
    LFGGroupStatus* status = GetGroupStatus(groupGuid);

    bootStatusMap::iterator bIt = m_bootStatusMap.find(groupGuid);
    if (!status)
    {
        return;
    }

    status->state = LFG_STATE_BOOT;
    m_groupStatusMap[groupGuid] = *status;

    // This function is only called when a group is set/in a dungeon so we can go straight to the boot packets
    time_t now = time(NULL);
    proposalAnswerMap votes;

    // safe to say the person attempting to kick them will vote yes, the kick-ee will vote no
    votes[guid] = LFG_ANSWER_DENY;
    votes[kicker] = LFG_ANSWER_AGREE;

    // set group state to boot vote, same for player states until it's over
    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next()) //todo: check if we will need to use mail or not
    {
        if (Player* pGroupPlr = itr->getSource())
        {
            ObjectGuid pGroupPlrGuid = pGroupPlr->GetObjectGuid();

            SetPlayerState(pGroupPlrGuid, LFG_STATE_BOOT);

            if ( (pGroupPlrGuid != guid) && (pGroupPlrGuid != kicker) )
            {
                votes[pGroupPlrGuid] = LFG_ANSWER_PENDING;
            }
        }
    }

    LFGBoot boot(true, guid, reason, votes, now);
    m_bootStatusMap[groupGuid] = boot;

    for (GroupReference* it = pGroup->GetFirstMember(); it != NULL; it = it->next())
    {
        if (Player* groupPlr = it->getSource())
        {
            groupPlr->GetSession()->SendLfgBootUpdate(boot);
        }
    }
}

void LFGMgr::CastVote(Player* pPlayer, bool vote)
{
    if (!pPlayer)
    {
        return;
    }

    Group* pGroup = pPlayer->GetGroup();
    ObjectGuid groupGuid = pGroup->GetObjectGuid();

    LFGGroupStatus* status = GetGroupStatus(groupGuid);

    if (!status || status->state != LFG_STATE_BOOT)
    {
        return;
    }

    bootStatusMap::iterator it = m_bootStatusMap.find(groupGuid);
    if (it == m_bootStatusMap.end())
    {
        return;
    }

    LFGBoot boot = it->second;
    boot.answers[pPlayer->GetObjectGuid()] = LFGProposalAnswer(vote);

    int32 yay = 0, nay = 0; // keep a count of votes
    for (proposalAnswerMap::iterator pIt = boot.answers.begin(); pIt != boot.answers.end(); ++pIt)
    {
        LFGProposalAnswer answer = pIt->second;
        if (answer == LFG_ANSWER_AGREE)
        {
            ++yay;
        }
        else if (answer == LFG_ANSWER_DENY)
        {
            ++nay;
        }
    }

    if (yay < REQUIRED_VOTES_FOR_BOOT && nay < REQUIRED_VOTES_FOR_BOOT)
    {
        m_bootStatusMap[groupGuid] = boot;
        return;
    }

    // if we dont have enough votes to kick or keep plr, don't send packet update
    // if else, set boot.inProgress to false, set plr + group states back to lfg-state-dungeon,
    // send packet update to group, kick plr if we had the votes, and then erase entry from boot map

    boot.inProgress = false;
    status->state = LFG_STATE_IN_DUNGEON;

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        if (Player* pGroupPlr = itr->getSource())
        {
            ObjectGuid plrGuid = pGroupPlr->GetObjectGuid();

            if (plrGuid != boot.playerVotedOn)
            {
                SetPlayerState(plrGuid, LFG_STATE_IN_DUNGEON);
                pGroupPlr->GetSession()->SendLfgBootUpdate(boot);
            }
        }
    }

    if (yay == REQUIRED_VOTES_FOR_BOOT)
    {
        // kick player from group
        if (pGroup->RemoveMember(boot.playerVotedOn, 1) <= 1)
        {
            // group->Disband(); already disbanded in RemoveMember
            sObjectMgr.RemoveGroup(pGroup);
            delete pGroup;
            // removemember sets the player's group pointer to NULL
        }
    }
}

void LFGMgr::SendRoleChosen(ObjectGuid plrGuid, ObjectGuid confirmedGuid, uint8 roles)
{
    Player* pPlayer = sObjectAccessor.FindPlayer(plrGuid);

    if (pPlayer)
    {
        pPlayer->GetSession()->SendLfgRoleChosen(confirmedGuid.GetRawValue(), roles);
    }
}

void LFGMgr::SendRoleCheckUpdate(ObjectGuid plrGuid, LFGRoleCheck const& roleCheck)
{
    Player* pPlayer = sObjectAccessor.FindPlayer(plrGuid);

    if (pPlayer)
    {
        pPlayer->GetSession()->SendLfgRoleCheckUpdate(roleCheck);
    }
}

void LFGMgr::SendLfgUpdate(ObjectGuid plrGuid, LFGPlayerStatus status, bool isGroup)
{
    Player* pPlayer = sObjectAccessor.FindPlayer(plrGuid);

    if (pPlayer)
    {
        pPlayer->GetSession()->SendLfgUpdate(isGroup, status);
    }
}

void LFGMgr::SendLfgJoinResult(ObjectGuid plrGuid, LfgJoinResult result, uint8 detail, partyForbidden const& lockedDungeons)
{
    Player* pPlayer = sObjectAccessor.FindPlayer(plrGuid);

    if (pPlayer)
    {
        pPlayer->GetSession()->SendLfgJoinResult(result, detail, lockedDungeons);
    }
}

void LFGMgr::RemoveOldRoleChecks()
{
    // Erase-safe iteration. m_roleCheckMap is an unordered_map, so erasing by
    // key destroys the node roleItr points at and the following ++roleItr walks
    // freed memory. This is the FIRST thing LFGMgr::Update calls, so it would
    // crash or spin the world thread on the first tick that finds an expired
    // check.
    for (roleCheckMap::iterator roleItr = m_roleCheckMap.begin(); roleItr != m_roleCheckMap.end(); )
    {
        ObjectGuid groupGuid = roleItr->first;

        LFGRoleCheck roleCheck = roleItr->second;
        if ((roleCheck.waitForRoleTime - time(NULL)) <= 0) // no time left
        {
            roleCheck.state = LFG_ROLECHECK_NO_ROLE;

            for (roleMap::iterator roleMapItr = roleCheck.currentRoles.begin(); roleMapItr != roleCheck.currentRoles.end(); ++roleMapItr)
            {
                ObjectGuid plrGuid = roleMapItr->first;

                SetPlayerState(plrGuid, LFG_STATE_NONE);

                SendRoleCheckUpdate(plrGuid, roleCheck);                 // role check failed
                SendLfgUpdate(plrGuid, GetPlayerStatus(plrGuid), true);  // not in lfg system anymore
            }

            // Advance BEFORE erasing, and drop the queue data this check owned:
            // the entries JoinLFG wrote for the group would otherwise survive
            // with nothing left to resolve them.
            m_playerData.erase(groupGuid);
            m_queueSet.erase(groupGuid);
            roleItr = m_roleCheckMap.erase(roleItr);
        }
        else
        {
            ++roleItr;
        }
    }
}
