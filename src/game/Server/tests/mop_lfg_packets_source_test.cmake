file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/LFGMgr.h" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/LFGHandler.cpp" lfg_sender)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
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
require_once("${opcode_registry}"
    "DefS(SMSG_LFG_UPDATE_STATUS, \"SMSG_LFG_UPDATE_STATUS\");"
    "LFG status registration")
require_once("${opcode_registry}"
    "DefC(CMSG_LFG_GET_STATUS, \"CMSG_LFG_GET_STATUS\""
    "LFG get-status registration")
require_once("${opcode_header}"
    "SMSG_LFG_UPDATE_STATUS                       = 0x0C2E,"
    "LFG status opcode value")
require_once("${lfg_sender}"
    "void WorldSession::HandleLfgGetStatusOpcode"
    "LFG get-status handler")
require_once("${lfg_sender}"
    "status.updateType = LFG_UPDATE_STATUS;"
    "LFG get-status update reason")

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
