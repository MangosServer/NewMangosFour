if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellHandler.cpp" spell_handler)
file(READ "${SOURCE_ROOT}/src/game/Object/GameObjectUse.cpp" gameobject_use)
file(READ "${SOURCE_ROOT}/src/game/Object/WorldObjectChat.cpp" worldobject_chat)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/QueryHandler.cpp" query_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ItemHandler.cpp" item_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" world_session_header)

if(DEFINED MUTATION)
    if(MUTATION STREQUAL "use_parser")
        string(REPLACE "MopGameObjectPackets::ParseUseRequest(recv_data, rawGuid)"
            "false" spell_handler "${spell_handler}")
    elseif(MUTATION STREQUAL "report_parser")
        string(REPLACE "MopGameObjectPackets::ParseReportUseRequest(recvPacket, rawGuid)"
            "false" spell_handler "${spell_handler}")
    elseif(MUTATION STREQUAL "guid_type_guard")
        string(REPLACE "if (!guid.IsGameObject())"
            "if (false)" spell_handler "${spell_handler}")
    elseif(MUTATION STREQUAL "custom_sender")
        string(REPLACE "MopGameObjectPackets::BuildCustomAnimation(data, animation)"
            "false" worldobject_chat "${worldobject_chat}")
    elseif(MUTATION STREQUAL "despawn_sender")
        string(REPLACE "MopGameObjectPackets::BuildDespawnAnimation(data, guid.GetRawValue());"
            "data.Initialize(SMSG_GAMEOBJECT_DESPAWN_ANIM);"
            worldobject_chat "${worldobject_chat}")
    elseif(MUTATION STREQUAL "pagetext_sender")
        string(REPLACE "MopGameObjectPackets::BuildPageText(data, GetObjectGuid().GetRawValue());"
            "data.Initialize(SMSG_GAMEOBJECT_PAGETEXT);"
            gameobject_use "${gameobject_use}")
    elseif(MUTATION STREQUAL "client_registration")
        string(REPLACE "DefC(CMSG_GAMEOBJ_USE,"
            "DefC(CMSG_UNUSED_GAMEOBJ_USE," opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "report_registration")
        string(REPLACE "DefC(CMSG_GAMEOBJ_REPORT_USE,"
            "DefC(CMSG_UNUSED_GAMEOBJ_REPORT_USE," opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "server_registration")
        string(REPLACE "DefS(SMSG_GAMEOBJECT_CUSTOM_ANIM,"
            "DefS(SMSG_UNUSED_GAMEOBJECT_CUSTOM_ANIM,"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "sender_admission")
        string(REPLACE "case SMSG_GAMEOBJECT_PAGETEXT:"
            "case SMSG_UNUSED_GAMEOBJECT_PAGETEXT:"
            world_session "${world_session}")
    elseif(MUTATION STREQUAL "opcode_values")
        string(REPLACE "CMSG_GAMEOBJ_USE                             = 0x06D8"
            "CMSG_GAMEOBJ_USE                             = 0x06D7"
            opcode_header "${opcode_header}")
    elseif(MUTATION STREQUAL "legacy_reader")
        string(REPLACE "MopGameObjectPackets::ParseUseRequest(recv_data, rawGuid)"
            "recv_data >> guid" spell_handler "${spell_handler}")
    elseif(MUTATION STREQUAL "page_query_parser")
        string(REPLACE
            "MopQueryPackets::ParsePageTextQueryRequest(recv_data, request)"
            "false" query_handler "${query_handler}")
    elseif(MUTATION STREQUAL "page_query_builder")
        string(REPLACE
            "MopQueryPackets::BuildPageTextQueryResponse(data, response)"
            "false" query_handler "${query_handler}")
    elseif(MUTATION STREQUAL "page_query_client_registration")
        string(REPLACE "DefC(CMSG_PAGE_TEXT_QUERY,"
            "DefC(CMSG_UNUSED_PAGE_TEXT_QUERY,"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "page_query_server_registration")
        string(REPLACE "DefS(SMSG_PAGE_TEXT_QUERY_RESPONSE,"
            "DefS(SMSG_UNUSED_PAGE_TEXT_QUERY_RESPONSE,"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "page_query_admission")
        string(REPLACE "case SMSG_PAGE_TEXT_QUERY_RESPONSE:"
            "case SMSG_UNUSED_PAGE_TEXT_QUERY_RESPONSE:"
            world_session "${world_session}")
    elseif(MUTATION STREQUAL "page_query_reference_status")
        string(REPLACE
            "SMSG_PAGE_TEXT_QUERY_RESPONSE                  0x081E  ACTIVE"
            "SMSG_PAGE_TEXT_QUERY_RESPONSE                  0x081E  DORMANT"
            opcode_reference "${opcode_reference}")
    elseif(MUTATION STREQUAL "page_query_duplicate_handler")
        string(APPEND item_handler
            "\nvoid WorldSession::HandlePageQuerySkippedOpcode")
    elseif(MUTATION STREQUAL "page_query_legacy_sender")
        string(APPEND query_handler
            "\nWorldPacket data(SMSG_PAGE_TEXT_QUERY_RESPONSE, 50);")
    else()
        message(FATAL_ERROR "unknown MUTATION=${MUTATION}")
    endif()
endif()

function(require_once text needle label)
    string(REGEX MATCHALL "${needle}" matches "${text}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${label}: expected exactly once, found ${count}")
    endif()
endfunction()

require_once("${spell_handler}"
    "MopGameObjectPackets::ParseUseRequest\\(recv_data, rawGuid\\)"
    "direct GAMEOBJ_USE parser")
require_once("${spell_handler}"
    "MopGameObjectPackets::ParseReportUseRequest\\(recvPacket, rawGuid\\)"
    "direct GAMEOBJ_REPORT_USE parser")

string(REGEX MATCHALL "if \\(!guid\\.IsGameObject\\(\\)\\)" guid_guards
    "${spell_handler}")
list(LENGTH guid_guards guid_guard_count)
if(guid_guard_count LESS 2)
    message(FATAL_ERROR "both GameObject requests must reject spoofed GUID types")
endif()

require_once("${worldobject_chat}"
    "MopGameObjectPackets::BuildCustomAnimation\\(data, animation\\)"
    "custom animation builder")
require_once("${worldobject_chat}"
    "MopGameObjectPackets::BuildDespawnAnimation\\(data, guid\\.GetRawValue\\(\\)\\)"
    "despawn animation builder")
require_once("${gameobject_use}"
    "MopGameObjectPackets::BuildPageText\\(data, GetObjectGuid\\(\\)\\.GetRawValue\\(\\)\\)"
    "GameObject page-text builder")
require_once("${query_handler}"
    "MopQueryPackets::ParsePageTextQueryRequest\\(recv_data, request\\)"
    "page-text cache request parser")
require_once("${query_handler}"
    "MopQueryPackets::BuildPageTextQueryResponse\\(data, response\\)"
    "page-text cache response builder")

foreach(line IN ITEMS
        "DefC(CMSG_GAMEOBJ_USE, \"CMSG_GAMEOBJ_USE\""
        "DefC(CMSG_GAMEOBJ_REPORT_USE, \"CMSG_GAMEOBJ_REPORT_USE\""
        "DefS(SMSG_GAMEOBJECT_CUSTOM_ANIM, \"SMSG_GAMEOBJECT_CUSTOM_ANIM\");"
        "DefS(SMSG_GAMEOBJECT_DESPAWN_ANIM, \"SMSG_GAMEOBJECT_DESPAWN_ANIM\");"
        "DefS(SMSG_GAMEOBJECT_PAGETEXT, \"SMSG_GAMEOBJECT_PAGETEXT\");")
    string(FIND "${opcode_registry}" "${line}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing opcode registration: ${line}")
    endif()
endforeach()

foreach(line IN ITEMS
        "DefC(CMSG_PAGE_TEXT_QUERY, \"CMSG_PAGE_TEXT_QUERY\""
        "DefS(SMSG_PAGE_TEXT_QUERY_RESPONSE, \"SMSG_PAGE_TEXT_QUERY_RESPONSE\");")
    string(FIND "${opcode_registry}" "${line}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing page-text query registration: ${line}")
    endif()
endforeach()

foreach(opcode IN ITEMS
        SMSG_GAMEOBJECT_CUSTOM_ANIM
        SMSG_GAMEOBJECT_DESPAWN_ANIM
        SMSG_GAMEOBJECT_PAGETEXT
        SMSG_PAGE_TEXT_QUERY_RESPONSE)
    string(FIND "${world_session}" "case ${opcode}:" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${opcode} is not admitted through send suppression")
    endif()
endforeach()

foreach(value IN ITEMS
        "CMSG_PAGE_TEXT_QUERY                         = 0x1022"
        "SMSG_PAGE_TEXT_QUERY_RESPONSE                = 0x081E")
    string(FIND "${opcode_header}" "${value}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "page-text query opcode value drifted: ${value}")
    endif()
endforeach()

foreach(reference IN ITEMS
        "SMSG_PAGE_TEXT_QUERY_RESPONSE                  0x081E  ACTIVE"
        "CMSG_PAGE_TEXT_QUERY                           0x1022  ACTIVE")
    string(FIND "${opcode_reference}" "${reference}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "page-text query reference state drifted: ${reference}")
    endif()
endforeach()

foreach(value IN ITEMS
        "CMSG_GAMEOBJ_USE                             = 0x06D8"
        "CMSG_GAMEOBJ_REPORT_USE                      = 0x06D9"
        "SMSG_GAMEOBJECT_CUSTOM_ANIM                  = 0x001F"
        "SMSG_GAMEOBJECT_DESPAWN_ANIM                 = 0x108B"
        "SMSG_GAMEOBJECT_PAGETEXT                     = 0x14AF")
    string(FIND "${opcode_header}" "${value}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "opcode value drifted: ${value}")
    endif()
endforeach()

string(FIND "${spell_handler}"
    "void WorldSession::HandleGameObjectUseOpcode" use_handler_start)
string(FIND "${spell_handler}"
    "void WorldSession::HandleGameobjectReportUse" use_handler_end)
if(use_handler_start EQUAL -1 OR use_handler_end EQUAL -1)
    message(FATAL_ERROR "could not isolate HandleGameObjectUseOpcode")
endif()
math(EXPR use_handler_length "${use_handler_end} - ${use_handler_start}")
string(SUBSTRING "${spell_handler}" ${use_handler_start} ${use_handler_length}
    use_handler)
string(FIND "${use_handler}" "recv_data >> guid;" stale_use_reader)
if(NOT stale_use_reader EQUAL -1)
    message(FATAL_ERROR "legacy raw GAMEOBJ_USE GUID reader remains")
endif()

string(FIND "${worldobject_chat}" "data << ObjectGuid(guid);" stale_despawn)
if(NOT stale_despawn EQUAL -1)
    message(FATAL_ERROR "legacy raw GameObject animation GUID sender remains")
endif()

string(FIND "${item_handler}" "HandlePageQuerySkippedOpcode"
    duplicate_page_handler)
string(FIND "${world_session_header}" "HandlePageQuerySkippedOpcode"
    duplicate_page_declaration)
if(NOT duplicate_page_handler EQUAL -1 OR
    NOT duplicate_page_declaration EQUAL -1)
    message(FATAL_ERROR "duplicate skipped page-text handler remains")
endif()

foreach(legacy IN ITEMS
        "WorldPacket data(SMSG_PAGE_TEXT_QUERY_RESPONSE, 50)"
        "recv_data.read_skip<uint64>()"
        "Item page missing.")
    string(FIND "${query_handler}" "${legacy}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "legacy page-text query wire body remains: ${legacy}")
    endif()
endforeach()
