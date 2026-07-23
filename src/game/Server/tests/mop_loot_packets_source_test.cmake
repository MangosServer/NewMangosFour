file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/LootHandler.cpp" loot_handler)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerLoot.cpp" player_loot)
file(READ "${SOURCE_ROOT}/src/game/Object/LootMgr.cpp" loot_manager)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(MUTATION STREQUAL "open_parser")
    string(REPLACE "MopLootPackets::ParseLootRequest(recv_data, rawGuid)"
        "false /* removed open parser */" loot_handler "${loot_handler}")
elseif(MUTATION STREQUAL "take_parser")
    string(REPLACE "MopLootPackets::ParseAutostoreLootItemRequest(recv_data, entries)"
        "false /* removed item-take parser */" loot_handler "${loot_handler}")
elseif(MUTATION STREQUAL "money_parser")
    string(REPLACE "MopLootPackets::ParseLootMoneyRequest(recv_data)"
        "false /* removed money parser */" loot_handler "${loot_handler}")
elseif(MUTATION STREQUAL "release_parser")
    string(REPLACE "MopLootPackets::ParseLootReleaseRequest(recv_data, requestRawGuid)"
        "false /* removed release parser */" loot_handler "${loot_handler}")
elseif(MUTATION STREQUAL "response_sender")
    string(REPLACE "BuildMopLootResponse(data, LootView(*loot, this, permission), guid,"
        "false /* removed loot response sender */ && (data, LootView(*loot, this, permission), guid,"
        player_loot "${player_loot}")
elseif(MUTATION STREQUAL "release_sender")
    string(REPLACE "MopLootPackets::BuildLootReleaseResponse(data, guid.GetRawValue(),"
        "/* removed release response */ (data, guid.GetRawValue(),"
        player_loot "${player_loot}")
elseif(MUTATION STREQUAL "removed_sender")
    string(REPLACE "MopLootPackets::BuildLootRemoved(data, lootGuid.GetRawValue(),"
        "/* removed slot removal */ (data, lootGuid.GetRawValue(),"
        player_loot "${player_loot}")
elseif(MUTATION STREQUAL "money_sender")
    string(REPLACE "MopLootPackets::BuildLootMoneyNotify"
        "RemovedLootMoneyNotify" loot_handler "${loot_handler}")
elseif(MUTATION STREQUAL "situ_model")
    string(REPLACE "wireItem.situ.assign(4, 0);"
        "wireItem.situ.clear();" loot_manager "${loot_manager}")
elseif(MUTATION STREQUAL "registration_cmsg")
    string(REPLACE
        "DefC(CMSG_LOOT, \"CMSG_LOOT\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleLootOpcode);"
        "/* removed CMSG_LOOT registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "registration_smsg")
    string(REPLACE "DefS(SMSG_LOOT_RESPONSE, \"SMSG_LOOT_RESPONSE\");"
        "/* removed SMSG_LOOT_RESPONSE registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "admission")
    string(REPLACE "case SMSG_LOOT_RESPONSE:"
        "case 0xFFFF: /* removed loot response admission */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "opcode_values")
    string(REPLACE "CMSG_LOOT                                    = 0x1CE2"
        "CMSG_LOOT                                    = 0xFFFF"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "unknown_name")
    string(REPLACE "SMSG_UNKNOWN_080F                            = 0x080F"
        "SMSG_LOOT_ITEM_NOTIFY                        = 0x080F"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "legacy_reader")
    string(APPEND loot_handler
        "\nvoid damaged(WorldPacket& recv_data) { ObjectGuid guid; recv_data >> guid; }\n")
elseif(MUTATION STREQUAL "legacy_sender")
    string(APPEND player_loot
        "\nvoid damaged() { WorldPacket data(SMSG_LOOT_RESPONSE); data << uint8(0); }\n")
endif()

function(require_once source token context)
    string(FIND "${source}" "${token}" first)
    if(first EQUAL -1)
        message(FATAL_ERROR "${context}: required token absent: ${token}")
    endif()
    string(LENGTH "${token}" length)
    math(EXPR after "${first} + ${length}")
    string(SUBSTRING "${source}" ${after} -1 remainder)
    string(FIND "${remainder}" "${token}" second)
    if(NOT second EQUAL -1)
        message(FATAL_ERROR "${context}: required token occurs more than once: ${token}")
    endif()
endfunction()

function(require_present source token context)
    string(FIND "${source}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${context}: required token absent: ${token}")
    endif()
endfunction()

function(forbid source token context)
    string(FIND "${source}" "${token}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden legacy token remains: ${token}")
    endif()
endfunction()

require_once("${loot_handler}" "MopLootPackets::ParseLootRequest(recv_data, rawGuid)"
    "loot-open handler parser")
require_once("${loot_handler}" "MopLootPackets::ParseAutostoreLootItemRequest(recv_data, entries)"
    "loot-item handler parser")
require_once("${loot_handler}" "MopLootPackets::ParseLootMoneyRequest(recv_data)"
    "loot-money handler parser")
require_once("${loot_handler}" "MopLootPackets::ParseLootReleaseRequest(recv_data, requestRawGuid)"
    "loot-release handler parser")

require_once("${player_loot}"
    "BuildMopLootResponse(data, LootView(*loot, this, permission), guid,"
    "loot-window response sender")
require_once("${player_loot}"
    "MopLootPackets::BuildLootReleaseResponse(data, guid.GetRawValue(),"
    "loot-release response sender")
require_once("${player_loot}"
    "MopLootPackets::BuildLootClearMoney(data, lootGuid.GetRawValue())"
    "loot-money clear sender")
require_once("${player_loot}"
    "MopLootPackets::BuildLootRemoved(data, lootGuid.GetRawValue(),"
    "loot-slot removal sender")
require_present("${loot_handler}" "MopLootPackets::BuildLootMoneyNotify"
    "loot-money notification sender")
require_once("${loot_manager}" "MopLootPackets::BuildLootResponse(out, response)"
    "loot-view model encoder")
require_once("${loot_manager}" "wireItem.situ.assign(4, 0);"
    "binary-observed ordinary item modifier block")

foreach(binding IN ITEMS
        "CMSG_LOOT|HandleLootOpcode"
        "CMSG_AUTOSTORE_LOOT_ITEM|HandleAutostoreLootItemOpcode"
        "CMSG_LOOT_MONEY|HandleLootMoneyOpcode"
        "CMSG_LOOT_RELEASE|HandleLootReleaseOpcode")
    string(REPLACE "|" ";" fields "${binding}")
    list(GET fields 0 opcode)
    list(GET fields 1 handler)
    require_once("${opcode_registry}"
        "DefC(${opcode}, \"${opcode}\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::${handler});"
        "${opcode} registration")
endforeach()

foreach(opcode IN ITEMS
        SMSG_LOOT_RESPONSE
        SMSG_LOOT_RELEASE_RESPONSE
        SMSG_LOOT_REMOVED
        SMSG_LOOT_MONEY_NOTIFY
        SMSG_LOOT_CLEAR_MONEY)
    require_once("${opcode_registry}" "DefS(${opcode}, \"${opcode}\");"
        "${opcode} registration")
    require_once("${world_session}" "case ${opcode}:"
        "${opcode} suppression admission")
endforeach()

foreach(value IN ITEMS
        "CMSG_AUTOSTORE_LOOT_ITEM                     = 0x0354"
        "CMSG_LOOT                                    = 0x1CE2"
        "CMSG_LOOT_MONEY                              = 0x02F6"
        "CMSG_LOOT_RELEASE                            = 0x0840"
        "SMSG_LOOT_RESPONSE                           = 0x128A"
        "SMSG_LOOT_RELEASE_RESPONSE                   = 0x123F"
        "SMSG_LOOT_REMOVED                            = 0x0C3E"
        "SMSG_LOOT_MONEY_NOTIFY                       = 0x14C0"
        "SMSG_LOOT_CLEAR_MONEY                        = 0x1632"
        "SMSG_UNKNOWN_080F                            = 0x080F")
    require_once("${opcode_header}" "${value}" "binary-proven loot opcode")
endforeach()

require_once("${opcode_reference}" "SMSG_UNKNOWN_080F"
    "disproved 0x080F name in reference inventory")
forbid("${opcode_header}${opcode_reference}" "SMSG_LOOT_ITEM_NOTIFY"
    "disproved 0x080F loot-item-notify name")

foreach(legacy IN ITEMS
        "recv_data >> lootSlot;"
        "recv_data >> guid;"
        "recv_data.read_skip<uint64>()"
        "WorldPacket data(SMSG_LOOT_RESPONSE"
        "WorldPacket data(SMSG_LOOT_RELEASE_RESPONSE")
    forbid("${loot_handler}${player_loot}" "${legacy}"
        "legacy loot packet body")
endforeach()
