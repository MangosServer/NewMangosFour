file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerBattleGround.cpp" player_sender)
file(READ "${SOURCE_ROOT}/src/game/BattleGround/BattleGroundHandler.cpp" bg_handler)
file(READ "${SOURCE_ROOT}/src/game/BattleGround/BattleGround.h" bg_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)

if(MUTATION STREQUAL "builder_call")
    string(REPLACE
        "MopRatedBattlegroundPackets::BuildBattlefieldRatedInfo(data, records);"
        "/* removed builder call */"
        player_sender "${player_sender}")
elseif(MUTATION STREQUAL "inbound_registration")
    string(REPLACE
        "DefC(CMSG_REQUEST_RATED_BG_STATS, \"CMSG_REQUEST_RATED_BG_STATS\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestRatedBGStatsOpcode);"
        "/* removed inbound registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "outbound_registration")
    string(REPLACE
        "DefS(SMSG_BATTLEFIELD_RATED_INFO, \"SMSG_BATTLEFIELD_RATED_INFO\");"
        "/* removed outbound registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "conquest_builder_call")
    string(REPLACE
        "MopBattleGroundPackets::BuildConquestFormulaConstants(data);"
        "/* removed conquest builder call */"
        bg_handler "${bg_handler}")
elseif(MUTATION STREQUAL "conquest_inbound_registration")
    string(REPLACE
        "DefC(CMSG_REQUEST_CONQUEST_FORMULA_CONSTANTS, \"CMSG_REQUEST_CONQUEST_FORMULA_CONSTANTS\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestConquestFormulaConstantsOpcode);"
        "/* removed conquest inbound registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "conquest_outbound_registration")
    string(REPLACE
        "DefS(SMSG_CONQUEST_FORMULA_CONSTANTS, \"SMSG_CONQUEST_FORMULA_CONSTANTS\");"
        "/* removed conquest outbound registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "conquest_allowlist")
    string(REPLACE
        "case SMSG_CONQUEST_FORMULA_CONSTANTS:"
        "/* removed conquest allowlist */"
        session_source "${session_source}")
endif()

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
        math(EXPR next_position "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endwhile()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once("${player_sender}"
    "MopRatedBattlegroundPackets::BuildBattlefieldRatedInfo(data, records);"
    "rated-BG builder call")
require_once("${opcode_registry}"
    "DefC(CMSG_REQUEST_RATED_BG_STATS, \"CMSG_REQUEST_RATED_BG_STATS\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestRatedBGStatsOpcode);"
    "rated-BG inbound registration")
require_once("${opcode_registry}"
    "DefS(SMSG_BATTLEFIELD_RATED_INFO, \"SMSG_BATTLEFIELD_RATED_INFO\");"
    "rated-BG outbound registration")
require_once("${bg_handler}"
    "MopBattleGroundPackets::BuildConquestFormulaConstants(data);"
    "conquest formula builder call")
require_once("${opcode_registry}"
    "DefC(CMSG_REQUEST_CONQUEST_FORMULA_CONSTANTS, \"CMSG_REQUEST_CONQUEST_FORMULA_CONSTANTS\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestConquestFormulaConstantsOpcode);"
    "conquest formula inbound registration")
require_once("${opcode_registry}"
    "DefS(SMSG_CONQUEST_FORMULA_CONSTANTS, \"SMSG_CONQUEST_FORMULA_CONSTANTS\");"
    "conquest formula outbound registration")
require_once("${session_source}"
    "case SMSG_CONQUEST_FORMULA_CONSTANTS:"
    "conquest formula suppression allowlist")

foreach(source IN ITEMS "${player_sender}" "${bg_handler}" "${session_header}" "${opcode_header}")
    foreach(legacy IN ITEMS SMSG_RATED_BG_STATS CMSG_REQUEST_RATED_BG_INFO HandleRequestRatedBgInfo)
        string(FIND "${source}" "${legacy}" position)
        if(NOT position EQUAL -1)
            message(FATAL_ERROR "obsolete rated-BG path remains: ${legacy}")
        endif()
    endforeach()
endforeach()

foreach(token IN ITEMS
        "void HandleRequestConquestFormulaConstantsOpcode(WorldPacket& recv_data);"
        "CMSG_REQUEST_CONQUEST_FORMULA_CONSTANTS      = 0x0365"
        "SMSG_CONQUEST_FORMULA_CONSTANTS              = 0x0EAB")
    string(FIND "${session_header}${opcode_header}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "conquest formula declaration missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "out.Initialize(SMSG_CONQUEST_FORMULA_CONSTANTS, 20);"
        "out << uint32(2000);"
        "out << float(1639.28);"
        "out << float(0.00412);"
        "out << uint32(3500);"
        "out << float(1511.26);")
    string(FIND "${bg_header}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "conquest formula builder missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "for (RatedStatsRecord const& record : records)"
        "for (uint32 field : record)"
        "out << field")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "rated-BG builder missing: ${token}")
    endif()
endforeach()
