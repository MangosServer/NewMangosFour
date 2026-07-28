# Pins the legacy-Difficulty -> 5.4.8 DifficultyID translation.
#
# MapDifficulty.dbc is keyed on Difficulty.dbc ids, which start at 1 for instances,
# while the core's Difficulty enum is the 0-based WotLK-era one. Every instance
# lookup therefore missed and no instance in the game was enterable: Stockades has
# exactly one row, DifficultyID 1, and GetMapDifficultyData(34, 0) found nothing.
#
# The failure mode is what makes this worth a gate. A wrong or missing arm does not
# crash or log -- the lookup simply returns NULL and the caller reports
# AREA_LOCKSTATUS_MISSING_DIFFICULTY, or silently falls back to normal difficulty.
# There is nothing to notice until someone tries to walk into a dungeon.
#
# Values are not invented. Difficulty.dbc carries an instance type and a legacy
# 0-based index per row, and the pairs below are read straight off it:
#   id 1 type 1 legacy 0 | id 2 type 1 legacy 1 | id 8 type 1 legacy -1 (challenge)
#   id 3 type 2 legacy 0 | id 4 type 2 legacy 1 | id 5 type 2 legacy 2
#   id 6 type 2 legacy 3 | id 7 type 2 legacy 4
#
# Run:
#   cmake -DSOURCE_ROOT=<repo> -P mop_map_difficulty_source_test.cmake

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT must be set")
endif()

set(_src "${SOURCE_ROOT}/src/game/Server/DBCStores.cpp")
if(NOT EXISTS "${_src}")
    message(FATAL_ERROR "missing source: ${_src}")
endif()
file(READ "${_src}" _raw)
string(REGEX REPLACE "//[^\n]*" "" _text "${_raw}")

# ---------------------------------------------------------------------------
# Mutation arms. Each verifies it changed something and exits 0 otherwise, so a
# dead arm surfaces as a WILL_FAIL failure rather than a false pass.
# ---------------------------------------------------------------------------
if(DEFINED MUTATION)
    set(_before "${_text}")
    if(MUTATION STREQUAL "drop_dungeon_normal")
        string(REPLACE "case DUNGEON_DIFFICULTY_NORMAL:    return 1;" "" _text "${_text}")
    elseif(MUTATION STREQUAL "drop_challenge")
        string(REPLACE "case DUNGEON_DIFFICULTY_CHALLENGE: return 8;" "" _text "${_text}")
    elseif(MUTATION STREQUAL "wrong_raid_normal")
        string(REPLACE "case RAID_DIFFICULTY_10MAN_NORMAL: return 3;"
                       "case RAID_DIFFICULTY_10MAN_NORMAL: return 1;" _text "${_text}")
    elseif(MUTATION STREQUAL "translate_every_map")
        # Dropping the instance-type gate would translate world, battleground and
        # arena maps too. They only ever carry DifficultyID 0, so that breaks every
        # continent -- a far worse regression than the bug being fixed.
        string(REPLACE "if (mapEntry->InstanceType == MAP_INSTANCE)" "if (true)" _text "${_text}")
    elseif(MUTATION STREQUAL "bypass_translation")
        # Look up the raw difficulty again, i.e. revert the fix.
        string(REPLACE "MAKE_PAIR32(mapId, dbcDifficulty)" "MAKE_PAIR32(mapId, difficulty)" _text "${_text}")
    else()
        message(FATAL_ERROR "unknown MUTATION '${MUTATION}'")
    endif()
    if(_before STREQUAL "${_text}")
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()

# ---------------------------------------------------------------------------
# 1. Both mappings, in full. A missing arm is a silently unenterable tier.
# ---------------------------------------------------------------------------
set(_pairs
    "case DUNGEON_DIFFICULTY_NORMAL:    return 1;"
    "case DUNGEON_DIFFICULTY_HEROIC:    return 2;"
    "case DUNGEON_DIFFICULTY_CHALLENGE: return 8;"
    "case RAID_DIFFICULTY_10MAN_NORMAL: return 3;"
    "case RAID_DIFFICULTY_25MAN_NORMAL: return 4;"
    "case RAID_DIFFICULTY_10MAN_HEROIC: return 5;"
    "case RAID_DIFFICULTY_25MAN_HEROIC: return 6;"
)
foreach(_p IN LISTS _pairs)
    string(FIND "${_text}" "${_p}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "MapDifficulty translation is missing an arm:\n  ${_p}\n\n"
            "Every arm corresponds to a Difficulty.dbc row. A missing one makes that\n"
            "difficulty silently unenterable -- the lookup returns NULL and the caller\n"
            "reports AREA_LOCKSTATUS_MISSING_DIFFICULTY with nothing logged.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 2. The translation must be gated on instance type. World, battleground and arena
#    maps only ever carry DifficultyID 0; translating them would break continents.
# ---------------------------------------------------------------------------
foreach(_g "if (mapEntry->InstanceType == MAP_INSTANCE)"
           "if (mapEntry->InstanceType == MAP_RAID)")
    string(FIND "${_text}" "${_g}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "The difficulty translation is no longer gated on instance type:\n  ${_g}\n\n"
            "Non-instance maps carry DifficultyID 0 and must NOT be translated.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 3. It must happen inside GetMapDifficultyData, not at the call sites. There are
#    eleven callers; one missed translation reintroduces the same silent miss.
# ---------------------------------------------------------------------------
string(FIND "${_text}" "MAKE_PAIR32(mapId, dbcDifficulty)" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "GetMapDifficultyData no longer looks up the translated difficulty.\n"
        "Translating at the call sites instead would mean eleven places to keep\n"
        "right, and the failure mode of missing one is silent.")
endif()

message(STATUS "map difficulty guard: 7 translation arms present, gated on instance type")
