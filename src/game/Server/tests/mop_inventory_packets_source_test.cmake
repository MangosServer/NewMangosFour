file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ItemHandler.cpp" item_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ItemHandlerVendor.cpp" vendor_handler)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerItemStorage.cpp" item_storage)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(MUTATION STREQUAL "swap_inv_handler")
    string(REPLACE "MopItemPackets::ParseSwapInvItem(recv_data, request)"
        "false /* removed swap-inventory parser */" item_handler "${item_handler}")
elseif(MUTATION STREQUAL "swap_handler")
    string(REPLACE "MopItemPackets::ParseSwapItem(recv_data, request)"
        "false /* removed cross-container parser */" item_handler "${item_handler}")
elseif(MUTATION STREQUAL "autoequip_handler")
    string(REPLACE "MopItemPackets::ParseAutoEquipItem(recv_data, request)"
        "false /* removed auto-equip parser */" item_handler "${item_handler}")
elseif(MUTATION STREQUAL "autostore_handler")
    string(REPLACE "MopItemPackets::ParseAutoStoreBagItem(recv_data, request)"
        "false /* removed auto-store parser */" vendor_handler "${vendor_handler}")
elseif(MUTATION STREQUAL "split_handler")
    string(REPLACE "MopItemPackets::ParseSplitItem(recv_data, request)"
        "false /* removed split parser */" item_handler "${item_handler}")
elseif(MUTATION MATCHES "^registration_(.+)$")
    if(CMAKE_MATCH_1 STREQUAL "swap_inv")
        set(opcode "CMSG_SWAP_INV_ITEM")
        set(handler "HandleSwapInvItemOpcode")
    elseif(CMAKE_MATCH_1 STREQUAL "swap")
        set(opcode "CMSG_SWAP_ITEM")
        set(handler "HandleSwapItem")
    elseif(CMAKE_MATCH_1 STREQUAL "autoequip")
        set(opcode "CMSG_AUTOEQUIP_ITEM")
        set(handler "HandleAutoEquipItemOpcode")
    elseif(CMAKE_MATCH_1 STREQUAL "autostore")
        set(opcode "CMSG_AUTOSTORE_BAG_ITEM")
        set(handler "HandleAutoStoreBagItemOpcode")
    elseif(CMAKE_MATCH_1 STREQUAL "split")
        set(opcode "CMSG_SPLIT_ITEM")
        set(handler "HandleSplitItemOpcode")
    endif()
    string(REPLACE
        "DefC(${opcode}, \"${opcode}\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::${handler});"
        "/* removed ${opcode} registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "opcode_values")
    string(REPLACE "CMSG_AUTOEQUIP_ITEM                          = 0x025F"
        "CMSG_AUTOEQUIP_ITEM                          = 0xFFFF"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "failure_sender")
    string(REPLACE "MopItemPackets::BuildInventoryChangeFailure(data, failure);"
        "/* removed inventory failure builder */" item_storage "${item_storage}")
elseif(MUTATION STREQUAL "failure_registration")
    string(REPLACE "DefS(SMSG_INVENTORY_CHANGE_FAILURE, \"SMSG_INVENTORY_CHANGE_FAILURE\");"
        "/* removed inventory failure registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "failure_admission")
    string(REPLACE "case SMSG_INVENTORY_CHANGE_FAILURE:"
        "case 0xFFFF: /* removed inventory failure admission */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "failure_opcode_value")
    string(REPLACE "SMSG_INVENTORY_CHANGE_FAILURE                = 0x0C1E"
        "SMSG_INVENTORY_CHANGE_FAILURE                = 0xFFFF"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "failure_legacy_sender")
    string(APPEND item_storage
        "\nvoid damaged() { WorldPacket data(SMSG_INVENTORY_CHANGE_FAILURE); data << uint8(0); }\n")
elseif(MUTATION STREQUAL "legacy_reader")
    string(APPEND item_handler
        "\nvoid damaged(WorldPacket& recv_data) { uint8 srcbag, srcslot; recv_data >> srcbag >> srcslot; }\n")
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

function(forbid source token context)
    string(FIND "${source}" "${token}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden legacy token remains: ${token}")
    endif()
endfunction()

require_once("${item_handler}" "MopItemPackets::ParseSwapInvItem(recv_data, request)"
    "swap-inventory handler parser")
require_once("${item_handler}" "MopItemPackets::ParseSwapItem(recv_data, request)"
    "cross-container handler parser")
require_once("${item_handler}" "MopItemPackets::ParseAutoEquipItem(recv_data, request)"
    "auto-equip handler parser")
require_once("${vendor_handler}" "MopItemPackets::ParseAutoStoreBagItem(recv_data, request)"
    "auto-store handler parser")
require_once("${item_handler}" "MopItemPackets::ParseSplitItem(recv_data, request)"
    "split handler parser")

foreach(binding IN ITEMS
        "CMSG_SWAP_INV_ITEM|HandleSwapInvItemOpcode"
        "CMSG_SWAP_ITEM|HandleSwapItem"
        "CMSG_AUTOEQUIP_ITEM|HandleAutoEquipItemOpcode"
        "CMSG_AUTOSTORE_BAG_ITEM|HandleAutoStoreBagItemOpcode"
        "CMSG_SPLIT_ITEM|HandleSplitItemOpcode")
    string(REPLACE "|" ";" fields "${binding}")
    list(GET fields 0 opcode)
    list(GET fields 1 handler)
    require_once("${opcode_registry}"
        "DefC(${opcode}, \"${opcode}\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::${handler});"
        "${opcode} registration")
endforeach()

require_once("${item_storage}"
    "MopItemPackets::BuildInventoryChangeFailure(data, failure);"
    "inventory failure 18414 sender")
require_once("${opcode_registry}"
    "DefS(SMSG_INVENTORY_CHANGE_FAILURE, \"SMSG_INVENTORY_CHANGE_FAILURE\");"
    "inventory failure registration")
require_once("${world_session}" "case SMSG_INVENTORY_CHANGE_FAILURE:"
    "inventory failure suppression admission")

foreach(value IN ITEMS
        "CMSG_AUTOEQUIP_ITEM                          = 0x025F"
        "CMSG_AUTOSTORE_BAG_ITEM                      = 0x067C"
        "CMSG_SWAP_ITEM                               = 0x035D"
        "CMSG_SWAP_INV_ITEM                           = 0x03DF"
        "CMSG_SPLIT_ITEM                              = 0x02EC"
        "SMSG_INVENTORY_CHANGE_FAILURE                = 0x0C1E")
    require_once("${opcode_header}" "${value}" "binary-proven inventory opcode")
endforeach()

forbid("${item_storage}" "WorldPacket data(SMSG_INVENTORY_CHANGE_FAILURE"
    "legacy inventory failure sender")

set(handler_sources "${item_handler}${vendor_handler}")
foreach(legacy IN ITEMS
        "recv_data >> dstslot >> srcslot;"
        "recv_data >> dstbag >> dstslot >> srcbag >> srcslot"
        "recv_data >> srcbag >> srcslot;"
        "recv_data >> srcbag >> srcslot >> dstbag;"
        "recv_data >> srcbag >> srcslot >> dstbag >> dstslot >> count;")
    forbid("${handler_sources}" "${legacy}" "legacy inventory request reader")
endforeach()
