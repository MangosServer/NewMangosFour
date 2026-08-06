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
    // SMSG_LFG_JOIN_RESULT is now built to the 18414 layout and admitted, so a
    // refused join reaches the player. See MopLfgPackets::BuildJoinResult for the
    // three captures it is pinned to.
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

    // A grouped player leaves on behalf of the group, which is how the queue stores it
    // -- JoinLFG keys group entries by the GROUP guid.
    //
    // The test used to be `pGroup && pGroup->IsLeader(...)`, which sent a non-leader
    // down the SOLO branch. That branch erases m_playerData[playerGuid], and for a
    // grouped queuer no such entry exists: the party's real entry, keyed by the group
    // guid, was left in the queue untouched while the client was told it had left.
    // Whether a non-leader may cancel for the party is a permission question, answered
    // in LeaveLFG, not a reason to cancel the wrong thing.
    Group* pGroup = plr->GetGroup();

    sLFGMgr.LeaveLFG(plr, pGroup != nullptr);
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

    // Exactly ONE packet, with the dungeon list PRESENT.
    //
    // This used to send a second copy with dungeonList.clear(). No such body exists in
    // retail traffic: 0 of 5291 observed SMSG_LFG_UPDATE_STATUS carry an empty dungeon
    // list. It was the old 3.3.5 UPDATE_PARTY/UPDATE_PLAYER pair, and 5.4.8 has a
    // single opcode. Retail's reply to the zone-in probe is one reason-15 body that
    // still lists the dungeons (capture-000720 seq 1286, reproduced at capture-000044
    // seq 6354, capture-000656 seq 113708 and capture-000872 seq 14299).
    //
    // The LFG_STATE_NONE early return above is also correct and must stay: 1598 of 2144
    // GET_STATUS probes draw no reply at all, and none of the 504 post-completion or
    // post-leave probes do.
    status.updateType = LFG_UPDATE_STATUS;
    SendLfgUpdate(GetPlayer()->GetGroup() != nullptr, status);
}

void WorldSession::HandleLfgTeleportOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_TELEPORT");

    // The body is ONE BIT, MSB-first, not a uint8. All 47 corpus events are a single
    // byte carrying only 0x80 or 0x00, and the destination map of the SMSG_TRANSFER_PENDING
    // that follows classifies them: 0x80 precedes a move to an outdoor map (0, 530, 571,
    // 870, 974) and 0x00 precedes a move to an instance (70, 547, 556, 558, 574, 575,
    // 599, 600, 960, 1004, 1098, 1136). So 0x80 is OUT and 0x00 is back IN -- 0x00 is not
    // a leave. capture-000059 seqs 1038789..1040642 are all 0x00 with prevMap 960 and
    // destMap 960, i.e. re-summons into the same instance.
    //
    // A reader switching on 0 and 1 would match neither value.
    if (recv_data.size() - recv_data.rpos() != 1)
    {
        sLog.outError("WORLD: malformed CMSG_LFG_TELEPORT from %s", GetPlayerName());
        return;
    }

    bool const out = recv_data.ReadBit();

    Player* plr = GetPlayer();
    if (!plr)
    {
        return;
    }

    sLFGMgr.TeleportPlayer(plr, out);
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
    Player* plr = GetPlayer();
    if (!plr)
    {
        return;
    }

    // The eligibility data the client needs to grey out content it cannot enter.
    //
    // FindRandomDungeonsNotForPlayer already computes exactly this: a map keyed by
    // LfgDungeonsEntry::Entry() -- which IS the wire's dungeonEntry field -- with an
    // LFGForbiddenTypes value, and those codes are the client's LFG_INSTANCE_INVALID_CODES
    // verbatim (2 LEVEL_TOO_LOW, 3 LEVEL_TOO_HIGH, 1025 MISSING_ITEM, 1031 NOT_IN_SEASON
    // and so on). So no translation is required in either direction.
    dungeonForbidden const locked = sLFGMgr.FindRandomDungeonsNotForPlayer(plr);

    std::vector<MopLfgPackets::PlayerLockInfo> locks;
    locks.reserve(locked.size());

    for (dungeonForbidden::const_iterator it = locked.begin(); it != locked.end(); ++it)
    {
        MopLfgPackets::PlayerLockInfo entry;
        entry.dungeonEntry = it->first;
        entry.lockStatus = it->second;
        // subReason1/2 stay zero. They carry the required and current item level for the
        // gear-score reasons; all 206 records of the reference capture have them zero.
        locks.push_back(entry);
    }

    // 5-byte header plus 16 bytes per lock. The reference reply was 6068 bytes for 206
    // locks and 35 random records; ours is locks-only, so 5 + 16 * n.
    WorldPacket data(SMSG_LFG_PLAYER_INFO, 5 + locks.size() * 16);
    MopLfgPackets::BuildPlayerInfo(data, locks);

    DEBUG_LOG("SMSG_LFG_PLAYER_INFO: %s, %u locked dungeon(s), %u bytes.",
              GetPlayerName(), uint32(locks.size()), uint32(data.size()));

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

void WorldSession::SendLfgJoinResult(LfgJoinResult result, uint8 detail, partyForbidden const& lockedDungeons)
{
    MopLfgPackets::JoinResult update;
    update.result = uint8(result);
    update.detail = detail;

    // Retail zeroes the GUID and the whole ticket on a refusal -- that is what makes the
    // 18-byte form -- and carries both on a success. All 11 observed refusals are the
    // zeroed shape, so a refusal must not invent a ticket.
    if (result == ERR_LFG_OK)
    {
        if (Player* player = GetPlayer())
        {
            update.requesterGuid = player->GetObjectGuid().GetRawValue();
        }
        LFGStatusPacketData queueData;
        sLFGMgr.GetStatusPacketData(GetPlayer()->GetObjectGuid(), GetPlayer()->GetObjectGuid(), queueData);
        update.joinTime = queueData.joinedTime ? queueData.joinedTime : uint32(time(NULL));
        update.clientQueueId = queueData.ticketId;
        update.ticketType = 3;
    }

    for (partyForbidden::const_iterator it = lockedDungeons.begin(); it != lockedDungeons.end(); ++it)
    {
        MopLfgPackets::JoinResultPlayer player;
        player.guid = it->first.GetRawValue();

        for (dungeonForbidden::const_iterator itr = it->second.begin(); itr != it->second.end(); ++itr)
        {
            MopLfgPackets::PlayerLockInfo lock;
            lock.dungeonEntry = itr->first;
            lock.lockStatus = itr->second;
            player.locks.push_back(lock);
        }

        update.players.push_back(player);
    }

    WorldPacket data(SMSG_LFG_JOIN_RESULT, 24);
    MopLfgPackets::BuildJoinResult(data, update);

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
    case LFG_UPDATE_JOIN_QUEUE_INITIAL:
        joined = true;
        break;
    case LFG_UPDATE_STATUS:
        isQueued = (status.state == LFG_STATE_QUEUED);
        // `joined` must go FALSE once the player is inside. It used to be
        // `state != LFG_STATE_NONE`, and LFG_STATE_IN_DUNGEON is non-zero, so we
        // reported joined=1 from inside the dungeon where retail sends 0
        // (capture-000720 seq 1286, byte 1 = 0x80). UIParent.lua:3902 GetLFGMode then
        // returns "suspended" instead of falling through to "lfgparty" -- the client
        // believes the player is still queued rather than in the run.
        joined = (status.state != LFG_STATE_NONE
                  && status.state != LFG_STATE_IN_DUNGEON
                  && status.state != LFG_STATE_FINISHED_DUNGEON);
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
    // Retail leaves these 0,0,0 in all 5291 observed bodies without exception; the
    // role shortage is advertised in SMSG_LFG_QUEUE_STATUS instead.
    update.needs = {{ 0, 0, 0 }};
    // Always 1. Across 5291 retail bodies byte 1 takes only 0x00, 0x80 and 0xC0 --
    // the 0x40 our solo queue used to emit (bit9 set, bit8 clear) occurs zero times,
    // and bit8 is set even for a solo queue with no group at all. The name "isParty"
    // does not explain that; the wire value is not in doubt.
    update.isParty = true;
    update.joined = joined;
    // notifyUi tracks joined -- equal in 5288 of 5291 bodies, and 0 for every terminal
    // reason (8, 9, 11, 15, 25). It was defaulted true and never assigned.
    update.notifyUi = joined;
    // Not "did the player leave" and not "is the player inside": this bit says the
    // queue entry is owned by a GROUP. All 1931 bodies with a group-typed requesterGuid
    // carry it at every stage, including open-world queueing. It moves together with
    // requesterGuid, which is exactly the condition that selected queueGuid above.
    update.lfgJoined = (queueGuid != playerGuid);
    update.queued = isQueued;
    update.requestedRoles = queueData.roles;
    update.updateReason = uint8(status.updateType);
    // One ticket for the life of a queue, whatever happens to the entry underneath.
    //
    // The client keys its status records on the whole 20-byte RideTicket, so a body that
    // carries a different ticket does not update the record -- it creates a second one and
    // leaves the first stranded, queued and unclearable. GetStatusPacketData can legitimately
    // miss (the entry was erased by a merge, or the caller is announcing before it is
    // stored) and used to hand back a default-constructed struct, shipping ticketId = 0.
    // Retail sends 0 in none of 5291 observed bodies.
    //
    // So: take the live ticket when there is one and remember it; otherwise reuse whatever
    // this player's bodies have already gone out under.
    // Remember the first ticket this player's bodies go out under, then use THAT for
    // every later body -- including ones whose live lookup would now resolve elsewhere.
    sLFGMgr.RetainTicket(playerGuid, queueData.ticketId, queueData.joinedTime);

    LFGMgr::RetainedTicket retained;
    if (sLFGMgr.GetRetainedTicket(playerGuid, retained))
    {
        update.ticketId = retained.id;
        update.ticketTime = retained.time;
    }
    else
    {
        update.ticketId = queueData.ticketId;
        update.ticketTime = queueData.joinedTime;
        if (!update.ticketId)
        {
            sLog.outError("WORLD: SMSG_LFG_UPDATE_STATUS for %s has no ticket (reason %u); "
                          "the client cannot file this body against its queue record.",
                          GetPlayerName(), uint32(status.updateType));
        }
    }

    // The terminal ends the queue, so the ticket must not survive into the next join --
    // a stale one would make the new queue's bodies land on the old record.
    if (status.updateType == LFG_UPDATE_LEAVE)
    {
        sLFGMgr.ForgetTicket(playerGuid);
    }

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
    // Retail's clientQueueId IS the status packet's ticketId -- capture-000044 carries
    // 0x9BFF in SMSG_LFG_JOIN_RESULT seq 1547, SMSG_LFG_QUEUE_STATUS seq 1577 and the
    // status bodies alike. One identifier, three packets.
    update.clientQueueId = status.ticketId;
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

    // One byte, not four. Every 18414 capture of this opcode in the corpus is exactly
    // 1 byte (capture-000044 seq 70879 and 219256, capture-000465 seq 283035,
    // capture-000628 seq 31349, capture-000873 seq 154730).
    //
    // NOT admitted by IsEnterWorldConverted, deliberately. The size is settled but the
    // VALUE space is not: the one captured body carries 0x10 (16), while our
    // LFGTeleportError enum stops at 8, so our codes are provably not the client's.
    // Sending a correctly sized packet with a wrong code would show the player a
    // confidently wrong reason, which is worse than the current silence. Admit this
    // once the enum is derived from the client.
    WorldPacket data(SMSG_LFG_TELEPORT_DENIED, 1);
    data << uint8(error);
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
