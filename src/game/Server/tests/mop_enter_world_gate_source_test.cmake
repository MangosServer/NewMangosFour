file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Object/ObjectUpdate.cpp" object_update_source)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerQuest.cpp" player_quest_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GridNotifiers.cpp" grid_notifiers_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Map.cpp" map_source)

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
elseif(MUTATION STREQUAL "destroy_gate")
    string(REPLACE
        "case SMSG_DESTROY_OBJECT:"
        "/* removed destroy-object gate row */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "destroy_registration")
    string(REPLACE
        "DefS(SMSG_DESTROY_OBJECT, \"SMSG_DESTROY_OBJECT\");"
        "/* removed destroy-object metadata */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "destroy_bypass")
    string(REPLACE
        "SendPacket(&data);"
        "SendPacket(&data, true);"
        object_update_source "${object_update_source}")
elseif(MUTATION STREQUAL "update_gate")
    string(REPLACE
        "case SMSG_UPDATE_OBJECT:"
        "/* removed update-object gate row */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "update_registration")
    string(REPLACE
        "DefS(SMSG_UPDATE_OBJECT, \"SMSG_UPDATE_OBJECT\");"
        "/* removed update-object metadata */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "update_bypass")
    string(REPLACE
        "SendPacket(&packet);"
        "SendPacket(&packet, true);"
        grid_notifiers_source "${grid_notifiers_source}")
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
    SMSG_TIME_SYNC_REQ
    SMSG_TRIGGER_CINEMATIC
    SMSG_WORLD_SERVER_INFO
    SMSG_MOTD
    SMSG_CORPSE_RECLAIM_DELAY
    SMSG_SET_FORCED_REACTIONS
    SMSG_INIT_WORLD_STATES
    SMSG_ITEM_TIME_UPDATE
    SMSG_ITEM_ENCHANT_TIME_UPDATE
    SMSG_CLIENT_CONTROL_UPDATE
    SMSG_MOVE_SET_ACTIVE_MOVER
    SMSG_UPDATE_CURRENCY
    SMSG_SETUP_CURRENCY
    SMSG_SPELL_EXECUTE_LOG
    SMSG_SPELL_PERIODIC_AURA_LOG
    SMSG_SPELL_GO
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
    SMSG_CALENDAR_EVENT_MODERATOR_STATUS
    SMSG_UPDATE_ACCOUNT_DATA
    SMSG_UPDATE_OBJECT
    SMSG_DESTROY_OBJECT)

foreach(opcode IN LISTS converted_packets)
    require_once("${session_source}" "case[ \\t]+${opcode}:" "${opcode} suppression gate")
endforeach()

foreach(opcode IN ITEMS SMSG_TRANSFER_PENDING SMSG_TRAINER_BUY_FAILED SMSG_UPDATE_OBJECT SMSG_DESTROY_OBJECT)
    require_once("${opcode_registry}"
        "DefS\\(${opcode},[ \\t]*\"${opcode}\"\\)"
        "${opcode} metadata")
endforeach()

# All UPDATE_OBJECT and DESTROY_OBJECT routes now use normal central admission.
# These owning files deliberately permit no bypass at all: any future exceptional
# bootstrap must stay isolated elsewhere with its own registration/gate policy.
set(converted_update_sources
    "${object_update_source}\n${player_quest_source}\n${grid_notifiers_source}\n${map_source}")
string(REGEX MATCHALL "SendPacket\\([^;]*,[ \\t]*true\\);" update_bypass_matches "${converted_update_sources}")
list(LENGTH update_bypass_matches update_bypass_count)
if(NOT update_bypass_count EQUAL 0)
    message(FATAL_ERROR "UPDATE_OBJECT/DESTROY_OBJECT routes must not bypass the central suppression gate")
endif()
