file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerQuest.cpp" player_quest)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QuestHandler.cpp" quest_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QuestDef.h" quest_def)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "confirm_builder")
    string(REPLACE "MopQuestPackets::BuildQuestConfirmAccept(data," "LegacyQuestConfirmAccept(data,"
        player_quest "${player_quest}")
elseif(MUTATION STREQUAL "result_builder")
    string(REPLACE "MopQuestPackets::BuildQuestPushResult(data," "LegacyQuestPushResult(data,"
        player_quest "${player_quest}")
elseif(MUTATION STREQUAL "result_reader")
    string(REPLACE "MopQuestPackets::ReadQuestPushResult(recvPacket)" "LegacyQuestPushResult(recvPacket)"
        quest_handler "${quest_handler}")
elseif(MUTATION STREQUAL "sharer_validation")
    string(REPLACE "result.sharerGuid != _player->GetDividerGuid().GetRawValue()" "false"
        quest_handler "${quest_handler}")
elseif(MUTATION STREQUAL "result_range")
    string(REPLACE "result.result > QUEST_PARTY_MSG_NOT_ALLOWED" "false"
        quest_handler "${quest_handler}")
elseif(MUTATION STREQUAL "push_registration")
    string(REPLACE "DefC(CMSG_PUSHQUESTTOPARTY," "RemovedC(CMSG_PUSHQUESTTOPARTY,"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "confirm_registration")
    string(REPLACE "DefC(CMSG_QUEST_CONFIRM_ACCEPT," "RemovedC(CMSG_QUEST_CONFIRM_ACCEPT,"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "result_registration")
    string(REPLACE "DefC(CMSG_QUEST_PUSH_RESULT," "RemovedC(CMSG_QUEST_PUSH_RESULT,"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "outbound_registration")
    string(REPLACE "DefS(SMSG_QUEST_CONFIRM_ACCEPT," "RemovedS(SMSG_QUEST_CONFIRM_ACCEPT,"
        opcode_registry "${opcode_registry}")
    string(REPLACE "DefS(SMSG_QUEST_PUSH_RESULT," "RemovedS(SMSG_QUEST_PUSH_RESULT,"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "confirm_whitelist")
    string(REPLACE "case SMSG_QUEST_CONFIRM_ACCEPT:" "/* removed quest-confirm whitelist */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "result_whitelist")
    string(REPLACE "case SMSG_QUEST_PUSH_RESULT:" "/* removed quest-result whitelist */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "legacy_sender")
    string(REPLACE "MopQuestPackets::BuildQuestPushResult(data," "WorldPacket data(MSG_QUEST_PUSH_RESULT, 9);"
        player_quest "${player_quest}")
elseif(MUTATION STREQUAL "opcode_values")
    string(REPLACE
        "0x1370, // 5.4.8 18414 (Wow.exe writer; DeclineQuest path)"
        "0x1371, // 5.4.8 18414 (Wow.exe writer; DeclineQuest path)"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "result_values")
    string(REPLACE "QUEST_PARTY_MSG_DEAD                    = 5"
        "QUEST_PARTY_MSG_DEAD                    = 6"
        quest_def "${quest_def}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once("${player_quest}"
    "MopQuestPackets::BuildQuestConfirmAccept\\(data,"
    "quest confirmation builder call")
require_once("${player_quest}"
    "MopQuestPackets::BuildQuestPushResult\\(data,"
    "quest push-result builder call")
require_once("${quest_handler}"
    "MopQuestPackets::ReadQuestPushResult\\(recvPacket\\)"
    "quest push-result reader call")
require_once("${quest_handler}"
    "result.sharerGuid != _player->GetDividerGuid\\(\\).GetRawValue\\(\\)"
    "quest sharer identity validation")
require_once("${quest_handler}"
    "result.result > QUEST_PARTY_MSG_NOT_ALLOWED"
    "quest result range validation")

if(player_quest MATCHES "WorldPacket[ \t]+data\\(MSG_QUEST_PUSH_RESULT")
    message(FATAL_ERROR "live legacy quest push-result sender remains")
endif()
if(quest_handler MATCHES "WorldPacket[ \t]+data\\(MSG_QUEST_PUSH_RESULT")
    message(FATAL_ERROR "live legacy quest push-result relay remains")
endif()

foreach(registration IN ITEMS CMSG_PUSHQUESTTOPARTY CMSG_QUEST_CONFIRM_ACCEPT CMSG_QUEST_PUSH_RESULT)
    require_once("${opcode_registry}"
        "DefC\\(${registration},"
        "${registration} registration")
endforeach()

foreach(result_value IN ITEMS
        "QUEST_PARTY_MSG_SHARING_QUEST[ \t]*=[ \t]*0"
        "QUEST_PARTY_MSG_CANT_TAKE_QUEST[ \t]*=[ \t]*1"
        "QUEST_PARTY_MSG_ACCEPT_QUEST[ \t]*=[ \t]*2"
        "QUEST_PARTY_MSG_DECLINE_QUEST[ \t]*=[ \t]*3"
        "QUEST_PARTY_MSG_BUSY[ \t]*=[ \t]*4"
        "QUEST_PARTY_MSG_DEAD[ \t]*=[ \t]*5"
        "QUEST_PARTY_MSG_LOG_FULL[ \t]*=[ \t]*6"
        "QUEST_PARTY_MSG_HAVE_QUEST[ \t]*=[ \t]*7"
        "QUEST_PARTY_MSG_FINISH_QUEST[ \t]*=[ \t]*8"
        "QUEST_PARTY_MSG_CANT_BE_SHARED_TODAY[ \t]*=[ \t]*9"
        "QUEST_PARTY_MSG_SHARING_TIMER_EXPIRED[ \t]*=[ \t]*10"
        "QUEST_PARTY_MSG_NOT_IN_PARTY[ \t]*=[ \t]*11"
        "QUEST_PARTY_MSG_DIFFERENT_SERVER_DAILY[ \t]*=[ \t]*12"
        "QUEST_PARTY_MSG_NOT_ALLOWED[ \t]*=[ \t]*13")
    require_once("${quest_def}" "${result_value}" "quest-share result value")
endforeach()
foreach(registration IN ITEMS SMSG_QUEST_CONFIRM_ACCEPT SMSG_QUEST_PUSH_RESULT)
    require_once("${opcode_registry}"
        "DefS\\(${registration},"
        "${registration} registration")
    require_once("${session_source}"
        "case[ \t]+${registration}:"
        "${registration} suppression whitelist")
endforeach()

foreach(opcode IN ITEMS
        "CMSG_PUSHQUESTTOPARTY[ \t]*=[ \t]*0x03D2"
        "CMSG_QUEST_CONFIRM_ACCEPT[ \t]*=[ \t]*0x124B"
        "SMSG_QUEST_CONFIRM_ACCEPT[ \t]*=[ \t]*0x13C7"
        "SMSG_QUEST_PUSH_RESULT[ \t]*=[ \t]*0x074D"
        "CMSG_QUEST_PUSH_RESULT[ \t]*=[ \t]*0x1370")
    require_once("${opcode_header}" "${opcode}" "quest-share opcode")
endforeach()
