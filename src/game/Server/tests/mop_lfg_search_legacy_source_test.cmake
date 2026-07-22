file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/LFGHandler.cpp" lfg_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)

if(MUTATION STREQUAL "legacy_sender")
    string(APPEND lfg_handler "\nvoid WorldSession::SendLfgSearchResults(LfgType, uint32) {}\n")
elseif(MUTATION STREQUAL "legacy_handlers")
    string(APPEND lfg_handler "\nvoid WorldSession::HandleSearchLfgJoinOpcode(WorldPacket&) {}\nvoid WorldSession::HandleSearchLfgLeaveOpcode(WorldPacket&) {}\n")
elseif(MUTATION STREQUAL "legacy_opcodes")
    string(APPEND opcode_header "\nWorldPacket a(CMSG_LFG_SEARCH_JOIN);\nWorldPacket b(CMSG_LFG_SEARCH_LEAVE);\nWorldPacket c(SMSG_LFG_SEARCH_RESULTS);\n")
elseif(MUTATION STREQUAL "modern_update")
    string(REPLACE
        "SMSG_LFG_UPDATE_SEARCH"
        "REMOVED_MODERN_LFG_UPDATE"
        opcode_header "${opcode_header}")
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

function(forbid source token context)
    string(FIND "${source}" "${token}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden token remains: ${token}")
    endif()
endfunction()

require_once("${opcode_header}"
    "SMSG_LFG_UPDATE_SEARCH"
    "distinct modern LFG search-state update")
require_once("${lfg_handler}"
    "void WorldSession::SendLfgUpdate("
    "active LFG update backend")

set(production "${lfg_handler}${session_header}${opcode_header}")
foreach(token IN ITEMS
        "SendLfgSearchResults"
        "HandleSearchLfgJoinOpcode"
        "HandleSearchLfgLeaveOpcode"
        "CMSG_LFG_SEARCH_JOIN"
        "CMSG_LFG_SEARCH_LEAVE"
        "SMSG_LFG_SEARCH_RESULTS")
    forbid("${production}" "${token}" "removed legacy LFG search endpoint")
endforeach()
