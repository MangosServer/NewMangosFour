file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerAreaTrigger.cpp" area_trigger)
file(READ "${SOURCE_ROOT}/src/game/ChatCommands/CommunicationCommands.cpp" communication)

if(MUTATION STREQUAL "length_width")
    string(REPLACE "WriteBits(strlen(szStr), 12)" "WriteBits(strlen(szStr), 13)"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefS(SMSG_NOTIFICATION, \"SMSG_NOTIFICATION\");"
        "/* removed notification registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE "case SMSG_NOTIFICATION:"
        "case 0xFFFF: /* removed notification allowlist */" world_session "${world_session}")
elseif(MUTATION STREQUAL "area_trigger_route")
    string(REPLACE "GetSession()->SendNotification(" "GetSession()->SendAreaTriggerMessage("
        area_trigger "${area_trigger}")
elseif(MUTATION STREQUAL "admin_route")
    string(REPLACE "rPlayerSession->SendNotification(" "rPlayerSession->SendAreaTriggerMessage("
        communication "${communication}")
elseif(MUTATION STREQUAL "compatibility_forward")
    string(REPLACE "SendNotification(format, args...);"
        "/* removed Eluna compatibility forwarding */"
        session_header "${session_header}")
elseif(MUTATION STREQUAL "legacy_backend")
    string(APPEND world_session "\nvoid WorldSession::SendAreaTriggerMessage(const char*, ...) {}\n")
elseif(MUTATION STREQUAL "legacy_opcode")
    string(APPEND opcode_header "\nWorldPacket oldPacket(SMSG_AREA_TRIGGER_MESSAGE);\n")
elseif(MUTATION STREQUAL "opcode_value")
    string(REPLACE "SMSG_NOTIFICATION                            = 0x0C2A"
        "SMSG_NOTIFICATION                            = 0x0C2B"
        opcode_header "${opcode_header}")
endif()

function(require_count source token expected context)
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
    if(NOT count EQUAL expected)
        message(FATAL_ERROR "${context}: expected ${expected} active occurrences, found ${count}")
    endif()
endfunction()

function(forbid source token context)
    string(FIND "${source}" "${token}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden token remains: ${token}")
    endif()
endfunction()

require_count("${world_session}" "WriteBits(strlen(szStr), 12)" 2
    "12-bit notification text lengths")
require_count("${world_session}" "data.append(szStr, strlen(szStr))" 2
    "raw non-NUL notification text bodies")
require_count("${opcode_registry}" "DefS(SMSG_NOTIFICATION, \"SMSG_NOTIFICATION\");" 1
    "notification opcode registration")
require_count("${world_session}" "case SMSG_NOTIFICATION:" 1
    "notification suppression allowlist")
require_count("${opcode_header}" "SMSG_NOTIFICATION                            = 0x0C2A" 1
    "binary-proven notification opcode")
require_count("${area_trigger}" "GetSession()->SendNotification(" 2
    "area-trigger feedback migration")
require_count("${communication}" "rPlayerSession->SendNotification(" 2
    "administrator message migration")
require_count("${session_header}" "SendNotification(format, args...);" 1
    "Eluna source-compatibility forwarding facade")

set(production "${world_session}${session_header}${opcode_header}${area_trigger}${communication}")
foreach(token IN ITEMS
        "WorldSession::SendAreaTriggerMessage"
        "SMSG_AREA_TRIGGER_MESSAGE")
    forbid("${production}" "${token}" "retired area-trigger message endpoint")
endforeach()
