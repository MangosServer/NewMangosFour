if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QuestHandler.cpp"
    quest_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GossipDef.cpp"
    gossip_sender)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h"
    opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(DEFINED MUTATION)
    if(MUTATION STREQUAL "hello_parser")
        string(REPLACE "MopQuestGiverPackets::ParseHello(recv_data, rawGuid)"
            "false" quest_handler "${quest_handler}")
    elseif(MUTATION STREQUAL "query_parser")
        string(REPLACE
            "MopQuestGiverPackets::ParseQueryQuest(recv_data, request)"
            "false" quest_handler "${quest_handler}")
    elseif(MUTATION STREQUAL "accept_parser")
        string(REPLACE
            "MopQuestGiverPackets::ParseAcceptQuest(recv_data, request)"
            "false" quest_handler "${quest_handler}")
    elseif(MUTATION STREQUAL "query_interaction_gate")
        string(REPLACE
            "CanInteractWithQuestGiver(guid, \"CMSG_QUESTGIVER_QUERY_QUEST\")"
            "false" quest_handler "${quest_handler}")
    elseif(MUTATION STREQUAL "list_builder")
        string(REPLACE "MopQuestGiverPackets::BuildQuestList(data, list)"
            "false" gossip_sender "${gossip_sender}")
    elseif(MUTATION STREQUAL "details_builder")
        string(REPLACE
            "MopQuestGiverPackets::BuildQuestDetails(data, details)"
            "false" gossip_sender "${gossip_sender}")
    elseif(MUTATION STREQUAL "details_auto_launch_zero")
        string(REPLACE "details.questDetailsAcceptFlag = false;"
            "details.questDetailsAcceptFlag = ActivateAccept;"
            gossip_sender "${gossip_sender}")
    elseif(MUTATION STREQUAL "gossip_complete_empty_body")
        string(REPLACE "WorldPacket data(SMSG_GOSSIP_COMPLETE, 0);"
            "WorldPacket data(SMSG_GOSSIP_COMPLETE, 1);"
            gossip_sender "${gossip_sender}")
    elseif(MUTATION STREQUAL "hello_registration")
        string(REPLACE "DefC(CMSG_QUESTGIVER_HELLO,"
            "DefC(CMSG_UNUSED_QUESTGIVER_HELLO,"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "accept_registration")
        string(REPLACE "DefC(CMSG_QUESTGIVER_ACCEPT_QUEST,"
            "DefC(CMSG_UNUSED_QUESTGIVER_ACCEPT_QUEST,"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "list_admission")
        string(REPLACE "case SMSG_QUESTGIVER_QUEST_LIST:"
            "case SMSG_UNUSED_QUESTGIVER_QUEST_LIST:"
            world_session "${world_session}")
    elseif(MUTATION STREQUAL "details_admission")
        string(REPLACE "case SMSG_QUESTGIVER_QUEST_DETAILS:"
            "case SMSG_UNUSED_QUESTGIVER_QUEST_DETAILS:"
            world_session "${world_session}")
    elseif(MUTATION STREQUAL "gossip_complete_admission")
        string(REPLACE "case SMSG_GOSSIP_COMPLETE:"
            "case SMSG_UNUSED_GOSSIP_COMPLETE:"
            world_session "${world_session}")
    elseif(MUTATION STREQUAL "opcode_value")
        string(REPLACE "CMSG_QUESTGIVER_HELLO                        = 0x02DB"
            "CMSG_QUESTGIVER_HELLO                        = 0x02DC"
            opcode_header "${opcode_header}")
    elseif(MUTATION STREQUAL "reference_status")
        string(REPLACE
            "SMSG_QUESTGIVER_QUEST_DETAILS                  0x134C  ACTIVE"
            "SMSG_QUESTGIVER_QUEST_DETAILS                  0x134C  DORMANT"
            opcode_reference "${opcode_reference}")
    elseif(MUTATION STREQUAL "gossip_complete_reference_status")
        string(REPLACE
            "SMSG_GOSSIP_COMPLETE                           0x034E  ACTIVE"
            "SMSG_GOSSIP_COMPLETE                           0x034E  DORMANT"
            opcode_reference "${opcode_reference}")
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

require_once("${quest_handler}"
    "MopQuestGiverPackets::ParseHello\\(recv_data, rawGuid\\)"
    "questgiver hello parser")
require_once("${quest_handler}"
    "MopQuestGiverPackets::ParseQueryQuest\\(recv_data, request\\)"
    "questgiver query parser")
require_once("${quest_handler}"
    "MopQuestGiverPackets::ParseAcceptQuest\\(recv_data, request\\)"
    "questgiver accept parser")
require_once("${quest_handler}"
    "CanInteractWithQuestGiver\\(guid, \"CMSG_QUESTGIVER_QUERY_QUEST\"\\)"
    "questgiver query interaction gate")
require_once("${gossip_sender}"
    "MopQuestGiverPackets::BuildQuestList\\(data, list\\)"
    "questgiver list builder")
require_once("${gossip_sender}"
    "MopQuestGiverPackets::BuildQuestDetails\\(data, details\\)"
    "questgiver details builder")
require_once("${gossip_sender}"
    "details.questDetailsAcceptFlag = false"
    "unproven quest-details auto-launch bit stays clear")
require_once("${gossip_sender}"
    "WorldPacket data\\(SMSG_GOSSIP_COMPLETE, 0\\)"
    "gossip complete remains an empty-body packet")

foreach(line IN ITEMS
        "DefC(CMSG_QUESTGIVER_HELLO, \"CMSG_QUESTGIVER_HELLO\""
        "DefS(SMSG_QUESTGIVER_QUEST_LIST, \"SMSG_QUESTGIVER_QUEST_LIST\");"
        "DefC(CMSG_QUESTGIVER_QUERY_QUEST, \"CMSG_QUESTGIVER_QUERY_QUEST\""
        "DefS(SMSG_QUESTGIVER_QUEST_DETAILS, \"SMSG_QUESTGIVER_QUEST_DETAILS\");"
        "DefC(CMSG_QUESTGIVER_ACCEPT_QUEST, \"CMSG_QUESTGIVER_ACCEPT_QUEST\"")
    string(FIND "${opcode_registry}" "${line}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing opcode registration: ${line}")
    endif()
endforeach()

foreach(opcode IN ITEMS
        SMSG_QUESTGIVER_QUEST_LIST
        SMSG_QUESTGIVER_QUEST_DETAILS
        SMSG_GOSSIP_COMPLETE)
    string(FIND "${world_session}" "case ${opcode}:" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "${opcode} is not admitted through send suppression")
    endif()
endforeach()

foreach(value IN ITEMS
        "CMSG_QUESTGIVER_HELLO                        = 0x02DB"
        "SMSG_QUESTGIVER_QUEST_LIST                   = 0x02D4"
        "CMSG_QUESTGIVER_QUERY_QUEST                  = 0x12F0"
        "SMSG_QUESTGIVER_QUEST_DETAILS                = 0x134C"
        "CMSG_QUESTGIVER_ACCEPT_QUEST                 = 0x06D1")
    string(FIND "${opcode_header}" "${value}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "opcode value drifted: ${value}")
    endif()
endforeach()

foreach(reference IN ITEMS
        "SMSG_GOSSIP_COMPLETE                           0x034E  ACTIVE"
        "SMSG_QUESTGIVER_QUEST_LIST                     0x02D4  ACTIVE"
        "SMSG_QUESTGIVER_QUEST_DETAILS                  0x134C  ACTIVE"
        "CMSG_QUESTGIVER_HELLO                          0x02DB  ACTIVE"
        "CMSG_QUESTGIVER_ACCEPT_QUEST                   0x06D1  ACTIVE"
        "CMSG_QUESTGIVER_QUERY_QUEST                    0x12F0  ACTIVE")
    string(FIND "${opcode_reference}" "${reference}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "reference state drifted: ${reference}")
    endif()
endforeach()

string(FIND "${quest_handler}" "recv_data >> guid >> quest >> unk1"
    stale_quest_reader)
if(NOT stale_quest_reader EQUAL -1)
    message(FATAL_ERROR "legacy raw-GUID questgiver acquisition reader remains")
endif()

string(FIND "${gossip_sender}"
    "WorldPacket data(SMSG_QUESTGIVER_QUEST_DETAILS, 100)"
    stale_details_sender)
if(NOT stale_details_sender EQUAL -1)
    message(FATAL_ERROR "legacy quest-details body remains")
endif()
