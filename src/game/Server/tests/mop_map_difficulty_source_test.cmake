# Pins the difficulty key-space discipline across every boundary that persists one.
#
# MapDifficulty.dbc is keyed on Difficulty.dbc ids, which for instances START AT 1,
# while the core's Difficulty enum is the 0-based WotLK-era one. Every instance
# lookup therefore missed and nothing in the game was enterable: Stockades (map 34)
# has exactly one row, DifficultyID 1, and a request for DUNGEON_DIFFICULTY_NORMAL
# (0) found nothing, so the area trigger answered AREA_LOCKSTATUS_MISSING_DIFFICULTY.
#
# The rule: translate at the boundary, once, and never let a raw client DifficultyID
# cross a runtime or persistence boundary. THREE DBCs store raw ids and each has its
# own translation point:
#   MapDifficulty.dbc    -> sMapDifficultyLegacyMap, built at load
#   LfgDungeons.dbc      -> ToInternalDifficulty in LFGMgr::CreateDungeonGroup
#   DungeonEncounter.dbc -> EncounterDifficultyMatches at both credit sites
#
# All three were getting this wrong, and all three PERSIST the result, which is why
# each is pinned here rather than left to review:
#
#   * The reset scheduler enumerated the raw map, so m_resetTimeByMapDifficulty,
#     `instance_reset` and every DungeonResetEvent were raw-keyed while
#     AddPersistentState, MovementHandler, _ResetOrWarnAll and the instance/
#     instance_reset SQL join read them as internal. No raw id except 0 equals its own
#     internal mode, so 129 of 143 reset-bearing tiers missed outright (resetTime 0,
#     which SendRaidInfo transmits as a ~79-year lockout) and 14 took another tier's row.
#   * LFG cast LfgDungeons.DifficultyID straight to Difficulty, so LFG normal set
#     internal HEROIC and LFG heroic set CHALLENGE -- persisted into `groups`.`difficulty`
#     and `characters`.`dungeon_difficulty`.
#   * DungeonEncounter.DifficultyID was compared directly against an internal mode
#     before writing `instance`.`encountersMask`. Only the 238 wildcard rows ever
#     matched, and for the wrong reason; the 264 5-man normal rows were tested against
#     internal 1 (heroic); ids 5 and 6 exceed MAX_DIFFICULTY as raw values.
#
# Auditing callers of an accessor cannot find any of this -- the defect is in the key
# space, not the lookup -- so the assertions below pin key spaces and boundaries.
#
# Run:
#   cmake -DSOURCE_ROOT=<repo> -P mop_map_difficulty_source_test.cmake

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT must be set")
endif()

set(_dbc "${SOURCE_ROOT}/src/game/Server/DBCStores.cpp")
set(_dbh "${SOURCE_ROOT}/src/game/Server/DBCStores.h")
set(_mps "${SOURCE_ROOT}/src/game/WorldHandlers/MapPersistentStateMgr.cpp")
set(_lfg "${SOURCE_ROOT}/src/game/WorldHandlers/LFGMgrProposal.cpp")
set(_omg "${SOURCE_ROOT}/src/game/Object/ObjectMgr.cpp")
foreach(_f "${_dbc}" "${_dbh}" "${_mps}" "${_lfg}" "${_omg}")
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR "missing source: ${_f}")
    endif()
endforeach()

# Both comment forms are stripped, so no assertion can ever be satisfied by prose that
# merely names the thing it checks for. The block-comment pattern is the standard
# non-greedy C form; a plain /\*.*\*/ would swallow everything between the first and
# last comment in the file.
# Takes the NAME of a variable holding the text, never the text itself. A macro
# substitutes its arguments textually, so passing a file body in pastes that body into
# the CMake source and anything in it that looks like syntax -- a quote, a backslash --
# is re-parsed as code. Every source in this tree trips that.
macro(strip_comments _var)
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" ${_var} "${${_var}}")
    string(REGEX REPLACE "//[^\n]*" "" ${_var} "${${_var}}")
endmacro()

file(READ "${_dbc}" _dbc_src)
strip_comments(_dbc_src)
file(READ "${_dbh}" _dbh_src)
strip_comments(_dbh_src)
file(READ "${_mps}" _mps_src)
strip_comments(_mps_src)
file(READ "${_lfg}" _lfg_src)
strip_comments(_lfg_src)
file(READ "${_omg}" _omg_src)
strip_comments(_omg_src)

# ---------------------------------------------------------------------------
# Mutation arms. Each verifies it changed the text it targets and exits 0
# otherwise, so a dead arm surfaces as a WILL_FAIL failure rather than a pass.
# ---------------------------------------------------------------------------
set(_m_dbc "${_dbc_src}")
set(_m_dbh "${_dbh_src}")
set(_m_mps "${_mps_src}")
set(_m_lfg "${_lfg_src}")
set(_m_omg "${_omg_src}")
if(DEFINED MUTATION)
    if(MUTATION STREQUAL "drop_legacy_index")
        string(REPLACE "sMapDifficultyLegacyMap.find(MAKE_PAIR32(mapId, difficulty))"
                       "sMapDifficultyMap.find(MAKE_PAIR32(mapId, difficulty))" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_index_build")
        string(REPLACE "    BuildMapDifficultyLegacyIndex();" "" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "encounter_equality_only")
        # The original regression: compare equal and never consult the fallback chain.
        string(REPLACE "    if (sEncounterExactTiers.find(MAKE_PAIR32(mapId, uint32(difficulty))) != sEncounterExactTiers.end())"
                       "    if (true)" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "encounter_fallback_unconditional")
        # The tempting over-correction: walk the chain even when the map has its own row,
        # which double-credits every boss on the 33 map/tier pairs that carry both.
        string(REPLACE "sEncounterExactTiers.find(MAKE_PAIR32(mapId, uint32(difficulty))) != sEncounterExactTiers.end()"
                       "false" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "encounter_fallback_chain_broken")
        # ClientDifficultyFallback must mirror Difficulty.dbc field 1 in full. The 8 -> 2 link is
        # currently unreachable (challenge mode is not translated), but it is still what the DBC
        # says and the chain walk must not be truncated for whichever tier next needs it.
        string(REPLACE "        case 8:  return 2;" "        case 8:  return 0;" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "encounter_heroic_fallback_broken")
        string(REPLACE "        case 2:  return 1;" "        case 2:  return 0;" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_exact_tier_index_build")
        string(REPLACE "    BuildEncounterExactTierIndex();" "" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "lfg_raid_through_dungeon_setter")
        # The original defect: every LFG group, raid or not, through the dungeon setter.
        string(REPLACE "        pGroup->SetRaidDifficulty(Difficulty(dungeonMode));"
                       "        pGroup->SetDungeonDifficulty(Difficulty(dungeonMode));"
                       _m_lfg "${_lfg_src}")
    elseif(MUTATION STREQUAL "lfg_drops_raid_type_test")
        string(REPLACE "dungeon->TypeID == LFG_TYPE_RAID" "false" _m_lfg "${_lfg_src}")
    elseif(MUTATION STREQUAL "encounter_drops_map_context")
        # Without mapId the predicate cannot know whether an exact row exists.
        string(REPLACE "bool EncounterDifficultyMatches(uint32 mapId, uint32 encounterDifficultyId, Difficulty difficulty)"
                       "bool EncounterDifficultyMatches(uint32 encounterDifficultyId, Difficulty difficulty)"
                       _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_25man_widening")
        string(REPLACE "ToInternalDifficulty(mapDiff->DifficultyID) == 1"
                       "false" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_continent_mapping")
        # Client id 0 is the ONLY id that is also its own internal mode, so dropping it
        # is invisible to every "is the translation applied" check. It would strip all
        # 112 continent, 13 battleground and 7 arena maps out of the index.
        string(REPLACE "case 0:  return 0;" "" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "readd_challenge_mapping")
        # The inverse of the other arms: challenge mode must STAY untranslated, because no
        # challenge map has a bit-2 spawn. Re-adding it lets a player into an empty dungeon.
        string(REPLACE "        case 9:  return 0;"
                       "        case 8:  return 2;\n        case 9:  return 0;"
                       _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_40man_mapping")
        string(REPLACE "case 9:  return 0;" "" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "readd_challenge_spawn_mode")
        # The spawn-mask half of the same inverse.
        #
        # The anchor ENDS at "case 9:" and never spans the newline after it. Comments are stripped
        # before mutation and stripping "// legacy 40-player raids" leaves the run of spaces that
        # separated it, so any anchor continuing past "case 9:" has to reproduce that invisible
        # whitespace exactly. Two drafts of this arm died that way, and the note further down about
        # the same hazard was written after the first.
        string(REPLACE "mode = int32(mapDiff->DifficultyID) - 3;\n                break;\n            case 9:"
                       "mode = int32(mapDiff->DifficultyID) - 3;\n                break;\n            case 8:\n                mode = 2;\n                break;\n            case 9:"
                       _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "shift_dungeon_spawn_mode")
        string(REPLACE "mode = int32(mapDiff->DifficultyID) - 1;"
                       "mode = int32(mapDiff->DifficultyID) - 2;" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "drop_encounter_wildcard")
        # DungeonEncounter id 0 means "any difficulty of this map". 41 maps carry only
        # id 0 rows and 36 of those have more than one tier, so reading it as internal
        # mode 0 stops every heroic run on them from crediting an encounter.
        string(REPLACE "if (encounterDifficultyId == 0)" "if (false)" _m_dbc "${_dbc_src}")
    elseif(MUTATION STREQUAL "scheduler_iterates_raw_map")
        string(REPLACE "MapDifficultyMap const& legacyMap = GetMapDifficultyLegacyMap();"
                       "MapDifficultyMap const& legacyMap = sMapDifficultyMap;" _m_mps "${_mps_src}")
    elseif(MUTATION STREQUAL "reintroduce_client_id_lookup")
        string(REPLACE "GetMapDifficultyData(mapid, difficulty)"
                       "GetMapDifficultyDataByClientId(mapid, difficulty)" _m_mps "${_mps_src}")
    elseif(MUTATION STREQUAL "event_through_raw_lookup")
        string(REPLACE "GetMapDifficultyData(event.mapid, event.difficulty)"
                       "GetMapDifficultyDataByClientId(event.mapid, uint32(event.difficulty))" _m_mps "${_mps_src}")
    elseif(MUTATION STREQUAL "drop_reset_duration_guard")
        # The instance_reset migration. Without it, nine stale raw-id-2 heroic rows on
        # the challenge maps validate as internal mode 2 (challenge, RaidDuration 0),
        # load their old daily timestamp, and are never overwritten because the
        # enumeration skips RaidDuration == 0.
        string(REPLACE " || !resetDiff->RaidDuration" "" _m_mps "${_mps_src}")
    elseif(MUTATION STREQUAL "encounter_raw_compare")
        string(REPLACE "EncounterDifficultyMatches(dbcEntry->MapID, dbcEntry->DifficultyID, GetDifficulty())"
                       "Difficulty(dbcEntry->DifficultyID) == GetDifficulty()" _m_mps "${_mps_src}")
    elseif(MUTATION STREQUAL "encounter_condition_raw_compare")
        string(REPLACE "!EncounterDifficultyMatches(dbcEntry1->MapID, dbcEntry1->DifficultyID, map->GetDifficulty())"
                       "map->GetDifficulty() != Difficulty(dbcEntry1->DifficultyID)" _m_omg "${_omg_src}")
    elseif(MUTATION STREQUAL "lfg_raw_cast")
        string(REPLACE "ToInternalDifficulty(dungeon->DifficultyID)"
                       "int32(dungeon->DifficultyID)" _m_lfg "${_lfg_src}")
    else()
        message(FATAL_ERROR "unknown MUTATION '${MUTATION}'")
    endif()
    if(_m_dbc STREQUAL "${_dbc_src}" AND _m_dbh STREQUAL "${_dbh_src}" AND
       _m_mps STREQUAL "${_mps_src}" AND _m_lfg STREQUAL "${_lfg_src}" AND
       _m_omg STREQUAL "${_omg_src}")
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()
set(_dbc_src "${_m_dbc}")
set(_dbh_src "${_m_dbh}")
set(_mps_src "${_m_mps}")
set(_lfg_src "${_m_lfg}")
set(_omg_src "${_m_omg}")

# Whitespace-collapsed view, so an assertion about a switch arm is not hostage to the
# column its comment used to sit in. Derived AFTER mutation from the same text the other
# assertions read, so there is one pipeline and no before/after skew.
string(REGEX REPLACE "[ \t\r\n]+" " " _dbc_flat "${_dbc_src}")

# ---------------------------------------------------------------------------
# 1. The raw-id accessor must not exist ANYWHERE in the game tree.
#
#    An earlier version scanned three files and called that a ban. The whole point is
#    that a raw id can reappear in a file nobody thought to list, so the scan has to be
#    the tree. Mutated buffers are substituted in for the files under test so the
#    corresponding arms still fire.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE _tree_files "${SOURCE_ROOT}/src/game/*.cpp" "${SOURCE_ROOT}/src/game/*.h")
set(_banned "GetMapDifficultyDataByClientId")
set(_scanned 0)
foreach(_f IN LISTS _tree_files)
    if(NOT _f MATCHES "/tests/")
        if(_f STREQUAL "${_dbc}")
            set(_body "${_dbc_src}")
        elseif(_f STREQUAL "${_dbh}")
            set(_body "${_dbh_src}")
        elseif(_f STREQUAL "${_mps}")
            set(_body "${_mps_src}")
        elseif(_f STREQUAL "${_lfg}")
            set(_body "${_lfg_src}")
        elseif(_f STREQUAL "${_omg}")
            set(_body "${_omg_src}")
        else()
            # Raw, uncommented-stripped, deliberately: a stale mention of the banned
            # symbol in a comment should be cleaned up too, and running the regex over
            # every file in the tree is both slow and needless here.
            file(READ "${_f}" _body)
        endif()
        math(EXPR _scanned "${_scanned} + 1")
        string(FIND "${_body}" "${_banned}" _at)
        if(NOT _at EQUAL -1)
            message(FATAL_ERROR
                "${_banned} is back, in:\n  ${_f}\n\n"
                "Raw client DifficultyIDs must not escape DBCStores.cpp. A caller holding\n"
                "one does not merely read: the reset scheduler also WRITES its key into\n"
                "m_resetTimeByMapDifficulty, `instance_reset` and DungeonResetEvent, all of\n"
                "which are read back with internal modes. Translate at the boundary instead.")
        endif()
    endif()
endforeach()
if(_scanned LESS 200)
    message(FATAL_ERROR
        "The whole-tree ban only scanned ${_scanned} files, which means the glob broke.\n"
        "A ban that reads nothing passes trivially -- exactly the failure it exists to stop.")
endif()

# ---------------------------------------------------------------------------
# 2. GetMapDifficultyData answers from the internal-mode index, which is built.
# ---------------------------------------------------------------------------
string(FIND "${_dbc_src}" "sMapDifficultyLegacyMap.find(MAKE_PAIR32(mapId, difficulty))" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "GetMapDifficultyData no longer answers from the internal-mode index.\n"
        "Reading sMapDifficultyMap here reinterprets internal mode 3 as client id 3\n"
        "and silently selects raid 10-normal while claiming to be 25-heroic.")
endif()

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
        "case 9:  return 0;")
    string(FIND "${_dbc_src}" "${_row}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "ToInternalDifficulty is missing a mapping:\n  ${_row}\n\n"
            "Each row corresponds to a Difficulty.dbc id. A missing one makes that\n"
            "tier silently unenterable. Client id 9 is the legacy 40-player raids and\n"
            "is the only route to a regular tier on four maps.\n\n"
            "Client id 8 (challenge mode) is deliberately NOT in this list -- see the\n"
            "absence check below.")
    endif()
endforeach()

# Challenge mode must stay untranslated, and this is the inverse of every other check here.
#
# All nine challenge maps ship a MapDifficulty id 8 row, so translating it puts internal mode 2 in
# the legacy index and lets a player in. The world database cannot populate that tier: maps
# 959/960/961/962 carry 347/177/433/561 creatures, every one spawnMask 3 (bits 0 and 1), and
# 994/1001/1004/1007/1011 have no creature spawns at all in any mode. Nothing anywhere on those
# maps has bit 2, so the instance instantiates completely empty.
#
# This is subtler than the divergence the checks above guard against. Those verify the two switches
# agree with EACH OTHER; agreeing about a mode the DATA never populates still yields an empty
# dungeon. BuildMapSpawnModeMasks cannot rescue it either -- its output is a validation permission
# mask, not a spawn source.
#
# Re-enable both arms together once
#   SELECT COUNT(*) FROM creature WHERE map IN (959,960,961,962,994,1001,1004,1007,1011)
#     AND (spawnMask & 4) <> 0
# is non-zero, and test an actual challenge map rather than trusting the mask.
# Scoped to each function body on purpose. "case 8: return 2;" is NOT unique in this file --
# ClientDifficultyFallback legitimately contains it, because Difficulty.dbc really does record
# 8 -> 2 -- so a whole-file search for it can never distinguish the two and would either fire
# always or never. An earlier draft of this check searched the whitespace-flattened text for the
# two-space spelling, which matches nothing after flattening: it was vacuous and its arm passed.
macro(_body_of _out _src _signature)
    string(FIND "${_src}" "${_signature}" _fn_at)
    if(_fn_at EQUAL -1)
        message(FATAL_ERROR "cannot locate ${_signature} -- the check below would be vacuous")
    endif()
    string(SUBSTRING "${_src}" ${_fn_at} -1 ${_out})
    string(FIND "${${_out}}" "\n}" _fn_end)
    if(_fn_end EQUAL -1)
        message(FATAL_ERROR "cannot find the end of ${_signature}")
    endif()
    string(SUBSTRING "${${_out}}" 0 ${_fn_end} ${_out})
endmacro()

_body_of(_to_internal_body "${_dbc_src}" "int32 ToInternalDifficulty(uint32 clientDifficultyId)")
string(FIND "${_to_internal_body}" "case 8:" _at)
if(NOT _at EQUAL -1)
    message(FATAL_ERROR
        "ToInternalDifficulty translates challenge mode again.\n\n"
        "The world database has no bit-2 spawn on any of the nine challenge maps, so admitting\n"
        "internal mode 2 lets a player enter a dungeon with nothing in it. Refusing entry is\n"
        "better. If spawn data now exists, remove this check and re-enable BOTH switches together\n"
        "-- enabling one alone is exactly the divergence the checks above exist for.")
endif()

_body_of(_spawn_mask_body "${_dbc_src}" "void BuildMapSpawnModeMasks(std::map<uint32, uint32>& spawnMasks)")
string(FIND "${_spawn_mask_body}" "mode = 2;" _at)
if(NOT _at EQUAL -1)
    message(FATAL_ERROR
        "BuildMapSpawnModeMasks admits challenge mode again -- see the check above. Internal mode 2\n"
        "is still reachable here for raids (client id 5) through the arithmetic arm; only the\n"
        "explicit challenge case must stay absent.")
endif()

# BuildMapSpawnModeMasks must admit the same ids. A tier the difficulty index accepts
# but the spawn mask drops resolves at the area trigger and then instantiates empty.
foreach(_arm
        "mode = int32(mapDiff->DifficultyID) - 1;"
        "mode = int32(mapDiff->DifficultyID) - 3;")
    string(FIND "${_dbc_flat}" "${_arm}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "BuildMapSpawnModeMasks no longer agrees with ToInternalDifficulty:\n  ${_arm}\n\n"
            "The two must map the same client ids to the same internal modes. A player\n"
            "whose tier resolves in the index but not the mask passes the area trigger\n"
            "and arrives in an instance with no spawns filed under that mask bit.")
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
# 4. The reset scheduler enumerates the INTERNAL index, and refuses stale rows.
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

string(FIND "${_mps_src}" "!resetDiff || !resetDiff->RaidDuration" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "The `instance_reset` migration guard is gone.\n\n"
        "Rows written by a build that keyed this table on raw client ids mostly fail the\n"
        "lookup, but nine do not: a raw id 2 row (5-man heroic, 86400s) on a challenge\n"
        "map resolves as internal mode 2, which there is CHALLENGE and carries no global\n"
        "reset. The enumeration skips RaidDuration == 0, so the stale timestamp is never\n"
        "overwritten and a challenge instance inherits an old heroic lockout.")
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

# ---------------------------------------------------------------------------
# 5. The other two DBCs that store raw ids must translate at their boundary.
#    Both of these PERSIST what they compute, so a wrong value outlives the session.
# ---------------------------------------------------------------------------
string(FIND "${_dbc_src}" "if (encounterDifficultyId == 0)" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "EncounterDifficultyMatches lost its wildcard arm.\n\n"
        "DungeonEncounter.dbc id 0 means 'every difficulty of this map', not internal\n"
        "mode 0. 41 maps carry nothing but id 0 rows and 36 of those have more than one\n"
        "tier, so treating it as normal-only stops every heroic run on them from ever\n"
        "crediting an encounter into `instance`.`encountersMask`.")
endif()

string(FIND "${_mps_src}" "EncounterDifficultyMatches(dbcEntry->MapID, dbcEntry->DifficultyID, GetDifficulty())" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "UpdateEncounterState compares DungeonEncounter difficulty directly again.\n"
        "Of 699 shipped rows only the 238 wildcards ever matched that way, and the 264\n"
        "5-man normal rows (id 1) were tested against internal mode 1, which is HEROIC.")
endif()

string(FIND "${_omg_src}" "EncounterDifficultyMatches(dbcEntry1->MapID, dbcEntry1->DifficultyID, map->GetDifficulty())" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "CONDITION_COMPLETED_ENCOUNTER compares DungeonEncounter difficulty directly.\n"
        "It must use the same predicate as UpdateEncounterState or the condition\n"
        "disagrees with the encountersMask it is testing.")
endif()

string(FIND "${_lfg_src}" "ToInternalDifficulty(dungeon->DifficultyID)" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "LFG casts LfgDungeons.DifficultyID straight to Difficulty again.\n\n"
        "That raw id made LFG normal (1) select internal HEROIC and LFG heroic (2)\n"
        "select CHALLENGE, and Group::SetDungeonDifficulty persists it to\n"
        "`groups`.`difficulty` and `characters`.`dungeon_difficulty`.")
endif()

# ---------------------------------------------------------------------------
# Encounter fallback. Difficulty.dbc defines chains (2->1, 5->3, 6->4, 7->4, 8->2, 11->12) and
# some maps tag their encounters only for a lower tier, so pure equality credits nothing there.
# Measured against the shipped DBCs, walking the chain recovers 30 rows across four map/tier
# pairs -- maps 189, 289, 309 and 598 at heroic, 6, 13, 10 and 1 rows, all tagged id 1. An earlier
# revision said 32 across five, the fifth being map 994 at challenge mode; that gain existed only
# while challenge mode was translated, and it no longer is.
#
# The chain must NOT be walked when the map ships its own row for the tier: 33 map/tier pairs
# carry both, and widening them credits every boss twice. That guard is the whole reason the
# predicate needs mapId, so all three are pinned together.
# ---------------------------------------------------------------------------
string(FIND "${_dbc_src}" "bool EncounterDifficultyMatches(uint32 mapId, uint32 encounterDifficultyId, Difficulty difficulty)" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "EncounterDifficultyMatches no longer takes mapId.\n\n"
        "Without the map it cannot tell whether an exact row exists, so it must either never\n"
        "fall back (losing 32 rows on five map/tier pairs) or always fall back (double-crediting\n"
        "33 pairs). Both are worse than the bug this replaced.")
endif()

string(FIND "${_dbc_src}" "sEncounterExactTiers.find(MAKE_PAIR32(mapId, uint32(difficulty))) != sEncounterExactTiers.end()" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "The exact-tier guard is gone from EncounterDifficultyMatches.\n\n"
        "33 map/tier pairs ship BOTH an exact encounter row and a fallback-reachable one.\n"
        "Without this guard every one of those bosses is credited twice.")
endif()

string(FIND "${_dbc_src}" "    BuildEncounterExactTierIndex();" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "BuildEncounterExactTierIndex is never called.\n\n"
        "sEncounterExactTiers stays empty, so the guard above always reads 'no exact row' and\n"
        "the fallback fires everywhere -- the double-credit case, silently.")
endif()

foreach(_chain "        case 2:  return 1;"
               "        case 5:  return 3;"
               "        case 6:  return 4;"
               "        case 8:  return 2;")
    string(FIND "${_dbc_src}" "${_chain}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "A Difficulty.dbc fallback link is missing:\n  ${_chain}\n\n"
            "These mirror field 1 of the shipped Difficulty.dbc. Breaking 8 -> 2 in particular\n"
            "is easy to miss: challenge mode reaches its rows only through the full 8 -> 2 -> 1\n"
            "walk, so truncating it silently drops map 994 and nothing else changes.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# LFG raids must use the raid setter. The two setters persist to different columns
# (`groups`.`difficulty` vs `raiddifficulty`, and the matching `characters` columns), so sending a
# raid through the dungeon setter files the tier in the wrong slot and leaves the right one unset.
# 61 of 343 LfgDungeons rows are TypeID 2, and their tiers reach internal 3 (25-player heroic),
# which is outside the range a dungeon difficulty can hold at all.
# ---------------------------------------------------------------------------
string(FIND "${_lfg_src}" "dungeon->TypeID == LFG_TYPE_RAID" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "LFG no longer distinguishes raids when setting difficulty.\n\n"
        "Without the TypeID test every LFG group goes through SetDungeonDifficulty, so all 61\n"
        "raid rows write their tier to `groups`.`difficulty` and every member's\n"
        "`characters`.`dungeon_difficulty`, leaving the raid columns untouched.")
endif()

string(FIND "${_lfg_src}" "pGroup->SetRaidDifficulty(Difficulty(dungeonMode));" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "LFG raid groups are not using SetRaidDifficulty.\n\n"
        "Raid tiers translate across internal 0..3; 25-player heroic is 3, which no 5-man tier\n"
        "corresponds to, so persisting it as a dungeon difficulty stores a mode the dungeon\n"
        "fields cannot represent.")
endif()

message(STATUS "map difficulty guard: raw-id accessor absent across ${_scanned} tree files, "
               "8 id mappings incl. continents, challenge deliberately absent, spawn masks in step, "
               "25-man widening present, scheduler on the internal index with the "
               "instance_reset migration guard, LFG and DungeonEncounter translated, "
               "encounter fallback chain-walked behind the exact-tier guard")
