file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" packet_helpers)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QueryHandler.cpp" query_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)

if(MUTATION STREQUAL "request_parser_call")
    string(REPLACE
        "        MopQueryPackets::ReadGameObjectQueryRequest(recv_data);"
        "        // MopQueryPackets::ReadGameObjectQueryRequest(recv_data);"
        query_handler "${query_handler}")
elseif(MUTATION STREQUAL "response_builder_call")
    string(REPLACE
        "    MopQueryPackets::BuildGameObjectQueryResponse(data, response);"
        "    // MopQueryPackets::BuildGameObjectQueryResponse(data, response);"
        query_handler "${query_handler}")
elseif(MUTATION STREQUAL "inbound_registration")
    string(REPLACE
        "    DefC(CMSG_GAMEOBJECT_QUERY, \"CMSG_GAMEOBJECT_QUERY\", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleGameObjectQueryOpcode);"
        "    // DefC(CMSG_GAMEOBJECT_QUERY, \"CMSG_GAMEOBJECT_QUERY\", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleGameObjectQueryOpcode);"
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

strip_cpp_comments(packet_helpers "${packet_helpers}")
strip_cpp_comments(query_handler "${query_handler}")
strip_cpp_comments(opcode_registry "${opcode_registry}")

extract_body(gameobject_handler "${query_handler}"
    "void WorldSession::HandleGameObjectQueryOpcode"
    "void WorldSession::HandleCorpseQueryOpcode")

set(legacy_hits)
foreach(forbidden IN ITEMS
        "recv_data >> guid"
        "0x80000000"
        "data.append(info->raw.data, 24)"
        "data << uint32(entryID)")
    string(FIND "${gameobject_handler}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        list(APPEND legacy_hits "${forbidden}")
    endif()
endforeach()
if(legacy_hits)
    list(JOIN legacy_hits ", " joined_legacy_hits)
    message(FATAL_ERROR "gameobject query handler: forbidden active legacy tokens: ${joined_legacy_hits}")
endif()

require_ordered("${gameobject_handler}" "gameobject query handler"
    "MopQueryPackets::ReadGameObjectQueryRequest(recv_data)"
    "ObjectMgr::GetGameObjectInfo(request.entry)"
    "MopQueryPackets::BuildGameObjectQueryResponse(data, response)"
    "SendPacket(&data)")
require_exactly_one("${gameobject_handler}"
    "MopQueryPackets::ReadGameObjectQueryRequest(recv_data)"
    "gameobject request parser call")
require_exactly_one("${gameobject_handler}"
    "MopQueryPackets::BuildGameObjectQueryResponse(data, response)"
    "gameobject response builder call")
require_exactly_one("${gameobject_handler}" "SendPacket(&data)"
    "gameobject response send")

foreach(required IN ITEMS
        "response.names[0] = name ? name : \"\""
        "response.iconName = info->IconName ? info->IconName : \"\""
        "response.castBarCaption = castBarCaption ? castBarCaption : \"\""
        "response.unknownString = info->unk1 ? info->unk1 : \"\""
        "GetGameObjectLocale(request.entry)"
        "gl->Name"
        "gl->CastBarCaption"
        "for (size_t i = 0; i < response.data.size(); ++i)"
        "response.data[i] = info->raw.data[i]"
        "for (uint32 questItem : info->questItems)"
        "response.questItems.push_back(questItem)")
    string(FIND "${gameobject_handler}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "gameobject query handler: missing active token: ${required}")
    endif()
endforeach()

extract_body(request_parser "${packet_helpers}"
    "MopQueryPackets::GameObjectQueryRequest MopQueryPackets::ReadGameObjectQueryRequest"
    "void MopQueryPackets::BuildGameObjectQueryResponse")
require_ordered("${request_parser}" "gameobject request parser"
    "in >> request.entry"
    "guidBytes[5] = in.ReadBit()"
    "guidBytes[3] = in.ReadBit()"
    "guidBytes[6] = in.ReadBit()"
    "guidBytes[2] = in.ReadBit()"
    "guidBytes[7] = in.ReadBit()"
    "guidBytes[1] = in.ReadBit()"
    "guidBytes[0] = in.ReadBit()"
    "guidBytes[4] = in.ReadBit()"
    "in.ReadByteSeq(guidBytes[1])"
    "in.ReadByteSeq(guidBytes[5])"
    "in.ReadByteSeq(guidBytes[3])"
    "in.ReadByteSeq(guidBytes[4])"
    "in.ReadByteSeq(guidBytes[6])"
    "in.ReadByteSeq(guidBytes[2])"
    "in.ReadByteSeq(guidBytes[7])"
    "in.ReadByteSeq(guidBytes[0])")

string(FIND "${packet_helpers}"
    "void MopQueryPackets::BuildGameObjectQueryResponse" builder_start)
if(builder_start EQUAL -1)
    message(FATAL_ERROR "Could not isolate game-object response builder")
endif()
string(SUBSTRING "${packet_helpers}" ${builder_start} -1 response_builder)
require_ordered("${response_builder}" "gameobject response builder"
    "ByteBuffer blob"
    "if (record.hasData)"
    "blob << record.type"
    "blob << record.displayId"
    "for (std::string const& name : record.names)"
    "blob << name"
    "blob << record.iconName"
    "blob << record.castBarCaption"
    "blob << record.unknownString"
    "for (uint32 value : record.data)"
    "blob << value"
    "blob << record.size"
    "blob << uint8(record.questItems.size())"
    "for (uint32 questItem : record.questItems)"
    "blob << questItem"
    "blob << record.trailingUnknown"
    "out.WriteBit(record.hasData)"
    "out.FlushBits()"
    "out << record.entry"
    "out << uint32(blob.size())"
    "out.append(blob)")

string(REGEX REPLACE "[ \t\r\n]+" " " normalized_registry "${opcode_registry}")
set(inbound_registration
    "DefC(CMSG_GAMEOBJECT_QUERY, \"CMSG_GAMEOBJECT_QUERY\", STATUS_LOGGEDIN, PROCESS_INPLACE, &WorldSession::HandleGameObjectQueryOpcode);")
set(outbound_registration
    "DefS(SMSG_GAMEOBJECT_QUERY_RESPONSE, \"SMSG_GAMEOBJECT_QUERY_RESPONSE\");")
require_exactly_one("${normalized_registry}" "${inbound_registration}"
    "CMSG_GAMEOBJECT_QUERY registration")
require_exactly_one("${normalized_registry}" "${outbound_registration}"
    "SMSG_GAMEOBJECT_QUERY_RESPONSE registration")
