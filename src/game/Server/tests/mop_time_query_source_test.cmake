file(READ "${SOURCE_ROOT}/src/game/Server/MopQueryPackets.cpp" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QueryHandler.cpp" query_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GMTicketHandler.cpp" gm_ticket_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)

if(MUTATION STREQUAL "query_builder_call")
    string(REPLACE
        "    MopQueryPackets::BuildQueryTimeResponse(data,"
        "    // MopQueryPackets::BuildQueryTimeResponse(data,"
        query_handler "${query_handler}")
elseif(MUTATION STREQUAL "played_builder_call")
    string(REPLACE
        "    MopQueryPackets::BuildPlayedTimeResponse(data,"
        "    // MopQueryPackets::BuildPlayedTimeResponse(data,"
        misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "query_registration")
    string(REPLACE
        "    DefC(CMSG_QUERY_TIME, \"CMSG_QUERY_TIME\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQueryTimeOpcode);"
        "    // DefC(CMSG_QUERY_TIME, \"CMSG_QUERY_TIME\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQueryTimeOpcode);"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "played_registration")
    string(REPLACE
        "    DefC(CMSG_PLAYED_TIME, \"CMSG_PLAYED_TIME\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePlayedTime);"
        "    // DefC(CMSG_PLAYED_TIME, \"CMSG_PLAYED_TIME\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePlayedTime);"
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

function(extract_body output source start_marker end_marker)
    string(FIND "${source}" "${start_marker}" start)
    if(end_marker STREQUAL "<EOF>")
        string(LENGTH "${source}" end)
    else()
        string(FIND "${source}" "${end_marker}" end)
    endif()
    if(start EQUAL -1 OR end EQUAL -1 OR NOT start LESS end)
        message(FATAL_ERROR "Could not isolate ${start_marker}")
    endif()
    math(EXPR length "${end} - ${start}")
    string(SUBSTRING "${source}" ${start} ${length} body)
    set(${output} "${body}" PARENT_SCOPE)
endfunction()

strip_cpp_comments(packet_builder "${packet_builder}")
strip_cpp_comments(query_handler "${query_handler}")
strip_cpp_comments(misc_handler "${misc_handler}")
strip_cpp_comments(gm_ticket_handler "${gm_ticket_handler}")
strip_cpp_comments(opcode_registry "${opcode_registry}")
strip_cpp_comments(opcode_header "${opcode_header}")

extract_body(query_sender "${query_handler}"
    "void WorldSession::SendQueryTimeResponse()" "<EOF>")
extract_body(played_handler "${misc_handler}"
    "void WorldSession::HandlePlayedTime" "void WorldSession::HandleInspectOpcode")

# Check the legacy production body before requiring the new helper definitions so
# the baseline RED run proves the old byte/inline writer is still active.
foreach(forbidden IN ITEMS
        "uint8 unk1"
        "recv_data >>"
        "data << uint32(_player->GetTotalPlayedTime())"
        "data << uint32(_player->GetLevelPlayedTime())"
        "data << uint8")
    string(FIND "${played_handler}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "played-time handler: forbidden legacy token: ${forbidden}")
    endif()
endforeach()

extract_body(query_builder "${packet_builder}"
    "void MopQueryPackets::BuildQueryTimeResponse" "bool MopQueryPackets::ReadPlayedTimeRequest")
extract_body(played_request "${packet_builder}"
    "bool MopQueryPackets::ReadPlayedTimeRequest" "void MopQueryPackets::BuildPlayedTimeResponse")
extract_body(played_builder "${packet_builder}"
    "void MopQueryPackets::BuildPlayedTimeResponse" "void MopQueryPackets::BuildCreatureQueryResponse")

require_ordered("${query_sender}" "query-time sender"
    "WorldPacket data(SMSG_QUERY_TIME_RESPONSE"
    "MopQueryPackets::BuildQueryTimeResponse(data"
    "SendPacket(&data)")
require_count("${query_sender}" "time(NULL)" 2 "query-time sender time acquisition")
require_count("${gm_ticket_handler}" "SendQueryTimeResponse();" 2 "GM-ticket query-time call sites")

require_ordered("${played_handler}" "played-time handler"
    "MopQueryPackets::ReadPlayedTimeRequest(recv_data)"
    "WorldPacket data(SMSG_PLAYED_TIME"
    "MopQueryPackets::BuildPlayedTimeResponse(data"
    "SendPacket(&data)")

require_ordered("${query_builder}" "query-time response builder"
    "out << serverTime"
    "out << secondsUntilReset")
require_ordered("${played_request}" "played-time request reader" "in.ReadBit()")
require_ordered("${played_builder}" "played-time response builder"
    "out << totalPlayed"
    "out << levelPlayed"
    "out.WriteBit(displayEvent)"
    "out.FlushBits()")

string(REGEX REPLACE "[ \t\r\n]+" " " normalized_registry "${opcode_registry}")
set(query_inbound
    "DefC(CMSG_QUERY_TIME, \"CMSG_QUERY_TIME\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQueryTimeOpcode);")
set(query_outbound
    "DefS(SMSG_QUERY_TIME_RESPONSE, \"SMSG_QUERY_TIME_RESPONSE\");")
set(played_inbound
    "DefC(CMSG_PLAYED_TIME, \"CMSG_PLAYED_TIME\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePlayedTime);")
set(played_outbound
    "DefS(SMSG_PLAYED_TIME, \"SMSG_PLAYED_TIME\");")
require_exactly_one("${normalized_registry}" "${query_inbound}" "CMSG_QUERY_TIME registration")
require_exactly_one("${normalized_registry}" "${query_outbound}" "SMSG_QUERY_TIME_RESPONSE registration")
require_exactly_one("${normalized_registry}" "${played_inbound}" "CMSG_PLAYED_TIME registration")
require_exactly_one("${normalized_registry}" "${played_outbound}" "SMSG_PLAYED_TIME registration")

string(FIND "${opcode_header}" "SMSG_MOVE_CHARACTER_CHEAT" stale_alias)
if(NOT stale_alias EQUAL -1)
    message(FATAL_ERROR "stale compiled SMSG_MOVE_CHARACTER_CHEAT alias remains")
endif()
