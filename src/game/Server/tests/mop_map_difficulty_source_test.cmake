# Pins the two-key discipline around MapDifficulty.dbc.
#
# MapDifficulty.dbc is keyed on Difficulty.dbc ids, which for instances START AT 1,
# while the core's Difficulty enum is the 0-based WotLK-era one. Every instance
# lookup therefore missed and nothing in the game was enterable: Stockades (map 34)
# has exactly one row, DifficultyID 1, and a request for DUNGEON_DIFFICULTY_NORMAL
# (0) found nothing, so the area trigger answered AREA_LOCKSTATUS_MISSING_DIFFICULTY.
#
# There are now TWO keys and they must not be confused:
#   sMapDifficultyMap       raw client DifficultyID  -> GetMapDifficultyDataByClientId
#   sMapDifficultyLegacyMap internal 0-based mode    -> GetMapDifficultyData
#
# Both failure directions are silent. Feeding a raw id to the internal lookup
# reinterprets client id 3 as internal mode 3 and quietly selects raid 25 heroic
# instead of raid 10 normal; feeding an internal mode to the raw lookup misses. The
# reset scheduler is the site that matters: it walks sMapDifficultyMap directly, so
# it holds raw ids, and it persists them into `instance_reset` and into reset events.
#
# A static per-type translation is NOT sufficient and an earlier version of this fix
# was rejected for exactly that. Some raids have no ID 3 row at all -- 25-player-only
# raids carry only ID 4 and legacy 40-player raids only ID 9 -- so the index is built
# per map with the same widening BuildMapSpawnModeMasks already applies.
#
# Run:
#   cmake -DSOURCE_ROOT=<repo> -P mop_map_difficulty_source_test.cmake

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT must be set")
endif()

set(_dbc "${SOURCE_ROOT}/src/game/Server/DBCStores.cpp")
set(_mps "${SOURCE_ROOT}/src/game/WorldHandlers/MapPersistentStateMgr.cpp")
foreach(_f "${_dbc}" "${_mps}")
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR "missing source: ${_f}")
    endif()
endforeach()
file(READ "${_dbc}" _dbc_raw)
file(READ "${_mps}" _mps_raw)
string(REGEX REPLACE "//[^\n]*" "" _dbc_src "${_dbc_raw}")
string(REGEX REPLACE "//[^\n]*" "" _mps_src "${_mps_raw}")

# ---------------------------------------------------------------------------
# Mutation arms. Each verifies it changed the text it targets and exits 0
# otherwise, so a dead arm surfaces as a WILL_FAIL failure rather than a pass.
# ---------------------------------------------------------------------------
set(_m_dbc "${_dbc_src}")
set(_m_mps "${_mps_src}")
if(DEFINED MUTATION)
    if(MUTATION STREQUAL "drop_legacy_index")
        string(REPLACE "sMapDifficultyLegacyMap.find(MAKE_PAIR32(mapId, difficulty))"
                       "sMapDifficultyMap.find(MAKE_PAIR32(mapId, difficulty))" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_index_build")
        string(REPLACE "    BuildMapDifficultyLegacyIndex();" "" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_25man_widening")
        # Removes the 25-player-only raid alias. Seven raids lose their regular tier
        # and become unenterable, which is the defect this fix was rejected for.
        string(REPLACE "ToInternalDifficulty(mapDiff->DifficultyID) == 1"
                       "false" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_40man_mapping")
        string(REPLACE "case 9:  return 0;" "" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "raw_id_through_internal_lookup")
        # The double translation: reset scheduler raw ids sent to the internal lookup.
        string(REPLACE "GetMapDifficultyDataByClientId(event.mapid, uint32(event.difficulty))"
                       "GetMapDifficultyData(event.mapid, event.difficulty)" _m_mps "${_mps_src}")
    elseif(MUTATION STREQUAL "reset_row_through_internal_lookup")
        string(REPLACE "GetMapDifficultyDataByClientId(mapid, uint32(difficulty))"
                       "GetMapDifficultyData(mapid, difficulty)" _m_mps "${_mps_src}")
    else()
        message(FATAL_ERROR "unknown MUTATION '${MUTATION}'")
    endif()
    if(_m_dbc STREQUAL "${_dbc_src}" AND _m_mps STREQUAL "${_mps_src}")
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()
set(_dbc_src "${_m_dbc}")
set(_mps_src "${_m_mps}")

# ---------------------------------------------------------------------------
# 1. The two lookups must read their own key spaces.
# ---------------------------------------------------------------------------
foreach(_pair
        "sMapDifficultyLegacyMap.find(MAKE_PAIR32(mapId, difficulty))"
        "sMapDifficultyMap.find(MAKE_PAIR32(mapId, clientDifficultyId))")
    string(FIND "${_dbc_src}" "${_pair}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "A MapDifficulty lookup no longer reads its own key space:\n  ${_pair}\n\n"
            "GetMapDifficultyData answers internal 0-based modes from the legacy index;\n"
            "GetMapDifficultyDataByClientId answers raw client ids from the DBC map.\n"
            "Crossing them selects the wrong row silently.")
    endif()
endforeach()

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
# 3. The raw-id -> internal-mode table, including the two rows that only exist
#    because some raids have no modern normal row at all.
# ---------------------------------------------------------------------------
foreach(_row
        "case 1:  return 0;"
        "case 2:  return 1;"
        "case 3:  return 0;"
        "case 4:  return 1;"
        "case 5:  return 2;"
        "case 6:  return 3;"
        "case 9:  return 0;")
    string(FIND "${_dbc_src}" "${_row}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "ToInternalDifficulty is missing a mapping:\n  ${_row}\n\n"
            "Each row corresponds to a Difficulty.dbc id. A missing one makes that\n"
            "tier silently unenterable. Client id 9 is the legacy 40-player raids and\n"
            "is the only route to a regular tier on four maps.")
    endif()
endforeach()

string(FIND "${_dbc_src}" "ToInternalDifficulty(mapDiff->DifficultyID) == 1" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "The 25-player-only raid widening is gone. Seven raids carry only client id 4\n"
        "and would lose their regular tier entirely, which is exactly the defect the\n"
        "first version of this fix was rejected for.")
endif()

# ---------------------------------------------------------------------------
# 4. The reset scheduler holds RAW ids and must use the raw lookup. These are the
#    sites where a double translation would corrupt reset times.
# ---------------------------------------------------------------------------
foreach(_site
        "GetMapDifficultyDataByClientId(mapid, uint32(difficulty))"
        "GetMapDifficultyDataByClientId(event.mapid, uint32(event.difficulty))")
    string(FIND "${_mps_src}" "${_site}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "A reset-scheduler site no longer uses the raw-id lookup:\n  ${_site}\n\n"
            "Those values come from the sMapDifficultyMap key and from `instance_reset`,\n"
            "so they are raw client ids. Sending them through GetMapDifficultyData\n"
            "reinterprets client id 3 as internal mode 3 and picks raid 25 heroic.")
    endif()
endforeach()

message(STATUS "map difficulty guard: two key spaces separated, 7 id mappings, "
               "25-man widening present, 2 raw-id scheduler sites correct")
