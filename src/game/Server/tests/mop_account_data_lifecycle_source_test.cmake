file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc_source)

if(MUTATION STREQUAL "gate")
    string(REPLACE
        "case SMSG_UPDATE_ACCOUNT_DATA:"
        "/* removed account-data gate row */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "exact_frame")
    string(REPLACE
        "compressedSize != available - 1"
        "compressedSize > available"
        misc_source "${misc_source}")
elseif(MUTATION STREQUAL "clear_body")
    string(REPLACE
        "decompressedSize == 0 && compressedSize != 0"
        "decompressedSize == 0 && compressedSize == 0"
        misc_source "${misc_source}")
elseif(MUTATION STREQUAL "inflate_size")
    string(REPLACE
        "zResult != Z_OK || realSize != decompressedSize"
        "zResult != Z_OK"
        misc_source "${misc_source}")
elseif(MUTATION STREQUAL "ack_retirement")
    string(APPEND opcode_header
        "\nSMSG_UPDATE_ACCOUNT_DATA_COMPLETE = 0x2015;\n")
elseif(MUTATION STREQUAL "sender_bypass")
    string(REPLACE
        "SendPacket(&data);"
        "SendPacket(&data, true);"
        misc_source "${misc_source}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
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

require_once("${session_source}"
    "case[ \\t]+SMSG_UPDATE_ACCOUNT_DATA:"
    "SMSG_UPDATE_ACCOUNT_DATA suppression gate")
require_once("${opcode_registry}"
    "DefS\\(SMSG_UPDATE_ACCOUNT_DATA,[ \\t]*\"SMSG_UPDATE_ACCOUNT_DATA\"\\)"
    "SMSG_UPDATE_ACCOUNT_DATA registration")

if(NOT misc_source MATCHES "compressedSize != available - 1")
    message(FATAL_ERROR "account-data upload must reserve exactly one trailing type byte")
endif()
if(NOT misc_source MATCHES "decompressedSize == 0 && compressedSize != 0")
    message(FATAL_ERROR "account-data clear must reject a compressed body")
endif()
if(NOT misc_source MATCHES "zResult != Z_OK \\|\\| realSize != decompressedSize")
    message(FATAL_ERROR "account-data upload must reject short or oversized inflate results")
endif()

set(active_account_data_surface
    "${opcode_header}\n${opcode_registry}\n${misc_source}")
if(active_account_data_surface MATCHES "SMSG_UPDATE_ACCOUNT_DATA_COMPLETE")
    message(FATAL_ERROR "18414 completes account-data uploads locally; the stale ACK must stay retired")
endif()

string(FIND "${misc_source}"
    "The 18414 client completes account-data uploads locally"
    local_completion_comment)
if(local_completion_comment EQUAL -1)
    message(FATAL_ERROR "account-data upload lifecycle must document the direct 18414 no-ACK proof")
endif()

extract_body(request_handler "${misc_source}"
    "void WorldSession::HandleRequestAccountData("
    "void WorldSession::HandleSetActionButtonOpcode(")
if(request_handler MATCHES "SendPacket\\(&data,[ \\t]*true\\)")
    message(FATAL_ERROR "account-data replies must use central admission, not bypass suppression")
endif()
