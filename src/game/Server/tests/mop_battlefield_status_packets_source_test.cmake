file(READ "${SOURCE_ROOT}/src/game/BattleGround/BattleGroundMgr.cpp" battleground_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

set(opcodes
    SMSG_BATTLEFIELD_STATUS
    SMSG_BATTLEFIELD_STATUS_QUEUED
    SMSG_BATTLEFIELD_STATUS_NEEDCONFIRMATION
    SMSG_BATTLEFIELD_STATUS_ACTIVE
    SMSG_BATTLEFIELD_STATUS_FAILED)
set(builders
    BuildBattlefieldStatusNone
    BuildBattlefieldStatusQueued
    BuildBattlefieldStatusConfirmation
    BuildBattlefieldStatusActive
    BuildBattlefieldStatusFailed)

foreach(index RANGE 0 4)
    list(GET opcodes ${index} opcode)
    list(GET builders ${index} builder)
    string(FIND "${battleground_source}" "MopBattleGroundPackets::${builder}(*data, status);" builder_call)
    string(FIND "${opcode_source}" "DefS(${opcode}, \"${opcode}\");" registration)
    string(FIND "${session_source}" "case ${opcode}:" whitelist)
    if(builder_call EQUAL -1 OR registration EQUAL -1 OR whitelist EQUAL -1)
        message(FATAL_ERROR "${opcode} is not fully built, registered, and admitted")
    endif()
endforeach()

string(FIND "${opcode_header}" "SMSG_BATTLEFIELD_STATUS_ERROR                = 0x10A6" error_identity)
string(FIND "${opcode_header}" "SMSG_BATTLEFIELD_STATUS_WAITFORGROUPS" stale_name)
string(FIND "${battleground_source}" "Initialize(SMSG_BATTLEFIELD_STATUS_ERROR" error_sender)
if(error_identity EQUAL -1 OR NOT stale_name EQUAL -1 OR NOT error_sender EQUAL -1)
    message(FATAL_ERROR "0x10A6 must remain a directly labelled but unsent unresolved error variant")
endif()
