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
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Group.h"
#include "LFGMgr.h"
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
    // Every field beyond the inviter's name and GUID is deliberately left at
    // its default. FrameXML UIParent.lua:800 picks the dialog from these:
    // any role bit shows the LFG invite popup, and the cross-realm flag shows
    // PARTY_INVITE_XREALM. One realm means neither applies, so an ordinary
    // invite must clear both or the invitee sees the wrong dialog. The two
    // realm strings and the three identity scalars serve the cross-realm route
    // and stay empty/zero here; the ordinary popup reads only the name.
    MopGroupInvitePackets::Invite invite;
    invite.inviterGuid = player->GetObjectGuid();
    invite.inviterName = player->GetName();
    invite.notAlreadyInGroup = !alreadyInGroup;

    WorldPacket data;
    if (!MopGroupInvitePackets::BuildInvite(data, invite))
    {
        sLog.outError("SendGroupInvite: refusing to send a malformed 18414 invite popup for %s",
                      player->GetGuidStr().c_str());
        return;
    }

    SendPacket(&data);
}

/**
 * @brief Handles a request to invite a player into a party.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupInviteOpcode(WorldPacket& recv_data)
{
    MopGroupInvitePackets::Request request;
    if (!MopGroupInvitePackets::ParseRequest(recv_data, request))
    {
        return;
    }

    std::string membername = request.targetName;

    // Lua InviteUnit refuses a target longer than 48 bytes before it ever
    // reaches the wire, so a longer one is not something a real client sends.
    // Applied to the received bytes, ahead of normalisation.
    if (membername.empty() || membername.size() > 48)
    {
        SendPartyResult(PARTY_OP_INVITE, membername, ERR_BAD_PLAYER_NAME_S);
        return;
    }

    // This packet carries its own realm field rather than the "-Realm" suffix
    // that whisper and guild invite strip, so there is nothing to strip here.
    // Accepting a foreign realm would resolve the invite against a LOCAL
    // character of the same name and invite the wrong player, so anything
    // that is neither empty nor this realm is rejected. The client sends the
    // display form, which may contain spaces, so the space-free form is
    // accepted too.
    //
    // request.targetGuid and request.realmSelectorHint are deliberately
    // unused: they are client-supplied routing hints, and letting either
    // select a Player would hand the caller a target of their choosing.
    if (!request.realmName.empty() &&
        request.realmName != CachedRealmName() &&
        request.realmName != NormalizeRealmName(CachedRealmName()))
    {
        SendPartyResult(PARTY_OP_INVITE, membername, ERR_BAD_PLAYER_NAME_S);
        return;
    }

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

        // Tell the target they were invited but it failed because they are
        // already grouped. The popup names the INVITER -- passing `player` here
        // made the target's own name appear in their own invite dialog. That was
        // inert while SMSG_GROUP_INVITE was dropped by the send gate; admitting
        // the packet put it on the wire.
        player->GetSession()->SendGroupInvite(_player, true);

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
    MopGroupInvitePackets::Response response;
    if (!MopGroupInvitePackets::ParseResponse(recv_data, response))
        return;

    // Build 18414 sends no invite identity. A response therefore applies to
    // the authenticated player's current pending invite.
    Group* group = GetPlayer()->GetGroupInvite();
    if (!group)
    {
        return;
    }

    if (response.accepted)
    {
        if (group->GetLeaderGuid() == GetPlayer()->GetObjectGuid())
        {
            sLog.outError("HandleGroupInviteResponseOpcode: %s tried to accept an invite to his own group",
                          GetPlayer()->GetGuidStr().c_str());
            return;
        }

        // remove from invites only after authority checks
        group->RemoveInvite(GetPlayer());

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
        ObjectGuid const leaderGuid = group->GetLeaderGuid();

        // uninvite, group can be deleted
        GetPlayer()->UninviteFromGroup();

        // remember leader if online
        Player* leader = sObjectMgr.GetPlayer(leaderGuid);
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
    MopGroupUninvitePackets::Request request;
    if (!MopGroupUninvitePackets::ParseRequest(recv_data, request))
    {
        return;
    }

    ObjectGuid const guid = request.targetGuid;

    // can't uninvite yourself
    if (guid == GetPlayer()->GetObjectGuid())
    {
        sLog.outError("WorldSession::HandleGroupUninviteGuidOpcode: leader %s tried to uninvite himself from the group.", GetPlayer()->GetGuidStr().c_str());
        return;
    }

    Group* grp = GetPlayer()->GetGroup();
    if (!grp)
    {
        return;
    }

    // In a dungeon-finder group nobody may remove anybody unilaterally; the request
    // becomes a vote kick instead. That is why the client bothers to collect
    // `reason` here -- it is the free text shown in the boot dialog and it has no
    // other consumer.
    //
    // Deliberately BEFORE CanUninviteFromGroup, which requires leader or assistant.
    // An LFD group has no meaningful leadership for this purpose: any member may
    // start a vote, including against the leader, and the vote is what decides it.
    // Routing through the normal path would both refuse ordinary members and, for a
    // leader, silently perform a real removal that no one voted on.
    if (grp->isLFGGroup())
    {
        if (!grp->IsMember(guid))
        {
            SendPartyResult(PARTY_OP_LEAVE, "", ERR_TARGET_NOT_IN_GROUP_S);
            return;
        }

        sLFGMgr.AttemptToKickPlayer(grp, guid, GetPlayer()->GetObjectGuid(), request.reason);
        return;
    }

    PartyResult res = GetPlayer()->CanUninviteFromGroup();
    if (res != ERR_PARTY_RESULT_OK)
    {
        SendPartyResult(PARTY_OP_LEAVE, "", res);
        return;
    }

    // Nobody may kick the leader. Player::CanUninviteFromGroup grants the right
    // to any assistant with no leader test, and the client offers the Remove
    // entry in this case (UnitPopup.lua only hides assistant-on-assistant and
    // leader-on-self), so the server is the only thing that can refuse it.
    if (grp->IsLeader(guid))
    {
        SendPartyResult(PARTY_OP_LEAVE, "", ERR_NOT_LEADER);
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
 * @brief Changes the leader of the current group.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupSetLeaderOpcode(WorldPacket& recv_data)
{
    MopGroupPromotePackets::SetLeaderRequest request;
    if (!MopGroupPromotePackets::ParseSetLeader(recv_data, request))
    {
        return;
    }

    ObjectGuid const guid = request.targetGuid;

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
void WorldSession::HandleGroupDisbandOpcode(WorldPacket& recv_data)
{
    // One byte, observed 0x7F. It carries no authority -- the server acts on the caller --
    // but it must be consumed or the dispatcher logs "unprocessed tail data" on every
    // Leave Instance Group click.
    if (recv_data.size() - recv_data.rpos() >= 1)
    {
        recv_data.read_skip<uint8>();
    }

    Group* pGroup = GetPlayer()->GetGroup();
    if (!pGroup)
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

    // Leaving an LFG group from INSIDE its dungeon has to put the player back where
    // they came from. Retail answers the disband with SMSG_GROUP_LIST (the 40-byte
    // no-group form), SMSG_TRANSFER_PENDING (mapId 0) and SMSG_NEW_WORLD
    // (capture-000720 seq 46746, capture-000656 seq 191821).
    //
    // Without this the player simply stood in the instance, group gone, and was only
    // collected 60 seconds later by the homebind timer that fires when the instance
    // stops being valid for them. Observed live: "leave dungeon did not relocate me".
    //
    // Must run BEFORE RemoveFromGroup -- TeleportPlayer resolves the dungeon through the
    // group's LFG status, which is gone once the group is. It is a no-op unless the
    // player is actually standing on the dungeon's map.
    if (pGroup->isLFGGroup())
    {
        // REFUSE the leave outright while the player is fighting inside the dungeon.
        //
        // The teleport out is refused in combat, but the removal used to run anyway, so a
        // player who clicked Leave Instance Group mid-fight was taken out of the group and
        // left standing in the instance -- and at the time the refusal was mute, because
        // SMSG_LFG_TELEPORT_DENIED was not admitted. It is admitted now, so the player is
        // told; the removal-without-teleport is what this guard exists to stop. Observed live: "i did leave instance
        // group on the leader, i just got removed but not teleported out", while stuck in
        // a combat stance.
        //
        // Worse for everyone else: once the group is gone the remaining player has no LFG
        // state at all, so the minimap eye disappears and with it the only way out.
        // Observed in the same session -- "he had no dungeon finder eyeball to teleport out
        // or anything at all".
        //
        // Removing the group is the irreversible half, so it must not happen when the
        // half that gets the player out cannot. Refusing keeps the two consistent: the
        // player stays in the group, still able to leave once combat ends.
        if (GetPlayer()->IsInCombat() && sLFGMgr.IsPlayerInLfgDungeon(GetPlayer()))
        {
            SendPartyResult(PARTY_OP_LEAVE, GetPlayer()->GetName(), ERR_PARTY_RESULT_OK);
            DEBUG_LOG("HandleGroupDisbandOpcode: %s refused -- in combat inside an LFG dungeon",
                      GetPlayer()->GetName());
            return;
        }

        // Deserter BEFORE the teleport: OnPlayerLeftDungeonGroup only counts a player who
        // is still in a live run, and the teleport is about to move them out of it.
        sLFGMgr.OnPlayerLeftDungeonGroup(GetPlayer());
        sLFGMgr.TeleportPlayer(GetPlayer(), true);
    }

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
    MopGroupLootMethodPackets::Request request;
    if (!MopGroupLootMethodPackets::ParseRequest(recv_data, request))
    {
        return;
    }

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

    // A master looter who is not in the group would be unreachable for every
    // later loot decision, so refuse rather than store an unusable GUID. The
    // client only carries one for MASTER_LOOT; any other method clears it.
    ObjectGuid looter;
    if (request.method == MASTER_LOOT)
    {
        if (!request.looterGuid || !group->IsMember(request.looterGuid))
        {
            return;
        }
        looter = request.looterGuid;
    }
    /********************/

    // everything is fine, do it. Both values were range-checked while parsing.
    group->SetLootMethod((LootMethod)request.method);
    group->SetLooterGuid(looter);
    group->SetLootThreshold((ItemQualities)request.threshold);
    group->SendUpdate();
}

/**
 * @brief Handles a player's loot roll choice.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleLootRoll(WorldPacket& recv_data)
{
    MopGroupLootPackets::VoteRequest request;
    if (!MopGroupLootPackets::ParseVoteRequest(recv_data, request))
        return;

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    if (request.rollType >= MAX_ROLL_FROM_CLIENT)
    {
        return;
    }

    // The client-local Lua roll ID is not on the wire; the server resolves the
    // vote by the packed loot-source GUID and the raw loot-list slot.
    if (!group->CountRollVote(GetPlayer(), ObjectGuid(request.lootGuid),
            request.lootListId, RollVote(request.rollType)))
    {
        return;
    }

    switch (request.rollType)
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
    MopGroupMarkerPackets::MinimapPingRequest const request =
        MopGroupMarkerPackets::ReadMinimapPingRequest(recv_data);
    if (request.context != 0x7F)
        return;

    if (!GetPlayer()->GetGroup())
    {
        return;
    }

    // DEBUG_LOG("Received CMSG_MINIMAP_PING X: %f, Y: %f", request.x, request.y);

    /** error handling **/
    /********************/

    // everything is fine, do it
    WorldPacket data;
    MopGroupMarkerPackets::BuildMinimapPing(data,
        GetPlayer()->GetObjectGuid().GetRawValue(), request.x, request.y);
    GetPlayer()->GetGroup()->BroadcastPacket(&data, true, -1, GetPlayer()->GetObjectGuid());
}

/**
 * @brief Rolls a random value and broadcasts it to the party if applicable.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleRandomRollOpcode(WorldPacket& recv_data)
{
    // Build 18414 writer sub_66748A, vtable D62DF0 slot 1; slot 2 sub_661642
    // writes opcode 2211. The wire order is MAXIMUM then MINIMUM, then the 0x7F
    // family marker -- not minimum-first as inherited, and not eight bytes:
    //
    //   64 00 00 00 | 01 00 00 00 | 7F      = /roll 1 100
    //
    // Read in the inherited order this yielded minimum=100, maximum=1, which
    // tripped the range check below and dropped every roll in silence. The
    // trailing marker also has to be consumed or the packet reports unprocessed
    // tail data.
    if (recv_data.size() != 9)
    {
        return;
    }

    uint32 minimum, maximum, roll;
    uint8 marker = 0;
    recv_data >> maximum;
    recv_data >> minimum;
    recv_data >> marker;

    /** error handling **/
    if (marker != 0x7F)
    {
        return;
    }

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
    MopGroupMarkerPackets::RaidTargetRequest const request =
        MopGroupMarkerPackets::ReadRaidTargetRequest(recv_data);

    Player* player = GetPlayer();
    Group* group = NULL;
    if (request.context == 0)
        group = player->GetOriginalGroup() ? player->GetOriginalGroup() : player->GetGroup();
    else if (request.context == 1 || request.context == 0x7F)
        group = player->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    /********************/

    if (request.icon >= TARGET_ICON_COUNT)
        return;

    if (group->isRaidGroup() &&
            !group->IsLeader(player->GetObjectGuid()) &&
            !group->IsAssistant(player->GetObjectGuid()))
        return;

    group->SetTargetIcon(request.icon, player->GetObjectGuid(),
        ObjectGuid(request.targetGuid), request.context);
}

/**
 * @brief Converts the current party into a raid group.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupRaidConvertOpcode(WorldPacket& recv_data)
{
    // ONE opcode carries BOTH directions. Build 18414 writer sub_688B4B (vtable
    // D634B8 slot 1, slot 2 sub_66113B writes 812) emits a single bit and
    // flushes it, so the whole body is one byte: 0x80 set, 0x00 clear.
    //
    // The polarity is taken from the client's own Lua natives, which are
    // identical apart from that value:
    //
    //   sub_9056D2  ConvertToRaid    v5 = 1   -> 0x80
    //   sub_905736  ConvertToParty   v5 = 0   -> 0x00
    //
    // Discarding the body -- as this handler did while it was unregistered --
    // would make "Convert to Party" convert to raid instead.
    bool toRaid = false;
    if (recv_data.size() != 1)
    {
        return;
    }
    toRaid = recv_data.ReadBit();
    recv_data.ResetBitReader();

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    if (_player->InBattleGround())
    {
        return;
    }

    // A dungeon-finder group is owned by the LFG state machine, which tracks
    // its own composition; converting it out from under that would leave the
    // two disagreeing.
    if (group->isLFGGroup())
    {
        return;
    }

    /** error handling **/
    if (!group->IsLeader(GetPlayer()->GetObjectGuid()) || group->GetMembersCount() < 2)
    {
        return;
    }
    /********************/

    if (toRaid)
    {
        if (group->isRaidGroup())
        {
            return;
        }

        SendPartyResult(PARTY_OP_INVITE, "", ERR_PARTY_RESULT_OK);
        group->ConvertToRaid();
        return;
    }

    // Coming back the other way can fail -- a member parked outside the first
    // subgroup would be unreachable in a party frame -- so report rather than
    // appear to succeed.
    if (!group->ConvertToParty())
    {
        SendPartyResult(PARTY_OP_INVITE, "", ERR_NOT_LEADER);
        return;
    }

    SendPartyResult(PARTY_OP_INVITE, "", ERR_PARTY_RESULT_OK);
}

/**
 * @brief Moves a raid member into another subgroup.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupChangeSubGroupOpcode(WorldPacket& recv_data)
{
    MopGroupPromotePackets::ChangeSubGroupRequest request;
    if (!MopGroupPromotePackets::ParseChangeSubGroup(recv_data, request))
    {
        return;
    }

    uint8 const groupNr = request.subGroup;

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

    // Subgroups only exist in a raid. ConvertToParty leaves m_subGroupsCounts
    // allocated, so HasFreeSlotSubGroup still answers true for a party and this
    // handler -- unlike the assistant and party-assignment ones -- had no raid
    // test of its own.
    if (!group->isRaidGroup())
    {
        return;
    }

    // The 18414 request identifies the target by GUID, so there is no name to
    // resolve and no chance of moving a same-named character in another group.
    if (!group->IsMember(request.targetGuid))
    {
        return;
    }

    // everything is fine, do it
    if (Player* player = sObjectMgr.GetPlayer(request.targetGuid))
    {
        group->ChangeMembersGroup(player, groupNr);
    }
    else
    {
        group->ChangeMembersGroup(request.targetGuid, groupNr);
    }
}

/**
 * @brief Sets or clears the assistant leader flag for a raid member.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupAssistantLeaderOpcode(WorldPacket& recv_data)
{
    MopGroupPromotePackets::AssistantRequest request;
    if (!MopGroupPromotePackets::ParseAssistant(recv_data, request))
    {
        return;
    }

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

    // Assistant is a raid concept, and the target must actually be in the
    // group -- otherwise an arbitrary GUID would be stored against a raid slot.
    if (!group->isRaidGroup() || !group->IsMember(request.targetGuid))
    {
        return;
    }
    /********************/

    // everything is fine, do it
    group->SetAssistant(request.targetGuid, request.promote);
}

/**
 * @brief Handles the raid "Everyone is Assistant" toggle.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupEveryoneIsAssistantOpcode(WorldPacket& recv_data)
{
    // Build 18414 writer sub_666B59, vtable D62E40 slot 1; slot 2 sub_661450
    // writes opcode 481. The whole body is a 0x7F marker byte then a single
    // bit, flushed -- two bytes.
    //
    // No corpus packet exists for this opcode at 18414, so the layout rests on
    // the client writer alone. It is a rare action rather than an invented
    // value: the writer is present in the binary and the UI exposes the toggle.
    if (recv_data.size() != 2)
    {
        return;
    }

    uint8 marker = 0;
    recv_data >> marker;
    if (marker != 0x7F)
    {
        return;
    }

    bool const apply = recv_data.ReadBit();
    recv_data.ResetBitReader();

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    // Unlike individual promotion, this one is the leader's alone -- an
    // assistant could otherwise promote the whole raid to their own rank.
    if (!group->IsLeader(GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    group->SetEveryoneIsAssistant(apply);
}

/**
 * @brief Handles a player choosing their own LFG role.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupSetRolesOpcode(WorldPacket& recv_data)
{
    // Build 18414 writer sub_665BD9, vtable D6369C slot 1; slot 2 sub_660FCB
    // writes opcode 6802. Layout:
    //
    //   uint8  0x7F marker
    //   uint32 role bitmask
    //   bits   GUID mask 2,0,7,4,1,3,6,5, then FlushBits
    //   bytes  GUID 1,5,2,6,7,0,4,3, each ^1, omitted when zero
    //
    // Verified byte-exact against 7F 08 00 00 00 EC C9 3D 05 E9 04.
    if (recv_data.rpos() != 0 || recv_data.size() < 6)
    {
        recv_data.rfinish();
        return;
    }

    uint8 marker = 0;
    uint32 roles = 0;
    recv_data >> marker;
    if (marker != 0x7F)
    {
        recv_data.rfinish();
        return;
    }
    recv_data >> roles;

    static uint8 const maskOrder[8] = { 2, 0, 7, 4, 1, 3, 6, 5 };
    static uint8 const byteOrder[8] = { 1, 5, 2, 6, 7, 0, 4, 3 };

    bool present[8] = { false };
    for (uint8 index : maskOrder)
    {
        present[index] = recv_data.ReadBit();
    }
    recv_data.ResetBitReader();

    size_t presentCount = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        if (present[i])
        {
            ++presentCount;
        }
    }

    if (recv_data.size() - recv_data.rpos() != presentCount)
    {
        recv_data.rfinish();
        return;
    }

    uint8 bytes[8] = { 0 };
    for (uint8 index : byteOrder)
    {
        if (!present[index])
        {
            continue;
        }
        uint8 raw = 0;
        recv_data >> raw;
        if (raw == 1)
        {
            recv_data.rfinish();
            return;
        }
        bytes[index] = raw ^ 1;
    }

    uint64 rawGuid = 0;
    for (size_t i = 0; i < 8; ++i)
    {
        rawGuid |= uint64(bytes[i]) << (i * 8);
    }
    ObjectGuid const target(rawGuid);

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    // A player always sets their OWN role, and a leader or assistant may set
    // anyone's. The client drives both from the same menu: UnitPopup.lua:1732
    // passes the right-clicked unit to UnitSetRole, and :1195 shows the Set Role
    // submenu when isLeader, isAssistant, or the unit is the player. The client
    // binary agrees -- UnitSetRole compares the target to the caller's own GUID
    // and, when they differ, requires the caller's group rank to be leader or
    // assistant before sending the TARGET's GUID.
    //
    // An earlier version accepted only the sender's own GUID, on the stated but
    // wrong premise that the body always carries it. That silently discarded
    // every role a leader assigned.
    ObjectGuid const subject = target ? target : GetPlayer()->GetObjectGuid();
    if (subject != GetPlayer()->GetObjectGuid())
    {
        if (!group->IsLeader(GetPlayer()->GetObjectGuid()) &&
            !group->IsAssistant(GetPlayer()->GetObjectGuid()))
        {
            return;
        }

        if (!group->IsMember(subject))
        {
            return;
        }
    }

    // Only tank, healer and damage exist; anything else is not a role the
    // roster or the dungeon finder can represent.
    uint8 const accepted = uint8(roles & (PLAYER_ROLE_TANK | PLAYER_ROLE_HEALER | PLAYER_ROLE_DAMAGE));
    if (uint32(accepted) != roles)
    {
        return;
    }

    group->SetMemberRoles(subject, accepted);
}

/**
 * @brief Handles a leader or assistant starting a role check.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleGroupInitiateRolePollOpcode(WorldPacket& recv_data)
{
    // Build 18414 writer sub_6696A8, vtable D63110 slot 1; slot 2 sub_660E1F
    // writes opcode 6274. The whole body is a single byte.
    if (recv_data.size() != 1)
    {
        return;
    }

    uint8 partyIndex = 0;
    recv_data >> partyIndex;

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    if (!group->IsLeader(GetPlayer()->GetObjectGuid()) &&
        !group->IsAssistant(GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    group->BeginRolePoll(GetPlayer()->GetObjectGuid());
}

/**
 * @brief Updates main tank or main assist raid assignments.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandlePartyAssignmentOpcode(WorldPacket& recv_data)
{
    MopGroupPromotePackets::PartyAssignmentRequest request;
    if (!MopGroupPromotePackets::ParsePartyAssignment(recv_data, request))
    {
        return;
    }

    uint8 const role = request.assignment;                  // 0 = Main Tank, 1 = Main Assistant
    bool const apply = request.apply;
    ObjectGuid const guid = request.targetGuid;

    Group* group = GetPlayer()->GetGroup();
    if (!group)
    {
        return;
    }

    /** error handling **/
    // Assistants may set these too, matching the client, which shows the menu
    // entries for leader and assistant alike.
    if (!group->IsLeader(GetPlayer()->GetObjectGuid()) &&
        !group->IsAssistant(GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    // Main tank and main assist are raid roles, and an arbitrary GUID would be
    // stored against a slot nothing can later clear.
    if (!group->isRaidGroup() || !group->IsMember(guid))
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
