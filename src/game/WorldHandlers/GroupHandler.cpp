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
 * @file GroupHandler.cpp
 * @brief Group/party opcode handlers
 *
 * This file handles group-related opcodes including:
 * - CMSG_GROUP_INVITE: Invite player to group
 * - CMSG_GROUP_ACCEPT: Accept group invitation
 * - CMSG_GROUP_DECLINE: Decline group invitation
 * - CMSG_GROUP_UNINVITE: Remove member from group
 * - CMSG_GROUP_LEAVE: Leave group
 * - CMSG_GROUP_DISBAND: Disband group
 * - CMSG_GROUP_CHANGE_LEADER: Transfer leadership
 * - CMSG_GROUP_SET_LEADER: Set new leader
 * - CMSG_LOOT_METHOD: Set loot method
 * - CMSG_MINIMAP_PING: Send minimap ping
 *
 * Group operations require proper permission checks and state validation.
 */

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "Log.h"
#include "WorldPacket.h"
#include "MopCompactPackets.h"
#include "MopPartyStatsPackets.h"
#include "MopPartyUpdatePackets.h"
#include "MopReadyCheckPackets.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Group.h"
#include "SocialMgr.h"
#include "Util.h"
#include "DB2Structure.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "TransportSystem.h"

/* differeces from off:
    -you can uninvite yourself - is is useful
    -you can accept invitation even if leader went offline
*/
/* todo:
    -group_destroyed msg is sent but not shown
    -reduce xp gaining when in raid group
    -quest sharing has to be corrected
    -FIX sending PartyMemberStats
*/

/**
 * @brief Sends a party operation result packet to the client.
 *
 * @param operation The party operation being reported.
 * @param member The related member name.
 * @param res The result code to send.
 */
void WorldSession::SendPartyResult(PartyOperation operation, const std::string& member, PartyResult res)
{
    WorldPacket data(SMSG_PARTY_COMMAND_RESULT, 4 + member.size() + 1 + 4 + 4 + 8);
    data << uint32(operation);
    data << member;                                         // max len 48
    data << uint32(res);
    data << uint32(0);                                      // LFD cooldown related (used with ERR_PARTY_LFG_BOOT_COOLDOWN_S and ERR_PARTY_LFG_BOOT_NOT_ELIGIBLE_S)
    data << ObjectGuid();                                   // if result == 27 (ERR_VOTE_KICK_REASON_NEEDED), then it's guid of player being kicked (member's guid)

    SendPacket(&data);
}

void WorldSession::SendGroupInvite(Player* player, bool alreadyInGroup /*= false*/)
{
    WorldPacket data(SMSG_GROUP_INVITE, 21);                // guess size
    data.WriteBit(0);
    data.WriteGuidMask<0, 3, 2>(player->GetObjectGuid());
    data.WriteBit(!alreadyInGroup);
    data.WriteGuidMask<6, 5>(player->GetObjectGuid());
    data.WriteBits(0, 9);                                   // realm name length
    data.WriteGuidMask<4>(player->GetObjectGuid());
    data.WriteBits(strlen(player->GetName()), 7);
    data.WriteBits(0, 24);                                  // count
    data.WriteBit(0);
    data.WriteGuidMask<1, 7>(player->GetObjectGuid());

    data.WriteGuidBytes<1, 4>(player->GetObjectGuid());
    data << uint32(GameTime::GetGameTimeMS());
    data << uint32(0) << uint32(0);
    data.WriteGuidBytes<6, 0, 2, 3>(player->GetObjectGuid());
    // for(int i = 0; i < count; ++i)
    //    data << uint32(0);
    data.WriteGuidBytes<5>(player->GetObjectGuid());
    data.WriteGuidBytes<7>(player->GetObjectGuid());
    data.append(player->GetName(), strlen(player->GetName()));
    data << uint32(0);

    SendPacket(&data);
}

/**
 * @brief Handles a request to invite a player into a party.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupInviteOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data.read_skip<uint32>();      // cross-realm party related
    recv_data.read_skip<uint32>();      // roles mask?

    recv_data.ReadGuidMask<2, 7>(guid);
    uint32 realmLength = recv_data.ReadBits(9);
    recv_data.ReadGuidMask<3>(guid);
    uint32 nameLength = recv_data.ReadBits(10);
    recv_data.ReadGuidMask<5, 4, 6, 0, 1>(guid);

    recv_data.ReadGuidBytes<4, 7, 6>(guid);

    std::string membername = recv_data.ReadString(nameLength);
    std::string realmname = recv_data.ReadString(realmLength);

    // attempt add selected player

    // cheating
    if (!normalizePlayerName(membername))
    {
        SendPartyResult(PARTY_OP_INVITE, membername, ERR_BAD_PLAYER_NAME_S);
        return;
    }

    Player* player = sObjectMgr.GetPlayer(membername.c_str());

    // no player or cheat self-invite
    if (!player || player == GetPlayer())
    {
        SendPartyResult(PARTY_OP_INVITE, membername, ERR_BAD_PLAYER_NAME_S);
        return;
    }

    // can't group with
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP) && GetPlayer()->GetTeam() != player->GetTeam())
    {
        SendPartyResult(PARTY_OP_INVITE, membername, ERR_PLAYER_WRONG_FACTION);
        return;
    }

    if (GetPlayer()->GetInstanceId() != 0 && player->GetInstanceId() != 0 && GetPlayer()->GetInstanceId() != player->GetInstanceId() && GetPlayer()->GetMapId() == player->GetMapId())
    {
        SendPartyResult(PARTY_OP_INVITE, membername, ERR_TARGET_NOT_IN_INSTANCE_S);
        return;
    }

    // just ignore us
    if (player->GetSocial()->HasIgnore(GetPlayer()->GetObjectGuid()))
    {
        SendPartyResult(PARTY_OP_INVITE, membername, ERR_IGNORING_YOU_S);
        return;
    }

    Group* group = GetPlayer()->GetGroup();
    if (group && group->isBGGroup())
    {
        group = GetPlayer()->GetOriginalGroup();
    }

    if (group && group->isRaidGroup() && !player->GetAllowLowLevelRaid() && (player->getLevel() < sWorld.getConfig(CONFIG_UINT32_MIN_LEVEL_FOR_RAID)))
    {
        SendPartyResult(PARTY_OP_INVITE, "", ERR_RAID_DISALLOWED_BY_LEVEL);
        return;
    }

    // player already invited
    if (player->GetGroupInvite())
    {
        SendPartyResult(PARTY_OP_INVITE, membername, ERR_ALREADY_IN_GROUP_S);
        return;
    }

    Group* group2 = player->GetGroup();
    if (group2 && group2->isBGGroup())
    {
        group2 = player->GetOriginalGroup();
    }

    // player already in another group
    if (group2)
    {
        SendPartyResult(PARTY_OP_INVITE, membername, ERR_ALREADY_IN_GROUP_S);

        // tell the player that they were invited but it failed as they were already in a group
        player->GetSession()->SendGroupInvite(player, true);

        return;
    }

    if (group)
    {
        // not have permissions for invite
        if (!group->IsLeader(GetPlayer()->GetObjectGuid()) && !group->IsAssistant(GetPlayer()->GetObjectGuid()))
        {
            SendPartyResult(PARTY_OP_INVITE, "", ERR_NOT_LEADER);
            return;
        }
        // not have place
        if (group->IsFull())
        {
            SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
            return;
        }
    }

    // ok, but group not exist, start a new group
    // but don't create and save the group to the DB until
    // at least one person joins
    if (!group)
    {
        group = new Group;
        // new group: if can't add then delete
        if (!group->AddLeaderInvite(GetPlayer()))
        {
            delete group;
            return;
        }
        if (!group->AddInvite(player))
        {
            delete group;
            return;
        }
    }
    else
    {
        // already existing group: if can't add then just leave
        if (!group->AddInvite(player))
        {
            return;
        }
    }

    player->GetSession()->SendGroupInvite(_player);
    SendPartyResult(PARTY_OP_INVITE, membername, ERR_PARTY_RESULT_OK);
}

/**
 * @brief Accepts a pending group invite.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupInviteResponseOpcode(WorldPacket& recv_data)
{
    bool unk = recv_data.ReadBit();
    bool accepted = recv_data.ReadBit();
    if (unk)
    {
        recv_data.read_skip<uint32>();
    }

    Group* group = GetPlayer()->GetGroupInvite();
    if (!group)
    {
        return;
    }

    if (accepted)
    {
        // remove in from invites in any case
        group->RemoveInvite(GetPlayer());

        if (group->GetLeaderGuid() == GetPlayer()->GetObjectGuid())
        {
            sLog.outError("HandleGroupInviteResponseOpcode: %s tried to accept an invite to his own group",
                          GetPlayer()->GetGuidStr().c_str());
            return;
        }

        /** error handling **/
        /********************/

        // not have place
        if (group->IsFull())
        {
            SendPartyResult(PARTY_OP_INVITE, "", ERR_GROUP_FULL);
            return;
        }

        Player* leader = sObjectMgr.GetPlayer(group->GetLeaderGuid());

        // forming a new group, create it
        if (!group->IsCreated())
        {
            if (leader)
            {
                group->RemoveInvite(leader);
            }
            if (group->Create(group->GetLeaderGuid(), group->GetLeaderName()))
            {
                sObjectMgr.AddGroup(group);
            }
            else
            {
                return;
            }
        }

        // everything is fine, do it, PLAYER'S GROUP IS SET IN ADDMEMBER!!!
        if (!group->AddMember(GetPlayer()->GetObjectGuid(), GetPlayer()->GetName()))
        {
            return;
        }
    }
    else
    {
        // uninvite, group can be deleted
        GetPlayer()->UninviteFromGroup();

        // remember leader if online
        Player* leader = sObjectMgr.GetPlayer(group->GetLeaderGuid());
        if (!leader || !leader->GetSession())
        {
            return;
        }

        // report
        WorldPacket data(SMSG_GROUP_DECLINE, 10);               // guess size
        data << GetPlayer()->GetName();
        leader->GetSession()->SendPacket(&data);
    }
}

/**
 * @brief Uninvites a group member or invitee by guid.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupUninviteGuidOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;
    recv_data.read_skip<std::string>();                     // reason

    // can't uninvite yourself
    if (guid == GetPlayer()->GetObjectGuid())
    {
        sLog.outError("WorldSession::HandleGroupUninviteGuidOpcode: leader %s tried to uninvite himself from the group.", GetPlayer()->GetGuidStr().c_str());
        return;
    }

    PartyResult res = GetPlayer()->CanUninviteFromGroup();
    if (res != ERR_PARTY_RESULT_OK)
    {
        SendPartyResult(PARTY_OP_LEAVE, "", res);
        return;
    }

    Group* grp = GetPlayer()->GetGroup();
    if (!grp)
    {
        return;
    }

    if (grp->IsMember(guid))
    {
        Player::RemoveFromGroup(grp, guid);
        return;
    }

    if (Player* plr = grp->GetInvited(guid))
    {
        plr->UninviteFromGroup();
        return;
    }

    SendPartyResult(PARTY_OP_LEAVE, "", ERR_TARGET_NOT_IN_GROUP_S);
}

/**
 * @brief Uninvites a group member or invitee by player name.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupUninviteOpcode(WorldPacket& recv_data)
{
    std::string membername;
    recv_data >> membername;

    // player not found
    if (!normalizePlayerName(membername))
    {
        return;
    }

    // can't uninvite yourself
    if (GetPlayer()->GetName() == membername)
    {
        sLog.outError("WorldSession::HandleGroupUninviteOpcode: leader %s tried to uninvite himself from the group.", GetPlayer()->GetGuidStr().c_str());
        return;
    }

    PartyResult res = GetPlayer()->CanUninviteFromGroup();
    if (res != ERR_PARTY_RESULT_OK)
    {
        SendPartyResult(PARTY_OP_LEAVE, "", res);
        return;
    }

    Group* grp = GetPlayer()->GetGroup();
    if (!grp)
    {
        return;
    }

    if (ObjectGuid guid = grp->GetMemberGuid(membername))
    {
        Player::RemoveFromGroup(grp, guid);
        return;
    }

    if (Player* plr = grp->GetInvited(membername))
    {
        plr->UninviteFromGroup();
        return;
    }

    SendPartyResult(PARTY_OP_LEAVE, membername, ERR_TARGET_NOT_IN_GROUP_S);
}

/**
 * @brief Changes the leader of the current group.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupSetLeaderOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    Player* player = sObjectMgr.GetPlayer(guid);

    /** error handling **/
    if (!player || !group->IsLeader(GetPlayer()->GetObjectGuid()) || player->GetGroup() != group)
    {
        return;
    }
    /********************/

    // everything is fine, do it
    group->ChangeLeader(guid);
}

/**
 * @brief Handles a request to leave or disband the current group.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupDisbandOpcode(WorldPacket& /*recv_data*/)
{
    if (!GetPlayer()->GetGroup())
    {
        return;
    }

    if (_player->InBattleGround())
    {
        SendPartyResult(PARTY_OP_INVITE, "", ERR_INVITE_RESTRICTED);
        return;
    }

    /** error handling **/
    /********************/

    // everything is fine, do it
    SendPartyResult(PARTY_OP_LEAVE, GetPlayer()->GetName(), ERR_PARTY_RESULT_OK);

    GetPlayer()->RemoveFromGroup();
}

/**
 * @brief Updates the group's loot rules.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleLootMethodOpcode(WorldPacket& recv_data)
{
    uint32 lootMethod;
    ObjectGuid lootMaster;
    uint32 lootThreshold;
    recv_data >> lootMethod >> lootMaster >> lootThreshold;

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(GetPlayer()->GetObjectGuid()))
    {
        return;
    }
    /********************/

    // everything is fine, do it
    group->SetLootMethod((LootMethod)lootMethod);
    group->SetLooterGuid(lootMaster);
    group->SetLootThreshold((ItemQualities)lootThreshold);
    group->SendUpdate();
}

/**
 * @brief Handles a player's loot roll choice.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleLootRoll(WorldPacket& recv_data)
{
    ObjectGuid lootedTarget;
    uint32 itemSlot;
    uint8  rollType;
    recv_data >> lootedTarget;                              // guid of the item rolled
    recv_data >> itemSlot;
    recv_data >> rollType;

    // DEBUG_LOG("WORLD RECIEVE CMSG_LOOT_ROLL, From:%u, Numberofplayers:%u, rollType:%u", (uint32)Guid, NumberOfPlayers, rollType);

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    if (rollType >= MAX_ROLL_FROM_CLIENT)
    {
        return;
    }

    // everything is fine, do it, if false then some cheating problem found
    if (!group->CountRollVote(GetPlayer(), lootedTarget, itemSlot, RollVote(rollType)))
    {
        return;
    }

    switch (rollType)
    {
        case ROLL_NEED:
            GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ROLL_NEED, 1);
            break;
        case ROLL_GREED:
        case ROLL_DISENCHANT:
            GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_ROLL_GREED, 1);
            break;
    }
}

/**
 * @brief Broadcasts a minimap ping to the player's group.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleMinimapPingOpcode(WorldPacket& recv_data)
{
    float x, y;
    recv_data >> x;
    recv_data >> y;

    if (!GetPlayer()->GetGroup())
    {
        return;
    }

    // DEBUG_LOG("Received opcode MSG_MINIMAP_PING X: %f, Y: %f", x, y);

    /** error handling **/
    /********************/

    // everything is fine, do it
    WorldPacket data(MSG_MINIMAP_PING, (8 + 4 + 4));
    data << GetPlayer()->GetObjectGuid();
    data << float(x);
    data << float(y);
    GetPlayer()->GetGroup()->BroadcastPacket(&data, true, -1, GetPlayer()->GetObjectGuid());
}

/**
 * @brief Rolls a random value and broadcasts it to the party if applicable.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleRandomRollOpcode(WorldPacket& recv_data)
{
    uint32 minimum, maximum, roll;
    recv_data >> minimum;
    recv_data >> maximum;

    /** error handling **/
    if (minimum > maximum || maximum > 10000)               // < 32768 for urand call
    {
        return;
    }
    /********************/

    // everything is fine, do it
    roll = urand(minimum, maximum);

    // DEBUG_LOG("ROLL: MIN: %u, MAX: %u, ROLL: %u", minimum, maximum, roll);

    WorldPacket data(SMSG_RANDOM_ROLL, 4 + 4 + 4 + 1 + 8);
    MopCompactPackets::BuildRandomRoll(data, GetPlayer()->GetObjectGuid().GetRawValue(), minimum, maximum, roll);
    if (GetPlayer()->GetGroup())
    {
        GetPlayer()->GetGroup()->BroadcastPacket(&data, false);
    }
    else
    {
        SendPacket(&data);
    }
}

/**
 * @brief Handles raid target icon queries and updates.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleRaidTargetUpdateOpcode(WorldPacket& recv_data)
{
    uint8  x;
    recv_data >> x;

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    /********************/

    // everything is fine, do it
    if (x == 0xFF)                                          // target icon request
    {
        group->SendTargetIconList(this);
    }
    else                                                    // target icon update
    {
        if (group->isRaidGroup() &&
                !group->IsLeader(GetPlayer()->GetObjectGuid()) &&
                !group->IsAssistant(GetPlayer()->GetObjectGuid()))
            return;

        ObjectGuid guid;
        recv_data >> guid;
        group->SetTargetIcon(x, _player->GetObjectGuid(), guid);
    }
}

/**
 * @brief Converts the current party into a raid group.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupRaidConvertOpcode(WorldPacket& /*recv_data*/)
{
    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    if (_player->InBattleGround())
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(GetPlayer()->GetObjectGuid()) || group->GetMembersCount() < 2)
    {
        return;
    }
    /********************/

    // everything is fine, do it (is it 0 (PARTY_OP_INVITE) correct code)
    SendPartyResult(PARTY_OP_INVITE, "", ERR_PARTY_RESULT_OK);
    group->ConvertToRaid();
}

/**
 * @brief Moves a raid member into another subgroup.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupChangeSubGroupOpcode(WorldPacket& recv_data)
{
    std::string name;
    uint8 groupNr;
    recv_data >> name;

    recv_data >> groupNr;

    if (groupNr >= MAX_RAID_SUBGROUPS)
    {
        return;
    }

    // we will get correct pointer for group here, so we don't have to check if group is BG raid
    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(GetPlayer()->GetObjectGuid()) &&
            !group->IsAssistant(GetPlayer()->GetObjectGuid()))
        return;

    if (!group->HasFreeSlotSubGroup(groupNr))
    {
        return;
    }
    /********************/

    // everything is fine, do it
    if (Player* player = sObjectMgr.GetPlayer(name.c_str()))
    {
        group->ChangeMembersGroup(player, groupNr);
    }
    else
    {
        if (ObjectGuid guid = sObjectMgr.GetPlayerGuidByName(name.c_str()))
        {
            group->ChangeMembersGroup(guid, groupNr);
        }
    }
}

/**
 * @brief Sets or clears the assistant leader flag for a raid member.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupAssistantLeaderOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    uint8 flag;
    recv_data >> guid;
    recv_data >> flag;

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(GetPlayer()->GetObjectGuid()))
    {
        return;
    }
    /********************/

    // everything is fine, do it
    group->SetAssistant(guid, (flag == 0 ? false : true));
}

/**
 * @brief Updates main tank or main assist raid assignments.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandlePartyAssignmentOpcode(WorldPacket& recv_data)
{
    uint8 role;
    uint8 apply;
    ObjectGuid guid;
    recv_data >> role >> apply;                             // role 0 = Main Tank, 1 = Main Assistant
    recv_data >> guid;

    DEBUG_LOG("MSG_PARTY_ASSIGNMENT");

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(GetPlayer()->GetObjectGuid()))
    {
        return;
    }
    /********************/

    // everything is fine, do it
    if (apply)
    {
        switch (role)
        {
            case 0: group->SetMainTank(guid); break;
            case 1: group->SetMainAssistant(guid); break;
            default: break;
        }
    }
    else
    {
        if (group->GetMainTankGuid() == guid)
        {
            group->SetMainTank(ObjectGuid());
        }
        if (group->GetMainAssistantGuid() == guid)
        {
            group->SetMainAssistant(ObjectGuid());
        }
    }
}

namespace
{
    Group* GetGroupByPartyIndex(Player* player, uint8 partyIndex)
    {
        if (partyIndex == 0)
            return player->GetOriginalGroup() ? player->GetOriginalGroup() : player->GetGroup();
        if (partyIndex == 1)
            return player->GetGroup();
        return NULL;
    }
}

/**
 * @brief Starts a raid ready check.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleRaidReadyCheckOpcode(WorldPacket& recv_data)
{
    uint8 const partyIndex = MopReadyCheckPackets::ReadStartRequest(recv_data);
    Player* player = GetPlayer();
    Group* group = GetGroupByPartyIndex(player, partyIndex);
    if (!group)
        return;

    ObjectGuid const playerGuid = player->GetObjectGuid();
    if (!group->IsLeader(playerGuid) && !group->IsAssistant(playerGuid))
        return;
    // One Player timer owns one active check. This also prevents a player with
    // original/current groups from stranding one check by starting both.
    if (player->HasReadyCheckTimer())
        return;
    if (!group->StartReadyCheck(partyIndex, playerGuid))
        return;

    // The 35-second policy matches the 5.4.8 server reference lineage; the
    // packet field itself and its width are direct 18414 client evidence.
    uint32 const readyCheckDuration = 35000;
    group->ReadyCheckMemberHasResponded(playerGuid);
    player->SetReadyCheckTimer(readyCheckDuration);

    WorldPacket data;
    MopReadyCheckPackets::BuildStarted(data,
        group->GetObjectGuid().GetRawValue(), playerGuid.GetRawValue(),
        readyCheckDuration, partyIndex);
    group->BroadcastPacket(&data, false);

    group->OfflineReadyCheck();
    if (group->ReadyCheckAllResponded())
        group->CompleteReadyCheck();
}

/**
 * @brief Records a member response to an active raid ready check.
 */
void WorldSession::HandleRaidReadyCheckConfirmOpcode(WorldPacket& recv_data)
{
    MopReadyCheckPackets::ResponseRequest const response =
        MopReadyCheckPackets::ReadResponseRequest(recv_data);

    Player* player = GetPlayer();
    Group* group = GetGroupByPartyIndex(player, response.partyIndex);
    if (!group || !group->ReadyCheckInProgress() ||
        group->GetReadyCheckPartyIndex() != response.partyIndex)
        return;

    // The packed GUID is present on the wire, but the client construction
    // paths do not populate it. Identity is therefore bound to the session.
    ObjectGuid const playerGuid = player->GetObjectGuid();
    if (!group->ReadyCheckMemberHasResponded(playerGuid))
        return;

    WorldPacket data;
    MopReadyCheckPackets::BuildResponse(data,
        group->GetObjectGuid().GetRawValue(), playerGuid.GetRawValue(),
        response.ready);
    group->BroadcastPacket(&data, false);

    if (group->ReadyCheckAllResponded())
        group->CompleteReadyCheck();
}

/**
 * @brief Handles the completion of a raid ready check.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleRaidReadyCheckFinishedOpcode(WorldPacket& /*recv_data*/)
{
    // Group* group = GetPlayer()->GetGroup();
    // if (!group)
    //    return;

    // if (!group->IsLeader(GetPlayer()->GetGUID()) && !group->IsAssistant(GetPlayer()->GetGUID()))
    //    return;

    // Is any reaction need?
}

namespace
{
    uint8 MopAuraFlags(uint16 legacyFlags)
    {
        uint8 mopFlags = 0;
        if (legacyFlags & AFLAG_NOT_CASTER)
            mopFlags |= 0x01;
        if (legacyFlags & AFLAG_POSITIVE)
            mopFlags |= 0x02;
        if (legacyFlags & AFLAG_DURATION)
            mopFlags |= 0x04;
        if (legacyFlags & AFLAG_EFFECT_AMOUNT_SEND)
            mopFlags |= 0x08;
        if (legacyFlags & AFLAG_NEGATIVE)
            mopFlags |= 0x10;
        return mopFlags;
    }

    void AppendPartyAuraRecord(ByteBuffer& payload, SpellAuraHolder* holder)
    {
        if (!holder)
        {
            payload << uint32(0) << uint8(0) << uint32(0);
            return;
        }

        uint16 const legacyFlags = holder->GetAuraFlags();
        uint8 const mopFlags = MopAuraFlags(legacyFlags);
        uint32 effectMask = 0;
        uint8 effectCount = 0;
        for (uint32 effectIndex = 0; effectIndex < MAX_EFFECT_INDEX; ++effectIndex)
        {
            if (holder->GetAuraByEffectIndex(SpellEffectIndex(effectIndex)))
            {
                effectMask |= uint32(1) << effectIndex;
                ++effectCount;
            }
        }

        payload << uint32(holder->GetId());
        payload << uint8(mopFlags);
        payload << uint32(effectMask);
        if (mopFlags & 0x08)
        {
            payload << uint8(effectCount);
            for (uint32 effectIndex = 0; effectIndex < MAX_EFFECT_INDEX; ++effectIndex)
            {
                if (Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(effectIndex)))
                    payload << float(aura->GetModifier()->m_amount);
            }
        }
    }

    template <typename UnitType>
    void AppendPartyAuraBlock(ByteBuffer& payload, UnitType* unit, bool reset)
    {
        uint64 auraMask = 0;
        if (unit)
        {
            if (reset)
            {
                for (uint32 slot = 0; slot < MAX_AURAS; ++slot)
                    if (unit->GetVisibleAura(slot))
                        auraMask |= uint64(1) << slot;
            }
            else
                auraMask = unit->GetAuraUpdateMask();
        }

        payload << uint8(reset ? 1 : 0);
        payload << uint64(auraMask);
        payload << uint32(unit ? MAX_AURAS : 0);
        if (!unit)
            return;

        for (uint32 slot = 0; slot < MAX_AURAS; ++slot)
            if (auraMask & (uint64(1) << slot))
                AppendPartyAuraRecord(payload, unit->GetVisibleAura(slot));
    }

    void AppendPartyStatsPayload(ByteBuffer& payload, Player* player, uint32 mask,
        uint16 status, uint16 zone, uint16 x, uint16 y, uint16 z,
        bool resetAuras)
    {
        Powers const powerType = player->GetPowerType();
        Pet* const pet = player->GetPet();

        if (mask & GROUP_UPDATE_FLAG_STATUS)
            payload << status;
        if (mask & GROUP_UPDATE_FLAG_CUR_HP)
            payload << uint32(player->GetHealth());
        if (mask & GROUP_UPDATE_FLAG_MAX_HP)
            payload << uint32(player->GetMaxHealth());
        if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)
            payload << uint8(powerType);
        if (mask & GROUP_UPDATE_FLAG_CUR_POWER)
            payload << uint16(player->GetPower(powerType));
        if (mask & GROUP_UPDATE_FLAG_MAX_POWER)
            payload << uint16(player->GetMaxPower(powerType));
        if (mask & GROUP_UPDATE_FLAG_LEVEL)
            payload << uint16(player->getLevel());
        if (mask & GROUP_UPDATE_FLAG_ZONE)
            payload << zone;
        if (mask & GROUP_UPDATE_FLAG_POSITION)
            payload << x << y << z;
        if (mask & GROUP_UPDATE_FLAG_AURAS)
            AppendPartyAuraBlock(payload, player, resetAuras);

        if (mask & GROUP_UPDATE_FLAG_PET_GUID)
            payload << (pet ? pet->GetObjectGuid() : ObjectGuid());
        if (mask & GROUP_UPDATE_FLAG_PET_NAME)
        {
            if (pet)
                payload << pet->GetName();
            else
                payload << uint8(0);
        }
        if (mask & GROUP_UPDATE_FLAG_PET_MODEL_ID)
            payload << uint16(pet ? pet->GetDisplayId() : 0);
        if (mask & GROUP_UPDATE_FLAG_PET_CUR_HP)
            payload << uint32(pet ? pet->GetHealth() : 0);
        if (mask & GROUP_UPDATE_FLAG_PET_MAX_HP)
            payload << uint32(pet ? pet->GetMaxHealth() : 0);
        if (mask & GROUP_UPDATE_FLAG_PET_POWER_TYPE)
            payload << uint8(pet ? pet->GetPowerType() : 0);
        if (mask & GROUP_UPDATE_FLAG_PET_CUR_POWER)
            payload << uint16(pet ? pet->GetPower(pet->GetPowerType()) : 0);
        if (mask & GROUP_UPDATE_FLAG_PET_MAX_POWER)
            payload << uint16(pet ? pet->GetMaxPower(pet->GetPowerType()) : 0);
        if (mask & GROUP_UPDATE_FLAG_PET_AURAS)
            AppendPartyAuraBlock(payload, pet, resetAuras);

        if (mask & GROUP_UPDATE_FLAG_VEHICLE_SEAT)
        {
            uint32 vehicleSeat = 0;
            if (player->GetTransportInfo())
            {
                vehicleSeat = ((Unit*)player->GetTransportInfo()->GetTransport())->GetVehicleInfo()->GetVehicleEntry()->SeatID[
                    player->GetTransportInfo()->GetTransportSeat()];
            }
            payload << vehicleSeat;
        }

        if (mask & GROUP_UPDATE_FLAG_PHASE)
        {
            std::vector<uint16> phaseIds;
            payload << uint32(8);
            payload.WriteBits(phaseIds.size(), 23);
            payload.FlushBits();
            for (uint16 phaseId : phaseIds)
                payload << phaseId;
        }
    }
}

/**
 * @brief Builds a party member stats update packet.
 *
 * @param player The player whose stats are being serialized.
 * @param data The packet receiving the serialized fields.
 */
void WorldSession::BuildPartyMemberStatsChangedPacket(Player* player, WorldPacket* data)
{
    uint32 mask = player->GetGroupUpdateFlag() & uint32(GROUP_UPDATE_FULL);
    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)
        mask |= GROUP_UPDATE_FLAG_CUR_POWER | GROUP_UPDATE_FLAG_MAX_POWER;
    if (mask & GROUP_UPDATE_FLAG_PET_POWER_TYPE)
        mask |= GROUP_UPDATE_FLAG_PET_CUR_POWER | GROUP_UPDATE_FLAG_PET_MAX_POWER;

    uint16 const status = player->IsPvP()
        ? uint16(MEMBER_STATUS_ONLINE | MEMBER_STATUS_PVP)
        : uint16(MEMBER_STATUS_ONLINE);
    ByteBuffer payload;
    AppendPartyStatsPayload(payload, player, mask, status,
        uint16(player->GetZoneId()), uint16(player->GetPositionX()),
        uint16(player->GetPositionY()), uint16(player->GetPositionZ()), false);
    MopPartyStatsPackets::BuildResponse(*data,
        player->GetObjectGuid().GetRawValue(), mask, false, false, payload);
}

/*this procedure handles clients CMSG_REQUEST_PARTY_MEMBER_STATS request*/
void WorldSession::HandleRequestPartyMemberStatsOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_REQUEST_PARTY_MEMBER_STATS");
    MopPartyStatsPackets::Request const request = MopPartyStatsPackets::ReadRequest(recv_data);
    (void)request.mode;
    ObjectGuid const guid(request.memberGuid);

    Player* player = sObjectAccessor.FindPlayer(guid, false);
    if (!player)
    {
        ByteBuffer payload;
        payload << uint16(MEMBER_STATUS_OFFLINE);
        WorldPacket data;
        MopPartyStatsPackets::BuildResponse(data, request.memberGuid,
            GROUP_UPDATE_FLAG_STATUS, true, false, payload);
        SendPacket(&data);
        return;
    }

    uint16 zone = 0;
    uint16 x = 0;
    uint16 y = 0;
    uint16 z = 0;
    if (player->IsInWorld())
    {
        zone = player->GetZoneId();
        x = player->GetPositionX();
        y = player->GetPositionY();
        z = player->GetPositionZ();
    }
    else if (player->IsBeingTeleported())
    {
        WorldLocation& loc = player->GetTeleportDest();
        zone = sTerrainMgr.GetZoneId(loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z);
        x = loc.coord_x;
        y = loc.coord_y;
        z = loc.coord_z;
    }

    uint32 mask = uint32(GROUP_UPDATE_FULL);
    if (!player->GetPet())
        mask &= ~uint32(GROUP_UPDATE_PET);
    if (!player->GetTransportInfo())
        mask &= ~uint32(GROUP_UPDATE_FLAG_VEHICLE_SEAT);

    ByteBuffer payload;
    AppendPartyStatsPayload(payload, player, mask, MEMBER_STATUS_ONLINE,
        zone, x, y, z, true);
    WorldPacket data;
    MopPartyStatsPackets::BuildResponse(data, request.memberGuid, mask,
        true, false, payload);
    SendPacket(&data);
}

/**
 * @brief Sends the saved raid instance information to the client.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleRequestRaidInfoOpcode(WorldPacket& /*recv_data*/)
{
    // every time the player checks the character screen
    _player->SendRaidInfo();
}

/**
 * @brief Handles the client's opt-out-of-loot setting.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleOptOutOfLootOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_OPT_OUT_OF_LOOT");

    uint32 unkn;
    recv_data >> unkn;

    // ignore if player not loaded
    if (!GetPlayer())                                       // needed because STATUS_AUTHED
    {
        if (unkn != 0)
        {
            sLog.outError("CMSG_GROUP_PASS_ON_LOOT value<>0 for not-loaded character!");
        }
        return;
    }

    // Set player's opt-out-of-loot preference
    GetPlayer()->SetOptOutOfLoot(unkn != 0);
}

void WorldSession::HandleSetAllowLowLevelRaidOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_ALLOW_LOW_LEVEL_RAID: %4X", recv_data.GetOpcode());

    uint8 allow;
    recv_data >> allow;

    GetPlayer()->SetAllowLowLevelRaid(allow);
}

void WorldSession::HandleGroupRequestJoinUpdates(WorldPacket& recv_data)
{
    uint8 const partyIndex = MopPartyUpdatePackets::ReadRequest(recv_data);
    Player* player = GetPlayer();
    Group* group = GetGroupByPartyIndex(player, partyIndex);
    if (!group)
        return;

    group->SendUpdateToPlayer(player->GetObjectGuid());
}
