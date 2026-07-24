if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QuestHandler.cpp"
    QUEST_HANDLER)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GossipDef.cpp"
    GOSSIP_DEF)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerQuest.cpp"
    PLAYER_QUEST)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp"
    OPCODES_CPP)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp"
    WORLD_SESSION)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h"
    OPCODE_REFERENCE)

if(DEFINED MUTATION)
    if(MUTATION STREQUAL "complete_parser")
        string(REPLACE
            "MopQuestGiverPackets::ParseCompleteQuest(recv_data, request)"
            "false /* mutation: complete parser bypassed */"
            QUEST_HANDLER "${QUEST_HANDLER}")
    elseif(MUTATION STREQUAL "request_reward_parser")
        string(REPLACE
            "MopQuestGiverPackets::ParseRequestReward(recv_data, request)"
            "false /* mutation: request-reward parser bypassed */"
            QUEST_HANDLER "${QUEST_HANDLER}")
    elseif(MUTATION STREQUAL "choose_reward_parser")
        string(REPLACE
            "MopQuestGiverPackets::ParseChooseReward(recv_data, request)"
            "false /* mutation: choose-reward parser bypassed */"
            QUEST_HANDLER "${QUEST_HANDLER}")
    elseif(MUTATION STREQUAL "reward_item_resolution")
        string(REPLACE
            "MopQuestGiverPackets::ResolveRewardChoice("
            "MopQuestGiverPackets::ResolveRewardChoice_MUTATED("
            QUEST_HANDLER "${QUEST_HANDLER}")
    elseif(MUTATION STREQUAL "request_items_builder")
        string(REPLACE
            "MopQuestGiverPackets::BuildQuestRequestItems(data, response)"
            "false /* mutation: request-items builder bypassed */"
            GOSSIP_DEF "${GOSSIP_DEF}")
    elseif(MUTATION STREQUAL "offer_reward_builder")
        string(REPLACE
            "MopQuestGiverPackets::BuildQuestOfferReward(data, response)"
            "false /* mutation: offer-reward builder bypassed */"
            GOSSIP_DEF "${GOSSIP_DEF}")
    elseif(MUTATION STREQUAL "reward_summary_builder")
        string(REPLACE
            "MopQuestGiverPackets::BuildQuestRewardSummary(data, summary)"
            "/* mutation: reward summary builder bypassed */"
            PLAYER_QUEST "${PLAYER_QUEST}")
    elseif(MUTATION STREQUAL "quest_update_builder")
        string(REPLACE
            "MopQuestGiverPackets::BuildQuestUpdateComplete(data, quest_id)"
            "/* mutation: quest-update builder bypassed */"
            PLAYER_QUEST "${PLAYER_QUEST}")
    elseif(MUTATION STREQUAL "complete_registration")
        string(REPLACE
            "DefC(CMSG_QUESTGIVER_COMPLETE_QUEST,"
            "DefC_MUTATED(CMSG_QUESTGIVER_COMPLETE_QUEST,"
            OPCODES_CPP "${OPCODES_CPP}")
    elseif(MUTATION STREQUAL "request_reward_registration")
        string(REPLACE
            "DefC(CMSG_QUESTGIVER_REQUEST_REWARD,"
            "DefC_MUTATED(CMSG_QUESTGIVER_REQUEST_REWARD,"
            OPCODES_CPP "${OPCODES_CPP}")
    elseif(MUTATION STREQUAL "choose_reward_registration")
        string(REPLACE
            "DefC(CMSG_QUESTGIVER_CHOOSE_REWARD,"
            "DefC_MUTATED(CMSG_QUESTGIVER_CHOOSE_REWARD,"
            OPCODES_CPP "${OPCODES_CPP}")
    elseif(MUTATION STREQUAL "response_registration")
        string(REPLACE
            "DefS(SMSG_QUESTGIVER_REQUEST_ITEMS,"
            "DefS_MUTATED(SMSG_QUESTGIVER_REQUEST_ITEMS,"
            OPCODES_CPP "${OPCODES_CPP}")
    elseif(MUTATION STREQUAL "response_admission")
        string(REPLACE
            "case SMSG_QUESTGIVER_REQUEST_ITEMS:"
            "case SMSG_QUESTGIVER_REQUEST_ITEMS_MUTATED:"
            WORLD_SESSION "${WORLD_SESSION}")
    elseif(MUTATION STREQUAL "reference_status")
        string(REGEX REPLACE
            "(SMSG_QUESTGIVER_REQUEST_ITEMS[^\\r\\n]*)ACTIVE"
            "\\1DORMANT"
            OPCODE_REFERENCE "${OPCODE_REFERENCE}")
    else()
        message(FATAL_ERROR "Unknown MUTATION=${MUTATION}")
    endif()
endif()

function(require_match CONTENT REGEX DESCRIPTION)
    if(NOT "${CONTENT}" MATCHES "${REGEX}")
        message(FATAL_ERROR "Missing ${DESCRIPTION}")
    endif()
endfunction()

require_match("${QUEST_HANDLER}"
    "MopQuestGiverPackets::ParseCompleteQuest\\(recv_data, request\\)"
    "strict CMSG_QUESTGIVER_COMPLETE_QUEST parser")
require_match("${QUEST_HANDLER}"
    "MopQuestGiverPackets::ParseRequestReward\\(recv_data, request\\)"
    "strict CMSG_QUESTGIVER_REQUEST_REWARD parser")
require_match("${QUEST_HANDLER}"
    "MopQuestGiverPackets::ParseChooseReward\\(recv_data, request\\)"
    "strict CMSG_QUESTGIVER_CHOOSE_REWARD parser")
require_match("${QUEST_HANDLER}"
    "MopQuestGiverPackets::ResolveRewardChoice\\("
    "reward-item-id to configured-choice resolution")
require_match("${GOSSIP_DEF}"
    "MopQuestGiverPackets::BuildQuestRequestItems\\(data, response\\)"
    "18414 request-items builder")
require_match("${GOSSIP_DEF}"
    "MopQuestGiverPackets::BuildQuestOfferReward\\(data, response\\)"
    "18414 offer-reward builder")
require_match("${PLAYER_QUEST}"
    "MopQuestGiverPackets::BuildQuestRewardSummary\\(data, summary\\)"
    "18414 quest reward summary builder")
require_match("${PLAYER_QUEST}"
    "MopQuestGiverPackets::BuildQuestUpdateComplete\\(data, quest_id\\)"
    "18414 quest update-complete builder")

foreach(OPCODE IN ITEMS
        CMSG_QUESTGIVER_COMPLETE_QUEST
        CMSG_QUESTGIVER_REQUEST_REWARD
        CMSG_QUESTGIVER_CHOOSE_REWARD)
    require_match("${OPCODES_CPP}"
        "DefC\\(${OPCODE},"
        "${OPCODE} registration")
endforeach()
foreach(OPCODE IN ITEMS
        SMSG_QUESTGIVER_REQUEST_ITEMS
        SMSG_QUESTGIVER_OFFER_REWARD
        SMSG_QUESTGIVER_QUEST_COMPLETE
        SMSG_QUESTUPDATE_COMPLETE)
    require_match("${OPCODES_CPP}"
        "DefS\\(${OPCODE},"
        "${OPCODE} registration")
    require_match("${WORLD_SESSION}"
        "case ${OPCODE}:"
        "${OPCODE} sender admission")
    require_match("${OPCODE_REFERENCE}"
        "${OPCODE}[^\\n]*ACTIVE"
        "${OPCODE} active reference status")
endforeach()

if("${GOSSIP_DEF}" MATCHES
        "WorldPacket data\\(SMSG_QUESTGIVER_(REQUEST_ITEMS|OFFER_REWARD)")
    message(FATAL_ERROR "Legacy questgiver response body remains")
endif()
if("${PLAYER_QUEST}" MATCHES
        "WorldPacket data\\(SMSG_(QUESTGIVER_QUEST_COMPLETE|QUESTUPDATE_COMPLETE)")
    message(FATAL_ERROR "Legacy quest completion body remains")
endif()
