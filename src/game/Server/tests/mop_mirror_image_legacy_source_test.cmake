file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellHandler.cpp" spell_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)

if(MUTATION STREQUAL "legacy_handler")
    string(APPEND spell_handler "\nvoid WorldSession::HandleGetMirrorimageData(WorldPacket&) {}\n")
elseif(MUTATION STREQUAL "legacy_opcode")
    string(APPEND opcode_header "\nWorldPacket oldReply(SMSG_MIRRORIMAGE_DATA);\n")
elseif(MUTATION STREQUAL "modern_request")
    string(REPLACE
        "CMSG_GET_MIRRORIMAGE_DATA"
        "CMSG_REMOVED_MIRRORIMAGE_DATA"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "modern_split")
    string(REPLACE
        "SMSG_MIRROR_IMAGE_COMPONENTED_DATA"
        "SMSG_REMOVED_MIRROR_IMAGE_COMPONENTED_DATA"
        opcode_header "${opcode_header}")
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

set(production "${spell_handler}${session_header}${opcode_header}")
foreach(token IN ITEMS
        "HandleGetMirrorimageData"
        "SMSG_MIRRORIMAGE_DATA")
    forbid("${production}" "${token}" "removed dormant mirror-image legacy endpoint")
endforeach()

require_once("${opcode_header}"
    "CMSG_GET_MIRRORIMAGE_DATA"
    "binary-proven dormant mirror-image request")
require_once("${opcode_header}"
    "SMSG_MIRROR_IMAGE_CREATURE_DATA"
    "binary-proven creature/model response")
require_once("${opcode_header}"
    "SMSG_MIRROR_IMAGE_COMPONENTED_DATA"
    "binary-proven player component response")
