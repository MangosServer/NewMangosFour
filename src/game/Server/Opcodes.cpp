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
 * @file Opcodes.cpp
 * @brief Network opcode handler registration
 *
 * This file registers all network packet handlers for the world server.
 * It maps each opcode to its corresponding handler function in WorldSession,
 * along with session status requirements and processing mode.
 *
 * Opcode processing modes:
 * - PROCESS_INPLACE: Process immediately in network thread
 * - PROCESS_THREADUNSAFE: Process in world update thread
 *
 * Session status requirements:
 * - STATUS_NEVER: Never process (deprecated/debug opcodes)
 * - STATUS_LOGGEDIN: Require player to be logged in
 * - STATUS_UNHANDLED: No handler assigned
 *
 * @see Opcodes.h for opcode definitions
 * @see WorldSession for packet handler implementations
 */

#include "Opcodes.h"
#include "WorldSession.h"

/**
 * @brief Static integrity metadata for the Phase 1a login closure.
 *
 * Generated from out/phase1a_closure.txt + Four's real OPCODE() bindings.
 */
#include "opcode_closure.inc"

/// Correspondence between opcodes and their handlers, split by wire direction.
OpcodeHandler clientOpcodeTable[OPCODE_TABLE_SIZE];
OpcodeHandler serverOpcodeTable[OPCODE_TABLE_SIZE];

/**
 * @brief Register a client-received (inbound) opcode with its real handler.
 */
static void DefC(uint16 v, char const* name, SessionStatus s, PacketProcessing p, void (WorldSession::*h)(WorldPacket&))
{
    MANGOS_ASSERT(v < OPCODE_TABLE_SIZE);
    clientOpcodeTable[v] = OpcodeHandler{ name, s, p, h };
}

/**
 * @brief Register a server-sent (outbound) opcode name for logging metadata.
 */
static void DefS(uint16 v, char const* name)
{
    MANGOS_ASSERT(v < OPCODE_TABLE_SIZE);
    serverOpcodeTable[v] = OpcodeHandler{ name, STATUS_NEVER, PROCESS_INPLACE, &WorldSession::Handle_ServerSide };
}

/**
 * @brief Look up a dispatchable client opcode.
 * @return nullptr if out of range or not handled, otherwise the handler entry.
 */
OpcodeHandler const* LookupClientOpcode(uint16 value)
{
    if (value >= OPCODE_TABLE_SIZE)
    {
        return nullptr;
    }
    OpcodeHandler const& h = clientOpcodeTable[value];
    return h.status == STATUS_UNHANDLED ? nullptr : &h;
}

/// @brief Human-readable name of a client-direction opcode (greeting handled out-of-band).
char const* LookupClientOpcodeName(uint16 value)
{
    return value >= OPCODE_TABLE_SIZE ? (value == MSG_WOW_CONNECTION ? "MSG_WOW_CONNECTION" : "OUT_OF_RANGE") : clientOpcodeTable[value].name;
}

/// @brief Human-readable name of a server-direction opcode (greeting handled out-of-band).
char const* LookupServerOpcodeName(uint16 value)
{
    return value >= OPCODE_TABLE_SIZE ? (value == MSG_WOW_CONNECTION ? "MSG_WOW_CONNECTION" : "OUT_OF_RANGE") : serverOpcodeTable[value].name;
}

/// @brief Direction-aware opcode name lookup for human understandable logging.
char const* LookupOpcodeName(PacketDirection dir, uint16 value)
{
    return dir == DIR_CLIENT ? LookupClientOpcodeName(value) : LookupServerOpcodeName(value);
}

/**
 * @brief Verify the login closure registered exactly as its generated metadata expects.
 *
 * Confirms the greeting is rejected-but-named, every client opcode is dispatchable to
 * its real (or synthetic socket) handler, and server opcode names resolve.
 */
static void AssertLoginClosureIntegrity()
{
    for (auto const& c : kLoginClosure)
    {
        if (c.out_of_band)
        {
            MANGOS_ASSERT(LookupClientOpcode(uint16(c.value)) == nullptr);                       // greeting rejected, not aliased
            MANGOS_ASSERT(std::string(LookupClientOpcodeName(uint16(c.value))) == "MSG_WOW_CONNECTION");
            continue;
        }
        if (c.client)
        {
            OpcodeHandler const* actual = LookupClientOpcode(uint16(c.value));
            MANGOS_ASSERT(actual != nullptr);                                                     // dispatchable
            MANGOS_ASSERT(std::string(LookupClientOpcodeName(uint16(c.value))) == c.name);        // name matches
            MANGOS_ASSERT(actual->handler == c.handler);                                          // REAL/synthetic socket binding matches
        }
        else
        {
            MANGOS_ASSERT(std::string(LookupServerOpcodeName(uint16(c.value))) == c.name);        // metadata name
        }
    }
}

/**
 * @brief Initialize opcode handler metadata tables.
 *
 * Fills both direction tables with unhandled defaults, then registers the Phase 1a
 * login closure and asserts its integrity. The greeting (MSG_WOW_CONNECTION) is NOT
 * registered; it is handled out-of-band by WorldSocket.
 */
void InitializeOpcodes()
{
    for (int i = 0; i < OPCODE_TABLE_SIZE; ++i)
    {
        clientOpcodeTable[i] = OpcodeHandler{ "UNKNOWN", STATUS_UNHANDLED, PROCESS_INPLACE, &WorldSession::Handle_NULL };
        serverOpcodeTable[i] = OpcodeHandler{ "UNKNOWN", STATUS_NEVER, PROCESS_INPLACE, &WorldSession::Handle_ServerSide };
    }
#include "opcode_register.inc"     // login closure only (Phase 1a); greeting NOT registered
    AssertLoginClosureIntegrity();

    // --- Opcodes registered beyond the Phase 1a login closure (kept here so they survive
    //     regeneration of opcode_register.inc). ---

    // CMSG_CHAR_DELETE (0x04E2) / SMSG_CHAR_DELETE (0x0C9F): delete a character from char-select.
    // The handler already exists; MoP sends the GUID bit-packed (decoded in HandleCharDeleteOpcode).
    DefC(CMSG_CHAR_DELETE, "CMSG_CHAR_DELETE", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleCharDeleteOpcode);
    DefS(SMSG_CHAR_DELETE, "SMSG_CHAR_DELETE");

    // Character customization request/response at character select. Both bodies use the
    // 18414 GUID bit/byte permutations recovered directly from Wow.exe.
    DefC(CMSG_CHAR_CUSTOMIZE, "CMSG_CHAR_CUSTOMIZE", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleCharCustomizeOpcode);
    DefS(SMSG_CHAR_CUSTOMIZE, "SMSG_CHAR_CUSTOMIZE");

    // The direct 18414 writers prove the upload's three-u32/blob/3-bit-type body (0x0068) and the
    // download request's 3-bit type body (0x1D8A).
    DefC(CMSG_UPDATE_ACCOUNT_DATA, "CMSG_UPDATE_ACCOUNT_DATA", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleUpdateAccountData);

    // CMSG_REQUEST_ACCOUNT_DATA (0x1D8A): the DOWNLOAD counterpart of the upload above. The client
    // sends it when its local cache is older than the server's stored account data (as reported by
    // SMSG_ACCOUNT_DATA_TIMES); without this it dispatched as UNKNOWN and saved macros/config were
    // never served back (Codex PR #15 finding).
    DefC(CMSG_REQUEST_ACCOUNT_DATA, "CMSG_REQUEST_ACCOUNT_DATA", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestAccountData);
    // The direct 18414 reader sub_6F1A32 proves the complete 0x0AAE reply body, including the
    // non-empty per-character GUID permutation.
    DefS(SMSG_UPDATE_ACCOUNT_DATA, "SMSG_UPDATE_ACCOUNT_DATA");

    // Shipped UI C_PurchaseAPI.GetPurchaseList maps through the retained API
    // table directly to the empty 0x18B2 writer. The Store response/backend is
    // not implemented, so this registration is intentionally recognition-only.
    DefC(CMSG_BATTLE_PAY_GET_PURCHASE_LIST, "CMSG_BATTLE_PAY_GET_PURCHASE_LIST", STATUS_AUTHED, PROCESS_INPLACE, &WorldSession::Handle_NULL);

    // Wave 2 server messages whose 5.4.8 bodies are encoded by MopCompactPackets.
    DefS(SMSG_ATTACKSWING_ERROR, "SMSG_ATTACKSWING_ERROR");
    DefS(SMSG_MOVE_SET_SWIM_SPEED, "SMSG_MOVE_SET_SWIM_SPEED");
    DefS(SMSG_RANDOM_ROLL, "SMSG_RANDOM_ROLL");
    DefS(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, "SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT");
    DefS(SMSG_SET_RAID_DIFFICULTY, "SMSG_SET_RAID_DIFFICULTY");
    DefS(SMSG_SET_DUNGEON_DIFFICULTY, "SMSG_SET_DUNGEON_DIFFICULTY");
    DefS(SMSG_TRAINER_BUY_FAILED, "SMSG_TRAINER_BUY_FAILED");
    DefS(SMSG_GM_TICKET_UPDATE, "SMSG_GM_TICKET_UPDATE");
    DefC(CMSG_GMTICKET_CREATE, "CMSG_GMTICKET_CREATE", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGMTicketCreateOpcode);
    DefC(CMSG_GMTICKET_GETTICKET, "CMSG_GMTICKET_GETTICKET", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGMTicketGetTicketOpcode);
    DefS(SMSG_GMTICKET_GETTICKET, "SMSG_GMTICKET_GETTICKET");
    DefC(CMSG_GMTICKET_SYSTEMSTATUS, "CMSG_GMTICKET_SYSTEMSTATUS", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGMTicketSystemStatusOpcode);
    DefS(SMSG_GMTICKET_SYSTEMSTATUS, "SMSG_GMTICKET_SYSTEMSTATUS");
    DefC(CMSG_GM_UPDATE_TICKET_STATUS, "CMSG_GM_UPDATE_TICKET_STATUS", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGMUpdateTicketStatusOpcode);
    DefS(SMSG_GM_TICKET_CASE_STATUS, "SMSG_GM_TICKET_CASE_STATUS");
    DefS(SMSG_LOGIN_VERIFY_WORLD, "SMSG_LOGIN_VERIFY_WORLD");
    DefS(SMSG_NEW_WORLD, "SMSG_NEW_WORLD");
    DefS(SMSG_TRANSFER_PENDING, "SMSG_TRANSFER_PENDING");
    DefS(SMSG_TRANSFER_ABORTED, "SMSG_TRANSFER_ABORTED");
    DefS(SMSG_LOGIN_SETTIMESPEED, "SMSG_LOGIN_SETTIMESPEED");
    DefS(SMSG_TIME_SYNC_REQ, "SMSG_TIME_SYNC_REQ");
    DefC(CMSG_TIME_SYNC_RESP, "CMSG_TIME_SYNC_RESP", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTimeSyncResp);
    DefC(CMSG_TIME_SYNC_RESPONSE_FAILED, "CMSG_TIME_SYNC_RESPONSE_FAILED", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTimeSyncResponseFailed);
    DefC(CMSG_TIME_SYNC_RESPONSE_DROPPED, "CMSG_TIME_SYNC_RESPONSE_DROPPED", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTimeSyncResponseDropped);
    DefC(CMSG_DISCARDED_TIME_SYNC_ACKS, "CMSG_DISCARDED_TIME_SYNC_ACKS", STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT, PROCESS_THREADUNSAFE, &WorldSession::HandleDiscardedTimeSyncAcks);
    DefS(SMSG_TRIGGER_CINEMATIC, "SMSG_TRIGGER_CINEMATIC");
    DefS(SMSG_WORLD_SERVER_INFO, "SMSG_WORLD_SERVER_INFO");
    DefS(SMSG_MOTD, "SMSG_MOTD");
    DefS(SMSG_CORPSE_RECLAIM_DELAY, "SMSG_CORPSE_RECLAIM_DELAY");
    DefC(CMSG_REQUEST_FORCED_REACTIONS, "CMSG_REQUEST_FORCED_REACTIONS", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestForcedReactionsOpcode);
    DefS(SMSG_SET_FORCED_REACTIONS, "SMSG_SET_FORCED_REACTIONS");
    DefS(SMSG_INIT_WORLD_STATES, "SMSG_INIT_WORLD_STATES");
    DefS(SMSG_ITEM_TIME_UPDATE, "SMSG_ITEM_TIME_UPDATE");
    DefS(SMSG_ITEM_ENCHANT_TIME_UPDATE, "SMSG_ITEM_ENCHANT_TIME_UPDATE");
    DefS(SMSG_MOVE_TELEPORT, "SMSG_MOVE_TELEPORT");
    DefS(SMSG_CLIENT_CONTROL_UPDATE, "SMSG_CLIENT_CONTROL_UPDATE");
    DefS(SMSG_MOVE_SET_ACTIVE_MOVER, "SMSG_MOVE_SET_ACTIVE_MOVER");
    DefS(SMSG_UPDATE_CURRENCY, "SMSG_UPDATE_CURRENCY");
    DefS(SMSG_SETUP_CURRENCY, "SMSG_SETUP_CURRENCY");
    DefS(SMSG_SPELL_EXECUTE_LOG, "SMSG_SPELL_EXECUTE_LOG");
    DefS(SMSG_SPELL_PERIODIC_AURA_LOG, "SMSG_SPELL_PERIODIC_AURA_LOG");
    DefS(SMSG_SPELLDISPELLOG, "SMSG_SPELLDISPELLOG");
    DefS(SMSG_AURA_UPDATE, "SMSG_AURA_UPDATE");
    DefS(SMSG_UPDATE_OBJECT, "SMSG_UPDATE_OBJECT");
    DefS(SMSG_DESTROY_OBJECT, "SMSG_DESTROY_OBJECT");
    DefS(SMSG_MESSAGECHAT, "SMSG_MESSAGECHAT");
    DefC(CMSG_UNREGISTER_ALL_ADDON_PREFIXES, "CMSG_UNREGISTER_ALL_ADDON_PREFIXES", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleUnregisterAddonPrefixesOpcode);
    DefC(CMSG_ADDON_REGISTERED_PREFIXES, "CMSG_ADDON_REGISTERED_PREFIXES", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleAddonRegisteredPrefixesOpcode);
    DefC(CMSG_MESSAGECHAT_AFK, "CMSG_MESSAGECHAT_AFK", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMessagechatOpcode);
    DefC(CMSG_TEXT_EMOTE, "CMSG_TEXT_EMOTE", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTextEmoteOpcode);
    DefC(CMSG_EMOTE, "CMSG_EMOTE", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleEmoteOpcode);
    DefS(SMSG_TEXT_EMOTE, "SMSG_TEXT_EMOTE");
    DefS(SMSG_EMOTE, "SMSG_EMOTE");
    DefS(SMSG_NOTIFICATION, "SMSG_NOTIFICATION");
    DefS(SMSG_TRADE_STATUS, "SMSG_TRADE_STATUS");
    DefS(SMSG_TRADE_STATUS_EXTENDED, "SMSG_TRADE_STATUS_EXTENDED");

    // 18414 tutorial state requests: one uint32 flag index, then empty clear/reset controls.
    DefC(CMSG_TUTORIAL_FLAG, "CMSG_TUTORIAL_FLAG", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialFlagOpcode);
    DefC(CMSG_TUTORIAL_CLEAR, "CMSG_TUTORIAL_CLEAR", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialClearOpcode);
    DefC(CMSG_TUTORIAL_RESET, "CMSG_TUTORIAL_RESET", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTutorialResetOpcode);

    // Wave 5 regular initial UI/input envelope messages.
    DefS(SMSG_INITIAL_SPELLS, "SMSG_INITIAL_SPELLS");
    DefS(SMSG_SEND_UNLEARN_SPELLS, "SMSG_SEND_UNLEARN_SPELLS");
    DefS(SMSG_ACTION_BUTTONS, "SMSG_ACTION_BUTTONS");
    DefS(SMSG_INITIALIZE_FACTIONS, "SMSG_INITIALIZE_FACTIONS");
    DefS(SMSG_ALL_ACHIEVEMENT_DATA, "SMSG_ALL_ACHIEVEMENT_DATA");
    DefS(SMSG_BINDPOINTUPDATE, "SMSG_BINDPOINTUPDATE");
    DefS(SMSG_SET_PROFICIENCY, "SMSG_SET_PROFICIENCY");
    DefS(SMSG_WEATHER, "SMSG_WEATHER");

    // CMSG_LOGOUT_REQUEST (0x0643) is the manual "logout" API route; CMSG_LOGOUT_REQUEST_IDLE
    // (0x1349) is the distinct automatic-idle route. Both have empty bodies and use the existing logout
    // flow. CMSG_LOGOUT_CANCEL remains 0x06C1. STATUS_LOGGEDIN -- all require an in-world player
    // (the handlers dereference GetPlayer()).
    // The replies (SMSG_LOGOUT_RESPONSE/CANCEL_ACK/COMPLETE) pass the enter-world suppression via
    // IsEnterWorldConverted(); their 18414 bodies are simple (response = uint32 reason + instant bit;
    // cancel-ack/complete = empty). On the open-world start map logout is the non-instant 20s-timer
    // path (instant only in rest areas / for GMs).
    DefC(CMSG_LOGOUT_REQUEST, "CMSG_LOGOUT_REQUEST", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleLogoutRequestOpcode);
    DefC(CMSG_LOGOUT_REQUEST_IDLE, "CMSG_LOGOUT_REQUEST_IDLE", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleLogoutRequestOpcode);
    DefC(CMSG_LOGOUT_CANCEL, "CMSG_LOGOUT_CANCEL", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleLogoutCancelOpcode);
    DefS(SMSG_LOGOUT_RESPONSE, "SMSG_LOGOUT_RESPONSE");
    DefS(SMSG_LOGOUT_CANCEL_ACK, "SMSG_LOGOUT_CANCEL_ACK");
    DefS(SMSG_LOGOUT_COMPLETE, "SMSG_LOGOUT_COMPLETE");

    // Live-log worklist batch 1. Client constructors and body writers were
    // verified directly in the IDA 9.4 18414 Wow.exe database.
    DefC(CMSG_REQUEST_HOTFIX, "CMSG_REQUEST_HOTFIX", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleRequestHotfix);
    DefC(CMSG_JOIN_CHANNEL, "CMSG_JOIN_CHANNEL", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleJoinChannelOpcode);
    DefS(SMSG_CHANNEL_NOTIFY, "SMSG_CHANNEL_NOTIFY");
    DefS(SMSG_CHANNEL_LIST, "SMSG_CHANNEL_LIST");
    DefC(CMSG_CANCEL_TRADE, "CMSG_CANCEL_TRADE", STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT, PROCESS_THREADUNSAFE, &WorldSession::HandleCancelTradeOpcode);
    DefC(CMSG_UI_TIME_REQUEST, "CMSG_UI_TIME_REQUEST", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleUITimeRequestOpcode);
    DefC(CMSG_LOAD_SCREEN, "CMSG_LOAD_SCREEN", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleLoadScreenOpcode);
    DefC(CMSG_QUERY_COUNTDOWN_TIMER, "CMSG_QUERY_COUNTDOWN_TIMER", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQueryCountdownTimerOpcode);
    DefS(SMSG_UI_TIME, "SMSG_UI_TIME");
    DefS(SMSG_DB_REPLY, "SMSG_DB_REPLY");
    DefS(SMSG_START_TIMER, "SMSG_START_TIMER");

    // Live-log movement control requests. Client writers were verified directly
    // in the IDA 9.4 18414 Wow.exe database.
    DefC(CMSG_MOVE_TIME_SKIPPED, "CMSG_MOVE_TIME_SKIPPED", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMoveTimeSkippedOpcode);
    DefC(CMSG_SET_ACTIVE_MOVER, "CMSG_SET_ACTIVE_MOVER", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSetActiveMoverOpcode);

    // Live-log client preference toggles. The 18414 writers emit one byte for
    // action bars and two bits for the voice/microphone flags.
    DefC(CMSG_SET_ACTIONBAR_TOGGLES, "CMSG_SET_ACTIONBAR_TOGGLES", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleSetActionBarTogglesOpcode);
    DefC(CMSG_VIOLENCE_LEVEL, "CMSG_VIOLENCE_LEVEL", STATUS_AUTHED, PROCESS_INPLACE, &WorldSession::HandleViolenceLevelOpcode);
    DefC(CMSG_VOICE_SESSION_ENABLE, "CMSG_VOICE_SESSION_ENABLE", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleVoiceSessionEnableOpcode);

    // Live-log player state requests. The 18414 client writers emit a uint32
    // plus one presence bit for sheath state, a packed selection GUID, and one
    // uint32 for stand state. The paired stand-state response is one byte.
    DefC(CMSG_SETSHEATHED, "CMSG_SETSHEATHED", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSetSheathedOpcode);
    DefC(CMSG_SET_SELECTION, "CMSG_SET_SELECTION", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSetSelectionOpcode);
    DefC(CMSG_STANDSTATECHANGE, "CMSG_STANDSTATECHANGE", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleStandStateChangeOpcode);
    DefS(SMSG_STANDSTATE_UPDATE, "SMSG_STANDSTATE_UPDATE");
    DefC(CMSG_ATTACKSWING, "CMSG_ATTACKSWING", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleAttackSwingOpcode);
    DefC(CMSG_ATTACKSTOP, "CMSG_ATTACKSTOP", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleAttackStopOpcode);
    DefS(SMSG_ATTACKSTART, "SMSG_ATTACKSTART");
    DefS(SMSG_ATTACKSTOP, "SMSG_ATTACKSTOP");

    // Single quest-giver marker query and its packed-GUID status response.
    DefC(CMSG_QUESTGIVER_STATUS_QUERY, "CMSG_QUESTGIVER_STATUS_QUERY", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQuestgiverStatusQueryOpcode);
    DefS(SMSG_QUESTGIVER_STATUS, "SMSG_QUESTGIVER_STATUS");
    DefC(CMSG_GOSSIP_HELLO, "CMSG_GOSSIP_HELLO", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGossipHelloOpcode);
    DefS(SMSG_GOSSIP_MESSAGE, "SMSG_GOSSIP_MESSAGE");
    DefC(CMSG_LIST_INVENTORY, "CMSG_LIST_INVENTORY", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleListInventoryOpcode);
    DefS(SMSG_LIST_INVENTORY, "SMSG_LIST_INVENTORY");

    // Empty 18414 status refresh request. The handler replies through the
    // already-converted unified SMSG_LFG_UPDATE_STATUS body.
    DefC(CMSG_LFG_GET_STATUS, "CMSG_LFG_GET_STATUS", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleLfgGetStatusOpcode);

    // Unified 18414 lock-info request: 0x7F byte then one player/party bit.
    DefC(CMSG_LFG_LOCK_INFO_REQUEST, "CMSG_LFG_LOCK_INFO_REQUEST", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleLfgLockInfoRequestOpcode);
    DefS(SMSG_LFG_PLAYER_INFO, "SMSG_LFG_PLAYER_INFO");
    DefS(SMSG_LFG_PARTY_INFO, "SMSG_LFG_PARTY_INFO");

    // Empty 18414 raid-lock query and its 20-bit-count, packed-GUID response.
    DefC(CMSG_REQUEST_RAID_INFO, "CMSG_REQUEST_RAID_INFO", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestRaidInfoOpcode);
    DefS(SMSG_RAID_INSTANCE_INFO, "SMSG_RAID_INSTANCE_INFO");

    // Wave 6 creature query request and response.
    DefC(CMSG_CREATURE_QUERY, "CMSG_CREATURE_QUERY", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleCreatureQueryOpcode);
    DefS(SMSG_CREATURE_QUERY_RESPONSE, "SMSG_CREATURE_QUERY_RESPONSE");

    // Wave 8 game-object query request and response.
    DefC(CMSG_GAMEOBJECT_QUERY, "CMSG_GAMEOBJECT_QUERY", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleGameObjectQueryOpcode);
    DefS(SMSG_GAMEOBJECT_QUERY_RESPONSE, "SMSG_GAMEOBJECT_QUERY_RESPONSE");

    // Wave 34 corpse location and transport map-position queries.
    DefC(CMSG_CORPSE_QUERY, "CMSG_CORPSE_QUERY", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCorpseQueryOpcode);
    DefS(SMSG_CORPSE_QUERY_RESPONSE, "SMSG_CORPSE_QUERY_RESPONSE");
    DefC(CMSG_CORPSE_MAP_POSITION_QUERY, "CMSG_CORPSE_MAP_POSITION_QUERY", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCorpseMapPositionQueryOpcode);
    DefS(SMSG_CORPSE_MAP_POSITION_QUERY_RESPONSE, "SMSG_CORPSE_MAP_POSITION_QUERY_RESPONSE");

    // Wave 35 spirit-healer location state.
    DefS(SMSG_DEATH_RELEASE_LOC, "SMSG_DEATH_RELEASE_LOC");

    // Binary-proven scheduled cemetery-list refresh.
    DefC(CMSG_REQUEST_CEMETERY_LIST, "CMSG_REQUEST_CEMETERY_LIST", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestCemeteryListOpcode);
    DefS(SMSG_REQUEST_CEMETERY_LIST_RESPONSE, "SMSG_REQUEST_CEMETERY_LIST_RESPONSE");

    // Wave 36 quest-sharing requests, confirmation prompt, and split result paths.
    DefC(CMSG_PUSHQUESTTOPARTY, "CMSG_PUSHQUESTTOPARTY", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePushQuestToParty);
    DefC(CMSG_QUEST_CONFIRM_ACCEPT, "CMSG_QUEST_CONFIRM_ACCEPT", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQuestConfirmAccept);
    DefC(CMSG_QUEST_PUSH_RESULT, "CMSG_QUEST_PUSH_RESULT", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQuestPushResult);
    DefS(SMSG_QUEST_CONFIRM_ACCEPT, "SMSG_QUEST_CONFIRM_ACCEPT");
    DefS(SMSG_QUEST_PUSH_RESULT, "SMSG_QUEST_PUSH_RESULT");
    DefS(SMSG_INITIAL_SETUP, "SMSG_INITIAL_SETUP");
    DefS(SMSG_SET_QUEST_COMPLETED_BIT, "SMSG_SET_QUEST_COMPLETED_BIT");
    DefS(SMSG_CLEAR_QUEST_COMPLETED_BIT, "SMSG_CLEAR_QUEST_COMPLETED_BIT");
    DefS(SMSG_CLEAR_QUEST_COMPLETED_BITS, "SMSG_CLEAR_QUEST_COMPLETED_BITS");

    // Wave 9 name query request and response.
    DefC(CMSG_NAME_QUERY, "CMSG_NAME_QUERY", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleNameQueryOpcode);
    DefS(SMSG_NAME_QUERY_RESPONSE, "SMSG_NAME_QUERY_RESPONSE");

    // Realm-name query. The 18414 client fires this from its name-cache path when a
    // queried character's realm is not yet in its RealmCache; until it is answered the
    // client parks the queried name and never commits it (the name shows "Unknown").
    // CMSG value client-confirmed live (0x1A16, body = uint32 realmId); response
    // contract RE-verified against the client handler sub_1403073A0.
    DefC(CMSG_REALM_NAME_QUERY, "CMSG_REALM_NAME_QUERY", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRealmNameQueryOpcode);
    DefS(SMSG_REALM_NAME_QUERY_RESPONSE, "SMSG_REALM_NAME_QUERY_RESPONSE");

    // Wave 7 compact time query requests and responses.
    DefC(CMSG_QUERY_TIME, "CMSG_QUERY_TIME", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQueryTimeOpcode);
    DefS(SMSG_QUERY_TIME_RESPONSE, "SMSG_QUERY_TIME_RESPONSE");
    DefC(CMSG_PLAYED_TIME, "CMSG_PLAYED_TIME", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePlayedTime);
    DefS(SMSG_PLAYED_TIME, "SMSG_PLAYED_TIME");

    // Wave 10 core 5.4.8 player movement and server relay.
    DefC(MSG_MOVE_HEARTBEAT, "MSG_MOVE_HEARTBEAT", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_START_FORWARD, "CMSG_MOVE_START_FORWARD", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_START_BACKWARD, "CMSG_MOVE_START_BACKWARD", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_STOP, "CMSG_MOVE_STOP", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_SET_FACING, "CMSG_MOVE_SET_FACING", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_FALL_LAND, "CMSG_MOVE_FALL_LAND", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_START_STRAFE_LEFT, "CMSG_MOVE_START_STRAFE_LEFT", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_START_STRAFE_RIGHT, "CMSG_MOVE_START_STRAFE_RIGHT", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_STOP_STRAFE, "CMSG_MOVE_STOP_STRAFE", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_JUMP, "CMSG_MOVE_JUMP", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_START_TURN_LEFT, "CMSG_MOVE_START_TURN_LEFT", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_START_TURN_RIGHT, "CMSG_MOVE_START_TURN_RIGHT", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_MOVE_STOP_TURN, "CMSG_MOVE_STOP_TURN", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);
    DefC(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK, "CMSG_FORCE_SWIM_SPEED_CHANGE_ACK", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleForceSpeedChangeAckOpcodes);
    DefS(SMSG_PLAYER_MOVE, "SMSG_PLAYER_MOVE");
    DefS(SMSG_MONSTER_MOVE, "SMSG_MONSTER_MOVE");
    DefS(SMSG_SPLINE_MOVE_SET_NORMAL_FALL, "SMSG_SPLINE_MOVE_SET_NORMAL_FALL");
    DefS(SMSG_SPLINE_MOVE_SET_WATER_WALK, "SMSG_SPLINE_MOVE_SET_WATER_WALK");
    DefS(SMSG_SPLINE_MOVE_SET_FEATHER_FALL, "SMSG_SPLINE_MOVE_SET_FEATHER_FALL");
    DefS(SMSG_SPLINE_MOVE_SET_LAND_WALK, "SMSG_SPLINE_MOVE_SET_LAND_WALK");

    // Binary-proven 18414 integrated spell-cast request.
    DefC(CMSG_CAST_SPELL, "CMSG_CAST_SPELL", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCastSpellOpcode);
    DefS(SMSG_CAST_FAILED, "SMSG_CAST_FAILED");
    DefS(SMSG_PET_CAST_FAILED, "SMSG_PET_CAST_FAILED");
    DefS(SMSG_SPELL_START, "SMSG_SPELL_START");
    DefS(SMSG_SPELL_GO, "SMSG_SPELL_GO");

    // Guild event packets split from the pre-MoP generic guild-event packet.
    DefS(SMSG_GUILD_EVENT_MOTD, "SMSG_GUILD_EVENT_MOTD");
    DefS(SMSG_GUILD_EVENT_PLAYER_JOINED, "SMSG_GUILD_EVENT_PLAYER_JOINED");
    DefS(SMSG_GUILD_EVENT_PRESENCE_CHANGE, "SMSG_GUILD_EVENT_PRESENCE_CHANGE");
    DefS(SMSG_GUILD_EVENT_PLAYER_LEFT, "SMSG_GUILD_EVENT_PLAYER_LEFT");
    DefS(SMSG_GUILD_RANKS_UPDATE, "SMSG_GUILD_RANKS_UPDATE");
    DefS(SMSG_GUILD_EVENT_NEW_LEADER, "SMSG_GUILD_EVENT_NEW_LEADER");
    DefS(SMSG_GUILD_EVENT_DISBANDED, "SMSG_GUILD_EVENT_DISBANDED");

    // Live-log guild-bank withdrawal allowance query. The 18414 request is
    // empty and its response contains one uint64 remaining allowance.
    DefC(CMSG_GUILD_BANK_MONEY_WITHDRAWN, "CMSG_GUILD_BANK_MONEY_WITHDRAWN", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGuildBankMoneyWithdrawn);
    DefS(SMSG_GUILD_BANK_MONEY_WITHDRAWN, "SMSG_GUILD_BANK_MONEY_WITHDRAWN");

    // Wave 32 tabard-vendor interaction and guild-emblem save.
    DefC(CMSG_TABARD_VENDOR_ACTIVATE, "CMSG_TABARD_VENDOR_ACTIVATE", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTabardVendorActivateOpcode);
    DefS(SMSG_TABARD_VENDOR_ACTIVATE, "SMSG_TABARD_VENDOR_ACTIVATE");
    DefC(CMSG_SAVE_GUILD_EMBLEM, "CMSG_SAVE_GUILD_EMBLEM", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSaveGuildEmblemOpcode);
    DefS(SMSG_SAVE_GUILD_EMBLEM, "SMSG_SAVE_GUILD_EMBLEM");

    // Wave 33 innkeeper bind confirmation and completion.
    DefC(CMSG_BINDER_ACTIVATE, "CMSG_BINDER_ACTIVATE", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleBinderActivateOpcode);
    DefS(SMSG_BINDER_CONFIRM, "SMSG_BINDER_CONFIRM");
    DefS(SMSG_PLAYERBOUND, "SMSG_PLAYERBOUND");

    // Wave 22 LFG boot-vote update, binary-named LFG_BOOT_PLAYER.
    DefS(SMSG_LFG_BOOT_PLAYER, "SMSG_LFG_BOOT_PLAYER");

    // Wave 23 unified 5.4.8 LFG player/party queue status.
    DefS(SMSG_LFG_UPDATE_STATUS, "SMSG_LFG_UPDATE_STATUS");

    // Wave 13 talent-respec confirmation request and prompt.
    DefC(CMSG_CONFIRM_RESPEC_WIPE, "CMSG_CONFIRM_RESPEC_WIPE", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTalentWipeConfirmOpcode);
    DefS(SMSG_RESPEC_WIPE_CONFIRM, "SMSG_RESPEC_WIPE_CONFIRM");

    // Wave 14 party-member statistics request and shared delta/full response.
    DefC(CMSG_REQUEST_PARTY_MEMBER_STATS, "CMSG_REQUEST_PARTY_MEMBER_STATS", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestPartyMemberStatsOpcode);
    DefS(SMSG_PARTY_MEMBER_STATS, "SMSG_PARTY_MEMBER_STATS");

    // Wave 20 full party roster/update request and response.
    DefC(CMSG_GROUP_REQUEST_JOIN_UPDATES, "CMSG_GROUP_REQUEST_JOIN_UPDATES", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGroupRequestJoinUpdates);
    DefS(SMSG_GROUP_LIST, "SMSG_GROUP_LIST");

    // Wave 15 stable-pet list request, list response, and operation result.
    DefC(CMSG_REQUEST_STABLED_PETS, "CMSG_REQUEST_STABLED_PETS", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleListStabledPetsOpcode);
    DefS(SMSG_PET_STABLE_LIST, "SMSG_PET_STABLE_LIST");
    DefS(SMSG_STABLE_RESULT, "SMSG_STABLE_RESULT");

    // The 18414 client clears its local journal before this empty request.
    // Return a binary-safe empty, writable journal until collection persistence exists.
    DefC(CMSG_BATTLE_PET_REQUEST_JOURNAL, "CMSG_BATTLE_PET_REQUEST_JOURNAL", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleBattlePetRequestJournal);
    DefS(SMSG_BATTLE_PET_JOURNAL, "SMSG_BATTLE_PET_JOURNAL");

    // Wave 16 ready-check exchange. All five values and bodies are recovered
    // directly from the 18414 client; server-side state/recipient policy is
    // deliberately kept in Group and GroupHandler.
    DefC(CMSG_DO_READY_CHECK, "CMSG_DO_READY_CHECK", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRaidReadyCheckOpcode);
    DefC(CMSG_RAID_READY_CHECK_CONFIRM, "CMSG_RAID_READY_CHECK_CONFIRM", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRaidReadyCheckConfirmOpcode);
    DefS(SMSG_RAID_READY_CHECK, "SMSG_RAID_READY_CHECK");
    DefS(SMSG_RAID_READY_CHECK_CONFIRM, "SMSG_RAID_READY_CHECK_CONFIRM");
    DefS(SMSG_RAID_READY_CHECK_COMPLETED, "SMSG_RAID_READY_CHECK_COMPLETED");

    // Wave 27 minimap ping and raid target markers. The inbound serializers
    // and all three outbound readers are recovered directly from Wow.exe.
    DefC(CMSG_MINIMAP_PING, "CMSG_MINIMAP_PING", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMinimapPingOpcode);
    DefC(CMSG_RAID_TARGET_UPDATE, "CMSG_RAID_TARGET_UPDATE", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRaidTargetUpdateOpcode);
    DefS(SMSG_MINIMAP_PING, "SMSG_MINIMAP_PING");
    DefS(SMSG_RAID_TARGET_UPDATE_ALL, "SMSG_RAID_TARGET_UPDATE_ALL");
    DefS(SMSG_RAID_TARGET_UPDATE_SINGLE, "SMSG_RAID_TARGET_UPDATE_SINGLE");

    // Wave 28 auction hello plus the merged sold/expired owner notification.
    // All three bodies and both client receive routes are recovered directly
    // from Wow.exe; 0x1A8E selects sold (1) versus expired (0).
    DefC(CMSG_AUCTION_HELLO, "CMSG_AUCTION_HELLO", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleAuctionHelloOpcode);
    DefS(SMSG_AUCTION_HELLO, "SMSG_AUCTION_HELLO");
    DefS(SMSG_AUCTION_COMMAND_RESULT, "SMSG_AUCTION_COMMAND_RESULT");
    DefS(SMSG_AUCTION_OWNER_NOTIFICATION, "SMSG_AUCTION_OWNER_NOTIFICATION");
    DefS(SMSG_AUCTION_WON_NOTIFICATION, "SMSG_AUCTION_WON_NOTIFICATION");
    DefS(SMSG_AUCTION_OUTBID_NOTIFICATION, "SMSG_AUCTION_OUTBID_NOTIFICATION");
    DefS(SMSG_AUCTION_BID_UPDATE_NOTIFICATION, "SMSG_AUCTION_BID_UPDATE_NOTIFICATION");

    // Wave 17 next-mail-time query and result.
    DefC(CMSG_MAIL_QUERY_NEXT_TIME, "CMSG_MAIL_QUERY_NEXT_TIME", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQueryNextMailTime);
    DefS(SMSG_MAIL_QUERY_NEXT_TIME_RESULT, "SMSG_MAIL_QUERY_NEXT_TIME_RESULT");

    // Wave 18 rated-battleground self statistics. The inspect exchange is a
    // separate protocol and is deliberately not registered here.
    DefC(CMSG_BATTLEFIELD_STATUS, "CMSG_BATTLEFIELD_STATUS", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleBattlefieldStatusOpcode);
    DefC(CMSG_REQUEST_RATED_BG_STATS, "CMSG_REQUEST_RATED_BG_STATS", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestRatedBGStatsOpcode);
    DefS(SMSG_BATTLEFIELD_RATED_INFO, "SMSG_BATTLEFIELD_RATED_INFO");
    DefS(SMSG_BATTLEFIELD_STATUS, "SMSG_BATTLEFIELD_STATUS");
    DefS(SMSG_BATTLEFIELD_STATUS_QUEUED, "SMSG_BATTLEFIELD_STATUS_QUEUED");
    DefS(SMSG_BATTLEFIELD_STATUS_NEEDCONFIRMATION, "SMSG_BATTLEFIELD_STATUS_NEEDCONFIRMATION");
    DefS(SMSG_BATTLEFIELD_STATUS_ACTIVE, "SMSG_BATTLEFIELD_STATUS_ACTIVE");
    DefS(SMSG_BATTLEFIELD_STATUS_FAILED, "SMSG_BATTLEFIELD_STATUS_FAILED");

    // Live-log conquest formula request and its directly paired response.
    // Wow.exe proves the empty request and five-field response reader.
    DefC(CMSG_REQUEST_CONQUEST_FORMULA_CONSTANTS, "CMSG_REQUEST_CONQUEST_FORMULA_CONSTANTS", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestConquestFormulaConstantsOpcode);
    DefS(SMSG_CONQUEST_FORMULA_CONSTANTS, "SMSG_CONQUEST_FORMULA_CONSTANTS");

    // Wave 19 calendar update bodies. Names come through the 5.4.7 bridge;
    // values and layouts are proved by the 18414 receive routes.
    DefS(SMSG_CALENDAR_EVENT_INITIAL_INVITE, "SMSG_CALENDAR_EVENT_INITIAL_INVITE");
    DefS(SMSG_CALENDAR_EVENT_INVITE_STATUS, "SMSG_CALENDAR_EVENT_INVITE_STATUS");
    DefS(SMSG_CALENDAR_EVENT_MODERATOR_STATUS, "SMSG_CALENDAR_EVENT_MODERATOR_STATUS");

    // Shipped OpenCalendar/CalendarOpenEvent APIs reach these empty/uint64
    // requests; the paired response values and readers are proved in 18414.
    DefC(CMSG_CALENDAR_GET_CALENDAR, "CMSG_CALENDAR_GET_CALENDAR", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCalendarGetCalendar);
    DefC(CMSG_CALENDAR_GET_EVENT, "CMSG_CALENDAR_GET_EVENT", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCalendarGetEvent);
    DefS(SMSG_CALENDAR_SEND_CALENDAR, "SMSG_CALENDAR_SEND_CALENDAR");
    DefS(SMSG_CALENDAR_SEND_EVENT, "SMSG_CALENDAR_SEND_EVENT");

    // Live-log calendar pending-count pair. The 18414 client sends an empty
    // request and consumes exactly one uint32 from the response.
    DefC(CMSG_CALENDAR_GET_NUM_PENDING, "CMSG_CALENDAR_GET_NUM_PENDING", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCalendarGetNumPending);
    DefS(SMSG_CALENDAR_SEND_NUM_PENDING, "SMSG_CALENDAR_SEND_NUM_PENDING");
}
