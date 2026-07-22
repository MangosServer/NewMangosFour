file(GLOB_RECURSE game_sources
    "${SOURCE_ROOT}/src/game/*.cpp"
    "${SOURCE_ROOT}/src/game/*.h")

list(APPEND game_sources
    "${SOURCE_ROOT}/src/modules/Eluna/ElunaIncludes.h"
    "${SOURCE_ROOT}/src/modules/Eluna/methods/Mangos/PlayerMethods.h")

set(forbidden_patterns
        "class ArenaTeam"
        "ArenaTeam::"
        "`arena_team`"
        "`arena_team_member`"
        "`arena_team_stats`"
        LoadArenaTeams
        GetArenaTeamById
        GetArenaTeamByName
        GetArenaTeamByCaptain
        GenerateArenaTeamId
        SendNotInArenaTeamPacket
        SMSG_ARENA_TEAM_COMMAND_RESULT
        SMSG_ARENA_TEAM_QUERY_RESPONSE
        SMSG_ARENA_TEAM_ROSTER
        SMSG_ARENA_TEAM_INVITE
        SMSG_ARENA_TEAM_EVENT
        SMSG_ARENA_TEAM_STATS
        SMSG_ARENA_TEAM_CHANGE_FAILED
        MSG_INSPECT_ARENA_TEAMS
        CMSG_ARENA_TEAM_CREATE
        CMSG_ARENA_TEAM_QUERY
        CMSG_ARENA_TEAM_ROSTER
        CMSG_ARENA_TEAM_INVITE
        CMSG_ARENA_TEAM_ACCEPT
        CMSG_ARENA_TEAM_DECLINE
        CMSG_ARENA_TEAM_LEAVE
        CMSG_ARENA_TEAM_REMOVE
        CMSG_ARENA_TEAM_DISBAND
        CMSG_ARENA_TEAM_LEADER
        CMSG_CALENDAR_ARENA_TEAM
        SMSG_CALENDAR_ARENA_TEAM
        HandleInspectArenaTeamsOpcode
        HandleArenaTeamQueryOpcode
        HandleArenaTeamRosterOpcode
        HandleArenaTeamCreateOpcode
        HandleArenaTeamInviteOpcode
        HandleArenaTeamAcceptOpcode
        HandleArenaTeamDeclineOpcode
        HandleArenaTeamLeaveOpcode
        HandleArenaTeamRemoveOpcode
        HandleArenaTeamDisbandOpcode
        HandleArenaTeamLeaderOpcode
        HandleCalendarArenaTeam
        SendArenaTeamCommandResult)

set(required_patterns
        PLAYER_FIELD_ARENA_TEAM_INFO_1_1
        "enum ArenaType"
        "bool isArena() const")

foreach(required IN LISTS required_patterns)
    string(MAKE_C_IDENTIFIER "${required}" required_id)
    set("found_${required_id}" FALSE)
endforeach()

foreach(path IN LISTS game_sources)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "missing arena cleanup source: ${path}")
    endif()
    file(READ "${path}" source)

    foreach(forbidden IN LISTS forbidden_patterns)
        string(FIND "${source}" "${forbidden}" found)
        if(NOT found EQUAL -1)
            message(FATAL_ERROR "retired arena-team backend remains in ${path}: ${forbidden}")
        endif()
    endforeach()

    foreach(required IN LISTS required_patterns)
        string(FIND "${source}" "${required}" found)
        if(NOT found EQUAL -1)
            string(MAKE_C_IDENTIFIER "${required}" required_id)
            set("found_${required_id}" TRUE)
        endif()
    endforeach()
endforeach()

if(MUTATION STREQUAL "legacy_sender")
    set(mutation_source "WorldPacket data(SMSG_ARENA_TEAM_STATS);")
elseif(MUTATION STREQUAL "legacy_handler")
    set(mutation_source "void HandleArenaTeamQueryOpcode(WorldPacket&);")
elseif(MUTATION STREQUAL "backend")
    set(mutation_source "class ArenaTeam {};")
endif()

foreach(forbidden IN LISTS forbidden_patterns)
    string(FIND "${mutation_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "mutation escaped arena-team cleanup: ${forbidden}")
    endif()
endforeach()

foreach(required IN LISTS required_patterns)
    string(MAKE_C_IDENTIFIER "${required}" required_id)
    if(NOT found_${required_id})
        message(FATAL_ERROR "generic arena behavior or protocol layout was removed: ${required}")
    endif()
endforeach()
