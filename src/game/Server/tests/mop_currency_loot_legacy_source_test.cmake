file(READ "${SOURCE_ROOT}/src/game/Object/PlayerLoot.cpp" player_loot)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/CurrencyMgr.h" currency_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/LootHandler.cpp" loot_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(MUTATION STREQUAL "currency_send")
    string(REPLACE "player->SendNotifyLootItemRemoved(lootSlot);"
        "/* removed converted currency-row removal */"
        loot_handler "${loot_handler}")
elseif(MUTATION STREQUAL "legacy_selector")
    string(APPEND player_loot
        "\nvoid Player::SendNotifyLootItemRemoved(uint8, bool currency) { WorldPacket data(currency ? SMSG_LOOT_CURRENCY_REMOVED : SMSG_LOOT_REMOVED); }\n")
elseif(MUTATION STREQUAL "legacy_opcode")
    string(APPEND opcode_header "\nWorldPacket oldPacket(SMSG_LOOT_CURRENCY_REMOVED);\n")
elseif(MUTATION STREQUAL "currency_accounting")
    string(REPLACE "currency->is_looted = true;"
        "/* removed personal currency loot state */"
        loot_handler "${loot_handler}")
elseif(MUTATION STREQUAL "generic_backend")
    string(REPLACE "void Player::SendNotifyLootItemRemoved(uint8 lootSlot)"
        "void Player::RemovedGenericLootBackend(uint8 lootSlot)"
        player_loot "${player_loot}")
elseif(MUTATION STREQUAL "premature_registration")
    string(REPLACE "DefS(SMSG_LOOT_REMOVED, \"SMSG_LOOT_REMOVED\");"
        "/* removed generic loot-removal registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "premature_allowlist")
    string(REPLACE "case SMSG_LOOT_REMOVED:"
        "case 0xFFFF: /* removed generic loot-removal admission */"
        world_session "${world_session}")
endif()

function(require_once source token context)
    string(FIND "${source}" "${token}" first)
    if(first EQUAL -1)
        message(FATAL_ERROR "${context}: missing active token: ${token}")
    endif()
    string(LENGTH "${token}" token_length)
    math(EXPR next_position "${first} + ${token_length}")
    string(SUBSTRING "${source}" ${next_position} -1 remaining)
    string(FIND "${remaining}" "${token}" second)
    if(NOT second EQUAL -1)
        message(FATAL_ERROR "${context}: token occurs more than once: ${token}")
    endif()
endfunction()

function(forbid source token context)
    string(FIND "${source}" "${token}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden token remains: ${token}")
    endif()
endfunction()

function(extract_body output source start_marker end_marker)
    string(FIND "${source}" "${start_marker}" start)
    string(FIND "${source}" "${end_marker}" end)
    if(start EQUAL -1 OR end EQUAL -1 OR NOT start LESS end)
        message(FATAL_ERROR "Could not isolate ${start_marker}")
    endif()
    math(EXPR length "${end} - ${start}")
    string(SUBSTRING "${source}" ${start} ${length} body)
    set(${output} "${body}" PARENT_SCOPE)
endfunction()

extract_body(currency_branch "${loot_handler}"
    "if (currency)"
    "ItemPosCountVec dest;")

foreach(token IN ITEMS
        "player->ModifyCurrencyCount(item->itemid,"
        "int32(item->count * currencyEntry->GetPrecision()))"
        "currency->is_looted = true;"
        "--loot->unlootedCount;"
        "player->SendNotifyLootItemRemoved(lootSlot);")
    string(FIND "${currency_branch}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "live currency loot accounting missing: ${token}")
    endif()
endforeach()

require_once("${player_loot}" "void Player::SendNotifyLootItemRemoved(uint8 lootSlot)"
    "generic loot-removal backend")
require_once("${player_header}" "void SendNotifyLootItemRemoved(uint8 lootSlot);"
    "generic loot-removal declaration")
require_once("${opcode_header}" "SMSG_LOOT_REMOVED                            = 0x0C3E"
    "binary-proven generic loot-removal leaf")
require_once("${opcode_registry}" "DefS(SMSG_LOOT_REMOVED, \"SMSG_LOOT_REMOVED\");"
    "converted generic loot-removal registration")
require_once("${world_session}" "case SMSG_LOOT_REMOVED:"
    "converted generic loot-removal admission")

set(production "${player_loot}${player_header}${currency_header}${loot_handler}${opcode_header}")
foreach(token IN ITEMS
        "SMSG_LOOT_CURRENCY_REMOVED"
        "SendNotifyLootItemRemoved(uint8 lootSlot, bool currency"
        "SendNotifyLootItemRemoved(lootSlot, true)"
        "currency ? SMSG_LOOT_CURRENCY_REMOVED"
        "SendNotifyLootItemRemoved(_, bool currency)")
    forbid("${production}" "${token}" "retired currency-specific loot-removal wiring")
endforeach()
