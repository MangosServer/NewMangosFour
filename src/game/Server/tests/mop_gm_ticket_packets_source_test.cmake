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
elseif(MUTATION STREQUAL "get_client_registration")
    string(REPLACE "DefC(CMSG_GMTICKET_GETTICKET, \"CMSG_GMTICKET_GETTICKET\""
        "/* removed get-ticket client registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "get_server_registration")
    string(REPLACE "DefS(SMSG_GMTICKET_GETTICKET, \"SMSG_GMTICKET_GETTICKET\");"
        "/* removed get-ticket server registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "system_client_registration")
    string(REPLACE "DefC(CMSG_GMTICKET_SYSTEMSTATUS, \"CMSG_GMTICKET_SYSTEMSTATUS\""
        "/* removed system-status client registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "system_server_registration")
    string(REPLACE "DefS(SMSG_GMTICKET_SYSTEMSTATUS, \"SMSG_GMTICKET_SYSTEMSTATUS\");"
        "/* removed system-status server registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "get_whitelist")
    string(REPLACE "case SMSG_GMTICKET_GETTICKET:"
        "/* removed get-ticket whitelist */" session_source "${session_source}")
elseif(MUTATION STREQUAL "system_whitelist")
    string(REPLACE "case SMSG_GMTICKET_SYSTEMSTATUS:"
        "/* removed system-status whitelist */" session_source "${session_source}")
elseif(MUTATION STREQUAL "case_client_registration")
    string(REPLACE "DefC(CMSG_GM_UPDATE_TICKET_STATUS, \"CMSG_GM_UPDATE_TICKET_STATUS\""
        "/* removed case-status client registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "case_server_registration")
    string(REPLACE "DefS(SMSG_GM_TICKET_CASE_STATUS, \"SMSG_GM_TICKET_CASE_STATUS\");"
        "/* removed case-status server registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "case_whitelist")
    string(REPLACE "case SMSG_GM_TICKET_CASE_STATUS:"
        "/* removed case-status whitelist */" session_source "${session_source}")
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

foreach(required IN ITEMS
        "BuildGetTicket"
        "BuildSystemStatus"
        "BuildCaseStatus")
    string(FIND "${ticket_header}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "GM-ticket response builder missing: ${required}")
    endif()
endforeach()
foreach(required IN ITEMS
        "DefC(CMSG_GMTICKET_GETTICKET, \"CMSG_GMTICKET_GETTICKET\""
        "DefS(SMSG_GMTICKET_GETTICKET, \"SMSG_GMTICKET_GETTICKET\");"
        "DefC(CMSG_GMTICKET_SYSTEMSTATUS, \"CMSG_GMTICKET_SYSTEMSTATUS\""
        "DefS(SMSG_GMTICKET_SYSTEMSTATUS, \"SMSG_GMTICKET_SYSTEMSTATUS\");"
        "DefC(CMSG_GM_UPDATE_TICKET_STATUS, \"CMSG_GM_UPDATE_TICKET_STATUS\""
        "DefS(SMSG_GM_TICKET_CASE_STATUS, \"SMSG_GM_TICKET_CASE_STATUS\");")
    string(FIND "${opcode_registry}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "GM-ticket request/response registration missing: ${required}")
    endif()
endforeach()
foreach(required IN ITEMS
        "MopGMTicketPackets::BuildGetTicket(data"
        "MopGMTicketPackets::BuildSystemStatus(data"
        "MopGMTicketPackets::BuildCaseStatus(data")
    string(FIND "${ticket_handler}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "GM-ticket handler integration missing: ${required}")
    endif()
endforeach()
foreach(required IN ITEMS
        "case SMSG_GMTICKET_GETTICKET:"
        "case SMSG_GMTICKET_SYSTEMSTATUS:"
        "case SMSG_GM_TICKET_CASE_STATUS:")
    string(FIND "${session_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "GM-ticket response whitelist missing: ${required}")
    endif()
endforeach()
