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
 * @file MovementHandler.cpp
 * @brief Movement opcode handlers
 *
 * This file handles movement-related opcodes including:
 * - MSG_MOVE_WORLDPORT_ACK: Acknowledge map teleport
 * - MSG_MOVE_TELEPORT_ACK: Acknowledge teleport
 * - MSG_MOVE_HEARTBEAT: Movement heartbeat
 * - MSG_MOVE_SET_FACING: Set facing direction
 * - MSG_MOVE_JUMP: Jump
 * - MSG_MOVE_START_FORWARD: Start moving forward
 * - MSG_MOVE_START_BACKWARD: Start moving backward
 * - MSG_MOVE_STOP: Stop movement
 * - MSG_MOVE_START_STRAFE_LEFT: Start strafing left
 * - MSG_MOVE_START_STRAFE_RIGHT: Start strafing right
 * - MSG_MOVE_START_PITCH_UP: Start pitching up
 * - MSG_MOVE_START_PITCH_DOWN: Start pitching down
 * - MSG_MOVE_SET_RUN_MODE: Set run mode
 * - MSG_MOVE_SET_WALK_MODE: Set walk mode
 * - MSG_MOVE_FALL_LAND: Land after fall
 * - MSG_MOVE_START_SWIM: Start swimming
 * - MSG_MOVE_STOP_SWIM: Stop swimming
 * - MSG_MOVE_SPLASH: Water splash
 * - MSG_MOVE_ASCEND: Ascend (flying)
 * - MSG_MOVE_DESCEND: Descend (flying)
 *
 * Movement packets are validated and synchronized with the server's
 * authoritative position to prevent cheating.
 */

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "Corpse.h"
#include "Player.h"
#include "Vehicle.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "Transports.h"
#include "BattleGround/BattleGround.h"
#include "WaypointMovementGenerator.h"
#include "MapPersistentStateMgr.h"
#include "ObjectMgr.h"

#include <cmath>

#define MOVEMENT_PACKET_TIME_DELAY 0

void WorldSession::SendTransferRoot(uint32 counter)
{
    m_pendingTransferRootCounter = counter;
    m_waitingForTransferRootAck = true;
    m_pendingSuspendToken = 0;
    m_waitingForSuspendToken = false;

    WorldPacket data;
    _player->BuildForceMoveRootPacket(&data, true, counter);
    SendPacket(&data);
}

void WorldSession::SendSuspendToken()
{
    // The suspend transaction is connection-scoped, not a movement-force
    // counter. Retail 18414 uses independent values for the transfer root and
    // suspend token (for example 77 and 17 in the same world-port sequence).
    if (++m_suspendTokenCounter == 0)
    {
        ++m_suspendTokenCounter;
    }

    uint32 const token = m_suspendTokenCounter;
    m_pendingSuspendToken = token;
    m_waitingForSuspendToken = true;

    WorldPacket data(SMSG_SUSPEND_TOKEN, 5);
    data << token;
    data.WriteBits(3u, 2); // CLIENT_SUSPEND_FOR_WORLD_PORT
    SendPacket(&data);
}

void WorldSession::HandleSuspendTokenResponse(WorldPacket& recvPacket)
{
    size_t const remaining = recvPacket.size() - recvPacket.rpos();
    if (remaining != sizeof(uint32))
    {
        DEBUG_LOG("WORLD: ignoring malformed CMSG_SUSPEND_TOKEN_RESPONSE (%u bytes).", uint32(remaining));
        return;
    }

    uint32 token = 0;
    recvPacket >> token;

    if (!_player || !_player->IsBeingTeleportedFar() || !m_waitingForSuspendToken)
    {
        DEBUG_LOG("WORLD: ignoring stale CMSG_SUSPEND_TOKEN_RESPONSE token %u.", token);
        return;
    }

    if (token != m_pendingSuspendToken)
    {
        DEBUG_LOG("WORLD: ignoring mismatched CMSG_SUSPEND_TOKEN_RESPONSE token %u (expected %u).",
                  token, m_pendingSuspendToken);
        return;
    }

    m_waitingForSuspendToken = false;

    WorldLocation const& loc = _player->GetTeleportDest();
    WorldPacket data(SMSG_NEW_WORLD, 20);
    MopWorldEntryPackets::BuildNewWorld(data, loc.mapid, loc.coord_x,
                                        loc.coord_y, loc.coord_z, loc.orientation);
    SendPacket(&data);
    _player->SendSavedInstances();
}

/**
 * @brief Handles the packet-based worldport acknowledgement.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleMoveWorldportAckOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("WORLD: got MSG_MOVE_WORLDPORT_ACK.");

    if (m_waitingForTransferRootAck || m_waitingForSuspendToken)
    {
        DEBUG_LOG("WORLD: ignoring premature MSG_MOVE_WORLDPORT_ACK while waiting for the transfer handshake.");
        return;
    }

    HandleMoveWorldportAckOpcode();
}

/**
 * @brief Finalizes a far teleport after the client acknowledges worldport.
 */
void WorldSession::HandleMoveWorldportAckOpcode()
{
    m_waitingForTransferRootAck = false;
    m_pendingTransferRootCounter = 0;
    m_waitingForSuspendToken = false;
    m_pendingSuspendToken = 0;

    // ignore unexpected far teleports
    if (!GetPlayer()->IsBeingTeleportedFar())
    {
        return;
    }

    // The client destroyed its entire object manager when it processed the
    // SMSG_NEW_WORLD that this ack answers, so everything we believed it had is
    // now gone. Forget it here, before anything else, because every failure path
    // below re-teleports and so triggers another client-side wipe anyway.
    //
    // Leaving this stale is not cosmetic. HaveAtClient() drives the create
    // decision, so an object that exists on BOTH the old and the new map and is
    // still recorded here takes the "already at client" branch on arrival and is
    // never re-created. The client has no object for it and nothing on our side
    // ever notices -- permanently invisible, and drawn with the "out of phase"
    // party icon, because UnitInPhase (sub_8A29C1) is an object-manager lookup
    // rather than a phase comparison.
    //
    // Normally the stale entry is removed for us: the departing player stops
    // being iterated on the old map, lands in VisibleNotifier's leftover set and
    // is erased there. That only happens if an observer's notifier actually runs
    // on the old map in the gap. When a whole LFG party is teleported into a
    // dungeon they all leave within the same tick, so for the FIRST player out
    // that gap never exists and no observer ever erases them.
    //
    // Measured, world-server_2026-08-07_00-06-06.log, all five entering Wailing
    // Caverns at 00:10:31 -- Humanwarrior went first and is the only one with no
    // "out of range" event against any observer, the only one with no create in
    // the instance, and at 00:15:32 all four other clients reported
    // CMSG_OBJECT_UPDATE_FAILED for exactly his guid.
    GetPlayer()->m_clientGUIDs.clear();

    // Same reasoning: these are queued value re-sends aimed at objects the
    // client has just discarded. Delivering one now would only produce a
    // spurious CMSG_OBJECT_UPDATE_FAILED for an object it is correct not to have.
    GetPlayer()->ClearPendingEmoteRefresh();

    // get start teleport coordinates (will used later in fail case)
    WorldLocation old_loc;
    GetPlayer()->GetPosition(old_loc);

    // get the teleport destination
    WorldLocation& loc = GetPlayer()->GetTeleportDest();

    // possible errors in the coordinate validity check (only cheating case possible)
    if (!MapManager::IsValidMapCoord(loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation))
    {
        sLog.outError("WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far to a not valid location "
                      "(map:%u, x:%f, y:%f, z:%f) We port him to his homebind instead..",
                      GetPlayer()->GetGuidStr().c_str(), loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z);
        // stop teleportation else we would try this again and again in LogoutPlayer...
        GetPlayer()->SetSemaphoreTeleportFar(false);
        // and teleport the player to a valid place
        GetPlayer()->TeleportToHomebind();
        return;
    }

    // get the destination map entry, not the current one, this will fix homebind and reset greeting
    MapEntry const* mEntry = sMapStore.LookupEntry(loc.mapid);

    Map* map = NULL;

    // prevent crash at attempt landing to not existed battleground instance
    if (mEntry->IsBattleGroundOrArena())
    {
        if (GetPlayer()->GetBattleGroundId())
        {
            map = sMapMgr.FindMap(loc.mapid, GetPlayer()->GetBattleGroundId());
        }

        if (!map)
        {
            DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far to nonexisten battleground instance "
                       " (map:%u, x:%f, y:%f, z:%f) Trying to port him to his previous place..",
                       GetPlayer()->GetGuidStr().c_str(), loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z);

            GetPlayer()->SetSemaphoreTeleportFar(false);

            // Teleport to previous place, if can not be ported back TP to homebind place
            if (!GetPlayer()->TeleportTo(old_loc))
            {
                DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s can not be ported to his previous place, teleporting him to his homebind place...",
                           GetPlayer()->GetGuidStr().c_str());
                GetPlayer()->TeleportToHomebind();
            }
            return;
        }
    }

    InstanceTemplate const* mInstance = ObjectMgr::GetInstanceTemplate(loc.mapid);

    // reset instance validity, except if going to an instance inside an instance
    if (GetPlayer()->m_InstanceValid == false && !mInstance)
    {
        GetPlayer()->m_InstanceValid = true;
    }

    GetPlayer()->SetSemaphoreTeleportFar(false);

    // relocate the player to the teleport destination
    if (!map)
    {
        map = sMapMgr.CreateMap(loc.mapid, GetPlayer());
    }

    GetPlayer()->SetMap(map);
    GetPlayer()->Relocate(loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation);

    GetPlayer()->SendInitialPacketsBeforeAddToMap();
    // the CanEnter checks are done in TeleporTo but conditions may change
    // while the player is in transit, for example the map may get full
    if (!GetPlayer()->GetMap()->Add(GetPlayer()))
    {
        // if player wasn't added to map, reset his map pointer!
        GetPlayer()->ResetMap();

        DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far but couldn't be added to map "
                   " (map:%u, x:%f, y:%f, z:%f) Trying to port him to his previous place..",
                   GetPlayer()->GetGuidStr().c_str(), loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z);

        // Teleport to previous place, if can not be ported back TP to homebind place
        if (!GetPlayer()->TeleportTo(old_loc))
        {
            DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s can not be ported to his previous place, teleporting him to his homebind place...",
                       GetPlayer()->GetGuidStr().c_str());
            GetPlayer()->TeleportToHomebind();
        }
        return;
    }

    // battleground state prepare (in case join to BG), at relogin/tele player not invited
    // only add to bg group and object, if the player was invited (else he entered through command)
    if (_player->InBattleGround())
    {
        // cleanup setting if outdated
        if (!mEntry->IsBattleGroundOrArena())
        {
            // We're not in BG
            _player->SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);
            // reset destination bg team
            _player->SetBGTeam(TEAM_NONE);
        }
        // join to bg case
        else if (BattleGround* bg = _player->GetBattleGround())
        {
            if (_player->IsInvitedForBattleGroundInstance(_player->GetBattleGroundId()))
            {
                bg->AddPlayer(_player);
            }
        }
    }

    GetPlayer()->SendInitialPacketsAfterAddToMap();

    // flight fast teleport case
    if (GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
    {
        if (!_player->InBattleGround())
        {
            // short preparations to continue flight
            FlightPathMovementGenerator* flight = (FlightPathMovementGenerator*)(GetPlayer()->GetMotionMaster()->top());
            flight->Reset(*GetPlayer());
            return;
        }

        // battleground state prepare, stop flight
        GetPlayer()->GetMotionMaster()->MovementExpired();
        GetPlayer()->m_taxi.ClearTaxiDestinations();
    }

    if (mInstance)
    {
        Difficulty diff = GetPlayer()->GetDifficulty(mEntry->IsRaid());
        if (MapDifficultyEntry const* mapDiff = GetMapDifficultyData(mEntry->ID, diff))
        {
            if (mapDiff->RaidDuration)
            {
                if (time_t timeReset = sMapPersistentStateMgr.GetScheduler().GetResetTimeFor(mEntry->ID, diff))
                {
                    uint32 timeleft = uint32(timeReset - time(NULL));
                    GetPlayer()->SendInstanceResetWarning(mEntry->ID, diff, timeleft);
                }
            }
        }
    }

    // mount allow check
    if (!mEntry->IsMountAllowed())
    {
        _player->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);
        _player->RemoveSpellsCausingAura(SPELL_AURA_FLY);
    }
    else
    {
        // recheck mount capabilities at far teleport
        Unit::AuraList const& mMountAuras = _player->GetAurasByType(SPELL_AURA_MOUNTED);
        for (Unit::AuraList::const_iterator itr = mMountAuras.begin(); itr != mMountAuras.end(); )
        {
            Aura const* aura = *itr;

            // mount is no longer suitable
            MountCapabilityEntry const* entry = _player->GetMountCapability(aura->GetSpellEffect()->EffectMiscValueB);
            if (!entry)
            {
                _player->RemoveAurasDueToSpell(aura->GetId());
                itr = mMountAuras.begin();
                continue;
            }

            // mount capability changed
            if (entry->ID != aura->GetModifier()->m_amount)
            {
                if (MountCapabilityEntry const* oldEntry = sMountCapabilityStore.LookupEntry(aura->GetModifier()->m_amount))
                {
                    _player->RemoveAurasDueToSpell(oldEntry->SpeedModSpell);
                }

                _player->CastSpell(_player, entry->SpeedModSpell, true);

                const_cast<Aura*>(aura)->ChangeAmount(entry->ID);
            }

            ++itr;
        }

        uint32 zone, area;
        _player->GetZoneAndAreaId(zone, area);
        // recheck fly auras
        Unit::AuraList const& mFlyAuras = _player->GetAurasByType(SPELL_AURA_FLY);
        for (Unit::AuraList::const_iterator itr = mFlyAuras.begin(); itr != mFlyAuras.end(); )
        {
            Aura const* aura = *itr;
            if (!_player->CanStartFlyInArea(_player->GetMapId(), zone, area))
            {
                _player->RemoveAurasDueToSpell(aura->GetId());
                itr = mFlyAuras.begin();
                continue;
            }

            ++itr;
        }
    }

    // honorless target
    if (GetPlayer()->pvpInfo.inHostileArea)
    {
        GetPlayer()->CastSpell(GetPlayer(), 2479, true);
    }

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    // lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();

    // notify group after successful teleport
    if (_player->GetGroup())
    {
        _player->SetGroupUpdateFlag(GROUP_UPDATE_FULL);
    }
}

/**
 * @brief Finalizes a near teleport after the client acknowledges it.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleMoveTeleportAckOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_MOVE_TELEPORT_ACK");

    ObjectGuid guid;
    // 18414 writes its own millisecond timestamp first, then echoes the counter
    // the server put in SMSG_MOVE_TELEPORT. Reading them the other way round is
    // why the log printed "Counter <ms timestamp>, time 0" on every ack: the
    // zero is our own counter coming back, and the timestamp is the client's.
    uint32 clientTime, counter;
    recv_data >> clientTime >> counter;

    // The mover guid follows as a packed guid. The bit/byte permutation below
    // is NOT wire-confirmed: every character on this realm has a guid under 256,
    // so only one mask bit is ever set and only the first slot can be observed.
    // The captured acks carry mask 0x80 with a single trailing byte equal to the
    // player's guid, which places the first-read bit at byte 0 -- the order below
    // put it at byte 5, so the parse yielded 0x0000030000000000 and printed as
    // "Guid: 0". Byte consumption is permutation-independent (one mask byte, then
    // popcount(mask) bytes), so the remaining seven slots cannot be recovered
    // from our own traffic and are left as found rather than invented.
    recv_data.ReadGuidMask<5, 0, 1, 6, 3, 7, 2, 4>(guid);
    recv_data.ReadGuidBytes<4, 2, 7, 6, 5, 1, 3, 0>(guid);

    DEBUG_LOG("Guid: %s", guid.GetString().c_str());
    DEBUG_LOG("Counter %u, clientTime %u", counter, clientTime / IN_MILLISECONDS);

    Unit* mover = _player->GetMover();
    Player* plMover = mover->GetTypeId() == TYPEID_PLAYER ? (Player*)mover : NULL;

    if (!plMover || !plMover->IsBeingTeleportedNear())
    {
        return;
    }

    // No guid equality check. It rejected every valid ack -- the parse above
    // cannot be trusted until the permutation is recovered, and the guard adds
    // nothing here: this opcode arrives on the player's own authenticated
    // session, plMover comes from that session, and IsBeingTeleportedNear()
    // already establishes that a near teleport is outstanding. Rejecting on a
    // mis-parsed guid meant SetPosition() below never ran, so the server left
    // the player at the origin while the client had already moved -- no grid
    // transition, no visibility rebuild, and an empty destination until relog.

    plMover->SetSemaphoreTeleportNear(false);

    uint32 old_zone = plMover->GetZoneId();

    WorldLocation const& dest = plMover->GetTeleportDest();

    plMover->SetPosition(dest.coord_x, dest.coord_y, dest.coord_z, dest.orientation, true);

    uint32 newzone, newarea;
    plMover->GetZoneAndAreaId(newzone, newarea);
    plMover->UpdateZone(newzone, newarea);

    // new zone
    if (old_zone != newzone)
    {
        // honorless target
        if (plMover->pvpInfo.inHostileArea)
        {
            plMover->CastSpell(plMover, 2479, true);
        }
    }

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    // lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();
}

/**
 * @brief Processes standard client movement updates.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleMovementOpcodes(WorldPacket& recv_data)
{
    uint16 opcode = recv_data.GetOpcode();
    if (!sLog.HasLogFilter(LOG_FILTER_PLAYER_MOVES))
    {
        DEBUG_LOG("WORLD: Received opcode %s (%u, 0x%X)", LookupOpcodeName(DIR_CLIENT, opcode), opcode, opcode);
        recv_data.hexlike();
    }

    Unit* mover = _player->GetMover();
    Player* plMover = mover->GetTypeId() == TYPEID_PLAYER ? (Player*)mover : NULL;

    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode and WorldSession::HandleMoveTeleportAck
    if (plMover && plMover->IsBeingTeleported())
    {
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    /* extract packet */
    MovementInfo movementInfo;
    recv_data >> movementInfo;
    /*----------------*/

    if (!VerifyMovementInfo(movementInfo, movementInfo.GetGuid()))
    {
        return;
    }

    // fall damage generation (ignore in flight case that can be triggered also at lags in moment teleportation to another map).
    if (opcode == CMSG_MOVE_FALL_LAND && plMover && !plMover->IsTaxiFlying())
    {
        plMover->HandleFall(movementInfo);
    }

    /* process position-change */
    HandleMoverRelocation(movementInfo);

    if (plMover)
    {
        plMover->UpdateFallInformationIfNeed(movementInfo, opcode);
    }

    // stop some emotes at player move
    if (mover && (mover->GetUInt32Value(UNIT_NPC_EMOTESTATE) != 0))
    {
        mover->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_ONESHOT_NONE);
    }

    WorldPacket data(SMSG_PLAYER_MOVE, recv_data.size());
    data << movementInfo;
    mover->SendMessageToSetExcept(&data, _player);
}

/**
 * @brief Verifies client acknowledgement packets for forced speed changes.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleForceSpeedChangeAckOpcodes(WorldPacket& recv_data)
{
    uint16 opcode = recv_data.GetOpcode();
    DEBUG_LOG("WORLD: Received %s (%u, 0x%X) opcode", LookupOpcodeName(DIR_CLIENT, recv_data.GetOpcode()), opcode, opcode);

    /* extract packet */
    MovementInfo movementInfo;
    float  newspeed;

    // Only four of the nine acknowledgements LEAD with the speed. The other five
    // carry it among the leading scalars in their own per-opcode order, so it
    // arrives through the movement block as MSESpeedFloat instead. Reading a
    // leading float from one of those would consume a coordinate and desync
    // everything after it.
    switch (opcode)
    {
        case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:
        case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:
        case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:
        case CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK:
            recv_data >> newspeed;
            recv_data >> movementInfo;
            break;
        default:
            recv_data >> movementInfo;
            newspeed = movementInfo.GetSpeedFloat();
            break;
    }

    // A non-finite speed defeats the check below entirely: every comparison
    // against NaN is false, so fabs(expected - NaN) > 0.01f does not fire and the
    // acknowledgement is accepted with neither a correction nor a kick. The
    // client has no reason to send one, which is precisely why it is worth
    // rejecting here rather than trusting the arithmetic downstream.
    //
    // This must come before the forced-change bookkeeping, or a NaN would also
    // consume the pending-change credit that suppresses the check.
    if (!std::isfinite(newspeed))
    {
        sLog.outError("%s: player %s sent a non-finite speed, ignored",
                      LookupClientOpcodeName(uint16(opcode)), _player->GetName());
        recv_data.rpos(recv_data.wpos());
        return;
    }

    // now can skip not our packet
    if (_player->GetObjectGuid() != movementInfo.GetGuid())
    {
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }
    /*----------------*/

    // client ACK send one packet for mounted/run case and need skip all except last from its
    // in other cases anti-cheat check can be fail in false case
    UnitMoveType move_type;
    UnitMoveType force_move_type;

    static char const* move_type_name[MAX_MOVE_TYPE] = {  "Walk", "Run", "RunBack", "Swim", "SwimBack", "TurnRate", "Flight", "FlightBack", "PitchRate" };

    switch (opcode)
    {
        case CMSG_FORCE_WALK_SPEED_CHANGE_ACK:          move_type = MOVE_WALK;          force_move_type = MOVE_WALK;        break;
        case CMSG_FORCE_RUN_SPEED_CHANGE_ACK:           move_type = MOVE_RUN;           force_move_type = MOVE_RUN;         break;
        case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:      move_type = MOVE_RUN_BACK;      force_move_type = MOVE_RUN_BACK;    break;
        case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:          move_type = MOVE_SWIM;          force_move_type = MOVE_SWIM;        break;
        case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:     move_type = MOVE_SWIM_BACK;     force_move_type = MOVE_SWIM_BACK;   break;
        case CMSG_FORCE_TURN_RATE_CHANGE_ACK:           move_type = MOVE_TURN_RATE;     force_move_type = MOVE_TURN_RATE;   break;
        case CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK:        move_type = MOVE_FLIGHT;        force_move_type = MOVE_FLIGHT;      break;
        case CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK:   move_type = MOVE_FLIGHT_BACK;   force_move_type = MOVE_FLIGHT_BACK; break;
        case CMSG_FORCE_PITCH_RATE_CHANGE_ACK:          move_type = MOVE_PITCH_RATE;    force_move_type = MOVE_PITCH_RATE;  break;
        default:
            sLog.outError("WorldSession::HandleForceSpeedChangeAck: Unknown move type opcode: %u", opcode);
            return;
    }

    // skip all forced speed changes except last and unexpected
    // in run/mounted case used one ACK and it must be skipped.m_forced_speed_changes[MOVE_RUN} store both.
    if (_player->m_forced_speed_changes[force_move_type] > 0)
    {
        --_player->m_forced_speed_changes[force_move_type];
        if (_player->m_forced_speed_changes[force_move_type] > 0)
        {
            return;
        }
    }

    if (!_player->GetTransport() && fabs(_player->GetSpeed(move_type) - newspeed) > 0.01f)
    {
        if (_player->GetSpeed(move_type) > newspeed)        // must be greater - just correct
        {
            sLog.outError("%sSpeedChange player %s is NOT correct (must be %f instead %f), force set to correct value",
                          move_type_name[move_type], _player->GetName(), _player->GetSpeed(move_type), newspeed);

            // ignoreChange MUST be true. This resends the rate the player
            // already has, and SetSpeedRate opens with
            //     if (m_speed_rate[mtype] != rate || ignoreChange)
            // so without it the entire body is skipped: no packet, no forced
            // change registered, no correction. The client keeps the speed the
            // server just rejected and acknowledges it again, and each
            // acknowledgement writes another unthrottled line to the error log.
            // A disagreement that should self-heal in one round trip instead
            // looped for as long as the session lasted.
            //
            // Sending it for real also balances the books: the forced-change
            // counter this increments is what the next acknowledgement
            // decrements, which is the mechanism that stops the check firing on
            // a change the server itself made.
            _player->SetSpeedRate(move_type, _player->GetSpeedRate(move_type), true, true);
        }
        else                                                // must be lesser - cheating
        {
            BASIC_LOG("Player %s from account id %u kicked for incorrect speed (must be %f instead %f)",
                      _player->GetName(), _player->GetSession()->GetAccountId(), _player->GetSpeed(move_type), newspeed);
            _player->GetSession()->KickPlayer();
        }
    }
}

/**
 * @brief Validates the active mover guid reported by the client.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetActiveMoverOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_ACTIVE_MOVER");
    MopControlPackets::ActiveMoverRequest request;
    if (!MopControlPackets::ReadSetActiveMover(recv_data, request))
    {
        sLog.outError("HandleSetActiveMoverOpcode: malformed mover guid");
        return;
    }
    ObjectGuid guid(request.moverGuid);

    if (_player->GetMover()->GetObjectGuid() != guid)
    {
        sLog.outError("HandleSetActiveMoverOpcode: incorrect mover guid: mover is %s and should be %s",
                      _player->GetMover()->GetGuidStr().c_str(), guid.GetString().c_str());
        return;
    }
    else
    {
        if (Unit* mover = sObjectAccessor.GetUnit(*GetPlayer(), guid))
        {
            _player->SetMover(mover);
        }
    }
}

/**
 * @brief Stores movement info sent for a non-active mover.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleMoveNotActiveMoverOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_NOT_ACTIVE_MOVER");
    recv_data.hexlike();

    MovementInfo mi;
    recv_data >> mi;

    if (_player->GetMover()->GetObjectGuid() == mi.GetGuid())
    {
        sLog.outError("HandleMoveNotActiveMover: incorrect mover guid: mover is %s and should be %s instead of %s",
                      _player->GetMover()->GetGuidStr().c_str(),
                      _player->GetGuidStr().c_str(),
                      mi.GetGuid().GetString().c_str());
        return;
    }

    _player->m_movementInfo = mi;
}

/**
 * @brief Broadcasts the player's mount special animation.
 *
 * @param recvdata The received opcode packet.
 */
void WorldSession::HandleMountSpecialAnimOpcode(WorldPacket& /*recvdata*/)
{
    // DEBUG_LOG("WORLD: Received opcode CMSG_MOUNTSPECIAL_ANIM");

    WorldPacket data(SMSG_MOUNTSPECIAL_ANIM, 8);
    data << GetPlayer()->GetObjectGuid();

    GetPlayer()->SendMessageToSet(&data, false);
}

/**
 * @brief Handles knockback acknowledgement movement updates.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleMoveKnockBackAck(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_MOVE_KNOCK_BACK_ACK");

    Unit* mover = _player->GetMover();
    Player* plMover = mover->GetTypeId() == TYPEID_PLAYER ? (Player*)mover : NULL;

    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode and WorldSession::HandleMoveTeleportAck
    if (plMover && plMover->IsBeingTeleported())
    {
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    MovementInfo movementInfo;
    recv_data >> movementInfo;

    if (!VerifyMovementInfo(movementInfo, movementInfo.GetGuid()))
    {
        return;
    }

    HandleMoverRelocation(movementInfo);

    WorldPacket data(SMSG_MOVE_UPDATE_KNOCK_BACK, recv_data.size() + 15);
    data << movementInfo;
    mover->SendMessageToSetExcept(&data, _player);
}

/**
 * @brief Sends a knockback packet to the client.
 *
 * @param angle The horizontal knockback angle.
 * @param horizontalSpeed The horizontal speed component.
 * @param verticalSpeed The vertical speed component.
 */
void WorldSession::SendKnockBack(float angle, float horizontalSpeed, float verticalSpeed)
{
    ObjectGuid guid = GetPlayer()->GetObjectGuid();
    float vsin = sin(angle);
    float vcos = cos(angle);

    WorldPacket data(SMSG_MOVE_KNOCK_BACK, 29);
    // Not 0: the client reads counter == 0 as "this change originated here",
    // because its own local entry points pass 0. Knockback is server-originated.
    uint32 const counter = GetPlayer()->NextMovementCounter();
    MopCompactPackets::BuildMoveKnockBack(data, guid.GetRawValue(), counter,
        horizontalSpeed, verticalSpeed, vcos, vsin);
    SendPacket(&data);
}

/**
 * @brief Handles hover movement acknowledgement packets.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleMoveHoverAck(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_MOVE_HOVER_ACK");
    uint64 guid;
    guid = recv_data.readPackGUID(); // unused
    recv_data.read_skip<uint32>();

    MovementInfo movementInfo;
    recv_data >> movementInfo;
    recv_data.read_skip<uint32>();

    /*
    MovementInfo movementInfo;
    recv_data >> movementInfo;
    */
}

/**
 * @brief Handles water-walk acknowledgement packets.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleMoveWaterWalkAck(WorldPacket& recv_data)
{
    MovementInfo movementInfo;
    recv_data >> movementInfo;

    if (recv_data.rpos() != recv_data.size() ||
        !VerifyMovementInfo(movementInfo, movementInfo.GetGuid()))
    {
        return;
    }

    DEBUG_LOG("CMSG_MOVE_WATER_WALK_ACK: mover %s counter %u",
        movementInfo.GetGuid().GetString().c_str(), movementInfo.GetMovementCounter());
}

/**
 * @brief Handles the client's response to a summon request.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSummonResponseOpcode(WorldPacket& recv_data)
{
    if (!_player->IsAlive() || _player->IsInCombat())
    {
        return;
    }

    ObjectGuid summonerGuid;
    bool agree;
    recv_data >> summonerGuid;
    recv_data >> agree;

    _player->SummonIfPossible(agree);
}

/**
 * @brief Verifies movement data for a specific mover guid.
 *
 * @param movementInfo The movement state to validate.
 * @param guid The expected mover guid.
 * @return true if the movement data is valid; otherwise false.
 */
bool WorldSession::VerifyMovementInfo(MovementInfo const& movementInfo, ObjectGuid const& guid) const
{
    // ignore wrong guid (player attempt cheating own session for not own guid possible...)
    if (guid != _player->GetMover()->GetObjectGuid())
    {
        return false;
    }

    return VerifyMovementInfo(movementInfo);
}

/**
 * @brief Verifies movement coordinates and transport offsets.
 *
 * @param movementInfo The movement state to validate.
 * @return true if the movement data is valid; otherwise false.
 */
bool WorldSession::VerifyMovementInfo(MovementInfo const& movementInfo) const
{
    if (!MaNGOS::IsValidMapCoord(movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z, movementInfo.GetPos()->o))
    {
        return false;
    }

    if (movementInfo.GetTransportGuid())
    {
        // transports size limited
        // (also received at zeppelin/lift leave by some reason with t_* as absolute in continent coordinates, can be safely skipped)
        if (movementInfo.GetTransportPos()->x > 50 || movementInfo.GetTransportPos()->y > 50 || movementInfo.GetTransportPos()->z > 100)
        {
            return false;
        }

        if (!MaNGOS::IsValidMapCoord(movementInfo.GetPos()->x + movementInfo.GetTransportPos()->x, movementInfo.GetPos()->y + movementInfo.GetTransportPos()->y,
                                     movementInfo.GetPos()->z + movementInfo.GetTransportPos()->z, movementInfo.GetPos()->o + movementInfo.GetTransportPos()->o))
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Applies validated movement info to the current mover.
 *
 * @param movementInfo The movement state to apply.
 */
void WorldSession::HandleMoverRelocation(MovementInfo& movementInfo)
{
    //if (m_clientTimeDelay == 0)
    //{
    //    m_clientTimeDelay = GameTime::GetGameTimeMS - movementInfo.GetTime();
    //}
    //movementInfo.UpdateTime(movementInfo.GetTime() + m_clientTimeDelay + MOVEMENT_PACKET_TIME_DELAY);
    movementInfo.UpdateTime(movementInfo.GetTime() + GetLatency());

    Unit* mover = _player->GetMover();

    if (Player* plMover = mover->GetTypeId() == TYPEID_PLAYER ? (Player*)mover : NULL)
    {
        if (movementInfo.GetTransportGuid())
        {
            if (!plMover->m_transport)
            {
                // elevators also cause the client to send transport guid - just unmount if the guid can be found in the transport list
                for (MapManager::TransportSet::const_iterator iter = sMapMgr.m_Transports.begin(); iter != sMapMgr.m_Transports.end(); ++iter)
                {
                    if ((*iter)->GetObjectGuid() == movementInfo.GetTransportGuid())
                    {
                        plMover->m_transport = (*iter);
                        (*iter)->AddPassenger(plMover);
                        break;
                    }
                }
            }
        }
        else if (plMover->m_transport)               // if we were on a transport, leave
        {
            plMover->m_transport->RemovePassenger(plMover);
            plMover->m_transport = NULL;
            movementInfo.ClearTransportData();
        }

        if (movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING) != plMover->IsInWater())
        {
            // now client not include swimming flag in case jumping under water
            plMover->SetInWater(!plMover->IsInWater() || plMover->GetTerrain()->IsUnderWater(movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z));
        }

        plMover->SetPosition(movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z, movementInfo.GetPos()->o);
        plMover->m_movementInfo = movementInfo;

        /* Movement should cancel looting */
        if (ObjectGuid lootGUID = plMover->GetLootGuid())
        {
            plMover->SendLootRelease(lootGUID);
        }

        if (movementInfo.GetPos()->z < -500.0f)
        {
            if (plMover->GetBattleGround()
                && plMover->GetBattleGround()->HandlePlayerUnderMap(_player))
            {
                // do nothing, the handle already did if returned true
            }
            else
            {
                // NOTE: this is actually called many times while falling
                // even after the player has been teleported away
                // TODO: discard movement packets after the player is rooted
                if (plMover->IsAlive())
                {
                    plMover->EnvironmentalDamage(DAMAGE_FALL_TO_VOID, plMover->GetMaxHealth());
                    // pl can be alive if GM/etc
                    if (!plMover->IsAlive())
                    {
                        // change the death state to CORPSE to prevent the death timer from
                        // starting in the next player update
                        plMover->KillPlayer();
                        plMover->BuildPlayerRepop();
                    }
                }

                // cancel the death timer here if started
                plMover->RepopAtGraveyard();
            }
        }
    }
    else                                                    // creature charmed
    {
        if (mover->IsInWorld() && mover->GetTypeId() == TYPEID_UNIT)
        {
            mover->GetMap()->CreatureRelocation((Creature*)mover, movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z, movementInfo.GetPos()->o);
        }
    }
}
