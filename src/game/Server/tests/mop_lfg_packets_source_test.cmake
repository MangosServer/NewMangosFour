file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/LFGMgr.h" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/LFGHandler.cpp" lfg_sender)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/LFGMgr.h" lfg_manager_header)

if(MUTATION STREQUAL "builder_call")
    string(REPLACE
        "MopLfgPackets::BuildBootPlayer(data, update)"
        "false /* removed LFG boot builder */"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "outbound_registration")
    string(REPLACE
        "DefS(SMSG_LFG_BOOT_PLAYER, \"SMSG_LFG_BOOT_PLAYER\");"
        "/* removed LFG boot registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "opcode_value")
    string(REPLACE
        "SMSG_LFG_BOOT_PLAYER                         = 0x183A"
        "SMSG_LFG_BOOT_PLAYER                         = 0x136E"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "countdown")
    string(REPLACE
        "expires > now ? uint32(expires - now) : 0"
        "uint32((expires - now) / 1000)"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "status_builder_call")
    string(REPLACE
        "MopLfgPackets::BuildUpdateStatus(data, update)"
        "false /* removed LFG status builder */"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "status_registration")
    string(REPLACE
        "DefS(SMSG_LFG_UPDATE_STATUS, \"SMSG_LFG_UPDATE_STATUS\");"
        "/* removed LFG status registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "status_opcode")
    string(REPLACE
        "SMSG_LFG_UPDATE_STATUS                       = 0x0C2E,"
        "SMSG_LFG_UPDATE_STATUS                       = 0x1368,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "status_field_mapping")
    string(REPLACE
        "update.updateReason = uint8(status.updateType);"
        "update.dungeonCategory = uint8(status.updateType);"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "status_reason_value")
    string(REPLACE
        "LFG_UPDATE_JOIN                 = 6,"
        "LFG_UPDATE_JOIN                 = 5,"
        lfg_manager_header "${lfg_manager_header}")
elseif(MUTATION STREQUAL "lfr_join_registration")
    string(REPLACE
        "DefC(CMSG_LFG_LFR_JOIN, \"CMSG_LFG_LFR_JOIN\""
        "DefC(REMOVED_LFR_JOIN, \"CMSG_LFG_LFR_JOIN\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "lfr_leave_registration")
    string(REPLACE
        "DefC(CMSG_LFG_LFR_LEAVE, \"CMSG_LFG_LFR_LEAVE\""
        "DefC(REMOVED_LFR_LEAVE, \"CMSG_LFG_LFR_LEAVE\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "lfr_search_registration")
    string(REPLACE
        "DefS(SMSG_LFG_UPDATE_SEARCH, \"SMSG_LFG_UPDATE_SEARCH\");"
        "/* removed LFR search response registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "lfr_join_declaration")
    string(REPLACE
        "void HandleLfrJoinOpcode(WorldPacket& recv_data);"
        "void RemovedLfrJoinOpcode(WorldPacket& recv_data);"
        session_header "${session_header}")
elseif(MUTATION STREQUAL "lfr_leave_declaration")
    string(REPLACE
        "void HandleLfrLeaveOpcode(WorldPacket& recv_data);"
        "void RemovedLfrLeaveOpcode(WorldPacket& recv_data);"
        session_header "${session_header}")
elseif(MUTATION STREQUAL "lfr_join_definition")
    string(REPLACE
        "void WorldSession::HandleLfrJoinOpcode(WorldPacket& recv_data)"
        "void WorldSession::RemovedLfrJoinOpcode(WorldPacket& recv_data)"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "lfr_leave_definition")
    string(REPLACE
        "void WorldSession::HandleLfrLeaveOpcode(WorldPacket& recv_data)"
        "void WorldSession::RemovedLfrLeaveOpcode(WorldPacket& recv_data)"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "lfr_parser_calls")
    string(REPLACE
        "MopLfgPackets::ParseLfrSearchRequest(recv_data, request)"
        "false /* removed LFR request parser */"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "lfr_builder_call")
    string(REPLACE
        "MopLfgPackets::BuildEmptyLfrSearchResponse(data, request)"
        "/* removed LFR empty-response builder */"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "lfr_dbc_lookup")
    string(REPLACE
        "sLfgDungeonsStore.LookupEntry(request.lfgId)"
        "nullptr /* removed LFR DBC lookup */"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "lfr_type_validation")
    string(REPLACE
        "dungeon->TypeID != request.typeId"
        "false /* removed LFR TypeID validation */"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "lfr_converted_packet")
    string(REPLACE
        "case SMSG_LFG_UPDATE_SEARCH:"
        "case REMOVED_LFG_UPDATE_SEARCH:"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "lfr_join_opcode")
    string(REPLACE
        "CMSG_LFG_LFR_JOIN                           = 0x1AA2,"
        "CMSG_LFG_LFR_JOIN                           = 0x1AA3,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "lfr_leave_opcode")
    string(REPLACE
        "CMSG_LFG_LFR_LEAVE                          = 0x00E3,"
        "CMSG_LFG_LFR_LEAVE                          = 0x00E4,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "lfr_search_opcode")
    string(REPLACE
        "SMSG_LFG_UPDATE_SEARCH                       = 0x1161,"
        "SMSG_LFG_UPDATE_SEARCH                       = 0x1162,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "lfr_bit_order")
    string(REPLACE
        "out.WriteBit(false);  // top GUID[4]"
        "out.WriteBit(false);  // top GUID[0]"
        packet_builder "${packet_builder}")
endif()

function(require_once source token context)
    set(remaining "${source}")
    set(count 0)
    while(TRUE)
        string(FIND "${remaining}" "${token}" position)
        if(position EQUAL -1)
            break()
        endif()
        math(EXPR count "${count} + 1")
        string(LENGTH "${token}" token_length)
        math(EXPR next_position "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endwhile()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

function(require_count source token expected context)
    set(remaining "${source}")
    set(count 0)
    while(TRUE)
        string(FIND "${remaining}" "${token}" position)
        if(position EQUAL -1)
            break()
        endif()
        math(EXPR count "${count} + 1")
        string(LENGTH "${token}" token_length)
        math(EXPR next_position "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endwhile()
    if(NOT count EQUAL expected)
        message(FATAL_ERROR "${context}: expected ${expected} active occurrences, found ${count}")
    endif()
endfunction()

require_once("${lfg_sender}"
    "MopLfgPackets::BuildBootPlayer(data, update)"
    "LFG boot builder call")
require_once("${opcode_registry}"
    "DefS(SMSG_LFG_BOOT_PLAYER, \"SMSG_LFG_BOOT_PLAYER\");"
    "LFG boot registration")
require_once("${opcode_header}"
    "SMSG_LFG_BOOT_PLAYER                         = 0x183A"
    "LFG boot opcode value")
require_once("${lfg_sender}"
    "expires > now ? uint32(expires - now) : 0"
    "seconds-based LFG boot countdown")
require_once("${lfg_sender}"
    "MopLfgPackets::BuildUpdateStatus(data, update)"
    "LFG status builder call")
require_once("${lfg_sender}"
    "MopLfgPackets::ParseLockInfoRequest(recv_data, forPlayer)"
    "LFG lock-info request parser call")
require_once("${lfg_sender}"
    "MopLfgPackets::BuildEmptyPlayerInfo(data)"
    "LFG player-info builder call")
require_once("${lfg_sender}"
    "MopLfgPackets::BuildEmptyPartyInfo(data)"
    "LFG party-info builder call")
require_once("${opcode_registry}"
    "DefS(SMSG_LFG_UPDATE_STATUS, \"SMSG_LFG_UPDATE_STATUS\");"
    "LFG status registration")
require_once("${opcode_registry}"
    "DefC(CMSG_LFG_GET_STATUS, \"CMSG_LFG_GET_STATUS\""
    "LFG get-status registration")
require_once("${opcode_registry}"
    "DefC(CMSG_LFG_LOCK_INFO_REQUEST, \"CMSG_LFG_LOCK_INFO_REQUEST\""
    "LFG lock-info request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_LFG_PLAYER_INFO, \"SMSG_LFG_PLAYER_INFO\");"
    "LFG player-info registration")
require_once("${opcode_registry}"
    "DefS(SMSG_LFG_PARTY_INFO, \"SMSG_LFG_PARTY_INFO\");"
    "LFG party-info registration")
require_once("${opcode_header}"
    "SMSG_LFG_UPDATE_STATUS                       = 0x0C2E,"
    "LFG status opcode value")
require_once("${lfg_sender}"
    "void WorldSession::HandleLfgGetStatusOpcode"
    "LFG get-status handler")
require_once("${lfg_sender}"
    "status.updateType = LFG_UPDATE_STATUS;"
    "LFG get-status update reason")
require_once("${opcode_registry}"
    "DefC(CMSG_LFG_LFR_JOIN, \"CMSG_LFG_LFR_JOIN\""
    "LFR join registration")
require_once("${opcode_registry}"
    "DefC(CMSG_LFG_LFR_LEAVE, \"CMSG_LFG_LFR_LEAVE\""
    "LFR leave registration")
require_once("${opcode_registry}"
    "DefS(SMSG_LFG_UPDATE_SEARCH, \"SMSG_LFG_UPDATE_SEARCH\");"
    "LFR search response registration")
require_once("${session_header}"
    "void HandleLfrJoinOpcode(WorldPacket& recv_data);"
    "LFR join handler declaration")
require_once("${session_header}"
    "void HandleLfrLeaveOpcode(WorldPacket& recv_data);"
    "LFR leave handler declaration")
require_once("${lfg_sender}"
    "void WorldSession::HandleLfrJoinOpcode(WorldPacket& recv_data)"
    "LFR join handler definition")
require_once("${lfg_sender}"
    "void WorldSession::HandleLfrLeaveOpcode(WorldPacket& recv_data)"
    "LFR leave handler definition")
require_count("${lfg_sender}"
    "MopLfgPackets::ParseLfrSearchRequest(recv_data, request)"
    2
    "LFR request parser calls")
require_once("${lfg_sender}"
    "MopLfgPackets::BuildEmptyLfrSearchResponse(data, request)"
    "LFR empty-response builder call")
require_count("${lfg_sender}"
    "sLfgDungeonsStore.LookupEntry(request.lfgId)"
    2
    "LFR DBC row validation")
require_count("${lfg_sender}"
    "dungeon->TypeID != request.typeId"
    2
    "LFR DBC TypeID validation")
require_once("${session_source}"
    "case SMSG_LFG_UPDATE_SEARCH:"
    "LFR converted-packet allowlist")

foreach(token IN ITEMS
        "CMSG_LFG_LFR_JOIN                           = 0x1AA2,"
        "CMSG_LFG_LFR_LEAVE                          = 0x00E3,"
        "SMSG_LFG_UPDATE_SEARCH                       = 0x1161,")
    string(FIND "${opcode_header}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "5.4.8 LFR search opcode missing: ${token}")
    endif()
endforeach()

set(lfr_empty_bit_grammar [=[
    out.WriteBits(0, 24); // removed result count
    out.WriteBit(false);  // top GUID[6]
    out.WriteBit(false);  // top GUID[2]
    out.WriteBit(false);  // top GUID[0]
    out.WriteBit(false);  // replacement mode: false clears current result caches
    out.WriteBits(0, 17); // player count
    out.WriteBit(false);  // top GUID[4]
    out.WriteBit(false);  // top GUID[1]
    out.WriteBits(0, 20); // group count
    out.WriteBit(false);  // top GUID[5]
    out.WriteBit(false);  // top GUID[7]
    out.WriteBit(false);  // top GUID[3]
]=])
require_once("${packet_builder}"
    "${lfr_empty_bit_grammar}"
    "binary-proven LFR empty-response bit grammar")

foreach(token IN ITEMS
        "out.WriteBit(update.reason.empty())"
        "out.WriteBits(update.reason.size(), 8)"
        "WriteGuidMask(out, update.victimGuid, { 1, 7, 5, 2, 0, 4 })"
        "WriteGuidBytes(out, update.victimGuid, { 2, 4, 3, 6 })"
        "out << update.votesNeeded"
        "out << update.timeLeft"
        "out << update.agreeCount"
        "out << update.voteCount")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "LFG boot builder missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "out.WriteBits(update.comment.size(), 8)"
        "out.WriteBits(update.dungeonEntries.size(), 22)"
        "out.WriteBits(update.suspendedPlayerGuids.size(), 24)"
        "WriteGuidMask(out, update.requesterGuid, { 2, 3, 1 })"
        "WriteGuidMask(out, guid, { 7, 0, 4, 2, 5, 3, 1, 6 })"
        "WriteGuidBytes(out, guid, { 7, 0, 1, 6, 4, 5, 2, 3 })"
        "out << update.requestedRoles"
        "out << update.ticketId"
        "out << update.ticketTime"
        "out << update.updateReason"
        "out << update.dungeonCategory"
        "out << update.ticketType")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "LFG status builder missing: ${token}")
    endif()
endforeach()

require_once("${lfg_sender}"
    "update.updateReason = uint8(status.updateType);"
    "binary-proven LFG update reason mapping")

foreach(token IN ITEMS
        "CMSG_LFG_LOCK_INFO_REQUEST                   = 0x006B,"
        "SMSG_LFG_PLAYER_INFO                         = 0x1861,"
        "SMSG_LFG_PARTY_INFO                          = 0x168E,")
    string(FIND "${opcode_header}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "5.4.8 LFG lock-info opcode missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "if (body[0] != 0x7F || (body[1] & 0x7F) != 0)"
        "out.WriteBits(0, 20); // locked dungeon count"
        "out.WriteBit(false);  // has player GUID"
        "out.WriteBits(0, 17); // random/seasonal dungeon count"
        "out.WriteBits(0, 22); // party member count")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "LFG lock-info codec missing: ${token}")
    endif()
endforeach()
require_once("${lfg_sender}"
    "update.dungeonCategory = sLFGMgr.GetDungeonCategory(*status.dungeonList.begin());"
    "reference-supported LFG dungeon category mapping")

foreach(token IN ITEMS
        "LFG_UPDATE_JOIN                 = 6,"
        "LFG_UPDATE_ROLECHECK_FAILED     = 7,"
        "LFG_UPDATE_LEAVE                = 8,"
        "LFG_UPDATE_PROPOSAL_FAILED      = 9,"
        "LFG_UPDATE_PROPOSAL_DECLINED    = 10,"
        "LFG_UPDATE_GROUP_FOUND          = 11,"
        "LFG_UPDATE_ADDED_TO_QUEUE       = 13,"
        "LFG_UPDATE_PROPOSAL_BEGIN       = 14,"
        "LFG_UPDATE_STATUS               = 15,"
        "LFG_UPDATE_GROUP_MEMBER_OFFLINE = 16,"
        "LFG_UPDATE_GROUP_DISBAND        = 17,")
    string(FIND "${lfg_manager_header}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "5.4.8 LFG update reason missing: ${token}")
    endif()
endforeach()

string(FIND "${opcode_header}" "SMSG_LFG_BOOT_PROPOSAL_UPDATE" obsolete_alias)
if(NOT obsolete_alias EQUAL -1)
    message(FATAL_ERROR "obsolete event-derived LFG boot alias remains in production opcode header")
endif()

foreach(obsolete IN ITEMS SMSG_LFG_UPDATE_PLAYER SMSG_LFG_UPDATE_PARTY)
    string(FIND "${opcode_header}" "${obsolete}" obsolete_opcode)
    if(NOT obsolete_opcode EQUAL -1)
        message(FATAL_ERROR "obsolete split LFG status opcode remains: ${obsolete}")
    endif()
endforeach()

foreach(obsolete IN ITEMS CMSG_LFG_GET_PLAYER_INFO CMSG_LFG_GET_PARTY_INFO)
    string(FIND "${opcode_header}" "${obsolete}" obsolete_opcode)
    if(NOT obsolete_opcode EQUAL -1)
        message(FATAL_ERROR "obsolete split LFG lock-info request remains: ${obsolete}")
    endif()
endforeach()

string(FIND "${lfg_sender}"
    "SMSG_LFG_UPDATE_PARTY : SMSG_LFG_UPDATE_PLAYER"
    obsolete_sender)
if(NOT obsolete_sender EQUAL -1)
    message(FATAL_ERROR "legacy split LFG status sender remains")
endif()

string(FIND "${lfg_sender}" "(expires - now) / 1000" stale_countdown)
if(NOT stale_countdown EQUAL -1)
    message(FATAL_ERROR "LFG boot countdown still divides second timestamps by 1000")
endif()
