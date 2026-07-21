file(READ "${SOURCE_ROOT}/src/game/Server/MopQueryPackets.cpp" packet_helpers)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QueryHandler.cpp" query_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)

if(MUTATION STREQUAL "request_parser_call")
    string(REPLACE "MopQueryPackets::ReadNameQueryRequest(recv_data)"
        "MopQueryPackets::ReadNameQueryRequest_disabled(recv_data)" query_handler "${query_handler}")
elseif(MUTATION STREQUAL "response_builder_call")
    string(REPLACE "MopQueryPackets::BuildNameQueryResponse(data, response)"
        "MopQueryPackets::BuildNameQueryResponse_disabled(data, response)" query_handler "${query_handler}")
elseif(MUTATION STREQUAL "inbound_registration")
    string(REPLACE
        "DefC(CMSG_NAME_QUERY, \"CMSG_NAME_QUERY\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleNameQueryOpcode);"
        "DefC_disabled(CMSG_NAME_QUERY, \"CMSG_NAME_QUERY\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleNameQueryOpcode);"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "early_result_delete")
    string(REPLACE
        "    if (!session)\n    {\n        delete result;\n        return;\n    }"
        "    if (!session)\n    {\n        return;\n    }"
        query_handler "${query_handler}")
elseif(MUTATION STREQUAL "tail_result_delete")
    string(REPLACE
        "    session->SendPacket(&data);\n    delete result;"
        "    session->SendPacket(&data);"
        query_handler "${query_handler}")
endif()

function(strip_cpp_comments output source)
    set(text "${source}")
    while(TRUE)
        string(FIND "${text}" "/*" start)
        if(start EQUAL -1)
            break()
        endif()
        math(EXPR tail_start "${start} + 2")
        string(SUBSTRING "${text}" ${tail_start} -1 tail)
        string(FIND "${tail}" "*/" stop)
        if(stop EQUAL -1)
            message(FATAL_ERROR "Unterminated block comment")
        endif()
        string(SUBSTRING "${text}" 0 ${start} before)
        math(EXPR after_start "${stop} + 2")
        string(SUBSTRING "${tail}" ${after_start} -1 after)
        set(text "${before}${after}")
    endwhile()
    string(REGEX REPLACE "//[^\r\n]*" "" text "${text}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

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
        math(EXPR next "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next} -1 remaining)
    endwhile()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected once, found ${count}: ${token}")
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

strip_cpp_comments(packet_helpers "${packet_helpers}")
strip_cpp_comments(query_handler "${query_handler}")
strip_cpp_comments(opcode_registry "${opcode_registry}")
string(REGEX REPLACE "[ \t\r\n]+" " " normalized_query_handler "${query_handler}")

extract_body(handle_name "${query_handler}"
    "void WorldSession::HandleNameQueryOpcode" "void WorldSession::HandleQueryTimeOpcode")
extract_body(send_name "${query_handler}"
    "void WorldSession::SendNameQueryOpcode(Player* p)" "void WorldSession::SendNameQueryOpcodeFromDB")
extract_body(start_db "${query_handler}"
    "void WorldSession::SendNameQueryOpcodeFromDB(ObjectGuid guid)"
    "void WorldSession::SendNameQueryOpcodeFromDBCallBack")
extract_body(db_callback "${query_handler}"
    "void WorldSession::SendNameQueryOpcodeFromDBCallBack" "void WorldSession::HandleNameQueryOpcode")

foreach(forbidden IN ITEMS "recv_data >> guid" "GetPackGUID()" "WriteAsPacked()")
    string(FIND "${handle_name}${send_name}${db_callback}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "name query: forbidden active legacy token: ${forbidden}")
    endif()
endforeach()

foreach(required IN ITEMS
        "MopQueryPackets::ReadNameQueryRequest(recv_data)"
        "sObjectMgr.GetPlayer(ObjectGuid(request.guid))"
        "SendNameQueryOpcodeFromDB(ObjectGuid(request.guid))")
    string(FIND "${handle_name}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "name query handler missing: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "&WorldSession::SendNameQueryOpcodeFromDBCallBack"
        "GetAccountId(), guid.GetRawValue()")
    string(FIND "${start_db}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "database request missing: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "MopQueryPackets::NameQueryResponse response"
        "response.result = 0"
        "response.realmId = realmID"
        "response.accountId = 0"
        "response.level = 0"
        "response.displayGuid = response.guid"
        "MopQueryPackets::BuildNameQueryResponse(data, response)"
        "SendPacket(&data)")
    string(FIND "${send_name}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "in-memory response missing: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "uint32 accountId, uint64 requestedGuid"
        "WorldSession* session = sWorld.FindSession(accountId)"
        "if (!session)"
        "delete result"
        "uint64 requestedGuid"
        "response.guid = requestedGuid"
        "if (result)"
        "response.result = 1"
        "response.accountId = 0"
        "response.level = 0"
        "MopQueryPackets::BuildNameQueryResponse(data, response)"
        "session->SendPacket(&data)"
        "delete result")
    string(FIND "${db_callback}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "database response missing: ${required}")
    endif()
endforeach()
require_once("${db_callback}" "response.guid = requestedGuid" "original GUID response mapping")
require_once("${db_callback}" "response.displayGuid = requestedGuid" "DB hit display GUID mapping")
string(REGEX REPLACE "[ \t\r\n]+" " " normalized_callback "${db_callback}")
string(FIND "${normalized_callback}"
    "if (!session) { delete result; return; }" early_ownership)
if(early_ownership EQUAL -1)
    message(FATAL_ERROR "database response: absent-session result cleanup missing")
endif()
string(FIND "${normalized_callback}"
    "session->SendPacket(&data); delete result;" tail_ownership)
if(tail_ownership EQUAL -1)
    message(FATAL_ERROR "database response: sent-result tail cleanup missing")
endif()

require_once("${query_handler}" "MopQueryPackets::ReadNameQueryRequest(recv_data)" "request parser")
require_once("${opcode_registry}"
    "DefC(CMSG_NAME_QUERY, \"CMSG_NAME_QUERY\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleNameQueryOpcode);"
    "CMSG_NAME_QUERY registration")
require_once("${opcode_registry}"
    "DefS(SMSG_NAME_QUERY_RESPONSE, \"SMSG_NAME_QUERY_RESPONSE\");"
    "SMSG_NAME_QUERY_RESPONSE registration")

string(FIND "${packet_helpers}" "MopQueryPackets::ReadNameQueryRequest" parser)
string(FIND "${packet_helpers}" "MopQueryPackets::BuildNameQueryResponse" builder)
if(parser EQUAL -1 OR builder EQUAL -1)
    message(FATAL_ERROR "name-query packet interfaces missing")
endif()
foreach(required IN ITEMS
        "MANGOS_ASSERT(record.name.size() <= 48)"
        "MANGOS_ASSERT(name.size() <= 64)")
    string(FIND "${packet_helpers}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "name-query client string bound missing: ${required}")
    endif()
endforeach()
