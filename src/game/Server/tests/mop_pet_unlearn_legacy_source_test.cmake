file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player_source)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerGossip.cpp" gossip_source)
file(READ "${SOURCE_ROOT}/src/game/Object/PetSpells.cpp" pet_spells)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/PetHandler.cpp" pet_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)

if(MUTATION STREQUAL "hidden_option")
    string(REPLACE
        "case GOSSIP_OPTION_UNLEARNPETSKILLS:\n                    hasMenuItem = false;"
        "case GOSSIP_OPTION_UNLEARNPETSKILLS:\n                    hasMenuItem = true;"
        gossip_source "${gossip_source}")
elseif(MUTATION STREQUAL "legacy_sender")
    string(APPEND player_source "\nvoid Player::SendPetSkillWipeConfirm() {}\n")
elseif(MUTATION STREQUAL "legacy_handler")
    string(APPEND pet_handler "\nvoid WorldSession::HandlePetUnlearnOpcode(WorldPacket&) {}\n")
elseif(MUTATION STREQUAL "legacy_opcodes")
    string(APPEND opcode_header "\nWorldPacket oldRequest(CMSG_PET_UNLEARN);\nWorldPacket oldReply(SMSG_PET_UNLEARN_CONFIRM);\n")
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

require_once("${gossip_source}"
    "case GOSSIP_OPTION_UNLEARNPETSKILLS:\n                    hasMenuItem = false;"
    "legacy pet-unlearn gossip option tombstone")
require_once("${pet_spells}"
    "bool Pet::resetTalents(bool no_cost)"
    "independently used pet reset backend")

set(production "${player_source}${player_header}${gossip_source}${pet_handler}${session_header}${opcode_header}")
foreach(token IN ITEMS
        "SendPetSkillWipeConfirm"
        "HandlePetUnlearnOpcode"
        "CMSG_PET_UNLEARN"
        "SMSG_PET_UNLEARN_CONFIRM")
    forbid("${production}" "${token}" "removed pet-unlearn protocol endpoint")
endforeach()
