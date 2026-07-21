file(READ "${SOURCE_ROOT}/src/game/Server/MopPartyUpdatePackets.cpp" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Group.cpp" group_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GroupHandler.cpp" handler_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)

if(MUTATION STREQUAL "normal_builder")
    string(REPLACE
        "MopPartyUpdatePackets::BuildPartyUpdate(data, update)"
        "false /* removed normal builder */"
        group_source "${group_source}")
elseif(MUTATION STREQUAL "removal_builder")
    string(REPLACE
        "MopPartyUpdatePackets::BuildRemovedPartyUpdate(data, update)"
        "false /* removed removal builder */"
        group_source "${group_source}")
elseif(MUTATION STREQUAL "targeted_request")
    string(REPLACE
        "group->SendUpdateToPlayer(player->GetObjectGuid())"
        "group->SendUpdate()"
        handler_source "${handler_source}")
elseif(MUTATION STREQUAL "inbound_registration")
    string(REPLACE
        "DefC(CMSG_GROUP_REQUEST_JOIN_UPDATES, \"CMSG_GROUP_REQUEST_JOIN_UPDATES\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGroupRequestJoinUpdates);"
        "/* removed join-update registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "outbound_registration")
    string(REPLACE
        "DefS(SMSG_GROUP_LIST, \"SMSG_GROUP_LIST\");"
        "/* removed party-update registration */"
        opcode_registry "${opcode_registry}")
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

require_once("${group_source}"
    "MopPartyUpdatePackets::BuildPartyUpdate(data, update)"
    "normal party-update builder call")
require_once("${group_source}"
    "MopPartyUpdatePackets::BuildRemovedPartyUpdate(data, update)"
    "removed party-update builder call")
require_once("${handler_source}"
    "group->SendUpdateToPlayer(player->GetObjectGuid())"
    "requester-targeted group update")
require_once("${opcode_registry}"
    "DefC(CMSG_GROUP_REQUEST_JOIN_UPDATES, \"CMSG_GROUP_REQUEST_JOIN_UPDATES\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGroupRequestJoinUpdates);"
    "join-update registration")
require_once("${opcode_registry}"
    "DefS(SMSG_GROUP_LIST, \"SMSG_GROUP_LIST\");"
    "party-update registration")

foreach(token IN ITEMS
        "out.WriteBits(uint32(update.members.size()), 21)"
        "out.WriteBits(uint32(member.name.size()), 6)"
        "out << update.raidDifficulty"
        "out << update.dungeonDifficulty"
        "out << update.partyIndex"
        "out << update.groupPosition"
        "out << update.sequence")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "party-update builder missing: ${token}")
    endif()
endforeach()

foreach(source IN ITEMS "${group_source}" "${handler_source}" "${opcode_header}")
    string(FIND "${source}" "SMSG_REAL_GROUP_UPDATE" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "obsolete SMSG_REAL_GROUP_UPDATE remains")
    endif()
endforeach()
