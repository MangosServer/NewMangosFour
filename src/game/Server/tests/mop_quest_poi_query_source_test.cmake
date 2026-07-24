file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" packet_helpers)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QueryHandler.cpp" query_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(MUTATION STREQUAL "request_parser")
    string(REPLACE
        "MopQueryPackets::ParseQuestPoiQueryRequest(recv_data, questIds)"
        "false /* removed quest-POI request parser */"
        query_handler "${query_handler}")
elseif(MUTATION STREQUAL "response_builder")
    string(REPLACE
        "MopQueryPackets::BuildQuestPoiQueryResponse(data, response)"
        "false /* removed quest-POI response builder */"
        query_handler "${query_handler}")
elseif(MUTATION STREQUAL "inbound_registration")
    string(REPLACE
        "DefC(CMSG_QUEST_POI_QUERY, \"CMSG_QUEST_POI_QUERY\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQuestPOIQueryOpcode);"
        "/* removed quest-POI registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "outbound_registration")
    string(REPLACE
        "DefS(SMSG_QUEST_POI_QUERY_RESPONSE, \"SMSG_QUEST_POI_QUERY_RESPONSE\");"
        "/* removed quest-POI response registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE
        "case SMSG_QUEST_POI_QUERY_RESPONSE:"
        "case 0xFFFF: /* removed quest-POI allowlist */"
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

strip_cpp_comments(packet_helpers "${packet_helpers}")
strip_cpp_comments(query_handler "${query_handler}")
strip_cpp_comments(opcode_registry "${opcode_registry}")
strip_cpp_comments(world_session "${world_session}")

extract_body(handler "${query_handler}"
    "void WorldSession::HandleQuestPOIQueryOpcode"
    "void WorldSession::SendQueryTimeResponse")
extract_body(parser "${packet_helpers}"
    "inline bool MopQueryPackets::ParseQuestPoiQueryRequest"
    "inline bool MopQueryPackets::BuildQuestPoiQueryResponse")
extract_body(builder "${packet_helpers}"
    "inline bool MopQueryPackets::BuildQuestPoiQueryResponse"
    "inline uint64 MopStablePackets::ReadStableListRequest")

require_ordered("${handler}" "quest-POI handler"
    "MopQueryPackets::ParseQuestPoiQueryRequest(recv_data, questIds)"
    "FindQuestSlot(questId)"
    "GetQuestPOIVector(questId)"
    "MopQueryPackets::BuildQuestPoiQueryResponse(data, response)"
    "SendPacket(&data)")
require_ordered("${parser}" "quest-POI request parser"
    "in.ReadBits(22)"
    "count > MaximumQuestPoiQueries"
    "remaining != size_t(count) * sizeof(uint32)"
    "in >> questId"
    "in.rpos() != in.size()")
require_ordered("${builder}" "quest-POI response builder"
    "built.WriteBits(uint32(response.size()), 20)"
    "built.WriteBits(uint32(quest.pois.size()), 18)"
    "built.WriteBits(uint32(poi.points.size()), 21)"
    "built.FlushBits()"
    "built << poi.worldEffectId"
    "built << point.x << point.y"
    "built << poi.objectiveIndex"
    "built << poi.poiId"
    "built << poi.unknown2"
    "built << poi.unknown4"
    "built << poi.mapId"
    "built << poi.floorId"
    "built << poi.mapAreaId"
    "built << poi.unknown3"
    "built << poi.unknown1"
    "built << poi.playerConditionId"
    "built << quest.questId"
    "built << uint32(quest.pois.size())"
    "built << uint32(response.size())")

foreach(registration IN ITEMS
        "DefC(CMSG_QUEST_POI_QUERY, \"CMSG_QUEST_POI_QUERY\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQuestPOIQueryOpcode);"
        "DefS(SMSG_QUEST_POI_QUERY_RESPONSE, \"SMSG_QUEST_POI_QUERY_RESPONSE\");")
    string(FIND "${opcode_registry}" "${registration}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing active registration: ${registration}")
    endif()
endforeach()

string(FIND "${world_session}" "case SMSG_QUEST_POI_QUERY_RESPONSE:" allowlist)
if(allowlist EQUAL -1)
    message(FATAL_ERROR "quest-POI response is absent from the MoP sender allowlist")
endif()

foreach(forbidden IN ITEMS
        "recv_data >> count"
        "data << uint32(count)"
        "data << uint32(itr->points.size())")
    string(FIND "${handler}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "legacy dense quest-POI wire code remains: ${forbidden}")
    endif()
endforeach()
