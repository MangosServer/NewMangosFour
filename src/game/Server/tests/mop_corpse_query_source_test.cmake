file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" packet_helpers)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QueryHandler.cpp" query_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(MUTATION STREQUAL "query_builder")
    string(REPLACE
        "MopQueryPackets::BuildCorpseQueryResponse(data, response)"
        "/* removed corpse-query response builder */"
        query_handler "${query_handler}")
elseif(MUTATION STREQUAL "map_reader")
    string(REPLACE
        "MopQueryPackets::ReadCorpseMapPositionQuery(recv_data)"
        "uint64(0) /* removed map-position request reader */"
        query_handler "${query_handler}")
elseif(MUTATION STREQUAL "map_builder")
    string(REPLACE
        "MopQueryPackets::BuildCorpseMapPositionQueryResponse(data,"
        "/* removed map-position response builder */ (void)(data), (void)("
        query_handler "${query_handler}")
elseif(MUTATION STREQUAL "inbound_registration")
    string(REPLACE
        "DefC(CMSG_CORPSE_QUERY, \"CMSG_CORPSE_QUERY\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCorpseQueryOpcode);"
        "/* removed corpse-query registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "outbound_registration")
    string(REPLACE
        "DefS(SMSG_CORPSE_QUERY_RESPONSE, \"SMSG_CORPSE_QUERY_RESPONSE\");"
        "/* removed corpse-query response registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE
        "case SMSG_CORPSE_QUERY_RESPONSE:"
        "case 0xFFFF: /* removed corpse response allowlist */"
        world_session "${world_session}")
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

strip_cpp_comments(packet_helpers "${packet_helpers}")
strip_cpp_comments(query_handler "${query_handler}")
strip_cpp_comments(opcode_registry "${opcode_registry}")
strip_cpp_comments(world_session "${world_session}")

extract_body(corpse_handler "${query_handler}"
    "void WorldSession::HandleCorpseQueryOpcode"
    "void WorldSession::HandleNpcTextQueryOpcode")
extract_body(map_handler "${query_handler}"
    "void WorldSession::HandleCorpseMapPositionQueryOpcode"
    "void WorldSession::HandleQueryQuestsCompletedOpcode")
extract_body(map_reader "${packet_helpers}"
    "inline uint64 MopQueryPackets::ReadCorpseMapPositionQuery"
    "inline void MopQueryPackets::BuildCorpseQueryResponse")
extract_body(corpse_builder "${packet_helpers}"
    "inline void MopQueryPackets::BuildCorpseQueryResponse"
    "inline void MopQueryPackets::BuildCorpseMapPositionQueryResponse")
extract_body(map_builder "${packet_helpers}"
    "inline void MopQueryPackets::BuildCorpseMapPositionQueryResponse"
    "inline uint64 MopStablePackets::ReadStableListRequest")

require_ordered("${corpse_handler}" "general corpse query handler"
    "MopQueryPackets::CorpseQueryResponse response"
    "response.found = true"
    "response.corpseMapId = corpse->GetMapId()"
    "response.displayMapId = int32(response.corpseMapId)"
    "response.x = corpse->GetPositionX()"
    "response.y = corpse->GetPositionY()"
    "response.z = corpse->GetPositionZ()"
    "WorldPacket data(SMSG_CORPSE_QUERY_RESPONSE"
    "MopQueryPackets::BuildCorpseQueryResponse(data, response)"
    "SendPacket(&data)")
require_ordered("${map_handler}" "corpse map-position handler"
    "MopQueryPackets::ReadCorpseMapPositionQuery(recv_data)"
    "WorldPacket data(SMSG_CORPSE_MAP_POSITION_QUERY_RESPONSE"
    "MopQueryPackets::BuildCorpseMapPositionQueryResponse(data,"
    "SendPacket(&data)")

foreach(forbidden IN ITEMS
        "WorldPacket data(MSG_CORPSE_QUERY"
        "SMSG_CORPSE_TRANSPORT_QUERY"
        "CMSG_CORPSE_TRANSPORT_QUERY"
        "recv_data >> unk")
    string(FIND "${query_handler}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "legacy corpse-query wire code remains: ${forbidden}")
    endif()
endforeach()

require_ordered("${map_reader}" "map-position packed GUID reader"
    "uint8 const maskOrder[] = { 7, 6, 3, 0, 4, 1, 5, 2 }"
    "uint8 const byteOrder[] = { 1, 6, 0, 5, 3, 2, 4, 7 }"
    "guidBytes[index] = in.ReadBit()"
    "in.ReadByteSeq(guidBytes[index])")
require_ordered("${corpse_builder}" "general corpse response builder"
    "uint8(0), uint8(3), uint8(2)"
    "out.WriteBit(response.found)"
    "uint8(5), uint8(4), uint8(1), uint8(7), uint8(6)"
    "out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 5))"
    "out << response.z"
    "out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 1))"
    "out << response.displayMapId"
    "out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 6))"
    "out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 4))"
    "out << response.x"
    "out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 3))"
    "out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 7))"
    "out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 2))"
    "out.WriteByteSeq(MopQueryPacketDetail::GuidByte(guid, 0))"
    "out << response.corpseMapId"
    "out << response.y")
require_ordered("${map_builder}" "map-position response builder"
    "out << x << orientation << z << y")

foreach(registration IN ITEMS
        "DefC(CMSG_CORPSE_QUERY, \"CMSG_CORPSE_QUERY\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCorpseQueryOpcode);"
        "DefS(SMSG_CORPSE_QUERY_RESPONSE, \"SMSG_CORPSE_QUERY_RESPONSE\");"
        "DefC(CMSG_CORPSE_MAP_POSITION_QUERY, \"CMSG_CORPSE_MAP_POSITION_QUERY\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleCorpseMapPositionQueryOpcode);"
        "DefS(SMSG_CORPSE_MAP_POSITION_QUERY_RESPONSE, \"SMSG_CORPSE_MAP_POSITION_QUERY_RESPONSE\");")
    require_once("${opcode_registry}" "${registration}" "corpse-query registration")
endforeach()

foreach(allowlist IN ITEMS
        "case SMSG_CORPSE_QUERY_RESPONSE:"
        "case SMSG_CORPSE_MAP_POSITION_QUERY_RESPONSE:")
    require_once("${world_session}" "${allowlist}" "corpse-query allowlist")
endforeach()
