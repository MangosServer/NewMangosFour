file(READ "${SOURCE_ROOT}/src/game/Server/MopLfgPackets.cpp" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/LFGHandler.cpp" lfg_sender)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)

if(MUTATION STREQUAL "builder_call")
    string(REPLACE
        "MopLfgPackets::BuildBootPlayer(data, update)"
        "false /* removed LFG boot builder */"
        lfg_sender "${lfg_sender}")
elseif(MUTATION STREQUAL "outbound_registration")
    string(REPLACE
        "DefS(SMSG_LFG_BOOT_PLAYER, \"SMSG_LFG_BOOT_PLAYER\");"
        "/* removed LFG boot registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "opcode_value")
    string(REPLACE
        "SMSG_LFG_BOOT_PLAYER                         = 0x183A"
        "SMSG_LFG_BOOT_PLAYER                         = 0x136E"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "countdown")
    string(REPLACE
        "expires > now ? uint32(expires - now) : 0"
        "uint32((expires - now) / 1000)"
        lfg_sender "${lfg_sender}")
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

require_once("${lfg_sender}"
    "MopLfgPackets::BuildBootPlayer(data, update)"
    "LFG boot builder call")
require_once("${opcode_registry}"
    "DefS(SMSG_LFG_BOOT_PLAYER, \"SMSG_LFG_BOOT_PLAYER\");"
    "LFG boot registration")
require_once("${opcode_header}"
    "SMSG_LFG_BOOT_PLAYER                         = 0x183A"
    "LFG boot opcode value")
require_once("${lfg_sender}"
    "expires > now ? uint32(expires - now) : 0"
    "seconds-based LFG boot countdown")

foreach(token IN ITEMS
        "out.WriteBit(update.reason.empty())"
        "out.WriteBits(update.reason.size(), 8)"
        "WriteGuidMask(out, update.victimGuid, { 1, 7, 5, 2, 0, 4 })"
        "WriteGuidBytes(out, update.victimGuid, { 2, 4, 3, 6 })"
        "out << update.votesNeeded"
        "out << update.timeLeft"
        "out << update.agreeCount"
        "out << update.voteCount")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "LFG boot builder missing: ${token}")
    endif()
endforeach()

string(FIND "${opcode_header}" "SMSG_LFG_BOOT_PROPOSAL_UPDATE" obsolete_alias)
if(NOT obsolete_alias EQUAL -1)
    message(FATAL_ERROR "obsolete event-derived LFG boot alias remains in production opcode header")
endif()

string(FIND "${lfg_sender}" "(expires - now) / 1000" stale_countdown)
if(NOT stale_countdown EQUAL -1)
    message(FATAL_ERROR "LFG boot countdown still divides second timestamps by 1000")
endif()
