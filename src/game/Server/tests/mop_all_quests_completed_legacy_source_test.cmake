file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QueryHandler.cpp" query_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)

if(MUTATION STREQUAL "legacy_handler")
    string(APPEND query_handler "\nvoid WorldSession::HandleQueryQuestsCompletedOpcode(WorldPacket&) {}\n")
elseif(MUTATION STREQUAL "legacy_opcode")
    string(APPEND opcode_header "\nWorldPacket oldReply(SMSG_ALL_QUESTS_COMPLETED);\n")
elseif(MUTATION STREQUAL "quest_backend")
    string(REPLACE
        "typedef std::map<uint32, QuestStatusData> QuestStatusMap;"
        "/* removed live quest-status backend */"
        player_header "${player_header}")
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

require_once("${player_header}"
    "typedef std::map<uint32, QuestStatusData> QuestStatusMap;"
    "independently used quest-status backend")

set(production "${query_handler}${session_header}${opcode_header}")
foreach(token IN ITEMS
        "HandleQueryQuestsCompletedOpcode"
        "SMSG_ALL_QUESTS_COMPLETED")
    forbid("${production}" "${token}" "removed completed-quests query endpoint")
endforeach()
