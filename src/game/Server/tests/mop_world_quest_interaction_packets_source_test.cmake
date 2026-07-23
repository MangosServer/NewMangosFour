if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QuestHandler.cpp" quest_handler)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerQuest.cpp" player_quest)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(DEFINED MUTATION)
    if(MUTATION STREQUAL "area_parser")
        string(REPLACE
            "MopAreaTriggerPackets::ParseRequest(recv_data, request)"
            "false" misc_handler "${misc_handler}")
    elseif(MUTATION STREQUAL "area_leave")
        string(REPLACE
            "if (!request.entered)"
            "if (false)" misc_handler "${misc_handler}")
    elseif(MUTATION STREQUAL "no_corpse_builder")
        string(REPLACE
            "MopAreaTriggerPackets::BuildNoCorpse(data)"
            "data.Initialize(SMSG_AREA_TRIGGER_NO_CORPSE)"
            misc_handler "${misc_handler}")
    elseif(MUTATION STREQUAL "quest_parser")
        string(REPLACE
            "MopQuestStatusPackets::ParseMultipleStatusQuery(recvPacket)"
            "false" quest_handler "${quest_handler}")
    elseif(MUTATION STREQUAL "quest_builder")
        string(REPLACE
            "MopQuestStatusPackets::BuildMultipleStatus(data, entries)"
            "false" player_quest "${player_quest}")
    elseif(MUTATION STREQUAL "client_registration")
        string(REPLACE
            "DefC(CMSG_AREATRIGGER,"
            "DefC(CMSG_UNUSED_AREATRIGGER,"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "server_registration")
        string(REPLACE
            "DefS(SMSG_QUESTGIVER_STATUS_MULTIPLE,"
            "DefS(SMSG_UNUSED_QUESTGIVER_STATUS_MULTIPLE,"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "sender_admission")
        string(REPLACE
            "case SMSG_AREA_TRIGGER_NO_CORPSE:"
            "case SMSG_UNUSED_AREA_TRIGGER_NO_CORPSE:"
            world_session "${world_session}")
    elseif(MUTATION STREQUAL "opcode_values")
        string(REPLACE
            "CMSG_AREATRIGGER                             = 0x1C44"
            "CMSG_AREATRIGGER                             = 0x1C45"
            opcode_header "${opcode_header}")
    elseif(MUTATION STREQUAL "npc_dormant")
        string(REPLACE
            "not retain; registering it would replace real gossip"
            "not retain; DefC(CMSG_NPC_TEXT_QUERY, would replace real gossip"
            opcode_registry "${opcode_registry}")
    else()
        message(FATAL_ERROR "unknown MUTATION=${MUTATION}")
    endif()
endif()

function(require_once text needle label)
    string(REGEX MATCHALL "${needle}" matches "${text}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${label}: expected exactly once, found ${count}")
    endif()
endfunction()

require_once("${misc_handler}"
    "MopAreaTriggerPackets::ParseRequest\\(recv_data, request\\)"
    "direct area-trigger parser")
require_once("${misc_handler}"
    "if \\(!request\\.entered\\)"
    "area-trigger leave guard")
require_once("${misc_handler}"
    "MopAreaTriggerPackets::BuildNoCorpse\\(data\\)"
    "no-corpse response builder")
require_once("${quest_handler}"
    "MopQuestStatusPackets::ParseMultipleStatusQuery\\(recvPacket\\)"
    "empty quest status query parser")
require_once("${player_quest}"
    "MopQuestStatusPackets::BuildMultipleStatus\\(data, entries\\)"
    "multiple quest status response builder")

foreach(line IN ITEMS
        "DefC(CMSG_AREATRIGGER, \"CMSG_AREATRIGGER\""
        "DefS(SMSG_AREA_TRIGGER_NO_CORPSE, \"SMSG_AREA_TRIGGER_NO_CORPSE\");"
        "DefC(CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY, \"CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY\""
        "DefS(SMSG_QUESTGIVER_STATUS_MULTIPLE, \"SMSG_QUESTGIVER_STATUS_MULTIPLE\");")
    string(FIND "${opcode_registry}" "${line}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing opcode registration: ${line}")
    endif()
endforeach()

foreach(opcode IN ITEMS
        SMSG_AREA_TRIGGER_NO_CORPSE
        SMSG_QUESTGIVER_STATUS_MULTIPLE)
    string(FIND "${world_session}" "case ${opcode}:" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${opcode} is not admitted through send suppression")
    endif()
endforeach()

foreach(value IN ITEMS
        "CMSG_AREATRIGGER                             = 0x1C44"
        "SMSG_AREA_TRIGGER_NO_CORPSE                  = 0x089E"
        "CMSG_NPC_TEXT_QUERY                          = 0x0287"
        "SMSG_NPC_TEXT_UPDATE                         = 0x140A"
        "CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY        = 0x02F1"
        "SMSG_QUESTGIVER_STATUS_MULTIPLE              = 0x06CE")
    string(FIND "${opcode_header}" "${value}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "opcode value drifted: ${value}")
    endif()
endforeach()

# NPC text is binary-identified and codec-tested, but activation must wait for
# a backend that preserves the eight BroadcastText.db2 IDs required by 18414.
string(FIND "${opcode_registry}" "DefC(CMSG_NPC_TEXT_QUERY," npc_registration)
if(NOT npc_registration EQUAL -1)
    message(FATAL_ERROR "backend-blocked CMSG_NPC_TEXT_QUERY was registered")
endif()
string(FIND "${world_session}" "case SMSG_NPC_TEXT_UPDATE:" npc_admission)
if(NOT npc_admission EQUAL -1)
    message(FATAL_ERROR "backend-blocked SMSG_NPC_TEXT_UPDATE was admitted")
endif()

string(FIND "${misc_handler}" "recv_data >> Trigger_ID;" stale_area_reader)
if(NOT stale_area_reader EQUAL -1)
    message(FATAL_ERROR "legacy area-trigger uint32-only reader remains")
endif()
string(FIND "${player_quest}" "data.put<uint32>(0, count)" stale_quest_sender)
if(NOT stale_quest_sender EQUAL -1)
    message(FATAL_ERROR "legacy questgiver multiple-status body remains")
endif()
