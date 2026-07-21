set(group_h_path "${SOURCE_ROOT}/src/game/WorldHandlers/Group.h")
set(group_cpp_path "${SOURCE_ROOT}/src/game/WorldHandlers/Group.cpp")
set(handler_path "${SOURCE_ROOT}/src/game/WorldHandlers/GroupHandler.cpp")
set(opcode_h_path "${SOURCE_ROOT}/src/game/Server/Opcodes.h")
set(opcode_cpp_path "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp")
set(session_cpp_path "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp")

foreach(path IN ITEMS group_h_path group_cpp_path handler_path opcode_h_path opcode_cpp_path session_cpp_path)
    if(NOT EXISTS "${${path}}")
        message(FATAL_ERROR "missing Wave 27 source: ${${path}}")
    endif()
endforeach()

file(READ "${group_h_path}" group_h)
file(READ "${group_cpp_path}" group_cpp)
file(READ "${handler_path}" group_handler)
file(READ "${opcode_h_path}" opcode_h)
file(READ "${opcode_cpp_path}" opcode_cpp)
file(READ "${session_cpp_path}" session_cpp)

# Keep a whitespace-normalized production copy for positive call-site checks.
# GroupHandler contains adjacent legacy block-comment separators that the
# intentionally simple comment stripper below can span on some CMake regex
# engines.
set(group_production "${group_cpp}\n${group_handler}")
string(REGEX REPLACE "[ \t\r\n]+" " " group_production "${group_production}")

function(strip_cpp_comments output source)
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" text "${source}")
    string(REGEX REPLACE "//[^\r\n]*" "" text "${text}")
    string(REGEX REPLACE "[ \t\r\n]+" " " text "${text}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

foreach(source IN ITEMS group_h group_cpp group_handler opcode_h opcode_cpp session_cpp)
    strip_cpp_comments(${source} "${${source}}")
endforeach()

foreach(required IN ITEMS
        "CMSG_MINIMAP_PING = 0x0837"
        "CMSG_RAID_TARGET_UPDATE = 0x0886"
        "SMSG_RAID_TARGET_UPDATE_ALL = 0x0283"
        "SMSG_RAID_TARGET_UPDATE_SINGLE = 0x160B"
        "SMSG_MINIMAP_PING = 0x168F")
    string(FIND "${opcode_h}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing exact Wave 27 opcode: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "DefC(CMSG_MINIMAP_PING, \"CMSG_MINIMAP_PING\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMinimapPingOpcode);"
        "DefC(CMSG_RAID_TARGET_UPDATE, \"CMSG_RAID_TARGET_UPDATE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRaidTargetUpdateOpcode);"
        "DefS(SMSG_MINIMAP_PING, \"SMSG_MINIMAP_PING\");"
        "DefS(SMSG_RAID_TARGET_UPDATE_ALL, \"SMSG_RAID_TARGET_UPDATE_ALL\");"
        "DefS(SMSG_RAID_TARGET_UPDATE_SINGLE, \"SMSG_RAID_TARGET_UPDATE_SINGLE\");")
    string(FIND "${opcode_cpp}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing Wave 27 registration: ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "namespace MopGroupMarkerPackets"
        "ReadMinimapPingRequest"
        "BuildMinimapPing"
        "ReadRaidTargetRequest"
        "BuildRaidTargetSingle"
        "BuildRaidTargetAll")
    string(FIND "${group_h}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "group-marker owner codec is missing ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "MopGroupMarkerPackets::ReadMinimapPingRequest(recv_data)"
        "MopGroupMarkerPackets::BuildMinimapPing(data"
        "MopGroupMarkerPackets::ReadRaidTargetRequest(recv_data)"
        "MopGroupMarkerPackets::BuildRaidTargetSingle(data"
        "MopGroupMarkerPackets::BuildRaidTargetAll(data")
    string(FIND "${group_production}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "group-marker production integration is missing ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "case SMSG_MINIMAP_PING:"
        "case SMSG_RAID_TARGET_UPDATE_ALL:"
        "case SMSG_RAID_TARGET_UPDATE_SINGLE:")
    string(FIND "${session_cpp}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "converted group-marker packet is not allowed through world suppression: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
        "MSG_MINIMAP_PING = 0x6635"
        "MSG_RAID_TARGET_UPDATE = 0x2C36"
        "WorldPacket data(MSG_MINIMAP_PING"
        "WorldPacket data(MSG_RAID_TARGET_UPDATE"
        "if (x == 0xFF)")
    set(haystack "${opcode_h} ${group_cpp} ${group_handler}")
    string(FIND "${haystack}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "legacy group-marker path remains live: ${forbidden}")
    endif()
endforeach()
