file(READ "${SOURCE_ROOT}/src/game/Object/GMTicketMgr.h" ticket_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GMTicketHandler.cpp" ticket_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "response_width")
    string(REPLACE "out << uint8(response);" "out << uint32(response);"
        ticket_header "${ticket_header}")
elseif(MUTATION STREQUAL "callsite")
    string(REPLACE
        "MopGMTicketPackets::BuildUpdate(data, responce);"
        "data << uint32(responce);"
        ticket_handler "${ticket_handler}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE
        "DefS(SMSG_GM_TICKET_UPDATE, \"SMSG_GM_TICKET_UPDATE\");"
        "/* removed GM-ticket update registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "whitelist")
    string(REPLACE
        "case SMSG_GM_TICKET_UPDATE:"
        "/* removed GM-ticket update whitelist */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode_value")
    string(REPLACE
        "SMSG_GM_TICKET_UPDATE                        = 0x02A6"
        "SMSG_GM_TICKET_UPDATE                        = 0x02A7"
        opcode_header "${opcode_header}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once("${ticket_header}"
    "out << uint8\\(response\\)"
    "GM-ticket update byte writer")
require_once("${ticket_header}"
    "out.Initialize\\(SMSG_GM_TICKET_UPDATE,[ \\t]*1\\)"
    "GM-ticket update opcode initialization")

string(REGEX MATCHALL
    "MopGMTicketPackets::BuildUpdate\\(data,[ \\t]*[A-Za-z0-9_]+\\)"
    update_calls "${ticket_handler}")
list(LENGTH update_calls update_call_count)
if(NOT update_call_count EQUAL 4)
    message(FATAL_ERROR "expected four GM-ticket update response call sites, found ${update_call_count}")
endif()

foreach(legacy IN ITEMS SMSG_GMTICKET_CREATE SMSG_GMTICKET_UPDATETEXT SMSG_GMTICKET_DELETETICKET)
    if(ticket_handler MATCHES "WorldPacket[ \\t]+data\\(${legacy}")
        message(FATAL_ERROR "live legacy GM-ticket response sender remains: ${legacy}")
    endif()
endforeach()

require_once("${opcode_registry}"
    "DefS\\(SMSG_GM_TICKET_UPDATE,[ \\t]*\"SMSG_GM_TICKET_UPDATE\"\\)"
    "GM-ticket update registration")
require_once("${session_source}"
    "case[ \\t]+SMSG_GM_TICKET_UPDATE:"
    "GM-ticket update suppression whitelist")
require_once("${opcode_header}"
    "SMSG_GM_TICKET_UPDATE[ \\t]*=[ \\t]*0x02A6"
    "GM-ticket update opcode")
