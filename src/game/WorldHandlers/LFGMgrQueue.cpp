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
#include <sstream>

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
    // Gated on a proposal that ACTUALLY EXISTS, not on the status flag alone.
    //
    // The flag is written in several places and cleared in fewer, so trusting it meant
    // any path that failed to reset it locked the player out of the dungeon finder until
    // relog -- which is exactly what happened when the success path forgot to tear the
    // queue entry down. Asking m_proposalMap directly cannot go stale: if there is no
    // live proposal listing this player, there is nothing to protect.
    if (HasLiveProposalFor(plr->GetObjectGuid()))
    {
        partyForbidden noneForbidden;
        plr->GetSession()->SendLfgJoinResult(ERR_LFG_NO_LFG_OBJECT, LFG_JOIN_DETAIL_NONE, noneForbidden);
        return;
    }

    // Keyed on whichever entry LISTS this player, not on their own guid.
    //
    // A solo queuer already absorbed into somebody else's entry has no m_playerData
    // under their own guid, so this lookup missed, the duplicate cleanup below was
    // skipped, and the solo branch built a SECOND live entry while the merged one still
    // listed them -- two queue entries for one player, and potentially two proposals.
    ObjectGuid const existingEntryGuid = pGroup ? guid : FindQueueEntryContaining(guid);
    LFGPlayers* currentInfo = existingEntryGuid ? GetPlayerOrPartyData(existingEntryGuid) : nullptr;

    // check if we actually have info on the player/group right now
    if (currentInfo)
    {
        bool groupCurrentlyInDungeon = pGroup && pGroup->isLFGGroup() && currentInfo->currentState != LFG_STATE_FINISHED_DUNGEON;

        // are they already queued?
        if (currentInfo->currentState == LFG_STATE_QUEUED)
        {
            // Take them out of whatever they are in now so they can join this instead.
            // RemovePlayerFromQueue rather than a bare m_queueSet.erase, because the
            // entry may be shared with other players who must stay queued.
            RemovePlayerFromQueue(guid);
            currentInfo = nullptr;
        }

        // are they already in a dungeon?
        if (currentInfo && groupCurrentlyInDungeon)
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

        // Diagnostic: what survived the eligibility filter, and what was removed.
        {
            std::ostringstream kept;
            for (std::set<uint32>::const_iterator it = dungeons.begin(); it != dungeons.end(); ++it)
            {
                kept << (it == dungeons.begin() ? "" : ",") << *it;
            }

            partyForbidden::const_iterator lockedFor = partyLockedDungeons.find(guid);

            DEBUG_LOG("LFG JoinLFG: %s isRandom=%u randomId=%u kept={%s} lockedCount=%u",
                      plr->GetName(), uint32(isRandom), randomDungeonID, kept.str().c_str(),
                      uint32(lockedFor != partyLockedDungeons.end() ? lockedFor->second.size() : 0));
        }

        if (!dungeons.empty())
        {
            partyLockedDungeons.clear();
        }
        else
        {
            // NO_SLOTS_PLAYER for a party too. 18414 has no NO_SLOTS_PARTY code: the
            // GlobalString survives at index 0x2EE but nothing in the client's result
            // table maps to it, so the old party value (0x06) matched no entry and the
            // client displayed nothing at all -- the most common way to queue and see
            // the button do nothing. 0x20 is also the code that carries the per-player
            // lock array, so the party is told WHICH dungeons were locked.
            result = ERR_LFG_NO_SLOTS_PLAYER;
        }
    }

    // If our result is not ERR_LFG_OK, send join result now with err message
    if (result != ERR_LFG_OK)
    {
        plr->GetSession()->SendLfgJoinResult(result, LFG_JOIN_DETAIL_NONE, partyLockedDungeons);
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
        //
        // The expansion is SAVED first. dungeonList must go back to the single category row --
        // that is what the client is shown and what the reward lookup keys on -- but a category
        // row is not a place: all 12 TypeID 6 rows carry MapID 0 or 0xFFFFFFFF. Discarding the
        // expansion here is what left a random queue proposing a row it could not teleport to.
        std::set<uint32> candidates;
        if (isRandom)
        {
            candidates = dungeons;
            dungeons.clear();
            dungeons.insert(randomDungeonID);
        }

        for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            if (Player* pGroupPlr = itr->getSource())
            {
                // ROLECHECK, not NONE -- same reason as the solo path below. The update
                // announced the join while reporting the state as NONE, and only moved to
                // ROLECHECK for the stored copy afterwards.
                // Reason 24, not 6 -- same correction the solo path already carries.
                // Reason 6 is retail's re-queue-from-inside-a-dungeon reason (257 of 276
                // observed joins open with 24 and none with 6), and BOTH 6 and 13 make the
                // client display ERR_LFG_JOINED_QUEUE. Opening with 6 and then having
                // PerformRoleCheck send 13 announced "You are now queued in the Dungeon
                // Finder" TWICE -- once in chat and once centre-screen. Observed live.
                LFGPlayerStatus overallStatus(LFG_STATE_ROLECHECK, LFG_UPDATE_JOIN_QUEUE_INITIAL, dungeons, comments);

                pGroupPlr->GetSession()->SendLfgUpdate(true, overallStatus);

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
        groupInfo.candidateDungeons = candidates;
        groupInfo.ticketId = AllocateTicketId();
        m_playerData[guid] = groupInfo;

        PerformRoleCheck(plr, pGroup, (uint8)roles);
    }
    else
    {
        // place original dungeon ID back in the set -- expansion saved first, as above
        std::set<uint32> candidates;
        if (isRandom)
        {
            candidates = dungeons;
            dungeons.clear();
            dungeons.insert(randomDungeonID);
        }

        // set up a role map and then an lfgplayer struct
        roleMap playerRole;
        playerRole[guid] = (uint8)roles;

        {
            std::ostringstream stored;
            for (std::set<uint32>::const_iterator it = dungeons.begin(); it != dungeons.end(); ++it)
            {
                stored << (it == dungeons.begin() ? "" : ",") << *it;
            }
            DEBUG_LOG("LFG JoinLFG: solo entry for %s stores dungeons={%s}",
                      plr->GetName(), stored.str().c_str());
        }

        LFGPlayers playerInfo(LFG_STATE_QUEUED, dungeons, playerRole, comments, false, time(NULL), 0, 0, 0);
        playerInfo.candidateDungeons = candidates;
        playerInfo.ticketId = AllocateTicketId();
        m_playerData[guid] = playerInfo;

        // set up a status struct for client requests/updates
        //
        // QUEUED, not NONE. This used to announce the join while reporting the player's LFG
        // state as LFG_STATE_NONE, and only correct it to QUEUED afterwards for storage -- so
        // the packet said "you have joined the dungeon finder" and "you are not in the dungeon
        // finder" at the same time, and the client had no active queue to announce. Observed
        // live: pressing Queue produced no notification at all, and the only one the player
        // ever saw was a stale status replayed after they had already entered the dungeon.
        LFGPlayerStatus plrStatus;
        plrStatus.updateType  = LFG_UPDATE_JOIN_QUEUE_INITIAL;
        plrStatus.state = LFG_STATE_QUEUED;
        plrStatus.dungeonList = dungeons;
        plrStatus.comment = comments;

        // Retail's join burst, in this order (capture-000720 seq 182-185, and the same
        // shape at capture-000044 seq 3601-3605):
        //
        //   1. SMSG_LFG_UPDATE_STATUS reason 24, queued = 0
        //   2. SMSG_LFG_UPDATE_STATUS reason 13, queued = 1
        //   3. SMSG_LFG_JOIN_RESULT
        //   4. SMSG_LFG_UPDATE_STATUS reason 13 again -- a byte-identical duplicate of 2
        //
        // We used to lead with the join result and send a single status packet. The
        // opening reason was 6, which retail uses for re-queueing from INSIDE a dungeon
        // and never to open a fresh queue: 257 of 276 observed joins lead with 24.
        //
        // Step 4 is not a mistake in the capture. Retail repeats reason 13 either side
        // of the join result in every session walked.
        plr->GetSession()->SendLfgUpdate(false, plrStatus);

        plrStatus.updateType = LFG_UPDATE_ADDED_TO_QUEUE;
        plr->GetSession()->SendLfgUpdate(false, plrStatus);

        plr->GetSession()->SendLfgJoinResult(result, LFG_JOIN_DETAIL_NONE, partyLockedDungeons);

        plr->GetSession()->SendLfgUpdate(false, plrStatus);

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

                if (grpPlrStatus.state == LFG_STATE_ROLECHECK)
                {
                    // A role check in progress is aborted rather than answered with a
                    // leave; PerformRoleCheck tells everyone and tears the check down.
                    PerformRoleCheck(NULL, pGroup, 0);
                }
                else
                {
                    // ALWAYS answer, whatever state is recorded.
                    //
                    // This was a switch over four states with no default, so any other
                    // state -- above all LFG_STATE_NONE, which is what a player holds
                    // after declining a proposal -- fell through and sent the client
                    // NOTHING. Observed live in an isolated repro: press I, accept the
                    // backfill, decline the proposal, then click Leave Queue fourteen
                    // times and receive not one packet in reply, because a player who is
                    // in an LFG party takes this branch rather than the solo one.
                    //
                    // The solo path already always answers. The two must not disagree:
                    // which branch runs depends only on whether the player happens to be
                    // grouped, which is not something the client can reason about when it
                    // asks to leave.
                    //
                    // Retail's pair is reason 14 then reason 8, but 14 carries joined = 1
                    // and the client files a status body by category, derived from the
                    // dungeon list -- so with an empty list it is only safe to send the
                    // terminal. 0 of 5291 retail bodies carry an empty dungeon list.
                    if (!grpPlrStatus.dungeonList.empty())
                    {
                        grpPlrStatus.updateType = LFG_UPDATE_PROPOSAL_BEGIN;
                        SendLfgUpdate(grpPlrGuid, grpPlrStatus, true);
                    }

                    grpPlrStatus.updateType = LFG_UPDATE_LEAVE;
                    grpPlrStatus.state = LFG_STATE_NONE;
                    SendLfgUpdate(grpPlrGuid, grpPlrStatus, true);

                    SetPlayerState(grpPlrGuid, LFG_STATE_NONE);
                }

                // Same hazard as the solo path: a party member may be listed in an
                // entry keyed by something other than their own guid.
                RemovePlayerFromQueue(grpPlrGuid);
            }
        }

        m_queueSet.erase(grpGuid);
        m_playerData.erase(grpGuid);
    }
    else
    {
        ObjectGuid plrGuid = plr->GetObjectGuid();

        // Snapshot BEFORE the teardown. CancelProposal erases m_playerStatusMap for the
        // players it blames, so reading the status afterwards handed back a default-
        // constructed record and the reply went out with an empty dungeon list -- a shape
        // that occurs 0 times in 5291 retail bodies.
        LFGPlayerStatus plrStatus = GetPlayerStatus(plrGuid);

        // Tear down a proposal the player never answered, so the queue entry it was built
        // from is released and cannot re-propose on the next tick. Note this may itself
        // send the player an LFG_UPDATE_LEAVE.
        bool const hadLiveProposal = HasLiveProposalFor(plrGuid);
        CancelProposalsFor(plrGuid);
        if (hadLiveProposal)
        {
            // CancelProposal already answered with a properly populated LEAVE. Sending a
            // second one here only adds a duplicate, so stop.
            RemovePlayerFromQueue(plrGuid);
            return;
        }

        // ALWAYS answer, whatever state we have recorded.
        //
        // This used to switch on the recorded state and simply fall through when it
        // matched nothing, sending the client no packet at all -- and a CMSG_LFG_LEAVE
        // that draws no reply leaves the dungeon finder showing a queue the player
        // cannot dismiss. Observed live: four leave requests in a row, zero packets sent,
        // stuck until relog.
        //
        // The state need not be one this switch ever knew about. TryFormGroup erases the
        // queue entry the moment a proposal goes out, so a player who ignores the popup
        // holds a status that matched none of the old cases; GetPlayerStatus then hands
        // back a default-constructed LFG_STATE_NONE for anyone with no record at all.
        // Neither is a reason to say nothing -- the client asked to leave, so tell it that
        // it has, and reconcile the server side underneath.
        //
        // Retail's pair is reason 14 then reason 8 -- but ONLY when we can name the
        // dungeons. Reason 14 carries joined = 1, and the client files a status body by
        // category, which it derives from the dungeon list. With an empty list it cannot
        // attribute either packet to a category, so the joined = 1 can stick where the
        // clearing packet does not reach: GetLFGMode then answers "suspended", which is
        // non-nil, and QueueStatusFrame_Update lights the minimap eye for any non-nil
        // mode. Observed live -- every click played the leave sound and left the eye on.
        //
        // 0 of 5291 retail status bodies carry an empty dungeon list, so the empty form
        // is outside anything the client is built to handle. When we have no record,
        // send the terminal reason 8 alone: it still answers the request and still fires
        // the notification, without first asserting a joined state we cannot then clear.
        if (!plrStatus.dungeonList.empty())
        {
            plrStatus.updateType = LFG_UPDATE_PROPOSAL_BEGIN;
            SendLfgUpdate(plrGuid, plrStatus, false);
        }

        plrStatus.updateType = LFG_UPDATE_LEAVE;
        plrStatus.state = LFG_STATE_NONE;
        SendLfgUpdate(plrGuid, plrStatus, false);

        SetPlayerState(plrGuid, LFG_STATE_NONE);

        // NOT `m_playerData.erase(plrGuid)`.
        //
        // A solo queuer who has already been merged into somebody else's entry has no
        // data under their own guid, so erasing by it did nothing at all: the client was
        // told LFG_UPDATE_LEAVE and cleared its UI while the server kept them queued
        // inside the merged entry -- and would have pulled them into a later proposal
        // for a dungeon they had left.
        RemovePlayerFromQueue(plrGuid);
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

    // Is the caller ALREADY STANDING IN an LFG dungeon?
    //
    // Two distinct things arrive as an ordinary CMSG_LFG_JOIN from inside a run, and the
    // Dungeon Cooldown must gate neither:
    //
    //   1. Backfill -- "A player has left your group. Would you like to find another
    //      player to finish X?" -- replacing a member in the run they are in.
    //   2. The leader re-queueing whatever is LEFT of a short-handed group for a
    //      DIFFERENT dungeon, which retail also allows.
    //
    // The test is therefore "already inside", not "same dungeon": case 2 names a
    // different dungeon entirely and must still be waived.
    //
    // Dungeon Cooldown exists to stop a player re-queuing for a new dungeon straight
    // after ENTERING one from the outside. Applied to either case above it means the
    // moment anyone leaves, the remaining players are refused both a replacement and a
    // fresh run, and the group is simply dead -- exactly the complaint players had, and
    // reproduced here as soon as the cooldown was wired up. The aura is invisible
    // (confirmed by applying 71328 by hand: no icon at all), so the refusal arrives with
    // nothing on the UI to explain it.
    //
    // Deserter is deliberately NOT waived. Its whole point is that the deserter cannot
    // use the finder at all for its duration, wherever they happen to be standing.
    bool alreadyInLfgDungeon = false;
    if (pGroup && pGroup->isLFGGroup())
    {
        if (LFGGroupStatus const* groupStatus = GetGroupStatus(pGroup->GetObjectGuid()))
        {
            LfgDungeonsEntry const* runDungeon = sLfgDungeonsStore.LookupEntry(groupStatus->dungeonID);
            alreadyInLfgDungeon = runDungeon && plr->GetMapId() == uint32(runDungeon->MapID);
        }
    }

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
    else if (!alreadyInLfgDungeon && plr->HasAura(LFG_COOLDOWN_SPELL))
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
                    else if (!alreadyInLfgDungeon && pGroupPlr->HasAura(LFG_COOLDOWN_SPELL))
                    {
                        // Waived for every member, not just the caller: they all entered
                        // together, so they all hold the same cooldown, and gating on any
                        // one of them refuses the backfill just as surely.
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
        ObjectGuid const containing = FindQueueEntryContaining(playerGuid);
        if (containing)
        {
            queue = m_playerData.find(containing);
        }
    }

    if (queue == m_playerData.end())
        return false;

    LFGPlayers const& information = queue->second;
    roleMap::const_iterator role = information.currentRoles.find(playerGuid);
    if (role != information.currentRoles.end())
        data.roles = role->second;

    data.joinedTime = uint32(information.joinedTime);
    data.ticketId = information.ticketId;
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
