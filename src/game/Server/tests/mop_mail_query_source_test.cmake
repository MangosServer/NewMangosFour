file(READ "${SOURCE_ROOT}/src/game/Server/MopQueryPackets.cpp" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MailHandler.cpp" mail_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)

if(MUTATION STREQUAL "builder_call")
    string(REPLACE
        "MopQueryPackets::BuildMailQueryNextTimeResult(data, records, nextMailTime)"
        "/* removed builder call */ true"
        mail_handler "${mail_handler}")
elseif(MUTATION STREQUAL "inbound_registration")
    string(REPLACE
        "DefC(CMSG_MAIL_QUERY_NEXT_TIME, \"CMSG_MAIL_QUERY_NEXT_TIME\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQueryNextMailTime);"
        "/* removed inbound registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "outbound_registration")
    string(REPLACE
        "DefS(SMSG_MAIL_QUERY_NEXT_TIME_RESULT, \"SMSG_MAIL_QUERY_NEXT_TIME_RESULT\");"
        "/* removed outbound registration */"
        opcode_registry "${opcode_registry}")
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

require_once("${mail_handler}"
    "MopQueryPackets::BuildMailQueryNextTimeResult(data, records, nextMailTime)"
    "mail result builder call")
require_once("${opcode_registry}"
    "DefC(CMSG_MAIL_QUERY_NEXT_TIME, \"CMSG_MAIL_QUERY_NEXT_TIME\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleQueryNextMailTime);"
    "mail query inbound registration")
require_once("${opcode_registry}"
    "DefS(SMSG_MAIL_QUERY_NEXT_TIME_RESULT, \"SMSG_MAIL_QUERY_NEXT_TIME_RESULT\");"
    "mail query outbound registration")

string(FIND "${mail_handler}" "WorldPacket data(MSG_QUERY_NEXT_MAIL_TIME" legacy_sender)
if(NOT legacy_sender EQUAL -1)
    message(FATAL_ERROR "legacy MSG_QUERY_NEXT_MAIL_TIME sender remains active")
endif()

foreach(token IN ITEMS
        "out.WriteBits(records.size(), 20)"
        "out.WriteBit(GuidByte(record.senderGuid, 3) != 0)"
        "out.WriteBit(record.hasVirtualRealmAddress)"
        "out.WriteBit(record.hasNativeRealmAddress)"
        "out << record.nonPlayerSender"
        "out << record.deliveryTime"
        "out << record.stationery"
        "out << nextMailTime")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "mail result builder missing: ${token}")
    endif()
endforeach()
