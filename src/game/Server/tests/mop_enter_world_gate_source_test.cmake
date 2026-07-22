file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)

if(MUTATION STREQUAL "converted_packet")
    string(REPLACE
        "case SMSG_PLAYER_MOVE:"
        "/* removed converted player-move gate row */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "transfer_registration")
    string(REPLACE
        "DefS(SMSG_TRANSFER_PENDING, \"SMSG_TRANSFER_PENDING\");"
        "/* removed transfer-pending metadata */"
        opcode_registry "${opcode_registry}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

# Every row below already has a production 18414 serializer plus focused byte fixtures.
# This test guards the separate transport policy: a converted body must not be silently
# discarded by the temporary enter-world suppression gate.
set(converted_packets
    SMSG_ATTACKSWING_ERROR
    SMSG_MOVE_SET_SWIM_SPEED
    SMSG_RANDOM_ROLL
    SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT
    SMSG_SET_RAID_DIFFICULTY
    SMSG_SET_DUNGEON_DIFFICULTY
    SMSG_TRAINER_BUY_FAILED
    SMSG_LOGIN_SETTIMESPEED
    SMSG_CLIENT_CONTROL_UPDATE
    SMSG_MOVE_SET_ACTIVE_MOVER
    SMSG_UPDATE_CURRENCY
    SMSG_SETUP_CURRENCY
    SMSG_SPELL_EXECUTE_LOG
    SMSG_SPELL_PERIODIC_AURA_LOG
    SMSG_PLAYED_TIME
    SMSG_PLAYER_MOVE
    SMSG_MONSTER_MOVE
    SMSG_SPLINE_MOVE_SET_NORMAL_FALL
    SMSG_SPLINE_MOVE_SET_WATER_WALK
    SMSG_SPLINE_MOVE_SET_FEATHER_FALL
    SMSG_SPLINE_MOVE_SET_LAND_WALK
    SMSG_GUILD_EVENT_MOTD
    SMSG_LFG_BOOT_PLAYER
    SMSG_LFG_UPDATE_STATUS
    SMSG_RESPEC_WIPE_CONFIRM
    SMSG_PARTY_MEMBER_STATS
    SMSG_GROUP_LIST
    SMSG_PET_STABLE_LIST
    SMSG_STABLE_RESULT
    SMSG_RAID_READY_CHECK
    SMSG_RAID_READY_CHECK_CONFIRM
    SMSG_RAID_READY_CHECK_COMPLETED
    SMSG_MAIL_QUERY_NEXT_TIME_RESULT
    SMSG_BATTLEFIELD_RATED_INFO
    SMSG_CALENDAR_EVENT_INITIAL_INVITE
    SMSG_CALENDAR_EVENT_INVITE_STATUS
    SMSG_CALENDAR_EVENT_MODERATOR_STATUS)

foreach(opcode IN LISTS converted_packets)
    require_once("${session_source}" "case[ \\t]+${opcode}:" "${opcode} suppression gate")
endforeach()

foreach(opcode IN ITEMS SMSG_TRANSFER_PENDING SMSG_TRAINER_BUY_FAILED)
    require_once("${opcode_registry}"
        "DefS\\(${opcode},[ \\t]*\"${opcode}\"\\)"
        "${opcode} metadata")
endforeach()

# These tempting high-impact senders still use stale or only partly verified bodies.
# Keeping them absent is deliberate until their complete 18414 readers are recovered.
foreach(opcode IN ITEMS SMSG_UPDATE_OBJECT SMSG_SPELL_GO SMSG_UPDATE_ACCOUNT_DATA)
    string(REGEX MATCHALL "case[ \\t]+${opcode}:" unsafe_matches "${session_source}")
    list(LENGTH unsafe_matches unsafe_count)
    if(NOT unsafe_count EQUAL 0)
        message(FATAL_ERROR "${opcode} must remain suppressed until its 18414 body is verified")
    endif()
endforeach()
