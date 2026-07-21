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
 * @file LFGHandler.cpp
 * @brief Looking For Group (Meeting Stone) opcode handlers
 *
 * This file handles player interactions with meeting stones (LFG system).
 * Meeting stones allow players/groups to queue for dungeons and be matched
 * with other players automatically.
 *
 * Opcodes handled:
 * - CMSG_MEETINGSTONE_JOIN: Join LFG queue at a meeting stone
 * - CMSG_MEETINGSTONE_LEAVE: Leave LFG queue
 * - CMSG_MEETINGSTONE_INFO: Request current queue status
 *
 * @see LFGMgr for the queue management implementation
 * @see LFGQueue for matching algorithm
 */

#include "WorldSession.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Player.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#include "World.h"
#include "Server/MopLfgPackets.h"

void WorldSession::HandleLfgJoinOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_JOIN");

    uint8 dungeonsCount, counter2;
    std::string comment;
    std::vector<uint32> dungeons;

    recv_data >> Unused<uint32>();                          // lfg roles
    recv_data >> Unused<uint8>();                           // lua: GetLFGInfoLocal
    recv_data >> Unused<uint8>();                           // lua: GetLFGInfoLocal

    recv_data >> dungeonsCount;

    dungeons.resize(dungeonsCount);

    for (uint8 i = 0; i < dungeonsCount; ++i)
    {
        recv_data >> dungeons[i];                           // dungeons id/type
    }

    recv_data >> counter2;                                  // const count = 3, lua: GetLFGInfoLocal

    for (uint8 i = 0; i < counter2; ++i)
    {
        recv_data >> Unused<uint8>();                       // lua: GetLFGInfoLocal
    }

    recv_data >> comment;                                   // lfg comment

    // SendLfgJoinResult(ERR_LFG_OK);
    // SendLfgUpdate(false, LFG_UPDATE_JOIN, dungeons[0]);
}

void WorldSession::HandleLfgLeaveOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("CMSG_LFG_LEAVE");

    // SendLfgUpdate(false, LFG_UPDATE_LEAVE, 0);
}

void WorldSession::HandleSearchLfgJoinOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_SEARCH_JOIN");

    uint32 temp, entry;
    recv_data >> temp;

    entry = (temp & 0x00FFFFFF);
    // LfgType type = LfgType((temp >> 24) & 0x000000FF);

    // SendLfgSearchResults(type, entry);
}

void WorldSession::HandleSearchLfgLeaveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_SEARCH_LEAVE");

    recv_data >> Unused<uint32>();                          // join id?
}

void WorldSession::HandleSetLfgCommentOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_SET_LFG_COMMENT");

    std::string comment;
    recv_data >> comment;
    DEBUG_LOG("LFG comment \"%s\"", comment.c_str());
}

void WorldSession::SendLfgSearchResults(LfgType type, uint32 entry)
{
    WorldPacket data(SMSG_LFG_SEARCH_RESULTS);
    data << uint32(type);                                   // type
    data << uint32(entry);                                  // entry from LFGDungeons.dbc

    uint8 isGuidsPresent = 0;
    data << uint8(isGuidsPresent);
    if (isGuidsPresent)
    {
        uint32 guids_count = 0;
        data << uint32(guids_count);
        for (uint32 i = 0; i < guids_count; ++i)
        {
            data << uint64(0);                              // player/group guid
        }
    }

    uint32 groups_count = 1;
    data << uint32(groups_count);                           // groups count
    data << uint32(groups_count);                           // groups count (total?)

    for (uint32 i = 0; i < groups_count; ++i)
    {
        data << uint64(1);                                  // group guid

        uint32 flags = 0x92;
        data << uint32(flags);                              // flags

        if (flags & 0x2)
        {
            data << uint8(0);                               // comment string, max len 256
        }

        if (flags & 0x10)
        {
            for (uint32 j = 0; j < 3; ++j)
            {
                data << uint8(0);                           // roles
            }
        }

        if (flags & 0x80)
        {
            data << uint64(0);                              // instance guid
            data << uint32(0);                              // completed encounters
        }
    }

    size_t pl_count_pos = data.wpos();
    data << uint32(0);                            // players count

    size_t tpl_count_pos = data.wpos();
    data << uint32(0);                            // players count (total?)

    uint32 playerCount = 0;

    sObjectAccessor.DoForAllPlayers([this, &playerCount, &data](Player* plr)->void
    {
        ++playerCount;
        if (plr && (plr->GetTeam() == _player->GetTeam()) && plr->IsInWorld())
        {
            data << plr->GetObjectGuid();                       // guid

            uint32 flags = 0xFF;
            data << uint32(flags);                              // flags

            if (flags & 0x1)
            {
                data << uint8(plr->getLevel());
                data << uint8(plr->getClass());
                data << uint8(plr->getRace());

                for (uint32 i = 0; i < 3; ++i)
                {
                    data << uint8(0);                           // talent spec x/x/x
                }

                data << uint32(0);                              // armor
                data << uint32(0);                              // spd/heal
                data << uint32(0);                              // spd/heal
                data << uint32(0);                              // HasteMelee
                data << uint32(0);                              // HasteRanged
                data << uint32(0);                              // HasteSpell
                data << float(0);                               // MP5
                data << float(0);                               // MP5 Combat
                data << uint32(0);                              // AttackPower
                data << uint32(0);                              // Agility
                data << uint32(0);                              // Health
                data << uint32(0);                              // Mana
                data << uint32(0);                              // Unk1
                data << float(0);                               // Unk2
                data << uint32(0);                              // Defence
                data << uint32(0);                              // Dodge
                data << uint32(0);                              // Block
                data << uint32(0);                              // Parry
                data << uint32(0);                              // Crit
                data << uint32(0);                              // Expertise
            }

            if (flags & 0x2)
            {
                data << "";                                     // comment
            }

            if (flags & 0x4)
            {
                data << uint8(0);                               // group leader
            }

            if (flags & 0x8)
            {
                data << uint64(1);                              // group guid
            }

            if (flags & 0x10)
            {
                data << uint8(0);                               // roles
            }

            if (flags & 0x20)
            {
                data << uint32(plr->GetZoneId());               // areaid
            }

            if (flags & 0x40)
            {
                data << uint8(0);                               // status
            }

            if (flags & 0x80)
            {
                data << uint64(0);                              // instance guid
                data << uint32(0);                              // completed encounters
            }
        }
    });

    data.put(pl_count_pos, playerCount);
    data.put(tpl_count_pos, playerCount);

    SendPacket(&data);
}

void WorldSession::SendLfgJoinResult(LfgJoinResult result, LFGState state, partyForbidden const& lockedDungeons)
{
    uint32 packetSize = 0;
    for (partyForbidden::const_iterator it = lockedDungeons.begin(); it != lockedDungeons.end(); ++it)
    {
        packetSize += 12 + uint32(it->second.size()) * 8;
    }

    WorldPacket data(SMSG_LFG_JOIN_RESULT, packetSize);
    data << uint32(result);
    data << uint32(state);

    if (!lockedDungeons.empty())
    {
        for (partyForbidden::const_iterator it = lockedDungeons.begin(); it != lockedDungeons.end(); ++it)
        {
            dungeonForbidden dungeonInfo = it->second;

            data << uint64(it->first); // object guid of player
            data << uint32(dungeonInfo.size()); // amount of their locked dungeons

            for (dungeonForbidden::iterator itr = dungeonInfo.begin(); itr != dungeonInfo.end(); ++itr)
            {
                data << uint32(itr->first); // dungeon entry
                data << uint32(itr->second); // reason for dungeon being forbidden/locked
            }
        }
    }

    SendPacket(&data);
}

void WorldSession::SendLfgUpdate(bool isGroup, LFGPlayerStatus status)
{
    bool joined = false;
    bool isQueued = false;

    switch (status.updateType)
    {
    case LFG_UPDATE_JOIN:
    case LFG_UPDATE_ADDED_TO_QUEUE:
        joined = true;
        isQueued = true;
        break;
    case LFG_UPDATE_PROPOSAL_BEGIN:
        joined = true;
        break;
    case LFG_UPDATE_STATUS:
        isQueued = (status.state == LFG_STATE_QUEUED);
        joined = status.state != LFG_STATE_NONE;
        break;
    default:
        break;
    }

    ObjectGuid const playerGuid = GetPlayer()->GetObjectGuid();
    ObjectGuid queueGuid = playerGuid;
    if (isGroup && GetPlayer()->GetGroup())
        queueGuid = GetPlayer()->GetGroup()->GetObjectGuid();

    LFGStatusPacketData queueData;
    sLFGMgr.GetStatusPacketData(queueGuid, playerGuid, queueData);

    MopLfgPackets::StatusUpdate update;
    update.requesterGuid = queueGuid.GetRawValue();
    update.comment = status.comment;
    update.needs = {{ queueData.neededTanks, queueData.neededHealers, queueData.neededDps }};
    update.isParty = isGroup;
    update.joined = joined;
    update.lfgJoined = status.updateType != LFG_UPDATE_LEAVE;
    update.queued = isQueued;
    update.requestedRoles = queueData.roles;
    update.updateReason = uint8(status.updateType);
    // This legacy single-queue manager does not track the client queue ID.
    update.ticketId = 0;
    update.ticketTime = queueData.joinedTime;

    if (!status.dungeonList.empty())
        update.dungeonCategory = sLFGMgr.GetDungeonCategory(*status.dungeonList.begin());

    for (std::set<uint32>::const_iterator it = status.dungeonList.begin(); it != status.dungeonList.end(); ++it)
        update.dungeonEntries.push_back(sLFGMgr.GetDungeonEntry(*it));

    WorldPacket data(SMSG_LFG_UPDATE_STATUS, 40 + status.comment.size() + update.dungeonEntries.size() * sizeof(uint32));
    if (MopLfgPackets::BuildUpdateStatus(data, update))
        SendPacket(&data);
    else
        sLog.outError("WORLD: LFG status fields exceed SMSG_LFG_UPDATE_STATUS wire limits");
}

void WorldSession::SendLfgQueueStatus(LFGQueueStatus const& status)
{
    WorldPacket data(SMSG_LFG_QUEUE_STATUS, 31);

    data << uint32(status.dungeonID);
    data << int32(status.playerAvgWaitTime);
    data << int32(status.avgWaitTime);
    data << int32(status.tankAvgWaitTime);
    data << int32(status.healerAvgWaitTime);
    data << int32(status.dpsAvgWaitTime);
    data << uint8(status.neededTanks);
    data << uint8(status.neededHeals);
    data << uint8(status.neededDps);
    data << uint32(status.timeSpentInQueue);

    SendPacket(&data);
}

void WorldSession::SendLfgRoleCheckUpdate(LFGRoleCheck const& roleCheck)
{
    WorldPacket data(SMSG_LFG_ROLE_CHECK_UPDATE);

    data << uint32(roleCheck.state);
    data << uint8(roleCheck.state == LFG_ROLECHECK_INITIALITING);

    std::set<uint32> dungeons;
    if (roleCheck.randomDungeonID)
    {
        dungeons.insert(roleCheck.randomDungeonID);
    }
    else
    {
        dungeons = roleCheck.dungeonList;
    }

    data << uint8(dungeons.size());
    if (!dungeons.empty())
        for (std::set<uint32>::iterator it = dungeons.begin(); it != dungeons.end(); ++it)
        {
            data << uint32(sLFGMgr.GetDungeonEntry(*it));
        }

    data << uint8(roleCheck.currentRoles.size());
    if (!roleCheck.currentRoles.empty())
    {
        ObjectGuid leaderGuid = ObjectGuid(roleCheck.leaderGuidRaw);
        uint8 leaderRoles = roleCheck.currentRoles.find(leaderGuid)->second;
        Player* pLeader = sObjectAccessor.FindPlayer(leaderGuid);

        data << uint64(leaderGuid.GetRawValue());
        data << uint8(leaderRoles > 0);
        data << uint32(leaderRoles);
        data << uint8(pLeader ? pLeader->getLevel() : 0);

        for (roleMap::const_iterator rItr = roleCheck.currentRoles.begin(); rItr != roleCheck.currentRoles.end(); ++rItr)
        {
            if (rItr->first == leaderGuid)
            {
                continue; // exclude the leader
            }

            ObjectGuid plrGuid = rItr->first;

            Player* pPlayer = sObjectAccessor.FindPlayer(plrGuid);

            data << uint64(plrGuid.GetRawValue());
            data << uint8(rItr->second > 0);
            data << uint32(rItr->second);
            data << uint8(pPlayer ? pPlayer->getLevel() : 0);
        }
    }

    SendPacket(&data);
}

void WorldSession::SendLfgRoleChosen(uint64 rawGuid, uint8 roles)
{
    WorldPacket data(SMSG_ROLE_CHOSEN, 13);
    data << uint64(rawGuid);
    data << uint8(roles > 0);
    data << uint32(roles);
    SendPacket(&data);
}

void WorldSession::SendLfgProposalUpdate(LFGProposal const& proposal)
{
    Player* pPlayer = GetPlayer();
    ObjectGuid plrGuid = pPlayer->GetObjectGuid();
    ObjectGuid plrGroupGuid = proposal.groups.find(plrGuid)->second;

    uint32 dungeonEntry = sLFGMgr.GetDungeonEntry(proposal.dungeonID);
    bool showProposal = !proposal.isNew && proposal.groupRawGuid == plrGroupGuid.GetRawValue();

    WorldPacket data(SMSG_LFG_PROPOSAL_UPDATE, 15 + (9 * proposal.currentRoles.size()));

    data << uint32(dungeonEntry);                // Dungeon Entry
    data << uint8(proposal.state);               // Proposal state
    data << uint32(proposal.id);                 // ID of proposal
    data << uint32(proposal.encounters);         // Encounters done
    data << uint8(showProposal);                 // Show or hide proposal window [todo-this]
    data << uint8(proposal.currentRoles.size()); // Size of group

    for (playerGroupMap::const_iterator it = proposal.groups.begin(); it != proposal.groups.end(); ++it)
    {
        ObjectGuid grpPlrGuid = it->first;
        uint8 grpPlrRole = proposal.currentRoles.find(grpPlrGuid)->second;
        LFGProposalAnswer grpPlrAnswer = proposal.answers.find(grpPlrGuid)->second;

        data << uint32(grpPlrRole);              // Player's role
        data << uint8(grpPlrGuid == plrGuid);    // Is this player me?

        if (it->second != 0)
        {
            data << uint8(it->second == ObjectGuid(proposal.groupRawGuid)); // Is player in the proposed group?
            data << uint8(it->second == plrGroupGuid);          // Is player in the same group as myself?
        }
        else
        {
            data << uint8(0);
            data << uint8(0);
        }

        data << uint8(grpPlrAnswer != LFG_ANSWER_PENDING);  // Has the player selected an answer?
        data << uint8(grpPlrAnswer == LFG_ANSWER_AGREE);    // Has the player agreed to do the dungeon?
    }
    SendPacket(&data);
}

void WorldSession::SendLfgTeleportError(uint8 error)
{
    DEBUG_LOG("SMSG_LFG_TELEPORT_DENIED");
    WorldPacket data(SMSG_LFG_TELEPORT_DENIED, 4);
    data << uint32(error);
    SendPacket(&data);
}

void WorldSession::SendLfgRewards(LFGRewards const& rewards)
{
    DEBUG_LOG("SMSG_LFG_PLAYER_REWARD");

    WorldPacket data(SMSG_LFG_PLAYER_REWARD, 42);
    data << uint32(rewards.randomDungeonEntry);
    data << uint32(rewards.groupDungeonEntry);
    data << uint8(rewards.hasDoneDaily);
    data << uint32(1);
    data << uint32(rewards.moneyReward);
    data << uint32(rewards.expReward);
    data << uint32(0);
    data << uint32(0);
    if (rewards.itemID != 0)
    {
        ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(rewards.itemID);
        if (pProto)
        {
            data << uint8(1);
            data << uint32(rewards.itemID);
            data << uint32(pProto->DisplayInfoID);
            data << uint32(rewards.itemAmount);
        }
    }
    else
    {
        data << uint8(0);
    }
    SendPacket(&data);
}

void WorldSession::SendLfgBootUpdate(LFGBoot const& boot)
{
    DEBUG_LOG("SMSG_LFG_BOOT_PLAYER (5.4.8)");

    ObjectGuid plrGuid = GetPlayer()->GetObjectGuid();
    LFGProposalAnswer plrAnswer = boot.answers.find(plrGuid)->second;

    uint32 voteCount = 0, yayCount = 0;
    for (proposalAnswerMap::const_iterator it = boot.answers.begin(); it != boot.answers.end(); ++it)
    {
        if (it->second != LFG_ANSWER_PENDING)
        {
            ++voteCount;
            if (it->second == LFG_ANSWER_AGREE)
            {
                ++yayCount;
            }
        }
    }

    time_t const expires = boot.startTime + LFG_TIME_BOOT;
    time_t const now = time(NULL);

    MopLfgPackets::BootUpdate update;
    update.victimGuid = boot.playerVotedOn.GetRawValue();
    update.reason = boot.reason;
    update.inProgress = boot.inProgress;
    update.didVote = plrAnswer != LFG_ANSWER_PENDING;
    update.votePassed = yayCount >= REQUIRED_VOTES_FOR_BOOT;
    update.agree = plrAnswer == LFG_ANSWER_AGREE;
    update.votesNeeded = REQUIRED_VOTES_FOR_BOOT;
    update.timeLeft = expires > now ? uint32(expires - now) : 0;
    update.agreeCount = yayCount;
    update.voteCount = voteCount;

    WorldPacket data(SMSG_LFG_BOOT_PLAYER, 30 + boot.reason.length());
    if (MopLfgPackets::BuildBootPlayer(data, update))
        SendPacket(&data);
    else
        sLog.outError("WORLD: LFG boot reason is too long for SMSG_LFG_BOOT_PLAYER");
}
