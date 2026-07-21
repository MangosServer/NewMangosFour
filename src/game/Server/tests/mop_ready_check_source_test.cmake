set(packet_cpp_path "${SOURCE_ROOT}/src/game/Server/MopReadyCheckPackets.cpp")
set(packet_h_path "${SOURCE_ROOT}/src/game/Server/MopReadyCheckPackets.h")
if(NOT EXISTS "${packet_cpp_path}" OR NOT EXISTS "${packet_h_path}")
    message(FATAL_ERROR "MopReadyCheckPackets packet module is missing")
endif()

file(READ "${packet_cpp_path}" packet_cpp)
file(READ "${packet_h_path}" packet_h)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GroupHandler.cpp" group_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Group.cpp" group_cpp)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Group.h" group_h)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player_cpp)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_h)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_h)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_enum)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)

function(strip_cpp_comments output source)
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" text "${source}")
    string(REGEX REPLACE "//[^\r\n]*" "" text "${text}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

foreach(source IN ITEMS group_handler group_cpp group_h player_cpp session_h opcode_enum opcode_registry)
    strip_cpp_comments(${source} "${${source}}")
endforeach()
foreach(source IN ITEMS group_handler group_cpp group_h player_cpp player_h session_h opcode_enum opcode_registry)
    string(REGEX REPLACE "[ \t\r\n]+" " " ${source} "${${source}}")
endforeach()

foreach(required IN ITEMS
        "struct ResponseRequest" "uint8 partyIndex" "uint64 reservedGuid" "bool ready"
        "ReadStartRequest" "ReadResponseRequest" "BuildStarted" "BuildResponse" "BuildCompleted")
    string(FIND "${packet_h}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "ready-check packet interface is missing ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "CMSG_DO_READY_CHECK = 0x0817"
        "CMSG_RAID_READY_CHECK_CONFIRM = 0x158B"
        "SMSG_RAID_READY_CHECK_CONFIRM = 0x02AF"
        "SMSG_RAID_READY_CHECK_COMPLETED = 0x15C2"
        "SMSG_RAID_READY_CHECK = 0x1C8E")
    string(FIND "${opcode_enum}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing exact Wave 16 opcode: ${required}")
    endif()
endforeach()

foreach(registration IN ITEMS
        "DefC(CMSG_DO_READY_CHECK, \"CMSG_DO_READY_CHECK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRaidReadyCheckOpcode);"
        "DefC(CMSG_RAID_READY_CHECK_CONFIRM, \"CMSG_RAID_READY_CHECK_CONFIRM\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRaidReadyCheckConfirmOpcode);"
        "DefS(SMSG_RAID_READY_CHECK, \"SMSG_RAID_READY_CHECK\");"
        "DefS(SMSG_RAID_READY_CHECK_CONFIRM, \"SMSG_RAID_READY_CHECK_CONFIRM\");"
        "DefS(SMSG_RAID_READY_CHECK_COMPLETED, \"SMSG_RAID_READY_CHECK_COMPLETED\");")
    string(FIND "${opcode_registry}" "${registration}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing Wave 16 opcode registration: ${registration}")
    endif()
endforeach()

foreach(required IN ITEMS
        "MopReadyCheckPackets::ReadStartRequest(recv_data)"
        "MopReadyCheckPackets::ReadResponseRequest(recv_data)"
        "HandleRaidReadyCheckConfirmOpcode"
        "StartReadyCheck"
        "ReadyCheckMemberHasResponded"
        "ReadyCheckAllResponded"
        "CompleteReadyCheck"
        "SetReadyCheckTimer"
        "HasReadyCheckTimer"
        "ReadyCheckComplete")
    set(haystack "${group_handler} ${group_cpp} ${group_h} ${player_cpp} ${player_h} ${session_h}")
    string(FIND "${haystack}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "ready-check production integration is missing ${required}")
    endif()
endforeach()

string(FIND "${group_handler}" "if (player->HasReadyCheckTimer()) return; if (!group->StartReadyCheck" timer_before_start)
if(timer_before_start EQUAL -1)
    message(FATAL_ERROR "a dual-group player can start two checks with one timer")
endif()

foreach(forbidden IN ITEMS
        "WorldPacket data(MSG_RAID_READY_CHECK"
        "WorldPacket data(MSG_RAID_READY_CHECK_CONFIRM"
        "DefC(MSG_RAID_READY_CHECK,"
        "DefC(MSG_RAID_READY_CHECK_CONFIRM,")
    set(haystack "${group_handler} ${group_cpp} ${opcode_registry}")
    string(FIND "${haystack}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "legacy ready-check path remains live: ${forbidden}")
    endif()
endforeach()

foreach(required IN ITEMS
        "out.Initialize(SMSG_RAID_READY_CHECK"
        "out.Initialize(SMSG_RAID_READY_CHECK_CONFIRM"
        "out.Initialize(SMSG_RAID_READY_CHECK_COMPLETED")
    string(FIND "${packet_cpp}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "ready-check packet builder is missing ${required}")
    endif()
endforeach()
