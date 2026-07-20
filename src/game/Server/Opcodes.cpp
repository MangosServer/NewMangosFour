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

    // CMSG_UPDATE_ACCOUNT_DATA (0x0068): the client uploads its account-level saved config (tutorial
    // flags, macros, etc.). Its 18414 value differs from the enum's stale 0x0800 and it was never
    // registered, so it only showed as "not handled opcode UNKNOWN (0x0068)". Registered by literal
    // value; the handler parses the captured MoP layout.
    DefC(0x0068, "CMSG_UPDATE_ACCOUNT_DATA", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleUpdateAccountData);

    // CMSG_REQUEST_ACCOUNT_DATA (0x1D8A): the DOWNLOAD counterpart of the upload above. The client
    // sends it when its local cache is older than the server's stored account data (as reported by
    // SMSG_ACCOUNT_DATA_TIMES); without this it dispatched as UNKNOWN and saved macros/config were
    // never served back (Codex PR #15 finding). The enum value is already correct for 18414 (matches
    // SkyFire) so it is registered by name. The reply's exact MoP wire format is being confirmed by
    // live capture -- see FACTS_mop548_codex_pr15_followup.md.
    DefC(CMSG_REQUEST_ACCOUNT_DATA, "CMSG_REQUEST_ACCOUNT_DATA", STATUS_AUTHED, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestAccountData);
    // SMSG_UPDATE_ACCOUNT_DATA (0x0AAE): the download reply the handler above sends. Not in
    // opcode_register.inc, so register it by name here -- otherwise the outbound packet logs as
    // UNKNOWN (0x0AAE) and muddies the RAD live-capture.
    DefS(SMSG_UPDATE_ACCOUNT_DATA, "SMSG_UPDATE_ACCOUNT_DATA");

    // CMSG_BATTLE_PAY_GET_PURCHASE_LIST (0x18B2): benign in-game Shop catalog probe at char-select
    // (value from SkyFire 5.4.8.18414). We do not implement the store, so consume it as a recognized
    // no-op (Handle_NULL) instead of letting it dispatch as an UNKNOWN "not handled" opcode. The
    // client tolerates no reply; this only clarifies the log.
    DefC(0x18B2, "CMSG_BATTLE_PAY_GET_PURCHASE_LIST", STATUS_AUTHED, PROCESS_INPLACE, &WorldSession::Handle_NULL);

    // Wave 2 server messages whose 5.4.8 bodies are encoded by MopCompactPackets.
    DefS(SMSG_ATTACKSWING_ERROR, "SMSG_ATTACKSWING_ERROR");
    DefS(SMSG_MOVE_SET_SWIM_SPEED, "SMSG_MOVE_SET_SWIM_SPEED");
    DefS(SMSG_RANDOM_ROLL, "SMSG_RANDOM_ROLL");
    DefS(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, "SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT");
    DefS(SMSG_SET_RAID_DIFFICULTY, "SMSG_SET_RAID_DIFFICULTY");
    DefS(SMSG_LOGIN_VERIFY_WORLD, "SMSG_LOGIN_VERIFY_WORLD");
    DefS(SMSG_NEW_WORLD, "SMSG_NEW_WORLD");
    DefS(SMSG_LOGIN_SETTIMESPEED, "SMSG_LOGIN_SETTIMESPEED");
    DefS(SMSG_MOVE_TELEPORT, "SMSG_MOVE_TELEPORT");
    DefS(SMSG_CLIENT_CONTROL_UPDATE, "SMSG_CLIENT_CONTROL_UPDATE");
    DefS(SMSG_MOVE_SET_ACTIVE_MOVER, "SMSG_MOVE_SET_ACTIVE_MOVER");

    // Wave 5 regular initial UI/input envelope messages.
    DefS(SMSG_INITIAL_SPELLS, "SMSG_INITIAL_SPELLS");
    DefS(SMSG_SEND_UNLEARN_SPELLS, "SMSG_SEND_UNLEARN_SPELLS");
    DefS(SMSG_ACTION_BUTTONS, "SMSG_ACTION_BUTTONS");
    DefS(SMSG_INITIALIZE_FACTIONS, "SMSG_INITIALIZE_FACTIONS");
    DefS(SMSG_ALL_ACHIEVEMENT_DATA, "SMSG_ALL_ACHIEVEMENT_DATA");
    DefS(SMSG_BINDPOINTUPDATE, "SMSG_BINDPOINTUPDATE");
    DefS(SMSG_SET_PROFICIENCY, "SMSG_SET_PROFICIENCY");
    DefS(SMSG_WEATHER, "SMSG_WEATHER");

    // Wave 6 creature query request and response.
    DefC(CMSG_CREATURE_QUERY, "CMSG_CREATURE_QUERY", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleCreatureQueryOpcode);
    DefS(SMSG_CREATURE_QUERY_RESPONSE, "SMSG_CREATURE_QUERY_RESPONSE");

    // Wave 7 compact time query requests and responses.
    DefC(CMSG_QUERY_TIME, "CMSG_QUERY_TIME", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQueryTimeOpcode);
    DefS(SMSG_QUERY_TIME_RESPONSE, "SMSG_QUERY_TIME_RESPONSE");
    DefC(CMSG_PLAYED_TIME, "CMSG_PLAYED_TIME", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePlayedTime);
    DefS(SMSG_PLAYED_TIME, "SMSG_PLAYED_TIME");
}
