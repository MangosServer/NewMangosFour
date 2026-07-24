if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QuestHandler.cpp"
    quest_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)

if(DEFINED MUTATION)
    if(MUTATION STREQUAL "parser_declaration")
        string(REPLACE "bool ParseQuestLogRemoveRequest("
            "bool RemovedParseQuestLogRemoveRequest("
            player_header "${player_header}")
    elseif(MUTATION STREQUAL "parser_implementation")
        string(REPLACE "in.size() - in.rpos() != sizeof(slot)"
            "false" player_header "${player_header}")
    elseif(MUTATION STREQUAL "handler_parser")
        string(REPLACE
            "MopQuestPackets::ParseQuestLogRemoveRequest(recv_data, slot)"
            "false" quest_handler "${quest_handler}")
    elseif(MUTATION STREQUAL "early_return")
        string(REPLACE
            "DEBUG_LOG(\"WORLD: Malformed CMSG_QUESTLOG_REMOVE_QUEST\");\n        return;"
            "DEBUG_LOG(\"WORLD: Malformed CMSG_QUESTLOG_REMOVE_QUEST\");"
            quest_handler "${quest_handler}")
    elseif(MUTATION STREQUAL "slot_range")
        string(REPLACE "if (slot < MAX_QUEST_LOG_SIZE)"
            "if (true)" quest_handler "${quest_handler}")
    elseif(MUTATION STREQUAL "registration")
        string(REPLACE "DefC(CMSG_QUESTLOG_REMOVE_QUEST,"
            "DefC(CMSG_UNUSED_QUESTLOG_REMOVE_QUEST,"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "opcode_value")
        string(REPLACE
            "CMSG_QUESTLOG_REMOVE_QUEST                   = 0x0779"
            "CMSG_QUESTLOG_REMOVE_QUEST                   = 0x077A"
            opcode_header "${opcode_header}")
    elseif(MUTATION STREQUAL "disproved_alias")
        string(APPEND opcode_header
            "\nSMSG_QUEST_FORCE_REMOVED = 0x121F;\n")
    else()
        message(FATAL_ERROR "unknown MUTATION=${MUTATION}")
    endif()
endif()

function(require_once text needle label)
    string(REGEX MATCHALL "${needle}" matches "${text}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR
            "${label}: expected exactly once, found ${count}")
    endif()
endfunction()

require_once("${player_header}"
    "bool ParseQuestLogRemoveRequest\\("
    "quest-log remove parser declaration")
require_once("${player_header}"
    "in.size\\(\\) - in.rpos\\(\\) != sizeof\\(slot\\)"
    "quest-log remove exact-length check")
require_once("${quest_handler}"
    "MopQuestPackets::ParseQuestLogRemoveRequest\\(recv_data, slot\\)"
    "quest-log remove handler parser")
string(FIND "${quest_handler}"
    "DEBUG_LOG(\"WORLD: Malformed CMSG_QUESTLOG_REMOVE_QUEST\");\n        return;"
    early_return_position)
if(early_return_position EQUAL -1)
    message(FATAL_ERROR
        "malformed quest-log request must return before slot handling")
endif()
require_once("${quest_handler}"
    "if \\(slot < MAX_QUEST_LOG_SIZE\\)"
    "quest-log slot range check")
require_once("${opcode_registry}"
    "DefC\\(CMSG_QUESTLOG_REMOVE_QUEST, \"CMSG_QUESTLOG_REMOVE_QUEST\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQuestLogRemoveQuest\\)"
    "quest-log remove registration")
require_once("${opcode_header}"
    "CMSG_QUESTLOG_REMOVE_QUEST[ \t]+=[ \t]+0x0779"
    "quest-log remove opcode")

string(FIND "${opcode_header}" "SMSG_QUEST_FORCE_REMOVED" disproved_alias)
if(NOT disproved_alias EQUAL -1)
    message(FATAL_ERROR
        "0x121F is a LootFrame packet, not SMSG_QUEST_FORCE_REMOVED")
endif()
