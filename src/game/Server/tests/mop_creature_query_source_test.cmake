file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QueryHandler.cpp" query_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)

if(MUTATION STREQUAL "builder_call")
    string(REPLACE
        "    MopQueryPackets::BuildCreatureQueryResponse(data, response);"
        "    // MopQueryPackets::BuildCreatureQueryResponse(data, response);"
        query_handler "${query_handler}")
elseif(MUTATION STREQUAL "inbound_registration")
    string(REPLACE
        "    DefC(CMSG_CREATURE_QUERY, \"CMSG_CREATURE_QUERY\", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleCreatureQueryOpcode);"
        "    // DefC(CMSG_CREATURE_QUERY, \"CMSG_CREATURE_QUERY\", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleCreatureQueryOpcode);"
        opcode_registry "${opcode_registry}")
endif()

function(strip_cpp_comments output source)
    set(text "${source}")
    while(TRUE)
        string(FIND "${text}" "/*" comment_start)
        if(comment_start EQUAL -1)
            break()
        endif()
        math(EXPR tail_start "${comment_start} + 2")
        string(SUBSTRING "${text}" ${tail_start} -1 tail)
        string(FIND "${tail}" "*/" comment_end)
        if(comment_end EQUAL -1)
            message(FATAL_ERROR "Unterminated block comment while scanning source")
        endif()
        string(SUBSTRING "${text}" 0 ${comment_start} before)
        math(EXPR after_start "${comment_end} + 2")
        string(SUBSTRING "${tail}" ${after_start} -1 after)
        set(text "${before}${after}")
    endwhile()
    string(REGEX REPLACE "//[^\r\n]*" "" text "${text}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

function(require_ordered source context)
    set(remaining "${source}")
    foreach(token IN LISTS ARGN)
        string(FIND "${remaining}" "${token}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR "${context}: missing active ordered token: ${token}")
        endif()
        string(LENGTH "${token}" token_length)
        math(EXPR next_position "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endforeach()
endfunction()

function(require_exactly_one source token context)
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
        message(FATAL_ERROR "${context}: expected exactly one active occurrence, found ${count}")
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

strip_cpp_comments(query_handler "${query_handler}")
strip_cpp_comments(opcode_registry "${opcode_registry}")
extract_body(creature_handler "${query_handler}"
    "void WorldSession::HandleCreatureQueryOpcode"
    "void WorldSession::HandleGameObjectQueryOpcode")

require_ordered("${creature_handler}" "creature query handler"
    "recv_data >> entry"
    "ObjectMgr::GetCreatureTemplate(entry)"
    "MopQueryPackets::BuildCreatureQueryResponse(data"
    "SendPacket(&data)")

foreach(forbidden IN ITEMS "ObjectGuid" "recv_data >> guid" "0x80000000" "data << uint32(entry)")
    string(FIND "${creature_handler}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "creature query handler: forbidden active token: ${forbidden}")
    endif()
endforeach()

string(REGEX REPLACE "[ \t\r\n]+" " " normalized_registry "${opcode_registry}")
set(inbound_registration
    "DefC(CMSG_CREATURE_QUERY, \"CMSG_CREATURE_QUERY\", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleCreatureQueryOpcode);")
set(outbound_registration
    "DefS(SMSG_CREATURE_QUERY_RESPONSE, \"SMSG_CREATURE_QUERY_RESPONSE\");")
require_exactly_one("${normalized_registry}" "${inbound_registration}" "CMSG_CREATURE_QUERY registration")
require_exactly_one("${normalized_registry}" "${outbound_registration}" "SMSG_CREATURE_QUERY_RESPONSE registration")
