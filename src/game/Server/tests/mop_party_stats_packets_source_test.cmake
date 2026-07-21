set(packet_cpp_path "${SOURCE_ROOT}/src/game/Server/MopPartyStatsPackets.cpp")
set(packet_h_path "${SOURCE_ROOT}/src/game/Server/MopPartyStatsPackets.h")
if(NOT EXISTS "${packet_cpp_path}" OR NOT EXISTS "${packet_h_path}")
    message(FATAL_ERROR "MopPartyStatsPackets packet module is missing")
endif()

file(READ "${packet_cpp_path}" packet_cpp)
file(READ "${packet_h_path}" packet_h)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Group.h" group_h)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GroupHandler.cpp" group_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)

function(strip_cpp_comments output source)
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" text "${source}")
    string(REGEX REPLACE "//[^\r\n]*" "" text "${text}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

strip_cpp_comments(packet_cpp "${packet_cpp}")
strip_cpp_comments(group_handler "${group_handler}")
strip_cpp_comments(opcode_registry "${opcode_registry}")
string(REGEX REPLACE "[ \t\r\n]+" " " normalized_group_h "${group_h}")
string(REGEX REPLACE "[ \t\r\n]+" " " normalized_handler "${group_handler}")
string(REGEX REPLACE "[ \t\r\n]+" " " normalized_opcodes "${opcode_registry}")

foreach(required IN ITEMS
        "struct Request" "uint8 mode" "uint64 memberGuid"
        "ReadRequest" "BuildResponse")
    string(FIND "${packet_h}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "packet interface is missing ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "GROUP_UPDATE_FLAG_STATUS = 0x00000001"
        "GROUP_UPDATE_FLAG_MOP_EXTRA = 0x00000002"
        "GROUP_UPDATE_FLAG_CUR_HP = 0x00000004"
        "GROUP_UPDATE_FLAG_MAX_HP = 0x00000008"
        "GROUP_UPDATE_FLAG_POWER_TYPE = 0x00000010"
        "GROUP_UPDATE_FLAG_POWER_EXTRA = 0x00000020"
        "GROUP_UPDATE_FLAG_CUR_POWER = 0x00000040"
        "GROUP_UPDATE_FLAG_MAX_POWER = 0x00000080"
        "GROUP_UPDATE_FLAG_LEVEL = 0x00000100"
        "GROUP_UPDATE_FLAG_ZONE = 0x00000200"
        "GROUP_UPDATE_FLAG_UNK = 0x00000400"
        "GROUP_UPDATE_FLAG_POSITION = 0x00000800"
        "GROUP_UPDATE_FLAG_AURAS = 0x00001000"
        "GROUP_UPDATE_FLAG_PET_GUID = 0x00002000"
        "GROUP_UPDATE_FLAG_PET_NAME = 0x00004000"
        "GROUP_UPDATE_FLAG_PET_MODEL_ID = 0x00008000"
        "GROUP_UPDATE_FLAG_PET_CUR_HP = 0x00010000"
        "GROUP_UPDATE_FLAG_PET_MAX_HP = 0x00020000"
        "GROUP_UPDATE_FLAG_PET_POWER_TYPE = 0x00040000"
        "GROUP_UPDATE_FLAG_PET_EXTRA = 0x00080000"
        "GROUP_UPDATE_FLAG_PET_CUR_POWER = 0x00100000"
        "GROUP_UPDATE_FLAG_PET_MAX_POWER = 0x00200000"
        "GROUP_UPDATE_FLAG_PET_AURAS = 0x00400000"
        "GROUP_UPDATE_FLAG_VEHICLE_SEAT = 0x00800000"
        "GROUP_UPDATE_FLAG_PHASE = 0x01000000")
    string(FIND "${normalized_group_h}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing exact MoP party flag: ${required}")
    endif()
endforeach()

set(inbound "DefC(CMSG_REQUEST_PARTY_MEMBER_STATS, \"CMSG_REQUEST_PARTY_MEMBER_STATS\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestPartyMemberStatsOpcode);")
set(outbound "DefS(SMSG_PARTY_MEMBER_STATS, \"SMSG_PARTY_MEMBER_STATS\");")
string(FIND "${normalized_opcodes}" "${inbound}" inbound_found)
string(FIND "${normalized_opcodes}" "${outbound}" outbound_found)
if(inbound_found EQUAL -1 OR outbound_found EQUAL -1)
    message(FATAL_ERROR "missing Wave 14 request/response opcode registration")
endif()

string(FIND "${normalized_handler}" "MopPartyStatsPackets::ReadRequest(recv_data)" request_parser)
string(FIND "${normalized_handler}" "MopPartyStatsPackets::BuildResponse(*data" delta_builder)
string(REGEX MATCHALL "MopPartyStatsPackets::BuildResponse\\(data" reset_builders "${normalized_handler}")
list(LENGTH reset_builders reset_builder_count)
if(request_parser EQUAL -1 OR delta_builder EQUAL -1 OR reset_builder_count LESS 2)
    message(FATAL_ERROR "both party-stat production paths must use MopPartyStatsPackets")
endif()

string(FIND "${group_handler}" "SMSG_PARTY_MEMBER_STATS_FULL" legacy_full)
if(NOT legacy_full EQUAL -1)
    message(FATAL_ERROR "live SMSG_PARTY_MEMBER_STATS_FULL sender remains")
endif()

foreach(required IN ITEMS
        "uint8 mopFlags" "uint32 effectMask" "payload << float("
        "AFLAG_NOT_CASTER" "AFLAG_POSITIVE" "AFLAG_DURATION"
        "AFLAG_EFFECT_AMOUNT_SEND" "AFLAG_NEGATIVE"
        "payload.WriteBits(phaseIds.size(), 23)")
    string(FIND "${normalized_handler}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing party inner-payload contract: ${required}")
    endif()
endforeach()

string(REGEX MATCHALL "if \\(mask & GROUP_UPDATE_FLAG_VEHICLE_SEAT\\)" vehicle_blocks "${normalized_handler}")
list(LENGTH vehicle_blocks vehicle_block_count)
if(NOT vehicle_block_count EQUAL 1)
    message(FATAL_ERROR "vehicle field must have exactly one mask-controlled writer")
endif()

string(FIND "${normalized_handler}" "uint16(holder->GetAuraFlags())" legacy_flags16)
string(FIND "${normalized_handler}" "int32(aura->GetModifier()->m_amount)" legacy_amount32)
string(FIND "${normalized_handler}" "mask1 = 0x" hardcoded_full_mask)
if(NOT legacy_flags16 EQUAL -1 OR NOT legacy_amount32 EQUAL -1 OR NOT hardcoded_full_mask EQUAL -1)
    message(FATAL_ERROR "legacy party-stat aura/full-mask serialization remains")
endif()
