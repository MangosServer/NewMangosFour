# Pins the single-key-space discipline around MapDifficulty.dbc.
#
# MapDifficulty.dbc is keyed on Difficulty.dbc ids, which for instances START AT 1,
# while the core's Difficulty enum is the 0-based WotLK-era one. Every instance
# lookup therefore missed and nothing in the game was enterable: Stockades (map 34)
# has exactly one row, DifficultyID 1, and a request for DUNGEON_DIFFICULTY_NORMAL
# (0) found nothing, so the area trigger answered AREA_LOCKSTATUS_MISSING_DIFFICULTY.
#
# The fix translates ONCE, at DBC load, into sMapDifficultyLegacyMap. Internal 0-based
# modes are then the only difficulty key space the server has. Raw client DifficultyIDs
# live entirely inside DBCStores.cpp, between reading the .dbc and building that index.
#
# An earlier revision instead kept BOTH key spaces alive and added a second accessor,
# GetMapDifficultyDataByClientId, for the reset scheduler. That was rejected: the
# scheduler does not merely look rows up, it also writes m_resetTimeByMapDifficulty,
# `instance_reset` and the scheduled DungeonResetEvents, all of which are read back
# with INTERNAL modes by AddPersistentState, by MovementHandler's reset warning, by
# the DungeonPersistentState::GetDifficulty() comparison in _ResetOrWarnAll and by the
# instance/instance_reset SQL join in _CleanupExpiredInstancesAtTime. Since no raw id
# except 0 equals its own internal mode, 129 of the 143 reset-bearing tiers missed the
# table outright and the other 14 silently took another tier's row. Auditing callers of
# the two accessors could not see any of that, because the defect was in the key space,
# not in the lookups. Hence the ban below: the second accessor must not come back.
#
# Run:
#   cmake -DSOURCE_ROOT=<repo> -P mop_map_difficulty_source_test.cmake

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT must be set")
endif()

set(_dbc "${SOURCE_ROOT}/src/game/Server/DBCStores.cpp")
set(_dbh "${SOURCE_ROOT}/src/game/Server/DBCStores.h")
set(_mps "${SOURCE_ROOT}/src/game/WorldHandlers/MapPersistentStateMgr.cpp")
foreach(_f "${_dbc}" "${_dbh}" "${_mps}")
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR "missing source: ${_f}")
    endif()
endforeach()
file(READ "${_dbc}" _dbc_raw)
file(READ "${_dbh}" _dbh_raw)
file(READ "${_mps}" _mps_raw)
# Comments are stripped first so an assertion can never be satisfied by prose that
# merely names the thing it is checking for.
string(REGEX REPLACE "//[^\n]*" "" _dbc_src "${_dbc_raw}")
string(REGEX REPLACE "//[^\n]*" "" _dbh_src "${_dbh_raw}")
string(REGEX REPLACE "//[^\n]*" "" _mps_src "${_mps_raw}")

# ---------------------------------------------------------------------------
# Mutation arms. Each verifies it changed the text it targets and exits 0
# otherwise, so a dead arm surfaces as a WILL_FAIL failure rather than a pass.
# ---------------------------------------------------------------------------
set(_m_dbc "${_dbc_src}")
set(_m_dbh "${_dbh_src}")
set(_m_mps "${_mps_src}")
if(DEFINED MUTATION)
    if(MUTATION STREQUAL "drop_legacy_index")
        string(REPLACE "sMapDifficultyLegacyMap.find(MAKE_PAIR32(mapId, difficulty))"
                       "sMapDifficultyMap.find(MAKE_PAIR32(mapId, difficulty))" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_index_build")
        string(REPLACE "    BuildMapDifficultyLegacyIndex();" "" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_25man_widening")
        # Removes the 25-player-only raid alias. Seven raids lose their regular tier
        # and become unenterable, which is the defect this fix was first rejected for.
        string(REPLACE "ToInternalDifficulty(mapDiff->DifficultyID) == 1"
                       "false" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_continent_mapping")
        # Client id 0 is the ONLY id that is also its own internal mode, so dropping it
        # is invisible to every "is the translation applied" check. It would strip all
        # 112 continent, 13 battleground and 7 arena maps out of the index.
        string(REPLACE "case 0:  return 0;" "" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_challenge_mapping")
        string(REPLACE "case 8:  return 2;" "" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_challenge_spawn_mode")
        # The other half of the challenge-mode pair. With the difficulty index admitting
        # id 8 but the spawn mask dropping it, the nine challenge dungeons resolve at the
        # area trigger and then instantiate with no spawns filed under mask bit 2.
        # Anchored on "mode = 2;" (unique in the file) rather than the whole case arm:
        # stripping the trailing comment leaves whitespace behind on the "case 8:" line,
        # so a multi-line anchor written against the pre-strip text is a dead arm.
        string(REPLACE "                mode = 2;"
                       "                mode = -1;" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_40man_mapping")
        string(REPLACE "case 9:  return 0;" "" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "scheduler_iterates_raw_map")
        # The defect itself: the reset scheduler keying its own tables on raw client ids.
        string(REPLACE "MapDifficultyMap const& legacyMap = GetMapDifficultyLegacyMap();"
                       "MapDifficultyMap const& legacyMap = sMapDifficultyMap;" _m_mps "${_mps_src}")
    elseif(MUTATION STREQUAL "reintroduce_client_id_lookup")
        # The rejected design returning. Both scheduler sites move back onto a raw-id
        # accessor, which is precisely what the ban assertion exists to reject.
        string(REPLACE "GetMapDifficultyData(mapid, difficulty)"
                       "GetMapDifficultyDataByClientId(mapid, difficulty)" _m_mps "${_mps_src}")
    elseif(MUTATION STREQUAL "event_through_raw_lookup")
        string(REPLACE "GetMapDifficultyData(event.mapid, event.difficulty)"
                       "GetMapDifficultyDataByClientId(event.mapid, uint32(event.difficulty))" _m_mps "${_mps_src}")
    else()
        message(FATAL_ERROR "unknown MUTATION '${MUTATION}'")
    endif()
    if(_m_dbc STREQUAL "${_dbc_src}" AND _m_dbh STREQUAL "${_dbh_src}" AND _m_mps STREQUAL "${_mps_src}")
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()
set(_dbc_src "${_m_dbc}")
set(_dbh_src "${_m_dbh}")
set(_mps_src "${_m_mps}")

# Whitespace-collapsed views, so an assertion about a switch arm is not hostage to the
# exact column its comment used to sit in. Derived AFTER mutation from the same text the
# other assertions read, so there is only one pipeline and no before/after skew.
string(REGEX REPLACE "[ \t\r\n]+" " " _dbc_flat "${_dbc_src}")

# ---------------------------------------------------------------------------
# 1. There is ONE difficulty key space. The raw-id accessor must not exist.
# ---------------------------------------------------------------------------
foreach(_pair "_dbc_src" "_dbh_src" "_mps_src")
    string(FIND "${${_pair}}" "GetMapDifficultyDataByClientId" _at)
    if(NOT _at EQUAL -1)
        message(FATAL_ERROR
            "GetMapDifficultyDataByClientId is back (in ${_pair}).\n\n"
            "Raw client DifficultyIDs must not escape DBCStores.cpp. A caller holding\n"
            "one does not merely read: the reset scheduler also WRITES its key into\n"
            "m_resetTimeByMapDifficulty, `instance_reset` and DungeonResetEvent, all of\n"
            "which are read back with internal modes. Translate at the boundary instead.")
    endif()
endforeach()

string(FIND "${_dbc_src}" "sMapDifficultyLegacyMap.find(MAKE_PAIR32(mapId, difficulty))" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "GetMapDifficultyData no longer answers from the internal-mode index.\n"
        "Reading sMapDifficultyMap here reinterprets internal mode 3 as client id 3\n"
        "and silently selects raid 10-normal while claiming to be 25-heroic.")
endif()

# ---------------------------------------------------------------------------
# 2. The legacy index must actually be built, and built after Map.dbc so the
#    25-player-only raid widening can ask whether a map is a raid.
# ---------------------------------------------------------------------------
# Anchored on the indented CALL, not the bare name: the forward declaration and the
# definition both contain "BuildMapDifficultyLegacyIndex();" at column 0, so a check
# for the bare name still passed with the call deleted. Its arm was dead.
string(FIND "${_dbc_src}" "\n    BuildMapDifficultyLegacyIndex();" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "BuildMapDifficultyLegacyIndex() is never called. GetMapDifficultyData would\n"
        "answer from an empty index and every instance lookup would miss again.")
endif()

# ---------------------------------------------------------------------------
# 3. The raw-id -> internal-mode table. Client id 0 is listed too: it is the one
#    id that equals its own internal mode, so its absence is invisible to any
#    check that only asks whether a translation happened at all.
# ---------------------------------------------------------------------------
foreach(_row
        "case 0:  return 0;"
        "case 1:  return 0;"
        "case 2:  return 1;"
        "case 3:  return 0;"
        "case 4:  return 1;"
        "case 5:  return 2;"
        "case 6:  return 3;"
        "case 8:  return 2;"
        "case 9:  return 0;")
    string(FIND "${_dbc_src}" "${_row}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "ToInternalDifficulty is missing a mapping:\n  ${_row}\n\n"
            "Each row corresponds to a Difficulty.dbc id. A missing one makes that\n"
            "tier silently unenterable. Client id 9 is the legacy 40-player raids and\n"
            "is the only route to a regular tier on four maps; client id 8 is challenge\n"
            "mode and the only route to internal mode 2 on nine 5-man maps.")
    endif()
endforeach()

# BuildMapSpawnModeMasks must admit the same ids. A tier the difficulty index accepts
# but the spawn mask drops resolves at the area trigger and then instantiates empty.
string(FIND "${_dbc_flat}" "case 8: mode = 2; break;" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "BuildMapSpawnModeMasks no longer maps client id 8 to spawn mode 2, but\n"
        "ToInternalDifficulty still maps it to internal mode 2. A player at dungeon\n"
        "difficulty 2 on maps 959/960/961/962/994/1001/1004/1007/1011 would pass the\n"
        "area trigger and arrive in an instance with no spawns under mask bit 2.")
endif()

string(FIND "${_dbc_src}" "ToInternalDifficulty(mapDiff->DifficultyID) == 1" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "The 25-player-only raid widening is gone. Seven raids carry only client id 4\n"
        "and would lose their regular tier entirely, which is exactly the defect the\n"
        "first version of this fix was rejected for.")
endif()

# ---------------------------------------------------------------------------
# 4. The reset scheduler enumerates the INTERNAL index. This is the one that
#    matters: the loop does not just read, it writes the key that
#    m_resetTimeByMapDifficulty, `instance_reset` and DungeonResetEvent all carry.
# ---------------------------------------------------------------------------
string(FIND "${_mps_src}" "MapDifficultyMap const& legacyMap = GetMapDifficultyLegacyMap();" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "The reset scheduler no longer enumerates the internal-mode index.\n\n"
        "Iterating sMapDifficultyMap keys every reset structure on raw client ids while\n"
        "AddPersistentState, MovementHandler and _ResetOrWarnAll read them with internal\n"
        "modes. No raw id except 0 equals its own internal mode, so 129 of 143\n"
        "reset-bearing tiers get reset time 0 and raid lockouts display as ~79 years.")
endif()

foreach(_site
        "GetMapDifficultyData(mapid, difficulty)"
        "GetMapDifficultyData(event.mapid, event.difficulty)")
    string(FIND "${_mps_src}" "${_site}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "A reset-scheduler site no longer uses the internal-mode lookup:\n  ${_site}\n\n"
            "`instance_reset`.`difficulty` and DungeonResetEvent::difficulty are written\n"
            "from the legacy index key, so they hold internal modes -- the same key space\n"
            "as `instance`.`difficulty` and DungeonPersistentState::GetDifficulty().")
    endif()
endforeach()

message(STATUS "map difficulty guard: one key space, raw-id accessor absent, "
               "9 id mappings incl. continents and challenge, spawn masks in step, "
               "25-man widening present, scheduler on the internal index")
