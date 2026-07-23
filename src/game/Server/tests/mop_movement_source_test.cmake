file(READ "${SOURCE_ROOT}/src/game/movement/MovementStructures.h" movement_structures)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit_source)
file(READ "${SOURCE_ROOT}/src/game/Object/CreatureMovement.cpp" creature_movement_source)
file(READ "${SOURCE_ROOT}/src/game/movement/MovementInfo.cpp" movement_codec)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MovementHandler.cpp" movement_handler)
file(READ "${SOURCE_ROOT}/src/game/movement/packet_builder.h" spline_packet_header)
file(READ "${SOURCE_ROOT}/src/game/movement/packet_builder.cpp" spline_packet_source)
file(READ "${SOURCE_ROOT}/src/game/movement/MoveSplineInit.cpp" spline_init_source)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session_source)

if(MUTATION STREQUAL "inbound_sequence")
    string(REPLACE "MSEPositionZ,\n    MSEPositionX,\n    MSEPositionY,\n    MSEHasMovementFlags2"
        "MSEPositionX,\n    MSEPositionZ,\n    MSEPositionY,\n    MSEHasMovementFlags2"
        movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "movement_gap_sequence")
    string(REPLACE "MSEPositionY,\n    MSEPositionZ,\n    MSEPositionX,\n    MSEGuidBit0"
        "MSEPositionZ,\n    MSEPositionY,\n    MSEPositionX,\n    MSEGuidBit0"
        movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "player_move_sequence")
    string(REPLACE "MSEHasPitch,\n    MSEGuidBit2,\n    MSEUnknownBit148"
        "MSEGuidBit2,\n    MSEHasPitch,\n    MSEUnknownBit148"
        movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "force_swim_sequence")
    string(REPLACE "MSEPositionY,\n    MSEMovementCounter,\n    MSEPositionZ"
        "MSEMovementCounter,\n    MSEPositionY,\n    MSEPositionZ"
        movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "force_swim_flags2_width")
    string(REPLACE "MSEUnknownBit148,\n    MSEFlags2_13,\n    MSEHasFallDirection"
        "MSEUnknownBit148,\n    MSEFlags2,\n    MSEHasFallDirection"
        movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "allocation_bound")
    string(REPLACE "movementForceCount > (data.size() - data.rpos()) / sizeof(uint32)"
        "movementForceCount > data.size() / sizeof(uint32)" movement_codec "${movement_codec}")
elseif(MUTATION STREQUAL "flags2_width")
    string(REPLACE "data.ReadBits(13)" "data.ReadBits(12)" movement_codec "${movement_codec}")
elseif(MUTATION STREQUAL "inbound_registration")
    string(REPLACE "DefC(CMSG_MOVE_START_FORWARD, \"CMSG_MOVE_START_FORWARD\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);"
        "DefC_disabled(CMSG_MOVE_START_FORWARD, \"CMSG_MOVE_START_FORWARD\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "movement_gap_registration")
    string(REPLACE "DefC(CMSG_MOVE_START_STRAFE_LEFT, \"CMSG_MOVE_START_STRAFE_LEFT\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);"
        "DefC_disabled(CMSG_MOVE_START_STRAFE_LEFT, \"CMSG_MOVE_START_STRAFE_LEFT\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "force_swim_registration")
    string(REPLACE "DefC(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK, \"CMSG_FORCE_SWIM_SPEED_CHANGE_ACK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleForceSpeedChangeAckOpcodes);"
        "DefC_disabled(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK, \"CMSG_FORCE_SWIM_SPEED_CHANGE_ACK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleForceSpeedChangeAckOpcodes);"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "force_swim_handler_order")
    string(REPLACE "recv_data >> newspeed;\n    recv_data >> movementInfo;"
        "recv_data >> movementInfo;\n    recv_data >> newspeed;"
        movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "output_metadata")
    string(REPLACE "DefS(SMSG_PLAYER_MOVE, \"SMSG_PLAYER_MOVE\");"
        "DefS_disabled(SMSG_PLAYER_MOVE, \"SMSG_PLAYER_MOVE\");" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "heartbeat_framing")
    string(REPLACE "WorldPacket data(SMSG_PLAYER_MOVE, 64);"
        "WorldPacket data(MSG_MOVE_HEARTBEAT, 64);" unit_source "${unit_source}")
elseif(MUTATION STREQUAL "mover_guid_validation")
    string(REPLACE "VerifyMovementInfo(movementInfo, movementInfo.GetGuid())"
        "VerifyMovementInfo(movementInfo)" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "spline_state_builder")
    string(REPLACE "MopCompactPackets::BuildSplineMoveSetFeatherFall(data, GetObjectGuid());"
        "data << GetObjectGuid();" creature_movement_source "${creature_movement_source}")
elseif(MUTATION STREQUAL "spline_state_registration")
    string(REPLACE "DefS(SMSG_SPLINE_MOVE_SET_NORMAL_FALL, \"SMSG_SPLINE_MOVE_SET_NORMAL_FALL\");"
        "DefS_disabled(SMSG_SPLINE_MOVE_SET_NORMAL_FALL, \"SMSG_SPLINE_MOVE_SET_NORMAL_FALL\");"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "monster_move_writer")
    string(REPLACE "data.WriteBits(type, 3)" "data.WriteBits(type, 2)"
        spline_packet_source "${spline_packet_source}")
elseif(MUTATION STREQUAL "monster_move_registration")
    string(REPLACE "DefS(SMSG_MONSTER_MOVE, \"SMSG_MONSTER_MOVE\");"
        "DefS_disabled(SMSG_MONSTER_MOVE, \"SMSG_MONSTER_MOVE\");"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "monster_move_integration")
    string(REPLACE "PacketBuilder::WriteStopMovement" "PacketBuilder::WriteLegacyStopMovement"
        spline_init_source "${spline_init_source}")
elseif(MUTATION STREQUAL "monster_move_gate")
    string(REPLACE "case SMSG_MONSTER_MOVE:" "case SMSG_MONSTER_MOVE_REMOVED:"
        world_session_source "${world_session_source}")
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

function(require_sequence source name expected)
    string(REGEX MATCH "MovementStatusElements[ \t\r\n]+${name}\\[\\][ \t\r\n]*=[ \t\r\n]*\\{([^}]*)\\}" match "${source}")
    if(NOT match)
        message(FATAL_ERROR "movement sequence missing: ${name}")
    endif()
    set(body "${CMAKE_MATCH_1}")
    string(REGEX MATCHALL "MSE[A-Za-z0-9_]+" actual "${body}")
    string(REPLACE ";" "," actual "${actual}")
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "movement sequence mismatch: ${name}\nactual=${actual}\nexpected=${expected}")
    endif()
endfunction()

strip_cpp_comments(movement_structures "${movement_structures}")
strip_cpp_comments(unit_header "${unit_header}")
strip_cpp_comments(unit_source "${unit_source}")
strip_cpp_comments(creature_movement_source "${creature_movement_source}")
strip_cpp_comments(movement_codec "${movement_codec}")
strip_cpp_comments(opcode_registry "${opcode_registry}")
strip_cpp_comments(opcode_header "${opcode_header}")
strip_cpp_comments(movement_handler "${movement_handler}")
strip_cpp_comments(spline_packet_header "${spline_packet_header}")
strip_cpp_comments(spline_packet_source "${spline_packet_source}")
strip_cpp_comments(spline_init_source "${spline_init_source}")
strip_cpp_comments(world_session_source "${world_session_source}")

set(start_forward "MSEPositionZ,MSEPositionX,MSEPositionY,MSEHasMovementFlags2,MSEUnknownBit149,MSEHasUnknownUInt32,MSEUnknownBit148,MSEGuidBit0,MSEHasOrientation,MSEHasFallData,MSEMovementForceCount,MSEGuidBit4,MSEGuidBit1,MSEHasTimestamp,MSEGuidBit7,MSEHasPitch,MSEHasTransportData,MSEGuidBit5,MSEHasMovementFlags,MSEGuidBit3,MSEHasSplineElevation,MSEGuidBit2,MSEGuidBit6,MSEUnknownBit172,MSETransportGuidBit1,MSEHasTransportTime3,MSETransportGuidBit3,MSETransportGuidBit4,MSETransportGuidBit2,MSETransportGuidBit5,MSETransportGuidBit0,MSETransportGuidBit7,MSETransportGuidBit6,MSEHasTransportTime2,MSEHasFallDirection,MSEFlags2_13,MSEFlags,MSEGuidByte1,MSEGuidByte6,MSEGuidByte7,MSEMovementForceIds,MSEGuidByte5,MSEGuidByte0,MSEGuidByte3,MSEGuidByte2,MSEGuidByte4,MSETransportGuidByte3,MSETransportGuidByte1,MSETransportGuidByte6,MSETransportPositionZ,MSETransportGuidByte4,MSETransportTime3,MSETransportSeat,MSETransportGuidByte7,MSETransportPositionO,MSETransportTime2,MSETransportGuidByte5,MSETransportGuidByte2,MSETransportPositionX,MSETransportGuidByte0,MSETransportPositionY,MSETransportTime,MSEFallCosAngle,MSEFallSinAngle,MSEFallHorizontalSpeed,MSEFallTime,MSEFallVerticalSpeed,MSETimestamp,MSEPitch,MSESplineElevation,MSEPositionO,MSEUnknownUInt32,MSEEnd")
set(start_backward "MSEPositionY,MSEPositionZ,MSEPositionX,MSEHasTimestamp,MSEHasOrientation,MSEGuidBit7,MSEGuidBit2,MSEMovementForceCount,MSEHasFallData,MSEUnknownBit172,MSEGuidBit5,MSEGuidBit3,MSEGuidBit6,MSEHasSplineElevation,MSEGuidBit4,MSEHasTransportData,MSEGuidBit0,MSEHasMovementFlags,MSEHasPitch,MSEHasUnknownUInt32,MSEHasMovementFlags2,MSEUnknownBit148,MSEGuidBit1,MSEUnknownBit149,MSETransportGuidBit1,MSEHasTransportTime2,MSETransportGuidBit0,MSETransportGuidBit7,MSEHasTransportTime3,MSETransportGuidBit3,MSETransportGuidBit5,MSETransportGuidBit6,MSETransportGuidBit2,MSETransportGuidBit4,MSEFlags2_13,MSEFlags,MSEHasFallDirection,MSEMovementForceIds,MSEGuidByte1,MSEGuidByte3,MSEGuidByte5,MSEGuidByte2,MSEGuidByte0,MSEGuidByte4,MSEGuidByte7,MSEGuidByte6,MSEUnknownUInt32,MSETransportTime,MSETransportGuidByte4,MSETransportGuidByte1,MSETransportGuidByte5,MSETransportGuidByte3,MSETransportGuidByte6,MSETransportSeat,MSETransportPositionO,MSETransportPositionX,MSETransportGuidByte0,MSETransportPositionY,MSETransportTime3,MSETransportGuidByte7,MSETransportTime2,MSETransportPositionZ,MSETransportGuidByte2,MSEPositionO,MSEFallTime,MSEFallSinAngle,MSEFallCosAngle,MSEFallHorizontalSpeed,MSEFallVerticalSpeed,MSEPitch,MSETimestamp,MSESplineElevation,MSEEnd")
set(start_strafe_left "MSEPositionY,MSEPositionZ,MSEPositionX,MSEGuidBit0,MSEHasTimestamp,MSEGuidBit3,MSEHasMovementFlags2,MSEHasPitch,MSEUnknownBit148,MSEGuidBit2,MSEUnknownBit149,MSEHasTransportData,MSEHasFallData,MSEGuidBit5,MSEMovementForceCount,MSEUnknownBit172,MSEGuidBit4,MSEHasOrientation,MSEHasSplineElevation,MSEGuidBit7,MSEHasUnknownUInt32,MSEGuidBit1,MSEGuidBit6,MSEHasMovementFlags,MSETransportGuidBit0,MSETransportGuidBit2,MSETransportGuidBit1,MSETransportGuidBit6,MSETransportGuidBit7,MSETransportGuidBit3,MSETransportGuidBit5,MSEHasTransportTime3,MSEHasTransportTime2,MSETransportGuidBit4,MSEFlags,MSEHasFallDirection,MSEFlags2_13,MSEGuidByte0,MSEGuidByte2,MSEMovementForceIds,MSEGuidByte3,MSEGuidByte5,MSEGuidByte1,MSEGuidByte7,MSEGuidByte4,MSEGuidByte6,MSETransportGuidByte2,MSETransportPositionZ,MSETransportTime3,MSETransportGuidByte6,MSETransportGuidByte3,MSETransportPositionO,MSETransportGuidByte5,MSETransportTime2,MSETransportGuidByte1,MSETransportPositionY,MSETransportGuidByte4,MSETransportTime,MSETransportSeat,MSETransportPositionX,MSETransportGuidByte0,MSETransportGuidByte7,MSEPitch,MSETimestamp,MSEFallTime,MSEFallSinAngle,MSEFallHorizontalSpeed,MSEFallCosAngle,MSEFallVerticalSpeed,MSEUnknownUInt32,MSESplineElevation,MSEPositionO,MSEEnd")
set(start_strafe_right "MSEPositionY,MSEPositionX,MSEPositionZ,MSEGuidBit0,MSEHasFallData,MSEMovementForceCount,MSEGuidBit7,MSEGuidBit6,MSEGuidBit4,MSEHasMovementFlags,MSEGuidBit5,MSEHasSplineElevation,MSEGuidBit3,MSEUnknownBit149,MSEHasTransportData,MSEHasUnknownUInt32,MSEGuidBit1,MSEUnknownBit172,MSEGuidBit2,MSEHasPitch,MSEHasMovementFlags2,MSEHasOrientation,MSEUnknownBit148,MSEHasTimestamp,MSEHasFallDirection,MSETransportGuidBit1,MSETransportGuidBit6,MSETransportGuidBit3,MSETransportGuidBit5,MSETransportGuidBit2,MSETransportGuidBit0,MSETransportGuidBit4,MSEHasTransportTime3,MSETransportGuidBit7,MSEHasTransportTime2,MSEFlags,MSEFlags2_13,MSEGuidByte6,MSEGuidByte7,MSEGuidByte0,MSEGuidByte4,MSEGuidByte1,MSEMovementForceIds,MSEGuidByte2,MSEGuidByte3,MSEGuidByte5,MSEPitch,MSETransportGuidByte1,MSETransportSeat,MSETransportGuidByte3,MSETransportTime2,MSETransportGuidByte7,MSETransportTime3,MSETransportGuidByte5,MSETransportGuidByte6,MSETransportGuidByte2,MSETransportGuidByte0,MSETransportTime,MSETransportPositionO,MSETransportPositionY,MSETransportPositionZ,MSETransportGuidByte4,MSETransportPositionX,MSETimestamp,MSEFallVerticalSpeed,MSEFallSinAngle,MSEFallHorizontalSpeed,MSEFallCosAngle,MSEFallTime,MSEPositionO,MSEUnknownUInt32,MSESplineElevation,MSEEnd")
set(stop_strafe "MSEPositionZ,MSEPositionX,MSEPositionY,MSEHasFallData,MSEHasOrientation,MSEHasSplineElevation,MSEHasTimestamp,MSEHasMovementFlags,MSEHasUnknownUInt32,MSEGuidBit6,MSEHasTransportData,MSEUnknownBit172,MSEHasMovementFlags2,MSEGuidBit4,MSEHasPitch,MSEGuidBit5,MSEGuidBit3,MSEGuidBit2,MSEMovementForceCount,MSEUnknownBit149,MSEGuidBit7,MSEGuidBit0,MSEUnknownBit148,MSEGuidBit1,MSETransportGuidBit7,MSEHasTransportTime3,MSETransportGuidBit3,MSETransportGuidBit1,MSETransportGuidBit6,MSEHasTransportTime2,MSETransportGuidBit2,MSETransportGuidBit5,MSETransportGuidBit4,MSETransportGuidBit0,MSEFlags2_13,MSEHasFallDirection,MSEFlags,MSEGuidByte5,MSEGuidByte3,MSEMovementForceIds,MSEGuidByte2,MSEGuidByte0,MSEGuidByte1,MSEGuidByte6,MSEGuidByte4,MSEGuidByte7,MSETransportGuidByte0,MSETransportTime3,MSETransportGuidByte1,MSETransportGuidByte6,MSETransportTime,MSETransportPositionY,MSETransportPositionZ,MSETransportGuidByte4,MSETransportTime2,MSETransportGuidByte3,MSETransportSeat,MSETransportPositionX,MSETransportGuidByte2,MSETransportGuidByte7,MSETransportGuidByte5,MSETransportPositionO,MSEPositionO,MSESplineElevation,MSETimestamp,MSEFallSinAngle,MSEFallCosAngle,MSEFallHorizontalSpeed,MSEFallTime,MSEFallVerticalSpeed,MSEPitch,MSEUnknownUInt32,MSEEnd")
set(jump "MSEPositionY,MSEPositionX,MSEPositionZ,MSEGuidBit1,MSEGuidBit7,MSEHasMovementFlags2,MSEGuidBit5,MSEHasSplineElevation,MSEHasOrientation,MSEGuidBit6,MSEGuidBit4,MSEUnknownBit149,MSEHasTransportData,MSEUnknownBit148,MSEMovementForceCount,MSEHasPitch,MSEHasMovementFlags,MSEHasTimestamp,MSEHasUnknownUInt32,MSEGuidBit3,MSEUnknownBit172,MSEHasFallData,MSEGuidBit2,MSEGuidBit0,MSETransportGuidBit2,MSETransportGuidBit3,MSETransportGuidBit1,MSETransportGuidBit4,MSEHasTransportTime2,MSETransportGuidBit5,MSETransportGuidBit6,MSETransportGuidBit0,MSETransportGuidBit7,MSEHasTransportTime3,MSEFlags,MSEFlags2_13,MSEHasFallDirection,MSEGuidByte7,MSEGuidByte1,MSEGuidByte0,MSEMovementForceIds,MSEGuidByte2,MSEGuidByte6,MSEGuidByte3,MSEGuidByte4,MSEGuidByte5,MSEFallVerticalSpeed,MSEFallCosAngle,MSEFallSinAngle,MSEFallHorizontalSpeed,MSEFallTime,MSETransportGuidByte5,MSETransportGuidByte7,MSETransportSeat,MSETransportGuidByte4,MSETransportGuidByte0,MSETransportPositionZ,MSETransportGuidByte6,MSETransportGuidByte2,MSETransportPositionY,MSETransportTime,MSETransportPositionX,MSETransportTime2,MSETransportGuidByte1,MSETransportGuidByte3,MSETransportTime3,MSETransportPositionO,MSESplineElevation,MSEPositionO,MSEPitch,MSEUnknownUInt32,MSETimestamp,MSEEnd")
set(start_turn_left "MSEPositionZ,MSEPositionX,MSEPositionY,MSEHasOrientation,MSEGuidBit4,MSEGuidBit5,MSEUnknownBit148,MSEHasTimestamp,MSEUnknownBit172,MSEUnknownBit149,MSEHasUnknownUInt32,MSEGuidBit3,MSEGuidBit1,MSEHasMovementFlags2,MSEHasMovementFlags,MSEGuidBit0,MSEGuidBit2,MSEMovementForceCount,MSEHasTransportData,MSEGuidBit7,MSEHasPitch,MSEHasSplineElevation,MSEHasFallData,MSEGuidBit6,MSEHasTransportTime3,MSETransportGuidBit5,MSETransportGuidBit6,MSETransportGuidBit2,MSETransportGuidBit3,MSETransportGuidBit4,MSETransportGuidBit7,MSEHasTransportTime2,MSETransportGuidBit0,MSETransportGuidBit1,MSEFlags,MSEFlags2_13,MSEHasFallDirection,MSEGuidByte7,MSEGuidByte3,MSEGuidByte6,MSEGuidByte4,MSEGuidByte1,MSEMovementForceIds,MSEGuidByte5,MSEGuidByte0,MSEGuidByte2,MSEFallTime,MSEFallHorizontalSpeed,MSEFallSinAngle,MSEFallCosAngle,MSEFallVerticalSpeed,MSEPitch,MSETransportPositionY,MSETransportGuidByte3,MSETransportPositionX,MSETransportPositionO,MSETransportGuidByte5,MSETransportTime2,MSETransportPositionZ,MSETransportGuidByte2,MSETransportGuidByte1,MSETransportGuidByte7,MSETransportGuidByte4,MSETransportGuidByte0,MSETransportTime3,MSETransportSeat,MSETransportGuidByte6,MSETransportTime,MSEPositionO,MSESplineElevation,MSEUnknownUInt32,MSETimestamp,MSEEnd")
set(start_turn_right "MSEPositionX,MSEPositionZ,MSEPositionY,MSEUnknownBit148,MSEUnknownBit172,MSEGuidBit1,MSEGuidBit0,MSEHasMovementFlags,MSEHasFallData,MSEHasPitch,MSEHasUnknownUInt32,MSEMovementForceCount,MSEHasSplineElevation,MSEHasMovementFlags2,MSEHasOrientation,MSEGuidBit2,MSEHasTimestamp,MSEGuidBit4,MSEGuidBit6,MSEGuidBit5,MSEGuidBit3,MSEUnknownBit149,MSEHasTransportData,MSEGuidBit7,MSETransportGuidBit2,MSEHasTransportTime2,MSETransportGuidBit6,MSETransportGuidBit5,MSETransportGuidBit3,MSETransportGuidBit7,MSETransportGuidBit4,MSEHasTransportTime3,MSETransportGuidBit0,MSETransportGuidBit1,MSEFlags,MSEFlags2_13,MSEHasFallDirection,MSEGuidByte5,MSEGuidByte1,MSEGuidByte3,MSEGuidByte0,MSEGuidByte4,MSEGuidByte2,MSEGuidByte6,MSEMovementForceIds,MSEGuidByte7,MSEFallCosAngle,MSEFallHorizontalSpeed,MSEFallSinAngle,MSEFallVerticalSpeed,MSEFallTime,MSEPitch,MSETransportTime3,MSETransportGuidByte3,MSETransportTime2,MSETransportGuidByte7,MSETransportGuidByte1,MSETransportPositionX,MSETransportSeat,MSETransportGuidByte5,MSETransportGuidByte4,MSETransportGuidByte2,MSETransportGuidByte0,MSETransportPositionZ,MSETransportTime,MSETransportPositionY,MSETransportGuidByte6,MSETransportPositionO,MSEPositionO,MSETimestamp,MSESplineElevation,MSEUnknownUInt32,MSEEnd")
set(stop_turn "MSEPositionX,MSEPositionZ,MSEPositionY,MSEHasTransportData,MSEMovementForceCount,MSEUnknownBit149,MSEGuidBit4,MSEGuidBit5,MSEHasUnknownUInt32,MSEGuidBit3,MSEUnknownBit172,MSEHasFallData,MSEGuidBit0,MSEGuidBit1,MSEHasPitch,MSEGuidBit6,MSEHasMovementFlags,MSEGuidBit2,MSEUnknownBit148,MSEHasMovementFlags2,MSEHasSplineElevation,MSEHasOrientation,MSEGuidBit7,MSEHasTimestamp,MSEFlags2_13,MSETransportGuidBit1,MSEHasTransportTime3,MSEHasTransportTime2,MSETransportGuidBit3,MSETransportGuidBit6,MSETransportGuidBit2,MSETransportGuidBit0,MSETransportGuidBit5,MSETransportGuidBit7,MSETransportGuidBit4,MSEFlags,MSEHasFallDirection,MSEGuidByte2,MSEGuidByte3,MSEGuidByte6,MSEMovementForceIds,MSEGuidByte0,MSEGuidByte5,MSEGuidByte4,MSEGuidByte7,MSEGuidByte1,MSETransportTime,MSETransportTime3,MSETransportSeat,MSETransportPositionY,MSETransportPositionX,MSETransportTime2,MSETransportGuidByte4,MSETransportGuidByte3,MSETransportPositionO,MSETransportGuidByte0,MSETransportPositionZ,MSETransportGuidByte6,MSETransportGuidByte7,MSETransportGuidByte5,MSETransportGuidByte1,MSETransportGuidByte2,MSEPositionO,MSETimestamp,MSEFallCosAngle,MSEFallSinAngle,MSEFallHorizontalSpeed,MSEFallVerticalSpeed,MSEFallTime,MSEUnknownUInt32,MSESplineElevation,MSEPitch,MSEEnd")
set(stop_sequence "MSEPositionX,MSEPositionY,MSEPositionZ,MSEGuidBit5,MSEGuidBit2,MSEHasFallData,MSEGuidBit0,MSEUnknownBit172,MSEUnknownBit148,MSEHasUnknownUInt32,MSEGuidBit1,MSEMovementForceCount,MSEHasPitch,MSEGuidBit3,MSEGuidBit4,MSEHasTransportData,MSEUnknownBit149,MSEGuidBit6,MSEHasMovementFlags,MSEHasTimestamp,MSEHasMovementFlags2,MSEHasOrientation,MSEHasSplineElevation,MSEGuidBit7,MSEHasTransportTime2,MSETransportGuidBit7,MSETransportGuidBit4,MSETransportGuidBit1,MSETransportGuidBit0,MSETransportGuidBit5,MSETransportGuidBit2,MSETransportGuidBit3,MSEHasTransportTime3,MSETransportGuidBit6,MSEHasFallDirection,MSEFlags2_13,MSEFlags,MSEGuidByte0,MSEGuidByte3,MSEMovementForceIds,MSEGuidByte6,MSEGuidByte1,MSEGuidByte4,MSEGuidByte2,MSEGuidByte5,MSEGuidByte7,MSEPositionO,MSEFallVerticalSpeed,MSEFallHorizontalSpeed,MSEFallSinAngle,MSEFallCosAngle,MSEFallTime,MSESplineElevation,MSETransportPositionX,MSETransportTime,MSETransportGuidByte3,MSETransportPositionO,MSETransportPositionY,MSETransportGuidByte2,MSETransportGuidByte6,MSETransportGuidByte7,MSETransportGuidByte1,MSETransportGuidByte4,MSETransportTime3,MSETransportGuidByte0,MSETransportSeat,MSETransportPositionZ,MSETransportGuidByte5,MSETransportTime2,MSEUnknownUInt32,MSEPitch,MSETimestamp,MSEEnd")
set(heartbeat "MSEPositionZ,MSEPositionX,MSEPositionY,MSEMovementForceCount,MSEHasMovementFlags,MSEUnknownBit148,MSEHasUnknownUInt32,MSEGuidBit3,MSEGuidBit6,MSEHasPitch,MSEUnknownBit149,MSEUnknownBit172,MSEGuidBit7,MSEGuidBit2,MSEGuidBit4,MSEHasMovementFlags2,MSEHasOrientation,MSEHasTimestamp,MSEHasTransportData,MSEHasFallData,MSEGuidBit5,MSEHasSplineElevation,MSEGuidBit1,MSEGuidBit0,MSETransportGuidBit5,MSETransportGuidBit3,MSETransportGuidBit6,MSETransportGuidBit0,MSETransportGuidBit7,MSEHasTransportTime3,MSETransportGuidBit1,MSETransportGuidBit2,MSETransportGuidBit4,MSEHasTransportTime2,MSEFlags,MSEHasFallDirection,MSEFlags2_13,MSEGuidByte2,MSEGuidByte3,MSEGuidByte6,MSEGuidByte1,MSEGuidByte4,MSEGuidByte7,MSEMovementForceIds,MSEGuidByte5,MSEGuidByte0,MSEFallSinAngle,MSEFallCosAngle,MSEFallHorizontalSpeed,MSEFallVerticalSpeed,MSEFallTime,MSETransportGuidByte1,MSETransportGuidByte3,MSETransportGuidByte2,MSETransportGuidByte0,MSETransportTime3,MSETransportSeat,MSETransportGuidByte7,MSETransportPositionX,MSETransportGuidByte4,MSETransportTime2,MSETransportPositionY,MSETransportGuidByte6,MSETransportGuidByte5,MSETransportPositionZ,MSETransportTime,MSETransportPositionO,MSEUnknownUInt32,MSEPositionO,MSEPitch,MSETimestamp,MSESplineElevation,MSEEnd")
set(set_facing "MSEPositionY,MSEPositionX,MSEPositionZ,MSEGuidBit5,MSEHasMovementFlags2,MSEGuidBit3,MSEGuidBit2,MSEMovementForceCount,MSEUnknownBit172,MSEHasPitch,MSEGuidBit0,MSEHasOrientation,MSEHasTimestamp,MSEUnknownBit148,MSEHasUnknownUInt32,MSEGuidBit4,MSEUnknownBit149,MSEGuidBit1,MSEGuidBit6,MSEHasFallData,MSEHasMovementFlags,MSEHasSplineElevation,MSEHasTransportData,MSEGuidBit7,MSETransportGuidBit0,MSETransportGuidBit7,MSEHasTransportTime2,MSETransportGuidBit3,MSETransportGuidBit6,MSEHasTransportTime3,MSETransportGuidBit2,MSETransportGuidBit5,MSETransportGuidBit1,MSETransportGuidBit4,MSEHasFallDirection,MSEFlags2_13,MSEFlags,MSEMovementForceIds,MSEGuidByte0,MSEGuidByte6,MSEGuidByte3,MSEGuidByte1,MSEGuidByte2,MSEGuidByte7,MSEGuidByte4,MSEGuidByte5,MSETransportGuidByte0,MSETransportGuidByte2,MSETransportPositionO,MSETransportGuidByte7,MSETransportTime3,MSETransportGuidByte5,MSETransportTime,MSETransportPositionX,MSETransportTime2,MSETransportPositionZ,MSETransportSeat,MSETransportPositionY,MSETransportGuidByte4,MSETransportGuidByte3,MSETransportGuidByte6,MSETransportGuidByte1,MSEFallTime,MSEFallVerticalSpeed,MSEFallHorizontalSpeed,MSEFallSinAngle,MSEFallCosAngle,MSEUnknownUInt32,MSETimestamp,MSESplineElevation,MSEPositionO,MSEPitch,MSEEnd")
set(fall_land "MSEPositionY,MSEPositionZ,MSEPositionX,MSEHasFallData,MSEUnknownBit172,MSEUnknownBit148,MSEHasTimestamp,MSEGuidBit7,MSEUnknownBit149,MSEHasSplineElevation,MSEGuidBit5,MSEHasPitch,MSEHasMovementFlags2,MSEGuidBit2,MSEGuidBit3,MSEGuidBit0,MSEHasOrientation,MSEMovementForceCount,MSEHasMovementFlags,MSEHasUnknownUInt32,MSEGuidBit1,MSEHasTransportData,MSEGuidBit6,MSEGuidBit4,MSETransportGuidBit0,MSEHasTransportTime2,MSETransportGuidBit3,MSETransportGuidBit5,MSETransportGuidBit1,MSETransportGuidBit7,MSETransportGuidBit4,MSETransportGuidBit2,MSETransportGuidBit6,MSEHasTransportTime3,MSEFlags2_13,MSEHasFallDirection,MSEFlags,MSEGuidByte4,MSEGuidByte3,MSEGuidByte7,MSEGuidByte0,MSEGuidByte2,MSEGuidByte5,MSEGuidByte1,MSEGuidByte6,MSEMovementForceIds,MSEFallSinAngle,MSEFallHorizontalSpeed,MSEFallCosAngle,MSEFallTime,MSEFallVerticalSpeed,MSETransportGuidByte4,MSETransportPositionY,MSETransportPositionO,MSETransportPositionZ,MSETransportSeat,MSETransportGuidByte3,MSETransportGuidByte6,MSETransportTime2,MSETransportGuidByte2,MSETransportGuidByte1,MSETransportGuidByte5,MSETransportTime3,MSETransportTime,MSETransportPositionX,MSETransportGuidByte7,MSETransportGuidByte0,MSEUnknownUInt32,MSETimestamp,MSESplineElevation,MSEPitch,MSEPositionO,MSEEnd")
set(player_move "MSEHasPitch,MSEGuidBit2,MSEUnknownBit148,MSEUnknownBit149,MSEGuidBit0,MSEHasOrientation,MSEHasFallData,MSEHasUnknownUInt32,MSEGuidBit3,MSEHasFallDirection,MSEHasTransportData,MSEGuidBit4,MSETransportGuidBit5,MSETransportGuidBit4,MSETransportGuidBit7,MSETransportGuidBit2,MSETransportGuidBit6,MSEHasTransportTime2,MSETransportGuidBit3,MSETransportGuidBit1,MSEHasTransportTime3,MSETransportGuidBit0,MSEHasSplineElevation,MSEHasMovementFlags,MSEUnknownBit172,MSEFlags,MSEHasMovementFlags2,MSEGuidBit7,MSEGuidBit1,MSEHasTimestamp,MSEFlags2_13,MSEGuidBit5,MSEMovementForceCount,MSEGuidBit6,MSEPositionY,MSETransportGuidByte7,MSETransportTime2,MSETransportPositionX,MSETransportGuidByte5,MSETransportSeat,MSETransportGuidByte2,MSETransportGuidByte0,MSETransportGuidByte3,MSETransportTime,MSETransportGuidByte4,MSETransportPositionZ,MSETransportGuidByte1,MSETransportPositionY,MSETransportPositionO,MSETransportGuidByte6,MSETransportTime3,MSEGuidByte5,MSEGuidByte1,MSEPositionZ,MSEMovementForceIds,MSETimestamp,MSEPositionO,MSEGuidByte3,MSEFallSinAngle,MSEFallHorizontalSpeed,MSEFallCosAngle,MSEFallVerticalSpeed,MSEFallTime,MSEGuidByte0,MSEPitch,MSEGuidByte2,MSEGuidByte6,MSESplineElevation,MSEUnknownUInt32,MSEPositionX,MSEGuidByte4,MSEGuidByte7,MSEEnd")
set(force_swim_speed_ack "MSEPositionY,MSEMovementCounter,MSEPositionZ,MSEPositionX,MSEGuidBit4,MSEUnknownBit149,MSEHasSplineElevation,MSEGuidBit2,MSEHasMovementFlags2,MSEGuidBit5,MSEGuidBit3,MSEHasMovementFlags,MSEGuidBit0,MSEHasPitch,MSEHasUnknownUInt32,MSEHasOrientation,MSEUnknownBit172,MSEGuidBit1,MSEHasFallData,MSEMovementForceCount,MSEHasTimestamp,MSEGuidBit7,MSEGuidBit6,MSEHasTransportData,MSEUnknownBit148,MSEFlags2_13,MSEHasFallDirection,MSETransportGuidBit4,MSETransportGuidBit2,MSETransportGuidBit7,MSEHasTransportTime3,MSETransportGuidBit1,MSETransportGuidBit6,MSETransportGuidBit3,MSETransportGuidBit0,MSEHasTransportTime2,MSETransportGuidBit5,MSEFlags,MSEGuidByte0,MSEGuidByte4,MSEGuidByte5,MSEGuidByte6,MSEMovementForceIds,MSEGuidByte1,MSEGuidByte3,MSEGuidByte7,MSEGuidByte2,MSETransportGuidByte7,MSETransportTime2,MSETransportSeat,MSETransportTime3,MSETransportGuidByte4,MSETransportPositionY,MSETransportPositionZ,MSETransportGuidByte0,MSETransportGuidByte6,MSETransportGuidByte3,MSETransportGuidByte2,MSETransportPositionO,MSETransportTime,MSETransportGuidByte5,MSETransportGuidByte1,MSETransportPositionX,MSEFallHorizontalSpeed,MSEFallSinAngle,MSEFallCosAngle,MSEFallVerticalSpeed,MSEFallTime,MSETimestamp,MSESplineElevation,MSEUnknownUInt32,MSEPitch,MSEPositionO,MSEEnd")

require_sequence("${movement_structures}" MovementStartForwardSequence "${start_forward}")
require_sequence("${movement_structures}" MovementStartBackwardSequence "${start_backward}")
require_sequence("${movement_structures}" MovementStartStrafeLeftSequence "${start_strafe_left}")
require_sequence("${movement_structures}" MovementStartStrafeRightSequence "${start_strafe_right}")
require_sequence("${movement_structures}" MovementStopStrafeSequence "${stop_strafe}")
require_sequence("${movement_structures}" MovementJumpSequence "${jump}")
require_sequence("${movement_structures}" MovementStartTurnLeftSequence "${start_turn_left}")
require_sequence("${movement_structures}" MovementStartTurnRightSequence "${start_turn_right}")
require_sequence("${movement_structures}" MovementStopTurnSequence "${stop_turn}")
require_sequence("${movement_structures}" MovementStopSequence "${stop_sequence}")
require_sequence("${movement_structures}" MovementHeartBeatSequence "${heartbeat}")
require_sequence("${movement_structures}" MovementSetFacingSequence "${set_facing}")
require_sequence("${movement_structures}" MovementFallLandSequence "${fall_land}")
require_sequence("${movement_structures}" PlayerMoveSequence "${player_move}")
require_sequence("${movement_structures}" MovementForceSwimSpeedChangeAckSequence "${force_swim_speed_ack}")

foreach(required IN ITEMS
        "bool GetUnknownBit148() const" "bool GetUnknownBit149() const" "bool GetUnknownBit172() const"
        "std::vector<uint32> const& GetMovementForceIds() const" "void SetMoverGuid(ObjectGuid guid)"
        "case MSEUnknownBit148:" "case MSEUnknownBit149:" "case MSEUnknownBit172:"
        "case MSEFlags2_13:" "data.ReadBits(13)" "data.WriteBits(moveFlags2, 13)"
        "case MSEMovementForceCount:" "data.ReadBits(22)" "data.WriteBits(movementForceIds.size(), 22)"
        "case MSEMovementForceIds:" "movementForceCount > (data.size() - data.rpos()) / sizeof(uint32)"
        "throw ByteBufferException(false, data.rpos(), movementForceCount * sizeof(uint32), data.size())"
        "case MSEHasUnknownUInt32:" "case MSEUnknownUInt32:")
    string(FIND "${unit_header}${movement_codec}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "movement storage/codec missing: ${required}")
    endif()
endforeach()
foreach(required IN ITEMS "uint32   t_time3" "data >> t_time3" "data << uint32(t_time3)")
    string(FIND "${unit_header}${movement_codec}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "movement transport-time3 storage missing: ${required}")
    endif()
endforeach()

foreach(name IN ITEMS MSG_MOVE_HEARTBEAT CMSG_MOVE_START_FORWARD CMSG_MOVE_START_BACKWARD
        CMSG_MOVE_STOP CMSG_MOVE_SET_FACING CMSG_MOVE_FALL_LAND
        CMSG_MOVE_START_STRAFE_LEFT CMSG_MOVE_START_STRAFE_RIGHT CMSG_MOVE_STOP_STRAFE
        CMSG_MOVE_JUMP CMSG_MOVE_START_TURN_LEFT CMSG_MOVE_START_TURN_RIGHT CMSG_MOVE_STOP_TURN)
    require_once("${opcode_registry}"
        "DefC(${name}, \"${name}\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);"
        "${name} registration")
endforeach()
require_once("${opcode_registry}" "DefS(SMSG_PLAYER_MOVE, \"SMSG_PLAYER_MOVE\");" "SMSG_PLAYER_MOVE metadata")
require_once("${opcode_registry}" "DefS(SMSG_MONSTER_MOVE, \"SMSG_MONSTER_MOVE\");" "SMSG_MONSTER_MOVE metadata")
require_once("${opcode_registry}"
    "DefC(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK, \"CMSG_FORCE_SWIM_SPEED_CHANGE_ACK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleForceSpeedChangeAckOpcodes);"
    "CMSG_FORCE_SWIM_SPEED_CHANGE_ACK registration")

string(REGEX MATCH "void WorldSession::HandleForceSpeedChangeAckOpcodes\\(WorldPacket& recv_data\\)(.*)void WorldSession::HandleSetActiveMoverOpcode" force_speed_handler_body "${movement_handler}")
string(FIND "${force_speed_handler_body}" "recv_data >> newspeed;" force_speed_position)
string(FIND "${force_speed_handler_body}" "recv_data >> movementInfo;" movement_info_position)
if(force_speed_position EQUAL -1 OR movement_info_position EQUAL -1 OR
        NOT force_speed_position LESS movement_info_position)
    message(FATAL_ERROR "force-swim ACK must extract leading speed before MovementInfo")
endif()
foreach(required IN ITEMS "_player->GetObjectGuid() != movementInfo.GetGuid()"
        "case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:" "move_type = MOVE_SWIM"
        "fabs(_player->GetSpeed(move_type) - newspeed)")
    string(FIND "${force_speed_handler_body}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "force-swim ACK handler missing: ${required}")
    endif()
endforeach()
foreach(forbidden IN ITEMS "guid.ReadAsPacked()" "recv_data >> Unused<uint32>()")
    string(FIND "${force_speed_handler_body}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "legacy force-speed framing remains live: ${forbidden}")
    endif()
endforeach()

foreach(required IN ITEMS
        "WriteBits(type, 3)" "WriteBits(uncompressedSplineCount, 20)"
        "WriteBits(compressedSplineCount, 22)"
        "WriteGuidMask<7, 1, 3, 0, 6, 4, 5, 2>(transportGuid)"
        "WriteGuidBytes<6, 4, 1, 7, 0, 3, 5, 2>(transportGuid)")
    string(FIND "${spline_packet_source}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "18414 monster-move writer missing: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "WorldPacket data(SMSG_MONSTER_MOVE, 64)"
        "PacketBuilder::WriteMonsterMove(move_spline, data, unit.GetObjectGuid()"
        "PacketBuilder::WriteStopMovement(real_position, move_spline.GetId(), data")
    string(FIND "${spline_init_source}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "18414 monster-move integration missing: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS "SMSG_MONSTER_MOVE_TRANSPORT" "GetPackGUID()")
    string(FIND "${spline_init_source}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "legacy monster-move framing remains live: ${forbidden}")
    endif()
endforeach()

string(FIND "${world_session_source}" "case SMSG_MONSTER_MOVE:" monster_move_whitelist)
if(monster_move_whitelist EQUAL -1)
    message(FATAL_ERROR "SMSG_MONSTER_MOVE is not in the converted enter-world allow-list")
endif()

string(FIND "${opcode_header}" "SMSG_MONSTER_MOVE                            = 0x1A07" monster_move_opcode)
if(monster_move_opcode EQUAL -1)
    message(FATAL_ERROR "SMSG_MONSTER_MOVE 18414 opcode value missing")
endif()

foreach(name IN ITEMS NORMAL_FALL WATER_WALK FEATHER_FALL LAND_WALK)
    require_once("${opcode_registry}"
        "DefS(SMSG_SPLINE_MOVE_SET_${name}, \"SMSG_SPLINE_MOVE_SET_${name}\");"
        "SMSG_SPLINE_MOVE_SET_${name} metadata")
endforeach()

foreach(required IN ITEMS
        "WorldPacket data(enable ? SMSG_SPLINE_MOVE_SET_FEATHER_FALL : SMSG_SPLINE_MOVE_SET_NORMAL_FALL, 9)"
        "MopCompactPackets::BuildSplineMoveSetFeatherFall(data, GetObjectGuid())"
        "MopCompactPackets::BuildSplineMoveSetNormalFall(data, GetObjectGuid())"
        "WorldPacket data(enable ? SMSG_SPLINE_MOVE_SET_WATER_WALK : SMSG_SPLINE_MOVE_SET_LAND_WALK, 9)"
        "MopCompactPackets::BuildSplineMoveSetWaterWalk(data, GetObjectGuid())"
        "MopCompactPackets::BuildSplineMoveSetLandWalk(data, GetObjectGuid())")
    string(FIND "${creature_movement_source}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "creature spline-state integration missing: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
        "SMSG_SPLINE_MOVE_FEATHER_FALL" "SMSG_SPLINE_MOVE_NORMAL_FALL"
        "SMSG_SPLINE_MOVE_WATER_WALK" "SMSG_SPLINE_MOVE_LAND_WALK")
    string(FIND "${creature_movement_source}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "legacy creature spline-state opcode remains live: ${forbidden}")
    endif()
endforeach()

foreach(required IN ITEMS
        "SMSG_SPLINE_MOVE_SET_NORMAL_FALL             = 0x0B08"
        "SMSG_SPLINE_MOVE_SET_WATER_WALK              = 0x1823"
        "SMSG_SPLINE_MOVE_SET_FEATHER_FALL            = 0x1893"
        "SMSG_SPLINE_MOVE_SET_LAND_WALK               = 0x18B6")
    string(FIND "${opcode_header}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "5.4.8 spline-state opcode value missing: ${required}")
    endif()
endforeach()

string(REGEX MATCH "void Unit::SendHeartBeat\\(\\)([^}]*)\\}" heartbeat_body "${unit_source}")
foreach(required IN ITEMS "WorldPacket data(SMSG_PLAYER_MOVE, 64)" "m_movementInfo.SetMoverGuid(GetObjectGuid())" "data << m_movementInfo")
    string(FIND "${heartbeat_body}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "SendHeartBeat missing: ${required}")
    endif()
endforeach()
foreach(forbidden IN ITEMS "GetPackGUID()" "MSG_MOVE_HEARTBEAT")
    string(FIND "${heartbeat_body}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "SendHeartBeat forbidden framing: ${forbidden}")
    endif()
endforeach()

string(REGEX MATCH "void WorldSession::HandleMovementOpcodes\\(WorldPacket& recv_data\\)(.*)void WorldSession::HandleForceSpeedChangeAckOpcodes" movement_handler_body "${movement_handler}")
foreach(required IN ITEMS "recv_data >> movementInfo" "VerifyMovementInfo(movementInfo, movementInfo.GetGuid())"
        "HandleMoverRelocation(movementInfo)" "WorldPacket data(SMSG_PLAYER_MOVE, recv_data.size())"
        "data << movementInfo" "mover->SendMessageToSetExcept(&data, _player)")
    string(FIND "${movement_handler_body}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "movement handler integration missing: ${required}")
    endif()
endforeach()
