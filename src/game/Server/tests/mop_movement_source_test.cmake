file(READ "${SOURCE_ROOT}/src/game/movement/MovementStructures.h" movement_structures)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit_source)
file(READ "${SOURCE_ROOT}/src/game/Object/CreatureMovement.cpp" creature_movement_source)
file(READ "${SOURCE_ROOT}/src/game/movement/MovementInfo.cpp" movement_codec)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MovementHandler.cpp" movement_handler)

if(MUTATION STREQUAL "inbound_sequence")
    string(REPLACE "MSEPositionZ,\n    MSEPositionX,\n    MSEPositionY,\n    MSEHasMovementFlags2"
        "MSEPositionX,\n    MSEPositionZ,\n    MSEPositionY,\n    MSEHasMovementFlags2"
        movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "player_move_sequence")
    string(REPLACE "MSEHasPitch,\n    MSEGuidBit2,\n    MSEUnknownBit148"
        "MSEGuidBit2,\n    MSEHasPitch,\n    MSEUnknownBit148"
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

set(start_forward "MSEPositionZ,MSEPositionX,MSEPositionY,MSEHasMovementFlags2,MSEUnknownBit149,MSEHasUnknownUInt32,MSEUnknownBit148,MSEGuidBit0,MSEHasOrientation,MSEHasFallData,MSEMovementForceCount,MSEGuidBit4,MSEGuidBit1,MSEHasTimestamp,MSEGuidBit7,MSEHasPitch,MSEHasTransportData,MSEGuidBit5,MSEHasMovementFlags,MSEGuidBit3,MSEHasSplineElevation,MSEGuidBit2,MSEGuidBit6,MSEUnknownBit172,MSETransportGuidBit1,MSEHasTransportTime3,MSETransportGuidBit3,MSETransportGuidBit4,MSETransportGuidBit2,MSETransportGuidBit5,MSETransportGuidBit0,MSETransportGuidBit7,MSETransportGuidBit6,MSEHasTransportTime2,MSEHasFallDirection,MSEFlags2_13,MSEFlags,MSEGuidByte1,MSEGuidByte6,MSEGuidByte7,MSEMovementForceIds,MSEGuidByte5,MSEGuidByte0,MSEGuidByte3,MSEGuidByte2,MSEGuidByte4,MSETransportGuidByte3,MSETransportGuidByte1,MSETransportGuidByte6,MSETransportPositionZ,MSETransportGuidByte4,MSETransportTime3,MSETransportSeat,MSETransportGuidByte7,MSETransportPositionO,MSETransportTime2,MSETransportGuidByte5,MSETransportGuidByte2,MSETransportPositionX,MSETransportGuidByte0,MSETransportPositionY,MSETransportTime,MSEFallCosAngle,MSEFallSinAngle,MSEFallHorizontalSpeed,MSEFallTime,MSEFallVerticalSpeed,MSETimestamp,MSEPitch,MSESplineElevation,MSEPositionO,MSEUnknownUInt32,MSEEnd")
set(start_backward "MSEPositionY,MSEPositionZ,MSEPositionX,MSEHasTimestamp,MSEHasOrientation,MSEGuidBit7,MSEGuidBit2,MSEMovementForceCount,MSEHasFallData,MSEUnknownBit172,MSEGuidBit5,MSEGuidBit3,MSEGuidBit6,MSEHasSplineElevation,MSEGuidBit4,MSEHasTransportData,MSEGuidBit0,MSEHasMovementFlags,MSEHasPitch,MSEHasUnknownUInt32,MSEHasMovementFlags2,MSEUnknownBit148,MSEGuidBit1,MSEUnknownBit149,MSETransportGuidBit1,MSEHasTransportTime2,MSETransportGuidBit0,MSETransportGuidBit7,MSEHasTransportTime3,MSETransportGuidBit3,MSETransportGuidBit5,MSETransportGuidBit6,MSETransportGuidBit2,MSETransportGuidBit4,MSEFlags2_13,MSEFlags,MSEHasFallDirection,MSEMovementForceIds,MSEGuidByte1,MSEGuidByte3,MSEGuidByte5,MSEGuidByte2,MSEGuidByte0,MSEGuidByte4,MSEGuidByte7,MSEGuidByte6,MSEUnknownUInt32,MSETransportTime,MSETransportGuidByte4,MSETransportGuidByte1,MSETransportGuidByte5,MSETransportGuidByte3,MSETransportGuidByte6,MSETransportSeat,MSETransportPositionO,MSETransportPositionX,MSETransportGuidByte0,MSETransportPositionY,MSETransportTime3,MSETransportGuidByte7,MSETransportTime2,MSETransportPositionZ,MSETransportGuidByte2,MSEPositionO,MSEFallTime,MSEFallSinAngle,MSEFallCosAngle,MSEFallHorizontalSpeed,MSEFallVerticalSpeed,MSEPitch,MSETimestamp,MSESplineElevation,MSEEnd")
set(stop_sequence "MSEPositionX,MSEPositionY,MSEPositionZ,MSEGuidBit5,MSEGuidBit2,MSEHasFallData,MSEGuidBit0,MSEUnknownBit172,MSEUnknownBit148,MSEHasUnknownUInt32,MSEGuidBit1,MSEMovementForceCount,MSEHasPitch,MSEGuidBit3,MSEGuidBit4,MSEHasTransportData,MSEUnknownBit149,MSEGuidBit6,MSEHasMovementFlags,MSEHasTimestamp,MSEHasMovementFlags2,MSEHasOrientation,MSEHasSplineElevation,MSEGuidBit7,MSEHasTransportTime2,MSETransportGuidBit7,MSETransportGuidBit4,MSETransportGuidBit1,MSETransportGuidBit0,MSETransportGuidBit5,MSETransportGuidBit2,MSETransportGuidBit3,MSEHasTransportTime3,MSETransportGuidBit6,MSEHasFallDirection,MSEFlags2_13,MSEFlags,MSEGuidByte0,MSEGuidByte3,MSEMovementForceIds,MSEGuidByte6,MSEGuidByte1,MSEGuidByte4,MSEGuidByte2,MSEGuidByte5,MSEGuidByte7,MSEPositionO,MSEFallVerticalSpeed,MSEFallHorizontalSpeed,MSEFallSinAngle,MSEFallCosAngle,MSEFallTime,MSESplineElevation,MSETransportPositionX,MSETransportTime,MSETransportGuidByte3,MSETransportPositionO,MSETransportPositionY,MSETransportGuidByte2,MSETransportGuidByte6,MSETransportGuidByte7,MSETransportGuidByte1,MSETransportGuidByte4,MSETransportTime3,MSETransportGuidByte0,MSETransportSeat,MSETransportPositionZ,MSETransportGuidByte5,MSETransportTime2,MSEUnknownUInt32,MSEPitch,MSETimestamp,MSEEnd")
set(heartbeat "MSEPositionZ,MSEPositionX,MSEPositionY,MSEMovementForceCount,MSEHasMovementFlags,MSEUnknownBit148,MSEHasUnknownUInt32,MSEGuidBit3,MSEGuidBit6,MSEHasPitch,MSEUnknownBit149,MSEUnknownBit172,MSEGuidBit7,MSEGuidBit2,MSEGuidBit4,MSEHasMovementFlags2,MSEHasOrientation,MSEHasTimestamp,MSEHasTransportData,MSEHasFallData,MSEGuidBit5,MSEHasSplineElevation,MSEGuidBit1,MSEGuidBit0,MSETransportGuidBit5,MSETransportGuidBit3,MSETransportGuidBit6,MSETransportGuidBit0,MSETransportGuidBit7,MSEHasTransportTime3,MSETransportGuidBit1,MSETransportGuidBit2,MSETransportGuidBit4,MSEHasTransportTime2,MSEFlags,MSEHasFallDirection,MSEFlags2_13,MSEGuidByte2,MSEGuidByte3,MSEGuidByte6,MSEGuidByte1,MSEGuidByte4,MSEGuidByte7,MSEMovementForceIds,MSEGuidByte5,MSEGuidByte0,MSEFallSinAngle,MSEFallCosAngle,MSEFallHorizontalSpeed,MSEFallVerticalSpeed,MSEFallTime,MSETransportGuidByte1,MSETransportGuidByte3,MSETransportGuidByte2,MSETransportGuidByte0,MSETransportTime3,MSETransportSeat,MSETransportGuidByte7,MSETransportPositionX,MSETransportGuidByte4,MSETransportTime2,MSETransportPositionY,MSETransportGuidByte6,MSETransportGuidByte5,MSETransportPositionZ,MSETransportTime,MSETransportPositionO,MSEUnknownUInt32,MSEPositionO,MSEPitch,MSETimestamp,MSESplineElevation,MSEEnd")
set(set_facing "MSEPositionY,MSEPositionX,MSEPositionZ,MSEGuidBit5,MSEHasMovementFlags2,MSEGuidBit3,MSEGuidBit2,MSEMovementForceCount,MSEUnknownBit172,MSEHasPitch,MSEGuidBit0,MSEHasOrientation,MSEHasTimestamp,MSEUnknownBit148,MSEHasUnknownUInt32,MSEGuidBit4,MSEUnknownBit149,MSEGuidBit1,MSEGuidBit6,MSEHasFallData,MSEHasMovementFlags,MSEHasSplineElevation,MSEHasTransportData,MSEGuidBit7,MSETransportGuidBit0,MSETransportGuidBit7,MSEHasTransportTime2,MSETransportGuidBit3,MSETransportGuidBit6,MSEHasTransportTime3,MSETransportGuidBit2,MSETransportGuidBit5,MSETransportGuidBit1,MSETransportGuidBit4,MSEHasFallDirection,MSEFlags2_13,MSEFlags,MSEMovementForceIds,MSEGuidByte0,MSEGuidByte6,MSEGuidByte3,MSEGuidByte1,MSEGuidByte2,MSEGuidByte7,MSEGuidByte4,MSEGuidByte5,MSETransportGuidByte0,MSETransportGuidByte2,MSETransportPositionO,MSETransportGuidByte7,MSETransportTime3,MSETransportGuidByte5,MSETransportTime,MSETransportPositionX,MSETransportTime2,MSETransportPositionZ,MSETransportSeat,MSETransportPositionY,MSETransportGuidByte4,MSETransportGuidByte3,MSETransportGuidByte6,MSETransportGuidByte1,MSEFallTime,MSEFallVerticalSpeed,MSEFallHorizontalSpeed,MSEFallSinAngle,MSEFallCosAngle,MSEUnknownUInt32,MSETimestamp,MSESplineElevation,MSEPositionO,MSEPitch,MSEEnd")
set(fall_land "MSEPositionY,MSEPositionZ,MSEPositionX,MSEHasFallData,MSEUnknownBit172,MSEUnknownBit148,MSEHasTimestamp,MSEGuidBit7,MSEUnknownBit149,MSEHasSplineElevation,MSEGuidBit5,MSEHasPitch,MSEHasMovementFlags2,MSEGuidBit2,MSEGuidBit3,MSEGuidBit0,MSEHasOrientation,MSEMovementForceCount,MSEHasMovementFlags,MSEHasUnknownUInt32,MSEGuidBit1,MSEHasTransportData,MSEGuidBit6,MSEGuidBit4,MSETransportGuidBit0,MSEHasTransportTime2,MSETransportGuidBit3,MSETransportGuidBit5,MSETransportGuidBit1,MSETransportGuidBit7,MSETransportGuidBit4,MSETransportGuidBit2,MSETransportGuidBit6,MSEHasTransportTime3,MSEFlags2_13,MSEHasFallDirection,MSEFlags,MSEGuidByte4,MSEGuidByte3,MSEGuidByte7,MSEGuidByte0,MSEGuidByte2,MSEGuidByte5,MSEGuidByte1,MSEGuidByte6,MSEMovementForceIds,MSEFallSinAngle,MSEFallHorizontalSpeed,MSEFallCosAngle,MSEFallTime,MSEFallVerticalSpeed,MSETransportGuidByte4,MSETransportPositionY,MSETransportPositionO,MSETransportPositionZ,MSETransportSeat,MSETransportGuidByte3,MSETransportGuidByte6,MSETransportTime2,MSETransportGuidByte2,MSETransportGuidByte1,MSETransportGuidByte5,MSETransportTime3,MSETransportTime,MSETransportPositionX,MSETransportGuidByte7,MSETransportGuidByte0,MSEUnknownUInt32,MSETimestamp,MSESplineElevation,MSEPitch,MSEPositionO,MSEEnd")
set(player_move "MSEHasPitch,MSEGuidBit2,MSEUnknownBit148,MSEUnknownBit149,MSEGuidBit0,MSEHasOrientation,MSEHasFallData,MSEHasUnknownUInt32,MSEGuidBit3,MSEHasFallDirection,MSEHasTransportData,MSEGuidBit4,MSETransportGuidBit5,MSETransportGuidBit4,MSETransportGuidBit7,MSETransportGuidBit2,MSETransportGuidBit6,MSEHasTransportTime2,MSETransportGuidBit3,MSETransportGuidBit1,MSEHasTransportTime3,MSETransportGuidBit0,MSEHasSplineElevation,MSEHasMovementFlags,MSEUnknownBit172,MSEFlags,MSEHasMovementFlags2,MSEGuidBit7,MSEGuidBit1,MSEHasTimestamp,MSEFlags2_13,MSEGuidBit5,MSEMovementForceCount,MSEGuidBit6,MSEPositionY,MSETransportGuidByte7,MSETransportTime2,MSETransportPositionX,MSETransportGuidByte5,MSETransportSeat,MSETransportGuidByte2,MSETransportGuidByte0,MSETransportGuidByte3,MSETransportTime,MSETransportGuidByte4,MSETransportPositionZ,MSETransportGuidByte1,MSETransportPositionY,MSETransportPositionO,MSETransportGuidByte6,MSETransportTime3,MSEGuidByte5,MSEGuidByte1,MSEPositionZ,MSEMovementForceIds,MSETimestamp,MSEPositionO,MSEGuidByte3,MSEFallSinAngle,MSEFallHorizontalSpeed,MSEFallCosAngle,MSEFallVerticalSpeed,MSEFallTime,MSEGuidByte0,MSEPitch,MSEGuidByte2,MSEGuidByte6,MSESplineElevation,MSEUnknownUInt32,MSEPositionX,MSEGuidByte4,MSEGuidByte7,MSEEnd")

require_sequence("${movement_structures}" MovementStartForwardSequence "${start_forward}")
require_sequence("${movement_structures}" MovementStartBackwardSequence "${start_backward}")
require_sequence("${movement_structures}" MovementStopSequence "${stop_sequence}")
require_sequence("${movement_structures}" MovementHeartBeatSequence "${heartbeat}")
require_sequence("${movement_structures}" MovementSetFacingSequence "${set_facing}")
require_sequence("${movement_structures}" MovementFallLandSequence "${fall_land}")
require_sequence("${movement_structures}" PlayerMoveSequence "${player_move}")

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
        CMSG_MOVE_STOP CMSG_MOVE_SET_FACING CMSG_MOVE_FALL_LAND)
    require_once("${opcode_registry}"
        "DefC(${name}, \"${name}\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMovementOpcodes);"
        "${name} registration")
endforeach()
require_once("${opcode_registry}" "DefS(SMSG_PLAYER_MOVE, \"SMSG_PLAYER_MOVE\");" "SMSG_PLAYER_MOVE metadata")

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
