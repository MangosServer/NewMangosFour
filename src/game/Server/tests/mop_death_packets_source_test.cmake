file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerDeath.cpp" player_death)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
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
