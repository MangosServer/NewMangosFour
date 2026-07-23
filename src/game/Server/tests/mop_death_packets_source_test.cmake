file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerDeath.cpp" player_death)
file(READ "${SOURCE_ROOT}/src/game/Object/ObjectMgr.h" object_mgr_header)
file(READ "${SOURCE_ROOT}/src/game/Object/ObjectMgrGraveyard.cpp" object_mgr_graveyard)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "wire_order")
    string(REPLACE "out << mapId << y << x << z;" "out << mapId << x << y << z;"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "player_call")
    string(REPLACE "MopDeathPackets::BuildDeathReleaseLocation(data," "LegacyDeathReleaseLocation(data,"
        player_death "${player_death}")
elseif(MUTATION STREQUAL "misc_call")
    string(REPLACE "MopDeathPackets::BuildDeathReleaseLocation(data," "LegacyDeathReleaseLocation(data,"
        misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE
        "DefS(SMSG_DEATH_RELEASE_LOC, \"SMSG_DEATH_RELEASE_LOC\");"
        "/* removed death-release registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "whitelist")
    string(REPLACE
        "case SMSG_DEATH_RELEASE_LOC:"
        "/* removed death-release whitelist */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode_value")
    string(REPLACE
        "SMSG_DEATH_RELEASE_LOC                       = 0x1063"
        "SMSG_DEATH_RELEASE_LOC                       = 0x1064"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "cemetery_wire")
    string(REPLACE
        "out.WriteBits(uint32(count), 22);"
        "/* removed cemetery count */"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "cemetery_bound")
    string(REPLACE
        "cemeteryIds.size() > CEMETERY_LIST_MAX ? CEMETERY_LIST_MAX : cemeteryIds.size()"
        "cemeteryIds.size()"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "cemetery_query")
    string(REPLACE
        "sObjectMgr.GetGraveYardIds(GetPlayer()->GetZoneId(), GetPlayer()->GetTeam(), MopDeathPackets::CEMETERY_LIST_MAX)"
        "std::vector<uint32>()"
        misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "cemetery_serializer")
    string(REPLACE
        "MopDeathPackets::BuildCemeteryListResponse(data, cemeteryIds, false);"
        "/* removed cemetery serializer */"
        misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "cemetery_registration")
    string(REPLACE
        "DefC(CMSG_REQUEST_CEMETERY_LIST, \"CMSG_REQUEST_CEMETERY_LIST\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestCemeteryListOpcode);"
        "/* removed cemetery request registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "cemetery_whitelist")
    string(REPLACE
        "case SMSG_REQUEST_CEMETERY_LIST_RESPONSE:"
        "/* removed cemetery response whitelist */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "cemetery_opcode")
    string(REPLACE
        "CMSG_REQUEST_CEMETERY_LIST                   = 0x06E4"
        "CMSG_REQUEST_CEMETERY_LIST                   = 0x06E5"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "cemetery_team_filter")
    string(REPLACE
        "data.team != TEAM_BOTH_ALLOWED && data.team != team"
        "data.team != team"
        object_mgr_graveyard "${object_mgr_graveyard}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once("${player_header}"
    "out << mapId << y << x << z"
    "death-release 18414 field order")

string(REGEX MATCHALL
    "MopDeathPackets::BuildDeathReleaseLocation\\(data,"
    player_calls "${player_death}")
list(LENGTH player_calls player_call_count)
if(NOT player_call_count EQUAL 2)
    message(FATAL_ERROR "expected two Player death-release builder calls, found ${player_call_count}")
endif()

string(REGEX MATCHALL
    "MopDeathPackets::BuildDeathReleaseLocation\\(data,"
    misc_calls "${misc_handler}")
list(LENGTH misc_calls misc_call_count)
if(NOT misc_call_count EQUAL 1)
    message(FATAL_ERROR "expected one repop death-release builder call, found ${misc_call_count}")
endif()

foreach(source IN ITEMS player_death misc_handler)
    if("${${source}}" MATCHES "WorldPacket[ \t]+data\\(SMSG_DEATH_RELEASE_LOC")
        message(FATAL_ERROR "live inline death-release sender remains in ${source}")
    endif()
endforeach()

require_once("${opcode_registry}"
    "DefS\\(SMSG_DEATH_RELEASE_LOC,[ \t]*\"SMSG_DEATH_RELEASE_LOC\"\\)"
    "death-release registration")
require_once("${session_source}"
    "case[ \t]+SMSG_DEATH_RELEASE_LOC:"
    "death-release suppression whitelist")
require_once("${opcode_header}"
    "SMSG_DEATH_RELEASE_LOC[ \t]*=[ \t]*0x1063"
    "death-release opcode")

require_once("${player_header}"
    "static size_t const CEMETERY_LIST_MAX[ \t]*=[ \t]*16"
    "cemetery-list client bound")
require_once("${player_header}"
    "void BuildCemeteryListResponse\\(WorldPacket& out,[ \t\r\n]*std::vector<uint32> const& cemeteryIds,[ \t\r\n]*bool isGossipTriggered\\)"
    "cemetery-list serializer")

string(FIND "${player_header}" "inline void BuildCemeteryListResponse" cemetery_serializer_start)
string(FIND "${player_header}" "namespace MopBattlePetPackets" cemetery_serializer_end)
if(cemetery_serializer_start EQUAL -1 OR cemetery_serializer_end EQUAL -1 OR
        cemetery_serializer_end LESS_EQUAL cemetery_serializer_start)
    message(FATAL_ERROR "could not isolate cemetery-list serializer")
endif()
math(EXPR cemetery_serializer_length "${cemetery_serializer_end} - ${cemetery_serializer_start}")
string(SUBSTRING "${player_header}" ${cemetery_serializer_start}
    ${cemetery_serializer_length} cemetery_serializer)

require_once("${cemetery_serializer}"
    "cemeteryIds\\.size\\(\\) > CEMETERY_LIST_MAX \\? CEMETERY_LIST_MAX : cemeteryIds\\.size\\(\\)"
    "cemetery-list serializer bound")
require_once("${cemetery_serializer}"
    "out\\.Initialize\\(SMSG_REQUEST_CEMETERY_LIST_RESPONSE,"
    "cemetery-list response opcode")
require_once("${cemetery_serializer}"
    "out\\.WriteBits\\(uint32\\(count\\), 22\\)"
    "cemetery-list 22-bit count")
require_once("${cemetery_serializer}"
    "out\\.WriteBit\\(isGossipTriggered\\)"
    "cemetery-list gossip bit")
require_once("${cemetery_serializer}"
    "out\\.FlushBits\\(\\)"
    "cemetery-list byte alignment")
require_once("${cemetery_serializer}"
    "out << cemeteryIds\\[i\\]"
    "cemetery-list ids")

require_once("${object_mgr_header}"
    "std::vector<uint32> GetGraveYardIds\\(uint32 zoneId, Team team, size_t maxCount\\) const"
    "bounded graveyard query declaration")
require_once("${object_mgr_graveyard}"
    "std::vector<uint32> ObjectMgr::GetGraveYardIds\\(uint32 zoneId, Team team, size_t maxCount\\) const"
    "bounded graveyard query definition")

string(FIND "${object_mgr_graveyard}" "std::vector<uint32> ObjectMgr::GetGraveYardIds" graveyard_query_start)
string(FIND "${object_mgr_graveyard}" "bool ObjectMgr::AddGraveYardLink" graveyard_query_end)
if(graveyard_query_start EQUAL -1 OR graveyard_query_end EQUAL -1 OR
        graveyard_query_end LESS_EQUAL graveyard_query_start)
    message(FATAL_ERROR "could not isolate bounded graveyard query")
endif()
math(EXPR graveyard_query_length "${graveyard_query_end} - ${graveyard_query_start}")
string(SUBSTRING "${object_mgr_graveyard}" ${graveyard_query_start}
    ${graveyard_query_length} graveyard_query)

require_once("${graveyard_query}"
    "mGraveYardMap\\.equal_range\\(zoneId\\)"
    "graveyard-zone selection")
require_once("${graveyard_query}"
    "data\\.team != TEAM_BOTH_ALLOWED && data\\.team != team"
    "neutral and same-team graveyard selection")
require_once("${graveyard_query}"
    "result\\.size\\(\\) == maxCount"
    "graveyard query bound")

require_once("${session_header}"
    "void HandleRequestCemeteryListOpcode\\(WorldPacket& recv_data\\)"
    "cemetery-list session declaration")
require_once("${misc_handler}"
    "void WorldSession::HandleRequestCemeteryListOpcode\\(WorldPacket& /\\*recv_data\\*/\\)"
    "empty cemetery-list request handler")

string(FIND "${misc_handler}" "void WorldSession::HandleRequestCemeteryListOpcode" cemetery_handler_start)
string(FIND "${misc_handler}" "void WorldSession::HandleRepopRequestOpcode" cemetery_handler_end)
if(cemetery_handler_start EQUAL -1 OR cemetery_handler_end EQUAL -1 OR
        cemetery_handler_end LESS_EQUAL cemetery_handler_start)
    message(FATAL_ERROR "could not isolate cemetery-list request handler")
endif()
math(EXPR cemetery_handler_length "${cemetery_handler_end} - ${cemetery_handler_start}")
string(SUBSTRING "${misc_handler}" ${cemetery_handler_start}
    ${cemetery_handler_length} cemetery_handler)

require_once("${cemetery_handler}"
    "sObjectMgr\\.GetGraveYardIds\\(GetPlayer\\(\\)->GetZoneId\\(\\), GetPlayer\\(\\)->GetTeam\\(\\), MopDeathPackets::CEMETERY_LIST_MAX\\)"
    "handler bounded graveyard query")
require_once("${cemetery_handler}"
    "MopDeathPackets::BuildCemeteryListResponse\\(data, cemeteryIds, false\\)"
    "handler scheduled-response serializer")
require_once("${cemetery_handler}"
    "SendPacket\\(&data\\)"
    "handler deterministic response send")

require_once("${opcode_registry}"
    "DefC\\(CMSG_REQUEST_CEMETERY_LIST,[ \t]*\"CMSG_REQUEST_CEMETERY_LIST\",[ \t]*STATUS_LOGGEDIN,[ \t]*PROCESS_THREADUNSAFE,[ \t]*&WorldSession::HandleRequestCemeteryListOpcode\\)"
    "cemetery-list request registration")
require_once("${opcode_registry}"
    "DefS\\(SMSG_REQUEST_CEMETERY_LIST_RESPONSE,[ \t]*\"SMSG_REQUEST_CEMETERY_LIST_RESPONSE\"\\)"
    "cemetery-list response registration")
require_once("${session_source}"
    "case[ \t]+SMSG_REQUEST_CEMETERY_LIST_RESPONSE:"
    "cemetery-list response suppression whitelist")
require_once("${opcode_header}"
    "CMSG_REQUEST_CEMETERY_LIST[ \t]*=[ \t]*0x06E4"
    "cemetery-list request opcode")
require_once("${opcode_header}"
    "SMSG_REQUEST_CEMETERY_LIST_RESPONSE[ \t]*=[ \t]*0x042A"
    "cemetery-list response opcode")
