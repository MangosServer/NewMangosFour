file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)

if(MUTATION STREQUAL "reason_mapping")
    string(REPLACE
        "TRANSFER_ABORT_ERROR                        = 0x05"
        "TRANSFER_ABORT_ERROR                        = 0x01"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "argument_bit")
    string(REPLACE
        "out.WriteBit(!hasTransferArgument);"
        "out.WriteBit(hasTransferArgument);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "reason_width")
    string(REPLACE
        "out.WriteBits(uint32(transferReason), 5);"
        "out.WriteBits(uint32(transferReason), 4);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE
        "DefS(SMSG_TRANSFER_ABORTED, \"SMSG_TRANSFER_ABORTED\");"
        "/* removed transfer-abort registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "gate")
    string(REPLACE
        "case SMSG_TRANSFER_ABORTED:"
        "/* removed transfer-abort gate */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "sender_builder")
    string(REPLACE
        "MopTransferPackets::BuildTransferAborted(data, mapid, reason, arg);"
        "data << uint32(mapid);"
        session_source "${session_source}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once("${player_header}"
    "TRANSFER_ABORT_ERROR[ \\t]*=[ \\t]*0x05"
    "18414 transfer-abort error reason")
require_once("${player_header}"
    "TRANSFER_ABORT_MAX_PLAYERS[ \\t]*=[ \\t]*0x17"
    "18414 transfer-abort max-player reason")
require_once("${player_header}"
    "TRANSFER_ABORT_DIFFICULTY[ \\t]*=[ \\t]*0x14"
    "18414 transfer-abort difficulty reason")
string(FIND "${player_header}" "out.WriteBit(!hasTransferArgument);" argument_bit_position)
if(argument_bit_position EQUAL -1)
    message(FATAL_ERROR "transfer-abort optional-argument bit is absent")
endif()
string(FIND "${player_header}" "out.WriteBits(uint32(transferReason), 5);" reason_width_position)
if(reason_width_position EQUAL -1)
    message(FATAL_ERROR "transfer-abort five-bit reason is absent")
endif()
require_once("${opcode_registry}"
    "DefS\\(SMSG_TRANSFER_ABORTED,[ \\t]*\"SMSG_TRANSFER_ABORTED\"\\)"
    "transfer-abort opcode registration")
require_once("${session_source}"
    "case[ \\t]+SMSG_TRANSFER_ABORTED:"
    "transfer-abort suppression admission")
string(FIND "${session_source}"
    "MopTransferPackets::BuildTransferAborted(data, mapid, reason, arg);"
    sender_builder_position)
if(sender_builder_position EQUAL -1)
    message(FATAL_ERROR "transfer-abort sender builder is absent")
endif()
