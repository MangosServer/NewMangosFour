if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QuestHandler.cpp" quest_handler)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerQuest.cpp" player_quest)
file(READ "${SOURCE_ROOT}/src/game/Object/ObjectMgrText.cpp" object_mgr_text)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/NPCHandler.h" npc_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QueryHandler.cpp" query_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GossipDef.cpp" gossip_def)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GossipDef.h" gossip_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/shared/revision_data.h.in" revision_data)

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
    elseif(MUTATION STREQUAL "npc_parser")
        string(REPLACE
            "MopNpcTextPackets::ParseRequest(recv_data, request)"
            "false"
            query_handler "${query_handler}")
    elseif(MUTATION STREQUAL "npc_mapping")
        string(REPLACE
            "MopNpcTextPackets::MakeResponse(request.textId, pGossip)"
            "MopNpcTextPackets::Response()"
            query_handler "${query_handler}")
    elseif(MUTATION STREQUAL "npc_loader")
        string(REPLACE
            "BroadcastTextId = fields[cic++].GetUInt32()"
            "BroadcastTextId = 0"
            object_mgr_text "${object_mgr_text}")
    elseif(MUTATION STREQUAL "npc_client_registration")
        string(REPLACE
            "DefC(CMSG_NPC_TEXT_QUERY,"
            "DefC(CMSG_UNUSED_NPC_TEXT_QUERY,"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "npc_server_registration")
        string(REPLACE
            "DefS(SMSG_NPC_TEXT_UPDATE,"
            "DefS(SMSG_UNUSED_NPC_TEXT_UPDATE,"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "npc_admission")
        string(REPLACE
            "case SMSG_NPC_TEXT_UPDATE:"
            "case SMSG_UNUSED_NPC_TEXT_UPDATE:"
            world_session "${world_session}")
    elseif(MUTATION STREQUAL "npc_legacy_sender")
        string(REPLACE
            "MopNpcTextPackets::BuildResponse(data, response)"
            "data << \"Greetings $N\""
            gossip_def "${gossip_def}")
    elseif(MUTATION STREQUAL "npc_db_structure")
        string(REPLACE
            "WORLD_DB_STRUCTURE_NR       \"2\""
            "WORLD_DB_STRUCTURE_NR       \"1\""
            revision_data "${revision_data}")
    elseif(MUTATION STREQUAL "npc_reference_status")
        string(REPLACE
            "CMSG_NPC_TEXT_QUERY                            0x0287  ACTIVE"
            "CMSG_NPC_TEXT_QUERY                            0x0287  DORMANT"
            opcode_reference "${opcode_reference}")
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

# NPC text must use the directly verified 18414 request/response codec and an
# explicit backend field. Inline legacy strings cannot be framed for this leaf.
require_once("${query_handler}"
    "MopNpcTextPackets::ParseRequest\\(recv_data, request\\)"
    "18414 NPC-text request parser")
require_once("${query_handler}"
    "MopNpcTextPackets::MakeResponse\\(request\\.textId, pGossip\\)"
    "NPC-text query response mapping")
require_once("${gossip_def}"
    "MopNpcTextPackets::MakeResponse\\(textID, pGossip\\)"
    "NPC-text direct response mapping")
require_once("${object_mgr_text}"
    "BroadcastTextId = fields\\[cic\\+\\+\\]\\.GetUInt32\\(\\)"
    "BroadcastText database field loader")

foreach(line IN ITEMS
        "DefC(CMSG_NPC_TEXT_QUERY, \"CMSG_NPC_TEXT_QUERY\""
        "DefS(SMSG_NPC_TEXT_UPDATE, \"SMSG_NPC_TEXT_UPDATE\");")
    string(FIND "${opcode_registry}" "${line}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing NPC-text opcode registration: ${line}")
    endif()
endforeach()

string(FIND "${world_session}" "case SMSG_NPC_TEXT_UPDATE:" npc_admission)
if(npc_admission EQUAL -1)
    message(FATAL_ERROR "SMSG_NPC_TEXT_UPDATE is not admitted through send suppression")
endif()

foreach(legacy_source IN ITEMS query_handler gossip_def)
    string(FIND "${${legacy_source}}" "Greetings $N" legacy_greeting)
    if(NOT legacy_greeting EQUAL -1)
        message(FATAL_ERROR "legacy inline NPC-text body remains in ${legacy_source}")
    endif()
endforeach()

string(FIND "${gossip_header}"
    "SendTalking(char const* title, char const* text)" dynamic_sender)
if(NOT dynamic_sender EQUAL -1)
    message(FATAL_ERROR "unframable arbitrary-string NPC-text sender remains")
endif()

foreach(reference IN ITEMS
        "SMSG_NPC_TEXT_UPDATE                           0x140A  ACTIVE"
        "CMSG_NPC_TEXT_QUERY                            0x0287  ACTIVE")
    string(FIND "${opcode_reference}" "${reference}" reference_position)
    if(reference_position EQUAL -1)
        message(FATAL_ERROR "NPC-text reference status is not active: ${reference}")
    endif()
endforeach()

require_once("${revision_data}"
    "WORLD_DB_STRUCTURE_NR[ \t]+\"2\""
    "BroadcastText-aware world schema requirement")

string(FIND "${misc_handler}" "recv_data >> Trigger_ID;" stale_area_reader)
if(NOT stale_area_reader EQUAL -1)
    message(FATAL_ERROR "legacy area-trigger uint32-only reader remains")
endif()
string(FIND "${player_quest}" "data.put<uint32>(0, count)" stale_quest_sender)
if(NOT stale_quest_sender EQUAL -1)
    message(FATAL_ERROR "legacy questgiver multiple-status body remains")
endif()
