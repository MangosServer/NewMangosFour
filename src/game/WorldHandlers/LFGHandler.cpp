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
#include "DBCStores.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Log.h"
#include "Player.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#include "World.h"


void WorldSession::HandleLfrJoinOpcode(WorldPacket& recv_data)
{
    MopLfgPackets::LfrSearchRequest request;
    if (!MopLfgPackets::ParseLfrSearchRequest(recv_data, request))
    {
        sLog.outError("WORLD: malformed CMSG_LFG_LFR_JOIN from %s",
            GetPlayerName());
        return;
    }

    LfgDungeonsEntry const* dungeon =
        sLfgDungeonsStore.LookupEntry(request.lfgId);
    if (!dungeon || dungeon->TypeID != request.typeId)
    {
        sLog.outError("WORLD: invalid CMSG_LFG_LFR_JOIN key %u:%u from %s",
            uint32(request.typeId), request.lfgId, GetPlayerName());
        return;
    }

    WorldPacket data(SMSG_LFG_UPDATE_SEARCH, 37);
    MopLfgPackets::BuildEmptyLfrSearchResponse(data, request);
    SendPacket(&data);
}

void WorldSession::HandleLfrLeaveOpcode(WorldPacket& recv_data)
{
    MopLfgPackets::LfrSearchRequest request;
    if (!MopLfgPackets::ParseLfrSearchRequest(recv_data, request))
    {
        sLog.outError("WORLD: malformed CMSG_LFG_LFR_LEAVE from %s",
            GetPlayerName());
        return;
    }

    LfgDungeonsEntry const* dungeon =
        sLfgDungeonsStore.LookupEntry(request.lfgId);
    if (!dungeon || dungeon->TypeID != request.typeId)
    {
        sLog.outError("WORLD: invalid CMSG_LFG_LFR_LEAVE key %u:%u from %s",
            uint32(request.typeId), request.lfgId, GetPlayerName());
        return;
    }

    DEBUG_LOG("WORLD: recognized CMSG_LFG_LFR_LEAVE key %u:%u from %s",
        uint32(request.typeId), request.lfgId, GetPlayerName());
}

void WorldSession::HandleLfgJoinOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_JOIN");

    // The inherited body was the 3.3.5 shape and shared no field with 18414.
    // See MopCompactPackets::ReadLfgJoin for the layout and for why the dungeon
    // count is bounded before anything is read.
    uint8 partyIndex = 0;
    uint32 roles = 0;
    uint32 flag = 0;
    std::string comment;
    std::vector<uint32> dungeons;

    if (!MopCompactPackets::ReadLfgJoin(recv_data, partyIndex, roles, flag, dungeons, comment))
    {
        sLog.outError("Malformed CMSG_LFG_JOIN body from %s: its declared dungeon "
                      "count and comment length do not account for its size.",
                      GetPlayerName());
        return;
    }

    // The high byte of each slot is the LFG type tag, not part of the id.
    for (size_t i = 0; i < dungeons.size(); ++i)
    {
        dungeons[i] &= 0x00FFFFFF;
    }

    DEBUG_LOG("CMSG_LFG_JOIN: %s roles %u, %u dungeon(s), comment %u byte(s).",
              GetPlayerName(), roles, uint32(dungeons.size()), uint32(comment.size()));

    if (dungeons.empty())
    {
        return;
    }

    // The two lines that stood here were commented-out 3.3.5 session sends with
    // the wrong arity for today's signatures; they would not have compiled, let
    // alone worked. The real entry point is LFGMgr::JoinLFG, which had no
    // callers at all, which is why nothing happened when a player queued.
    //
    // Safe to wire now: the matchmaker is reached ONLY through
    // LFGMgr::FindQueueMatches, which is called only from LFGMgr::Update, and
    // nothing ever checks the WUPDATE_LFGMGR timer. So this enters the player
    // into the queue and stops there. It does not wake MergeGroups, whose
    // needed-role arithmetic is still wrong -- LFGMgr.cpp gates role needs on
    // DifficultyID == 0 and no TypeID==1 row in LfgDungeons.dbc carries that,
    // so every entry reports needing nobody and any two would be matched.
    //
    // Known gap, deliberately not hidden: SendLfgJoinResult builds
    // SMSG_LFG_JOIN_RESULT, which is NOT admitted, so a REFUSED join tells the
    // player nothing. Success is unaffected -- SendLfgUpdate goes out over the
    // already-admitted SMSG_LFG_UPDATE_STATUS. The reply is held rather than
    // admitted because the only fixture for its non-empty form is synthetic.
    std::set<uint32> requested(dungeons.begin(), dungeons.end());
    sLFGMgr.JoinLFG(roles, requested, comment, GetPlayer());
}

void WorldSession::HandleLfgLeaveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_LEAVE");

    // The body is a ticket echo -- type, flags, time, queue id and a packed
    // GUID. None of it is authority: the server cancels the CALLER's own queue
    // entry, so the ticket only says what the client believes it is leaving.
    // It is parsed rather than skipped so a malformed body is refused instead
    // of silently cancelling something.
    MopLfgLeavePackets::Request request;
    if (!MopLfgLeavePackets::ParseRequest(recv_data, request))
    {
        return;
    }

    Player* plr = GetPlayer();
    if (!plr)
    {
        return;
    }

    // A grouped player leaves on behalf of the group, which is how the queue
    // stores it -- JoinLFG keys group entries by the GROUP guid.
    Group* pGroup = plr->GetGroup();
    bool const isGroup = pGroup && pGroup->IsLeader(plr->GetObjectGuid());

    sLFGMgr.LeaveLFG(plr, isGroup);
}

void WorldSession::HandleLfgSetRolesOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_SET_ROLES");

    // This is the reply half of the LFG role check, and it had no handler and no
    // registration at all -- the client's answer was dropped at the dispatcher without
    // so much as a log line, so a party entered LFG_STATE_ROLECHECK and stayed there.
    //
    // Note the Lua SetLFGRoles() does NOT send this; it only mutates local state. The
    // packet is emitted by CompleteLFGRoleCheck, i.e. when the player confirms.
    MopLfgSetRolesPackets::Request request;
    if (!MopLfgSetRolesPackets::ParseRequest(recv_data, request))
    {
        sLog.outError("Malformed CMSG_LFG_SET_ROLES body from %s: expected 5 bytes.",
                      GetPlayerName());
        return;
    }

    Player* plr = GetPlayer();
    if (!plr)
    {
        return;
    }

    // A role check only exists for a party. A solo queuer states their roles in
    // CMSG_LFG_JOIN and never reaches this path.
    Group* pGroup = plr->GetGroup();
    if (!pGroup)
    {
        return;
    }

    DEBUG_LOG("CMSG_LFG_SET_ROLES: %s roles 0x%02X.", GetPlayerName(), request.roles);

    // Truncated to the byte the role plumbing uses. The wire field is 32 bits, but only
    // the low four (leader/tank/healer/damage) are ever set; anything above them is
    // rejected by PerformRoleCheck's mask test rather than being silently accepted.
    sLFGMgr.PerformRoleCheck(plr, pGroup, uint8(request.roles & 0xFF));
}

void WorldSession::HandleLfgProposalResponseOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_PROPOSAL_RESPONSE");

    // Without this a proposal could be built and sent but never answered -- the accept
    // and decline buttons both did nothing, because the reply was dropped at the
    // dispatcher with no handler and no registration.
    MopLfgProposalResponsePackets::Request request;
    if (!MopLfgProposalResponsePackets::ParseRequest(recv_data, request))
    {
        sLog.outError("Malformed CMSG_LFG_PROPOSAL_RESPONSE body from %s.", GetPlayerName());
        return;
    }

    Player* plr = GetPlayer();
    if (!plr)
    {
        return;
    }

    DEBUG_LOG("CMSG_LFG_PROPOSAL_RESPONSE: %s %s proposal %u.",
              GetPlayerName(), request.accepted ? "accepted" : "declined", request.proposalId);

    // Answer on behalf of the CALLER, keyed on our own proposal id. The GUIDs and the
    // queue triplet in the body are echoes of what we sent and carry no authority; a
    // client that returns a different guidA must not be able to answer for someone else.
    sLFGMgr.ProposalUpdate(request.proposalId, plr->GetObjectGuid(), request.accepted);
}

void WorldSession::HandleLfgGetStatusOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("CMSG_LFG_GET_STATUS");

    LFGPlayerStatus status = sLFGMgr.GetPlayerStatus(GetPlayer()->GetObjectGuid());
    if (status.state == LFG_STATE_NONE)
        return;

    status.updateType = LFG_UPDATE_STATUS;
    bool const groupFirst = GetPlayer()->GetGroup() != nullptr;
    SendLfgUpdate(groupFirst, status);

    status.dungeonList.clear();
    SendLfgUpdate(!groupFirst, status);
}

void WorldSession::HandleLfgLockInfoRequestOpcode(WorldPacket& recv_data)
{
    bool forPlayer = false;
    if (!MopLfgPackets::ParseLockInfoRequest(recv_data, forPlayer))
    {
        sLog.outError("WORLD: malformed CMSG_LFG_LOCK_INFO_REQUEST from %s",
            GetPlayerName());
        return;
    }

    if (forPlayer)
        SendLfgPlayerLockInfo();
    else
        SendLfgPartyLockInfo();
}

void WorldSession::SendLfgPlayerLockInfo()
{
    // The legacy LFG manager cannot express the 18414 random-dungeon reward
    // records. Send the binary-proven empty shape instead of guessed fields.
    WorldPacket data(SMSG_LFG_PLAYER_INFO, 5);
    MopLfgPackets::BuildEmptyPlayerInfo(data);
    SendPacket(&data);
}

void WorldSession::SendLfgPartyLockInfo()
{
    // No compatible 18414 party lock model exists in this legacy manager.
    WorldPacket data(SMSG_LFG_PARTY_INFO, 3);
    MopLfgPackets::BuildEmptyPartyInfo(data);
    SendPacket(&data);
}

void WorldSession::HandleSetLfgCommentOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_SET_LFG_COMMENT");

    std::string comment;
    recv_data >> comment;
    DEBUG_LOG("LFG comment \"%s\"", comment.c_str());
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
    MopLfgPackets::QueueStatusUpdate update;
    update.queueGuid = status.queueGuid;
    update.queuedTime = status.timeSpentInQueue;
    update.waitTimeAvg = status.avgWaitTime;
    update.waitTimeTank = status.tankAvgWaitTime;
    update.tanks = status.neededTanks;
    update.waitTimeHealer = status.healerAvgWaitTime;
    update.healers = status.neededHeals;
    update.waitTimeDps = status.dpsAvgWaitTime;
    update.dps = status.neededDps;
    update.joinTime = status.joinTime;
    // This legacy single-queue manager has no client queue-ID allocation.
    update.clientQueueId = 0;
    update.waitTime = status.playerAvgWaitTime;
    update.dungeonEntry = sLFGMgr.GetDungeonEntry(status.dungeonID);

    WorldPacket data(SMSG_LFG_QUEUE_STATUS, 52);
    MopLfgPackets::BuildQueueStatus(data, update);

    SendPacket(&data);
}

void WorldSession::SendLfgRoleCheckUpdate(LFGRoleCheck const& roleCheck)
{
    // Rebuilt for 18414. See MopLfgPackets::BuildRoleCheckUpdate for the layout and the
    // two captures it was verified against; the previous body was the 3.3.5 shape and
    // shared no field order with this client, which is why the role check prompt never
    // appeared however correct the server-side state was.
    MopLfgPackets::RoleCheckUpdate update;
    update.state = uint8(roleCheck.state);

    std::set<uint32> dungeons;
    if (roleCheck.randomDungeonID)
    {
        dungeons.insert(roleCheck.randomDungeonID);
    }
    else
    {
        dungeons = roleCheck.dungeonList;
    }

    for (std::set<uint32>::const_iterator it = dungeons.begin(); it != dungeons.end(); ++it)
    {
        update.dungeonEntries.push_back(sLFGMgr.GetDungeonEntry(*it));
    }

    // The leader MUST be first: the client renders entry 0 as the initiator, and both
    // captures show the leader's roles carrying the LEADER bit while later members are
    // still zero.
    ObjectGuid const leaderGuid = ObjectGuid(roleCheck.leaderGuidRaw);

    roleMap::const_iterator leaderItr = roleCheck.currentRoles.find(leaderGuid);
    if (leaderItr != roleCheck.currentRoles.end())
    {
        // Unchecked find() here previously: a role check whose leader had already left
        // dereferenced end().
        MopLfgPackets::RoleCheckMember member;
        member.guid = leaderGuid.GetRawValue();
        member.roles = leaderItr->second;

        Player* pLeader = sObjectAccessor.FindPlayer(leaderGuid);
        member.level = pLeader ? uint8(pLeader->getLevel()) : uint8(0);

        update.members.push_back(member);
    }

    for (roleMap::const_iterator rItr = roleCheck.currentRoles.begin();
         rItr != roleCheck.currentRoles.end(); ++rItr)
    {
        if (rItr->first == leaderGuid)
        {
            continue;
        }

        MopLfgPackets::RoleCheckMember member;
        member.guid = rItr->first.GetRawValue();
        member.roles = rItr->second;

        Player* pPlayer = sObjectAccessor.FindPlayer(rItr->first);
        member.level = pPlayer ? uint8(pPlayer->getLevel()) : uint8(0);

        update.members.push_back(member);
    }

    WorldPacket data(SMSG_LFG_ROLE_CHECK_UPDATE, 16 + update.members.size() * 16);
    MopLfgPackets::BuildRoleCheckUpdate(data, update);

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
    if (!pPlayer)
    {
        return;
    }

    ObjectGuid const plrGuid = pPlayer->GetObjectGuid();

    // find() without checking end() dereferenced a past-the-end iterator here. It is
    // reachable, not theoretical: SendDungeonProposal skips offline players when filling
    // `groups` and `answers` but still lists them in `currentRoles`, so a player who
    // queues, logs out and logs back in arrives with no entry of their own.
    playerGroupMap::const_iterator myGroup = proposal.groups.find(plrGuid);
    if (myGroup == proposal.groups.end())
    {
        return;
    }

    ObjectGuid const plrGroupGuid = myGroup->second;

    // Rebuilt for 18414. See MopLfgPackets::BuildProposalUpdate for the layout and the
    // two captures it was verified against.
    MopLfgPackets::ProposalUpdate update;
    update.dungeonEntry = sLFGMgr.GetDungeonEntry(proposal.dungeonID);
    update.proposalId = proposal.id;
    update.state = uint8(proposal.state);
    update.encounters = proposal.encounters;
    update.joinTime = uint32(proposal.joinedQueue);

    // "silent" suppresses opening a fresh window: the client updates one it already has.
    // Only correct when this is not a new proposal AND the recipient is already in the
    // group the proposal will reuse.
    update.silent = !proposal.isNew && plrGroupGuid &&
                    plrGroupGuid.GetRawValue() == proposal.groupRawGuid;

    // The recipient's own group if they have one, else themselves -- this identifies who
    // the update is about, not the proposed group.
    update.requesterGuid = plrGroupGuid ? plrGroupGuid.GetRawValue() : plrGuid.GetRawValue();

    for (playerGroupMap::const_iterator it = proposal.groups.begin();
         it != proposal.groups.end(); ++it)
    {
        ObjectGuid const memberGuid = it->first;

        roleMap::const_iterator roleItr = proposal.currentRoles.find(memberGuid);
        proposalAnswerMap::const_iterator answerItr = proposal.answers.find(memberGuid);
        if (roleItr == proposal.currentRoles.end() || answerItr == proposal.answers.end())
        {
            continue;
        }

        MopLfgPackets::ProposalPlayer entry;
        entry.roles = roleItr->second;
        entry.isSelf = (memberGuid == plrGuid);
        entry.answered = (answerItr->second != LFG_ANSWER_PENDING);
        entry.agreed = (answerItr->second == LFG_ANSWER_AGREE);
        entry.inProposedGroup = it->second && !proposal.isNew &&
                                it->second.GetRawValue() == proposal.groupRawGuid;
        entry.sameGroupAsSelf = it->second && it->second == plrGroupGuid;

        update.players.push_back(entry);
    }

    WorldPacket data(SMSG_LFG_PROPOSAL_UPDATE, 40 + update.players.size() * 5);
    MopLfgPackets::BuildProposalUpdate(data, update);

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
