set(packet_cpp_path "${SOURCE_ROOT}/src/game/Server/WorldSession.h")
set(packet_h_path "${SOURCE_ROOT}/src/game/Server/WorldSession.h")
if(NOT EXISTS "${packet_cpp_path}" OR NOT EXISTS "${packet_h_path}")
    message(FATAL_ERROR "MopStablePackets packet module is missing")
endif()

file(READ "${packet_cpp_path}" packet_cpp)
file(READ "${packet_h_path}" packet_h)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/NPCHandler.cpp" npc_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_enum)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)

function(strip_cpp_comments output source)
    string(REGEX REPLACE "/\\*([^*]|\\*[^/])*\\*/" "" text "${source}")
    string(REGEX REPLACE "//[^\r\n]*" "" text "${text}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

strip_cpp_comments(packet_cpp "${packet_cpp}")
strip_cpp_comments(npc_handler "${npc_handler}")
strip_cpp_comments(opcode_enum "${opcode_enum}")
strip_cpp_comments(opcode_registry "${opcode_registry}")
string(REGEX REPLACE "[ \t\r\n]+" " " normalized_handler "${npc_handler}")
string(REGEX REPLACE "[ \t\r\n]+" " " normalized_enum "${opcode_enum}")
string(REGEX REPLACE "[ \t\r\n]+" " " normalized_registry "${opcode_registry}")

foreach(required IN ITEMS
        "struct StablePetRecord" "uint32 entry" "uint32 level" "uint8 state"
        "uint32 modelId" "std::string name" "uint32 petNumber" "uint32 slot"
        "ReadStableListRequest" "BuildPetStableList" "BuildStableResult")
    string(FIND "${packet_h}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "stable packet interface is missing ${required}")
    endif()
endforeach()

foreach(required IN ITEMS
        "CMSG_REQUEST_STABLED_PETS = 0x02CA"
        "SMSG_PET_STABLE_LIST = 0x1613"
        "SMSG_STABLE_RESULT = 0x14BE")
    string(FIND "${normalized_enum}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing exact Wave 15 opcode: ${required}")
    endif()
endforeach()

set(inbound "DefC(CMSG_REQUEST_STABLED_PETS, \"CMSG_REQUEST_STABLED_PETS\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleListStabledPetsOpcode);")
set(list_outbound "DefS(SMSG_PET_STABLE_LIST, \"SMSG_PET_STABLE_LIST\");")
set(result_outbound "DefS(SMSG_STABLE_RESULT, \"SMSG_STABLE_RESULT\");")
foreach(registration IN ITEMS "${inbound}" "${list_outbound}" "${result_outbound}")
    string(FIND "${normalized_registry}" "${registration}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing Wave 15 opcode registration: ${registration}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
        "DefC(CMSG_SET_PET_SLOT,"
        "DefC(CMSG_STABLE_PET," "DefC(CMSG_UNSTABLE_PET,"
        "DefC(CMSG_BUY_STABLE_SLOT," "DefC(CMSG_STABLE_REVIVE_PET,"
        "DefC(MSG_LIST_STABLED_PETS,")
    string(FIND "${normalized_registry}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "unsafe or legacy stable registration is live: ${forbidden}")
    endif()
endforeach()

foreach(required IN ITEMS
        "MopStablePackets::ReadStableListRequest(recv_data)"
        "MopStablePackets::BuildPetStableList(data, guid, records)"
        "MopStablePackets::BuildStableResult(data, res)"
        "pet->GetDisplayId()"
        "`modelid`"
        "uint8(petSlot <= uint32(PET_SLOT_LAST_ACTIVE_SLOT) ? 1 : 2)")
    string(FIND "${normalized_handler}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "stable production integration is missing ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
        "WorldPacket data(MSG_LIST_STABLED_PETS"
        "WorldPacket data(SMSG_PET_STABLE_LIST"
        "WorldPacket data(SMSG_STABLE_RESULT")
    string(FIND "${normalized_handler}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "legacy or direct stable packet constructor remains: ${forbidden}")
    endif()
endforeach()

string(FIND "${packet_cpp}" "out.Initialize(SMSG_PET_STABLE_LIST" list_initialize)
string(FIND "${packet_cpp}" "out.Initialize(SMSG_STABLE_RESULT" result_initialize)
if(list_initialize EQUAL -1 OR result_initialize EQUAL -1)
    message(FATAL_ERROR "stable packet builders must initialize their exact server opcodes")
endif()
