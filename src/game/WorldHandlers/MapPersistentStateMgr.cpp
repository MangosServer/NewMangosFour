/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file MapPersistentStateMgr.cpp
 * @brief Instance persistence manager implementation
 *
 * This file implements MapPersistentStateManager which manages
 * instance state persistence for dungeons and raids. Key features:
 *
 * - Instance ID allocation and management
 * - Instance reset timer tracking
 * - Instance binding to groups/players
 * - Instance data persistence to database
 * - Instance cleanup and unloading
 * - Reset event scheduling
 *
 * Instance states are persisted to the `instance` table and restored
 * when players re-enter the instance.
 *
 * @see MapPersistentStateManager for the manager class
 * @see MapPersistentState for individual instance state
 */

#include "MapPersistentStateMgr.h"
#include "LFGMgr.h"

#include "SQLStorages.h"
#include "Player.h"
#include "GridNotifiers.h"
#include "Log.h"
#include "GridStates.h"
#include "CellImpl.h"
#include "Map.h"
#include "MapManager.h"
#include "Timer.h"
#include "GridNotifiersImpl.h"
#include "Transports.h"
#include "ObjectMgr.h"
#include "GameEventMgr.h"
#include "World.h"
#include "Group.h"
#include "InstanceData.h"
#include "ProgressBar.h"

INSTANTIATE_SINGLETON_1(MapPersistentStateManager);

static uint32 resetEventTypeDelay[MAX_RESET_EVENT_TYPE] = { 0,                      // not used
                                                            3600, 900, 300, 60,     // (seconds) normal and official timer delay to inform player about instance reset
                                                            60, 30, 10, 5           // (seconds) fast reset by gm command inform timer
                                                          };

//== MapPersistentState functions ==========================
MapPersistentState::MapPersistentState(uint16 MapId, uint32 InstanceId, Difficulty difficulty)
    : m_instanceid(InstanceId), m_mapid(MapId),
      m_difficulty(difficulty), m_usedByMap(NULL)
{
}

MapPersistentState::~MapPersistentState()
{
}

/**
 * @brief Returns the DBC entry for this persistent state's map.
 *
 * @return The map entry, or null if not found.
 */
MapEntry const* MapPersistentState::GetMapEntry() const
{
    return sMapStore.LookupEntry(m_mapid);
}

/* true if the instance state is still valid */
bool MapPersistentState::UnloadIfEmpty()
{
    if (CanBeUnload())
    {
        sMapPersistentStateMgr.RemovePersistentState(GetMapId(), GetInstanceId());
        return false;
    }
    else
    {
        return true;
    }
}

/**
 * @brief Saves a creature respawn time in memory and in the database.
 *
 * @param loguid The creature spawn guid.
 * @param t The respawn time.
 */
void MapPersistentState::SaveCreatureRespawnTime(uint32 loguid, time_t t)
{
    SetCreatureRespawnTime(loguid, t);

    // BGs/Arenas always reset at server restart/unload, so no reason store in DB
    if (GetMapEntry()->IsBattleGroundOrArena())
    {
        return;
    }

    CharacterDatabase.BeginTransaction();

    static SqlStatementID delSpawnTime ;
    static SqlStatementID insSpawnTime ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delSpawnTime, "DELETE FROM `creature_respawn` WHERE `guid` = ? AND `instance` = ?");
    stmt.PExecute(loguid, m_instanceid);

    if (t > sWorld.GetGameTime())
    {
        stmt = CharacterDatabase.CreateStatement(insSpawnTime, "INSERT INTO `creature_respawn` VALUES ( ?, ?, ? )");
        stmt.PExecute(loguid, uint64(t), m_instanceid);
    }

    CharacterDatabase.CommitTransaction();
}

/**
 * @brief Saves a gameobject respawn time in memory and in the database.
 *
 * @param loguid The gameobject spawn guid.
 * @param t The respawn time.
 */
void MapPersistentState::SaveGORespawnTime(uint32 loguid, time_t t)
{
    SetGORespawnTime(loguid, t);

    // BGs/Arenas always reset at server restart/unload, so no reason store in DB
    if (GetMapEntry()->IsBattleGroundOrArena())
    {
        return;
    }

    CharacterDatabase.BeginTransaction();

    static SqlStatementID delSpawnTime ;
    static SqlStatementID insSpawnTime ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delSpawnTime, "DELETE FROM `gameobject_respawn` WHERE `guid` = ? AND `instance` = ?");
    stmt.PExecute(loguid, m_instanceid);

    if (t > sWorld.GetGameTime())
    {
        stmt = CharacterDatabase.CreateStatement(insSpawnTime, "INSERT INTO `gameobject_respawn` VALUES ( ?, ?, ? )");
        stmt.PExecute(loguid, uint64(t), m_instanceid);
    }

    CharacterDatabase.CommitTransaction();
}

/**
 * @brief Updates the cached creature respawn time for a spawn.
 *
 * @param loguid The creature spawn guid.
 * @param t The respawn time.
 */
void MapPersistentState::SetCreatureRespawnTime(uint32 loguid, time_t t)
{
    if (t > sWorld.GetGameTime())
    {
        m_creatureRespawnTimes[loguid] = t;
    }
    else
    {
        m_creatureRespawnTimes.erase(loguid);
        UnloadIfEmpty();
    }
}

/**
 * @brief Updates the cached gameobject respawn time for a spawn.
 *
 * @param loguid The gameobject spawn guid.
 * @param t The respawn time.
 */
void MapPersistentState::SetGORespawnTime(uint32 loguid, time_t t)
{
    if (t > sWorld.GetGameTime())
    {
        m_goRespawnTimes[loguid] = t;
    }
    else
    {
        m_goRespawnTimes.erase(loguid);
        UnloadIfEmpty();
    }
}

/**
 * @brief Clears all cached respawn times for this state.
 */
void MapPersistentState::ClearRespawnTimes()
{
    m_goRespawnTimes.clear();
    m_creatureRespawnTimes.clear();

    UnloadIfEmpty();
}

/**
 * @brief Registers a creature spawn in the owning grid cache.
 *
 * @param guid The creature spawn guid.
 * @param data The creature spawn data.
 */
void MapPersistentState::AddCreatureToGrid(uint32 guid, CreatureData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    m_gridObjectGuids[cell_id].creatures.insert(guid);
}

/**
 * @brief Removes a creature spawn from the owning grid cache.
 *
 * @param guid The creature spawn guid.
 * @param data The creature spawn data.
 */
void MapPersistentState::RemoveCreatureFromGrid(uint32 guid, CreatureData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    m_gridObjectGuids[cell_id].creatures.erase(guid);
}

/**
 * @brief Registers a gameobject spawn in the owning grid cache.
 *
 * @param guid The gameobject spawn guid.
 * @param data The gameobject spawn data.
 */
void MapPersistentState::AddGameobjectToGrid(uint32 guid, GameObjectData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    m_gridObjectGuids[cell_id].gameobjects.insert(guid);
}

/**
 * @brief Removes a gameobject spawn from the owning grid cache.
 *
 * @param guid The gameobject spawn guid.
 * @param data The gameobject spawn data.
 */
void MapPersistentState::RemoveGameobjectFromGrid(uint32 guid, GameObjectData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    m_gridObjectGuids[cell_id].gameobjects.erase(guid);
}

/**
 * @brief Initializes pool and game-event state for this persistent state.
 */
void MapPersistentState::InitPools()
{
    // pool system initialized already for persistent state (can be shared by map states)
    if (!GetSpawnedPoolData().IsInitialized())
    {
        GetSpawnedPoolData().SetInitialized();
        sPoolMgr.Initialize(this);                          // init pool system data for map persistent state
        sGameEventMgr.Initialize(this);                     // init pool system data for map persistent state
    }
}

//== WorldPersistentState functions ========================
SpawnedPoolData WorldPersistentState::m_sharedSpawnedPoolData;

/**
 * @brief Indicates whether a world persistent state may be unloaded.
 *
 * @return Always false for world states.
 */
bool WorldPersistentState::CanBeUnload() const
{
    // prevent unload if used for loaded map
    // prevent unload if respawn data still exist (will not prevent reset by scheduler)
    // Note: non instanceable Map never unload until server shutdown and in result for loaded non-instanceable maps map persistent states also not unloaded
    //       but for proper work pool systems with shared pools state for non-instanceable maps need
    //       load persistent map states for any non-instanceable maps before Map loading and make sure that it never unloaded
    return /*MapPersistentState::CanBeUnload() && !HasRespawnTimes()*/ false;
}

//== DungeonPersistentState functions =====================

DungeonPersistentState::DungeonPersistentState(uint16 MapId, uint32 InstanceId, Difficulty difficulty, time_t resetTime, bool canReset, uint32 completedEncountersMask)
    : MapPersistentState(MapId, InstanceId, difficulty), m_resetTime(resetTime), m_canReset(canReset), m_completedEncountersMask(completedEncountersMask)
{
}

DungeonPersistentState::~DungeonPersistentState()
{
    DEBUG_LOG("Unloading DungeonPersistantState of map %u instance %u", GetMapId(), GetInstanceId());
    UnbindThisState();
}

/**
 * @brief Unbinds all players and groups from this dungeon state.
 */
void DungeonPersistentState::UnbindThisState()
{
    while (!m_playerList.empty())
    {
        Player* player = *(m_playerList.begin());
        player->UnbindInstance(GetMapId(), GetDifficulty(), true);
    }
    while (!m_groupList.empty())
    {
        Group* group = *(m_groupList.begin());
        group->UnbindInstance(GetMapId(), GetDifficulty(), true);
    }
}

/**
 * @brief Indicates whether a dungeon persistent state may be unloaded.
 *
 * @return true if no bindings or respawn data remain; otherwise false.
 */
bool DungeonPersistentState::CanBeUnload() const
{
    // prevent unload if any bounded groups or online bounded player still exists
    return MapPersistentState::CanBeUnload() && !HasBounds() && !HasRespawnTimes();
}

/*
    Called from AddPersistentState
*/
void DungeonPersistentState::SaveToDB()
{
    // state instance data too
    std::string data;

    if (Map* map = GetMap())
    {
        InstanceData* iData = map->GetInstanceData();
        if (iData && iData->Save())
        {
            data = iData->Save();
            CharacterDatabase.escape_string(data);
        }
    }

    CharacterDatabase.PExecute("INSERT INTO `instance` VALUES ('%u', '%u', '" UI64FMTD "', '%u', '%u', '%s')", GetInstanceId(), GetMapId(), (uint64)GetResetTimeForDB(), GetDifficulty(), GetCompletedEncountersMask(), data.c_str());
}

/**
 * @brief Deletes all saved respawn times for this instance.
 */
void DungeonPersistentState::DeleteRespawnTimes()
{
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `creature_respawn` WHERE `instance` = '%u'", GetInstanceId());
    CharacterDatabase.PExecute("DELETE FROM `gameobject_respawn` WHERE `instance` = '%u'", GetInstanceId());
    CharacterDatabase.CommitTransaction();

    ClearRespawnTimes();                                    // state can be deleted at call if only respawn data prevent unload
}

/**
 * @brief Deletes this instance save from the database.
 */
void DungeonPersistentState::DeleteFromDB()
{
    MapPersistentStateManager::DeleteInstanceFromDB(GetInstanceId());
}

// to cache or not to cache, that is the question
InstanceTemplate const* DungeonPersistentState::GetTemplate() const
{
    return ObjectMgr::GetInstanceTemplate(GetMapId());
}

/**
 * @brief Returns the reset time value that should be stored in the database.
 *
 * @return The persisted reset time, or 0 for raid maps.
 */
time_t DungeonPersistentState::GetResetTimeForDB() const
{
    // only state the reset time for normal instances
    const MapEntry* entry = sMapStore.LookupEntry(GetMapId());
    if (!entry || entry->InstanceType == MAP_RAID || GetDifficulty() == DUNGEON_DIFFICULTY_HEROIC)
    {
        return 0;
    }
    else
    {
        return GetResetTime();
    }
}

void DungeonPersistentState::UpdateEncounterState(EncounterCreditType type, uint32 creditEntry)
{
    DungeonEncounterMapBounds bounds = sObjectMgr.GetDungeonEncounterBounds(creditEntry);

    for (DungeonEncounterMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
    {
        DungeonEncounterEntry const* dbcEntry = iter->second->dbcEntry;

        // DungeonEncounter.dbc holds RAW client DifficultyIDs, so this cannot be a
        // direct comparison against an internal mode -- see EncounterDifficultyMatches.
        if (iter->second->creditType == type && EncounterDifficultyMatches(dbcEntry->MapID, dbcEntry->DifficultyID, GetDifficulty()) && dbcEntry->MapID == GetMapId())
        {
            m_completedEncountersMask |= 1 << dbcEntry->Bit;

            CharacterDatabase.PExecute("UPDATE `instance` SET `encountersMask` = '%u' WHERE `id` = '%u'", m_completedEncountersMask, GetInstanceId());

            DEBUG_LOG("DungeonPersistentState: Dungeon %s (Id %u) completed encounter %s", GetMap()->GetMapName(), GetInstanceId(), dbcEntry->Name_lang[sWorld.GetDefaultDbcLocale()]);

            bool const isLastEncounter = iter->second->lastEncounterDungeon != 0;
            if (isLastEncounter)
            {
                DEBUG_LOG("DungeonPersistentState:: Dungeon %s (Instance-Id %u) completed last encounter %s", GetMap()->GetMapName(), GetInstanceId(), dbcEntry->Name_lang[sWorld.GetDefaultDbcLocale()]);
            }

            // The dungeon finder's only view of run progress. A credited encounter clears
            // the group to leave without Deserter, and the LAST one completes the run --
            // which is what the "Place LFG reward here" note stood for. HandleBossKilled
            // had no caller anywhere in the tree, so no LFG run had ever paid a reward.
            sLFGMgr.OnDungeonEncounterCredited(GetMap(), isLastEncounter);
            return;
        }
    }
}

//== BattleGroundPersistentState functions =================

bool BattleGroundPersistentState::CanBeUnload() const
{
    // prevent unload if used for loaded map
    // BGs/Arenas not locked by respawn data/etc
    return MapPersistentState::CanBeUnload();
}

//== DungeonResetScheduler functions ======================

uint32 DungeonResetScheduler::GetMaxResetTimeFor(MapDifficultyEntry const* mapDiff)
{
    if (!mapDiff || !mapDiff->RaidDuration)
    {
        return 0;
    }

    uint32 delay = uint32(mapDiff->RaidDuration / DAY * sWorld.getConfig(CONFIG_FLOAT_RATE_INSTANCE_RESET_TIME)) * DAY;

    if (delay < DAY)                                        // the reset_delay must be at least one day
    {
        delay = DAY;
    }

    return delay;
}

/**
 * @brief Calculates the next global reset time for an instance template.
 *
 * @param mapDiff The map difficulty entry.
 * @param prevResetTime The previous reset time.
 * @return The next reset timestamp.
 */
time_t DungeonResetScheduler::CalculateNextResetTime(MapDifficultyEntry const* mapDiff, time_t prevResetTime)
{
    uint32 diff = sWorld.getConfig(CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR) * HOUR;
    uint32 period = GetMaxResetTimeFor(mapDiff);
    return ((prevResetTime + MINUTE) / DAY * DAY) + period + diff;
}

/**
 * @brief Reads, migrates and applies the persisted `instance_reset` rows.
 *
 * Split out of LoadResetTimes: the key-space migration below is a self-contained concern
 * carrying a long rationale, and inlining it made that function considerably larger than
 * anything else in this file.
 *
 * @param diff The configured instance reset hour, already converted to seconds.
 */
void DungeonResetScheduler::LoadGlobalResetTimes(uint32 diff)
{
    QueryResult* result = CharacterDatabase.Query("SELECT `mapid`, `difficulty`, `resettime` FROM `instance_reset`");
    if (!result)
    {
        return;
    }

    // `instance_reset`.`difficulty` holds an INTERNAL 0-based mode, the same key space as
    // `instance`.`difficulty`, DungeonPersistentState::GetDifficulty and
    // m_resetTimeByMapDifficulty. The scheduler below writes it from the legacy index.
    //
    // A build that keyed this table on RAW client DifficultyIDs wrote a different key space into
    // the same column, and the two overlap, so this is a migration and not just validation.
    // Row-by-row validation is not sufficient, which an earlier revision of this code got wrong:
    //
    //   * The old scheduler enumerated the raw map and skipped RaidDuration == 0, so it could
    //     write 136 rows across 88 maps, with stored values 2, 3, 4, 5, 6 and 9.
    //   * Read as internal modes, 122 of those are detectable: 4, 5, 6 and 9 exceed
    //     MAX_DIFFICULTY, and raw 2 resolves to internal 2, which since challenge mode stopped
    //     being translated has no legacy-index entry at all. (An earlier comment here claimed
    //     nine raw-2 challenge rows survived and that the RaidDuration test was what caught
    //     them. Both halves are obsolete -- `!resetDiff` already rejects them.)
    //   * 14 are NOT detectable. Raw 3 is raid 10-normal; internal 3 is raid 25-heroic. On the
    //     14 maps carrying both with a global reset, a stale 10-normal timestamp validates
    //     perfectly as a 25-heroic one, reaches SetResetTimeFor, and then suppresses the fresh
    //     initialisation the scheduler below would otherwise do -- because that loop only fills
    //     tiers with no reset time yet. No per-row test can tell those 14 apart from a row we
    //     wrote ourselves; the value is legal in both key spaces.
    //
    // So the table is validated as a WHOLE, but the trigger is narrow, and BOTH halves of that
    // matter. An earlier revision of this code got each of them wrong in turn.
    //
    // The trigger is IsLegacyRawResetKey: the stored (map, value) pair is itself a real,
    // reset-bearing RAW MapDifficulty tier -- which is precisely what the old raw-keyed
    // scheduler wrote, and what this build never writes. It is NOT "value >= MAX_DIFFICULTY",
    // which an earlier revision used and which this comment described for longer than the code
    // did. That older test was both too broad and too narrow: out-of-range is not evidence of
    // the raw key space (a hand edit is out of range too), and it missed raw 2 and 3 entirely.
    //
    // The first attempt was broader still and condemned the table on ANY invalid row. That is
    // far too much -- a map dropped from the DBC, or one hand-edited row, would discard every
    // legitimate reset time, and that is NOT free. The rebuild below computes
    // `today + period + diff`, so wiping a valid row moves that lockout boundary by up to a full
    // reset period. Calling this table "just a cache" was wrong. Non-definitive invalid rows are
    // therefore deleted individually, exactly as before, and leave the rest of the table alone.
    //
    // Note the trigger is strong evidence, not proof: a hand-written row that happens to name a
    // real raw reset-bearing tier would also fire it. That is accepted deliberately. Such a row
    // is indistinguishable from one the old scheduler wrote, and the cost of acting on it is a
    // single rebuild of a table this code can regenerate in full.
    //
    // When the trigger fires, every row is suspect and all of them go, because the 14 ambiguous
    // ones cannot be identified. That is sound for a table the old build wrote in full, and it
    // is measured rather than assumed: re-derived against the shipped MapDifficulty.dbc, the old
    // scheduler wrote 136 rows across 88 maps, of which 122 fail the internal check and every one
    // of those trips IsLegacyRawResetKey by construction. The remaining 14 are the ambiguous
    // raw-3 rows, on 14 maps -- and ZERO of those maps lack a proving row elsewhere in the same
    // table, so the condemnation reaches all of them.
    //
    // Sniffing alone is NOT sufficient, and the durable answer is a version bump rather than
    // anything in this function. DBC co-occurrence is not TABLE co-occurrence: a legacy table can
    // hold an ambiguous raw-3 row with its raw-4/5/6 companions already gone -- after a partial
    // startup, manual cleanup, or one run of an earlier row-by-row version of this very
    // migration, which deleted exactly those detectable rows one at a time. Such a table passes
    // everything below, the stale 10-normal timestamp is applied as 25-heroic, and the rebuild is
    // suppressed. No inspection of row contents can catch it.
    //
    // So CHAR_DB_STRUCTURE_NR is bumped to 2, and the matching characters update deletes this
    // table and advances `db_version`. A version or structure mismatch is FATAL in
    // Database::CheckDatabaseVersion, so an un-migrated database cannot start -- which is the
    // point. Bumping CONTENT instead would not do: a content lag is only a warning there, and
    // the affected database would start silently.
    //
    // The detection below is therefore belt-and-braces for a database that reaches this code
    // anyway, and the log line names the statement to run.
    std::vector<std::pair<uint32 /*mapid*/, std::pair<uint32 /*difficulty*/, uint64 /*resettime*/> > > resetRows;
    bool rawKeySpaceProven = false;

    do
    {
        Field* fields = result->Fetch();

        uint32 mapid            = fields[0].GetUInt32();
        uint32 difficulty       = fields[1].GetUInt32();
        uint64 oldresettime     = fields[2].GetUInt64();

        MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
        MapDifficultyEntry const* resetDiff = difficulty < MAX_DIFFICULTY
            ? GetMapDifficultyData(mapid, Difficulty(difficulty))
            : NULL;

        if (!mapEntry || !mapEntry->IsDungeon() || !resetDiff || !resetDiff->RaidDuration)
        {
            sLog.outError("DungeonResetScheduler::LoadGlobalResetTimes: invalid mapid(%u)/difficulty(%u) pair in instance_reset!", mapid, difficulty);

            if (IsLegacyRawResetKey(mapid, difficulty))
            {
                // This row is exactly what the old raw-keyed scheduler wrote: a raw reset-bearing
                // (map, tier) pair. Flag the table; the row goes with the rest of it below.
                //
                // Not `difficulty >= MAX_DIFFICULTY`, which an earlier revision used. Out of
                // internal range is not evidence of the raw key space -- an arbitrary hand edit
                // is out of range too, and treating that as proof condemns a whole table of valid
                // reset times over one junk row, which is the blast radius this trigger exists to
                // avoid.
                rawKeySpaceProven = true;
            }
            else
            {
                // Invalid for some unrelated reason. Drop just this row, as this code always did.
                CharacterDatabase.DirectPExecute("DELETE FROM `instance_reset` WHERE `mapid` = '%u' AND `difficulty` = '%u'", mapid, difficulty);
            }
            continue;
        }

        resetRows.push_back(std::make_pair(mapid, std::make_pair(difficulty, oldresettime)));
    }
    while (result->NextRow());
    delete result;

    if (rawKeySpaceProven)
    {
        // A stored (map, value) pair that is itself a reset-bearing RAW MapDifficulty tier is
        // what the old raw-keyed scheduler wrote and what this build never writes, so the rows
        // that DID validate cannot be trusted either -- see above -- and none is applied.
        sLog.outString("DungeonResetScheduler::LoadGlobalResetTimes: `instance_reset` holds raw client "
                       "DifficultyIDs; discarding all %u remaining row(s) and rebuilding. Global instance "
                       "lockout boundaries are recomputed once. If this recurs, run "
                       "\"DELETE FROM `instance_reset`;\" once against the characters database.",
                       uint32(resetRows.size()));
        CharacterDatabase.DirectExecute("DELETE FROM `instance_reset`");
        resetRows.clear();
    }

    for (size_t i = 0; i < resetRows.size(); ++i)
    {
        uint32 mapid          = resetRows[i].first;
        uint32 difficulty     = resetRows[i].second.first;
        uint64 oldresettime   = resetRows[i].second.second;

        // update the reset time if the hour in the configs changes
        uint64 newresettime = (oldresettime / DAY) * DAY + diff;
        if (oldresettime != newresettime)
        {
            CharacterDatabase.DirectPExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE `mapid` = '%u' AND `difficulty` = '%u'", newresettime, mapid, difficulty);
        }

        SetResetTimeFor(mapid, Difficulty(difficulty), newresettime);
    }
}

/**
 * @brief Rewrites stale challenge-mode instance rows to heroic, before anything deletes them.
 *
 * `instance`.`difficulty` holds an internal mode, but a build predating the key-space split
 * could put a RAW id there: the old dungeon finder cast an LfgDungeons DifficultyID straight
 * into Difficulty, Group::SetDungeonDifficulty pushed it to the members, and
 * MapManager::CreateDungeonMap then created and saved the instance under it. Raw 2 means 5-man
 * HEROIC, and it was the one value that could get a character into a five-man at all on such a
 * build, because the old raw-keyed lookup accepted it while the default 0 matched nothing.
 *
 * Read as an internal mode, 2 is CHALLENGE, and no ordinary heroic dungeon has a challenge row.
 * Both validators treat that as a corrupt bind and DELETE it -- Player::_LoadBoundInstances at
 * login, and the `instance` sweep below at startup. So the upgrade would quietly destroy
 * `character_instance` rows for exactly the characters that had been able to run dungeons.
 *
 * Rewriting to HEROIC is what the value always meant, and it is unconditionally safe rather than
 * being safe only during an upgrade: challenge mode is unreachable in this core by design --
 * ToInternalDifficulty refuses raw 8, and both load clamps reject internal 2 -- so a dungeon-map
 * instance at internal 2 is a value this build cannot produce, whenever it was written.
 *
 * Raids are deliberately untouched. Internal 2 there is 10-player heroic, a legitimate tier on
 * 14 shipped maps, so the same numeric value is real data and must not be rewritten.
 */
static void NormalizeStaleChallengeInstances()
{
    QueryResult* result = CharacterDatabase.Query(
        "SELECT `id`, `map` FROM `instance` WHERE `difficulty` = 2");
    if (!result)
    {
        return;
    }

    std::vector<uint32> stale;
    do
    {
        Field* fields = result->Fetch();
        uint32 const id = fields[0].GetUInt32();
        uint32 const mapid = fields[1].GetUInt32();

        MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
        if (mapEntry && mapEntry->IsDungeon() && !mapEntry->IsRaid())
        {
            stale.push_back(id);
        }
    }
    while (result->NextRow());
    delete result;

    for (size_t i = 0; i < stale.size(); ++i)
    {
        // `resettime` is cleared in the same statement, and leaving it was a real defect.
        //
        // DungeonPersistentState::GetResetTimeForDB zeroes the column only for raids and for
        // DUNGEON_DIFFICULTY_HEROIC, so a five-man save stored at internal 2 -- neither of
        // those -- was written with a NON-ZERO per-instance reset. Rewriting only the
        // difficulty would leave (heroic, resettime > 0), a combination this code never
        // produces: the first query in LoadResetTimes selects `WHERE resettime > 0`, so the
        // converted save would be scheduled as a RESET_EVENT_NORMAL_DUNGEON and
        // AddPersistentState would honour that stale per-instance boundary instead of the
        // heroic global one.
        //
        // Zero is what GetResetTimeForDB would have written had the save been stored as heroic
        // in the first place, which is the state this rewrite is reconstructing.
        CharacterDatabase.DirectPExecute(
            "UPDATE `instance` SET `difficulty` = '%u', `resettime` = '0' WHERE `id` = '%u'",
            uint32(DUNGEON_DIFFICULTY_HEROIC), stale[i]);

        // Bring the BOUND characters and groups up to heroic with it, or the bind is preserved
        // in a form nothing can reach.
        //
        // A bind is stored under the INSTANCE's tier -- Player::BindToInstance indexes
        // m_boundInstances[state->GetDifficulty()] -- but looked up under the PLAYER's selected
        // tier, in GetBoundInstanceSaveForSelfOrGroup. The characters update clamps a stale
        // selection of 2 to 0, so after this rewrite the instance and its bind sit at 1 while
        // the owner asks at 0. On an ordinary dungeon that has a normal row the lookup succeeds
        // and does NOT fold, so it searches tier 0 and misses the bind entirely: the character
        // is treated as having no state, is relocated out of the instance at load, and can then
        // create a second one for a lockout they already hold.
        //
        // Narrow on purpose. The blanket clamp to NORMAL stays right for everyone else, because
        // a character left at HEROIC cannot enter a normal-only dungeon while the difficulty
        // setter is unregistered. Only a character actually bound to one of these rewritten
        // saves needs heroic, and for them it is not a preference but the tier their lockout
        // lives at.
        //
        // Which owners this actually reaches, measured rather than assumed -- two earlier
        // versions of this comment got the cohort wrong in both directions.
        //
        // REACHED: an ungrouped character of level 70 or more, where the value simply survives
        // login; and a grouped character of 70 or more whose GROUP is bound to the same save,
        // because the `groups` statement below sets that group to heroic and Player::_LoadGroup
        // syncs every member of 70+ from it.
        //
        // NOT REACHED: a character under LEVELREQUIREMENT_HEROIC (70), whom Player::LoadFromDB
        // clamps back to NORMAL; and a character of 70+ whose group is NOT bound to this save,
        // whom _LoadGroup moves to that group's tier instead.
        //
        // Neither miss is corruption. `characters`.`dungeon_difficulty` has exactly one
        // functional reader in the tree -- the clamp itself -- and Player::SaveToDB rewrites the
        // whole row from the in-memory value, so for a stripped cohort this UPDATE is
        // unobservable: nothing can read the heroic value before it is overwritten.
        //
        // The cohort it DOES reach pays a price worth stating. A rescued owner is pinned at
        // HEROIC with no way out on their own, because the difficulty setter is unregistered,
        // and Player::GetAreaTriggerLockStatus derives isRegularTargetMap from the player's own
        // tier -- the fold does not recompute it -- so its level gate refuses them entry to
        // five-mans of any expansion whose cap they have not reached. One group join clears it.
        //
        // The sub-70 case is a real gap, left open on scope. Be exact about why, because two
        // earlier versions of this comment were not:
        //
        //   Rejoining the group does NOT make the bind reachable. Group::GetBoundInstance keys
        //   its lookup on the PLAYER's tier, not the group's, so both legs of
        //   GetBoundInstanceSaveForSelfOrGroup miss for a sub-70 sitting at NORMAL. Neither fold
        //   rescues it either: 64 five-man maps carry both raw 1 and raw 2, so the NORMAL lookup
        //   succeeds and no fold fires. MapManager then resolves at the player's tier and
        //   CREATES at the group's, i.e. a duplicate instance, and DungeonMap::Add re-keys on the
        //   map's tier and reaches a MANGOS_ASSERT -- which in a Release build is a log line, so
        //   the player is silently added to the duplicate.
        //
        // That is a pre-existing core defect this rescue re-exposes for one upgrade cohort, not
        // one it invents, and the fix is a one-liner in Group::GetBoundInstance rather than
        // anything here. It is deferred because it wants a live check first: a sub-70 in a 70+
        // heroic group must land in the SAME instance id as the group.
        //
        // Do NOT "fix" it by teaching the login clamps that a bind-backed difficulty outranks the
        // level clamp, which an earlier version of this comment proposed. `dungeon_difficulty` is
        // a single global per-character selector, so raising a sub-70 to HEROIC to reach one
        // lockout trips that same area-trigger gate on every five-man of the expansion and locks
        // them out of normal dungeons they can run today. That is a regression, not a fix.
        CharacterDatabase.DirectPExecute(
            "UPDATE `characters` SET `dungeon_difficulty` = '%u' WHERE `guid` IN "
            "(SELECT `guid` FROM `character_instance` WHERE `instance` = '%u')",
            uint32(DUNGEON_DIFFICULTY_HEROIC), stale[i]);

        // Joined on leaderGuid, NOT groupId: `group_instance` keys on the leader, and
        // ObjectMgrInstanceData's own loader joins `groups`.`leaderGUID` to
        // `group_instance`.`leaderGUID`.
        CharacterDatabase.DirectPExecute(
            "UPDATE `groups` SET `difficulty` = '%u' WHERE `leaderGuid` IN "
            "(SELECT `leaderGuid` FROM `group_instance` WHERE `instance` = '%u')",
            uint32(DUNGEON_DIFFICULTY_HEROIC), stale[i]);
    }

    if (!stale.empty())
    {
        sLog.outString("MapPersistentStateManager: rewrote %u five-man instance save(s) from the "
                       "unreachable challenge mode to heroic, which is what the stored raw id "
                       "meant. Their character and group binds are preserved.",
                       uint32(stale.size()));
    }
}

/**
 * @brief Loads and schedules persisted dungeon reset times.
 */
void DungeonResetScheduler::LoadResetTimes()
{
    time_t now = time(NULL);
    time_t today = (now / DAY) * DAY;
    time_t nextWeek = today + (7 * DAY);

    // Must run before the `instance` sweep below and before any character logs in, because both
    // validators DELETE a bind whose difficulty has no MapDifficulty row.
    NormalizeStaleChallengeInstances();

    // NOTE: Use DirectPExecute for tables that will be queried later

    // get the current reset times for normal instances (these may need to be updated)
    // these are only kept in memory for InstanceSaves that are loaded later
    // resettime = 0 in the DB for raid/heroic instances so those are skipped
    typedef std::pair < uint32 /*PAIR32(map,difficulty)*/, time_t > ResetTimeMapDiffType;
    typedef std::map<uint32, ResetTimeMapDiffType> InstResetTimeMapDiffType;
    InstResetTimeMapDiffType instResetTime;

    QueryResult* result = CharacterDatabase.Query("SELECT `id`, `map`, `difficulty`, `resettime` FROM `instance` WHERE `resettime` > 0");
    if (result)
    {
        do
        {
            if (time_t resettime = time_t((*result)[3].GetUInt64()))
            {
                uint32 id = (*result)[0].GetUInt32();
                uint32 mapid = (*result)[1].GetUInt32();
                uint32 difficulty = (*result)[2].GetUInt32();

                MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);

                // `instance`.`difficulty` holds an internal 0-based mode, not a raw
                // client id, so this one stays on the internal lookup.
                if (!mapEntry || !mapEntry->IsDungeon() || !GetMapDifficultyData(mapid, Difficulty(difficulty)))
                {
                    sMapPersistentStateMgr.DeleteInstanceFromDB(id);
                    continue;
                }

                instResetTime[id] = ResetTimeMapDiffType(MAKE_PAIR32(mapid, difficulty), resettime);
            }
        }
        while (result->NextRow());
        delete result;

        // update reset time for normal instances with the max creature respawn time + X hours
        result = CharacterDatabase.Query("SELECT MAX(`respawntime`), `instance` FROM `creature_respawn` WHERE `instance` > 0 GROUP BY `instance`");
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();

                time_t resettime    = time_t(fields[0].GetUInt64() + 2 * HOUR);
                uint32 instance     = fields[1].GetUInt32();

                InstResetTimeMapDiffType::iterator itr = instResetTime.find(instance);
                if (itr != instResetTime.end() && itr->second.second != resettime)
                {
                    CharacterDatabase.DirectPExecute("UPDATE `instance` SET `resettime` = '" UI64FMTD "' WHERE `id` = '%u'", uint64(resettime), instance);
                    itr->second.second = resettime;
                }
            }
            while (result->NextRow());
            delete result;
        }

        // schedule the reset times
        for (InstResetTimeMapDiffType::iterator itr = instResetTime.begin(); itr != instResetTime.end(); ++itr)
            if (itr->second.second > now)
            {
                ScheduleReset(true, itr->second.second, DungeonResetEvent(RESET_EVENT_NORMAL_DUNGEON, PAIR32_LOPART(itr->second.first), Difficulty(PAIR32_HIPART(itr->second.first)), itr->first));
            }
    }

    // load the global respawn times for raid/heroic instances
    uint32 diff = sWorld.getConfig(CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR) * HOUR;
    LoadGlobalResetTimes(diff);

    // clean expired instances, references to them will be deleted in CleanupInstances
    // must be done before calculating new reset times
    m_InstanceSaves._CleanupExpiredInstancesAtTime(now);

    // calculate new global reset times for expired instances and those that have never been reset yet
    // add the global reset times to the priority queue
    // Iterates the INTERNAL-mode index, not the raw sMapDifficultyMap. Everything this
    // loop writes -- m_resetTimeByMapDifficulty, `instance_reset` and the scheduled
    // DungeonResetEvents -- is read back elsewhere with an internal mode: by
    // AddPersistentState, by MovementHandler's reset warning and by the
    // DungeonPersistentState::GetDifficulty() comparison in _ResetOrWarnAll. Keying it
    // on raw client ids made all three wrong, because no raw id except 0 equals its own
    // internal mode: 122 of the 136 reset-bearing tiers missed the table outright and
    // the remaining 14 (internal 3 against raw id 3) picked raid 10-normal's row while
    // claiming to be 25-heroic.
    //
    // 136, not the 143 an earlier revision of this comment gave. That count was measured
    // while BuildMapDifficultyLegacyIndex left the seven 25-player-only TBC raids holding
    // two keys apiece, so it counted this loop's own duplicate rows as though they were
    // real tiers. The widening is a move rather than a copy now, and 136 is the number of
    // physical lockouts that actually exist.
    MapDifficultyMap const& legacyMap = GetMapDifficultyLegacyMap();
    for (MapDifficultyMap::const_iterator itr = legacyMap.begin(); itr != legacyMap.end(); ++itr)
    {
        uint32 map_diff_pair = itr->first;
        uint32 mapid = PAIR32_LOPART(map_diff_pair);
        Difficulty difficulty = Difficulty(PAIR32_HIPART(map_diff_pair));
        MapDifficultyEntry const* mapDiff = itr->second;

        // skip mapDiff without global reset time
        if (!mapDiff->RaidDuration)
        {
            continue;
        }

        // only raid/heroic maps have a global reset time
        MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
        if (!mapEntry || !mapEntry->IsDungeon())
        {
            continue;
        }

        uint32 period = GetMaxResetTimeFor(mapDiff);
        time_t t = GetResetTimeFor(mapid, difficulty);
        if (!t)
        {
            // initialize the reset time
            t = today + period + diff;
            CharacterDatabase.DirectPExecute("INSERT INTO `instance_reset` VALUES ('%u','%u','" UI64FMTD "')", mapid, difficulty, (uint64)t);
        }

        if (t < now || t > nextWeek)
        {
            // assume that expired instances have already been cleaned
            // calculate the next reset time
            t = (t / DAY) * DAY;
            t += ((today - t) / period + 1) * period + diff;
            CharacterDatabase.DirectPExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE mapid = '%u' AND difficulty= '%u'", (uint64)t, mapid, difficulty);
        }

        SetResetTimeFor(mapid, difficulty, t);

        // schedule the global reset/warning
        ResetEventType type = RESET_EVENT_INFORM_1;
        for (; type < RESET_EVENT_INFORM_LAST; type = ResetEventType(type + 1))
            if (t > time_t(now + resetEventTypeDelay[type]))
            {
                break;
            }

        ScheduleReset(true, t - resetEventTypeDelay[type], DungeonResetEvent(type, mapid, difficulty, 0));
    }
}

/**
 * @brief Adds or removes a dungeon reset event from the scheduler.
 *
 * @param add True to add the event, false to cancel it.
 * @param time The event time.
 * @param event The event descriptor.
 */
void DungeonResetScheduler::ScheduleReset(bool add, time_t time, DungeonResetEvent event)
{
    if (add)
    {
        m_resetTimeQueue.insert(std::pair<time_t, DungeonResetEvent>(time, event));
    }
    else
    {
        // find the event in the queue and remove it
        ResetTimeQueue::iterator itr;
        std::pair<ResetTimeQueue::iterator, ResetTimeQueue::iterator> range;
        range = m_resetTimeQueue.equal_range(time);
        for (itr = range.first; itr != range.second; ++itr)
        {
            if (itr->second == event)
            {
                m_resetTimeQueue.erase(itr);
                return;
            }
        }
        // in case the reset time changed (should happen very rarely), we search the whole queue
        if (itr == range.second)
        {
            for (itr = m_resetTimeQueue.begin(); itr != m_resetTimeQueue.end(); ++itr)
            {
                if (itr->second == event)
                {
                    m_resetTimeQueue.erase(itr);
                    return;
                }
            }

            if (itr == m_resetTimeQueue.end())
            {
                sLog.outError("DungeonResetScheduler::ScheduleReset: cannot cancel the reset, the event(%d,%d,%d) was not found!", event.type, event.mapid, event.instanceId);
            }
        }
    }
}

/**
 * @brief Processes due dungeon reset and warning events.
 */
void DungeonResetScheduler::Update()
{
    time_t now = time(NULL), t;
    while (!m_resetTimeQueue.empty() && (t = m_resetTimeQueue.begin()->first) < now)
    {
        DungeonResetEvent& event = m_resetTimeQueue.begin()->second;
        if (event.type == RESET_EVENT_NORMAL_DUNGEON)
        {
            // for individual normal instances, max creature respawn + X hours
            m_InstanceSaves._ResetInstance(event.mapid, event.instanceId);
        }
        else
        {
            // global reset/warning for a certain map
            time_t resetTime = GetResetTimeFor(event.mapid, event.difficulty);
            uint32 timeLeft = uint32(std::max(int32(resetTime - now), 0));
            bool warn = event.type != RESET_EVENT_INFORM_LAST && event.type != RESET_EVENT_FORCED_INFORM_LAST;
            m_InstanceSaves._ResetOrWarnAll(event.mapid, event.difficulty, warn, timeLeft);
            if (event.type != RESET_EVENT_INFORM_LAST && event.type != RESET_EVENT_FORCED_INFORM_LAST)
            {
                // schedule the next warning/reset
                event.type = ResetEventType(event.type + 1);
                ScheduleReset(true, resetTime - resetEventTypeDelay[event.type], event);
            }
            else
            {
                // re-schedule the next/new global reset/warning
                // calculate the next reset time
                // DungeonResetEvent carries an INTERNAL mode, taken from the legacy
                // index key when the event was scheduled -- and the same key space the
                // RESET_EVENT_NORMAL_DUNGEON events built in AddPersistentState use.
                MapDifficultyEntry const* mapDiff = GetMapDifficultyData(event.mapid, event.difficulty);
                MANGOS_ASSERT(mapDiff);

                time_t next_reset = DungeonResetScheduler::CalculateNextResetTime(mapDiff, resetTime);

                CharacterDatabase.DirectPExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE `mapid` = '%u' AND `difficulty` = '%u'", uint64(next_reset), uint32(event.mapid), uint32(event.difficulty));

                SetResetTimeFor(event.mapid, event.difficulty, next_reset);

                ResetEventType type = RESET_EVENT_INFORM_1;
                for (; type < RESET_EVENT_INFORM_LAST; type = ResetEventType(type + 1))
                    if (next_reset > time_t(now + resetEventTypeDelay[type]))
                    {
                        break;
                    }

                // add new scheduler event to the queue
                event.type = type;
                ScheduleReset(true, next_reset - resetEventTypeDelay[event.type], event);
            }
        }
        m_resetTimeQueue.erase(m_resetTimeQueue.begin());
    }
}

/**
 * @brief Forces all raid reset events to restart from the forced warning sequence.
 */
void DungeonResetScheduler::ResetAllRaid()
{
    time_t now = time(NULL);
    ResetTimeQueue rTQ;
    rTQ.clear();

    time_t timeleft = resetEventTypeDelay[RESET_EVENT_FORCED_INFORM_1];

    for (ResetTimeQueue::iterator itr = m_resetTimeQueue.begin(); itr != m_resetTimeQueue.end(); ++itr)
    {
        DungeonResetEvent& event = itr->second;

        // we only reset raid dungeon
        if (event.type == RESET_EVENT_NORMAL_DUNGEON)
        {
            rTQ.insert(std::pair<time_t, DungeonResetEvent>(itr->first, event));
            continue;
        }
        event.type = RESET_EVENT_FORCED_INFORM_1;
        time_t next_reset = now + timeleft;
        SetResetTimeFor(event.mapid, event.difficulty, next_reset);
        rTQ.insert(std::pair<time_t, DungeonResetEvent>(now, event));
    }
    m_resetTimeQueue = rTQ;
}

//== MapPersistentStateManager functions =========================

MapPersistentStateManager::MapPersistentStateManager() : lock_instLists(false), m_Scheduler(*this)
{
}

MapPersistentStateManager::~MapPersistentStateManager()
{
    // it is undefined whether this or objectmgr will be unloaded first
    // so we must be prepared for both cases
    lock_instLists = true;
    for (PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.begin(); itr != m_instanceSaveByInstanceId.end(); ++itr)
    {
        delete  itr->second;
    }
    for (PersistentStateMap::iterator itr = m_instanceSaveByMapId.begin(); itr != m_instanceSaveByMapId.end(); ++itr)
    {
        delete  itr->second;
    }
}

/*
- adding instance into manager
- called from DungeonMap::Add, _LoadBoundInstances, LoadGroups
*/
MapPersistentState* MapPersistentStateManager::AddPersistentState(MapEntry const* mapEntry, uint32 instanceId, Difficulty difficulty, time_t resetTime, bool canReset, bool load /*=false*/, bool initPools /*= true*/, uint32 completedEncountersMask /*= 0*/)
{
    if (MapPersistentState* old_save = GetPersistentState(mapEntry->ID, instanceId))
    {
        return old_save;
    }

    if (mapEntry->IsDungeon())
    {
        if (!resetTime)
        {
            // initialize reset time
            // for normal instances if no creatures are killed the instance will reset in two hours
            if (mapEntry->InstanceType == MAP_RAID || difficulty > DUNGEON_DIFFICULTY_NORMAL)
            {
                resetTime = m_Scheduler.GetResetTimeFor(mapEntry->ID, difficulty);
            }
            else
            {
                resetTime = time(NULL) + 2 * HOUR;
                // normally this will be removed soon after in DungeonMap::Add, prevent error
                m_Scheduler.ScheduleReset(true, resetTime, DungeonResetEvent(RESET_EVENT_NORMAL_DUNGEON, mapEntry->ID, difficulty, instanceId));
            }
        }
    }

    DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "MapPersistentStateManager::AddPersistentState: mapid = %d, instanceid = %d, reset time = '" UI64FMTD "', canRset = %u", mapEntry->ID, instanceId, uint64(resetTime), canReset ? 1 : 0);

    MapPersistentState* state;
    if (mapEntry->IsDungeon())
    {
        DungeonPersistentState* dungeonState = new DungeonPersistentState(mapEntry->ID, instanceId, difficulty, resetTime, canReset, completedEncountersMask);
        if (!load)
        {
            dungeonState->SaveToDB();
        }
        state = dungeonState;
    }
    else if (mapEntry->IsBattleGroundOrArena())
    {
        state = new BattleGroundPersistentState(mapEntry->ID, instanceId, difficulty);
    }
    else
    {
        state = new WorldPersistentState(mapEntry->ID);
    }


    if (instanceId)
    {
        m_instanceSaveByInstanceId[instanceId] = state;
    }
    else
    {
        m_instanceSaveByMapId[mapEntry->ID] = state;
    }

    if (initPools)
    {
        state->InitPools();
    }

    return state;
}

/**
 * @brief Retrieves a persistent state by map and instance id.
 *
 * @param mapId The map id.
 * @param instanceId The instance id.
 * @return The matching persistent state, or null if none exists.
 */
MapPersistentState* MapPersistentStateManager::GetPersistentState(uint32 mapId, uint32 instanceId)
{
    if (instanceId)
    {
        PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.find(instanceId);
        return itr != m_instanceSaveByInstanceId.end() ? itr->second : NULL;
    }
    else
    {
        PersistentStateMap::iterator itr = m_instanceSaveByMapId.find(mapId);
        return itr != m_instanceSaveByMapId.end() ? itr->second : NULL;
    }
}

/**
 * @brief Deletes all database records associated with an instance id.
 *
 * @param instanceid The instance id to delete.
 */
void MapPersistentStateManager::DeleteInstanceFromDB(uint32 instanceid)
{
    if (instanceid)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `instance` WHERE `id` = '%u'", instanceid);
        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `instance` = '%u'", instanceid);
        CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `instance` = '%u'", instanceid);
        CharacterDatabase.PExecute("DELETE FROM `creature_respawn` WHERE `instance` = '%u'", instanceid);
        CharacterDatabase.PExecute("DELETE FROM `gameobject_respawn` WHERE `instance` = '%u'", instanceid);
        CharacterDatabase.CommitTransaction();
    }
}

/**
 * @brief Removes a persistent state from the manager and persists final data if needed.
 *
 * @param mapId The map id for non-instance states.
 * @param instanceId The instance id for instanced states.
 */
void MapPersistentStateManager::RemovePersistentState(uint32 mapId, uint32 instanceId)
{
    if (lock_instLists)
    {
        return;
    }

    if (instanceId)
    {
        PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.find(instanceId);
        if (itr != m_instanceSaveByInstanceId.end())
        {
            // state the resettime for normal instances only when they get unloaded
            if (itr->second->GetMapEntry()->IsDungeon())
                if (time_t resettime = ((DungeonPersistentState*)itr->second)->GetResetTimeForDB())
                {
                    CharacterDatabase.PExecute("UPDATE `instance` SET `resettime` = '" UI64FMTD "' WHERE `id` = '%u'", (uint64)resettime, instanceId);
                }

            _ResetSave(m_instanceSaveByInstanceId, itr);
        }
    }
    else
    {
        PersistentStateMap::iterator itr = m_instanceSaveByMapId.find(mapId);
        if (itr != m_instanceSaveByMapId.end())
        {
            _ResetSave(m_instanceSaveByMapId, itr);
        }
    }
}

/**
 * @brief Deletes rows selected by a query tail from a table.
 *
 * @param db The database connection.
 * @param fields The field list used to build delete predicates.
 * @param table The table name.
 * @param queryTail The trailing query clause.
 */
void MapPersistentStateManager::_DelHelper(DatabaseType& db, const char* fields, const char* table, const char* queryTail, ...)
{
    Tokens fieldTokens = StrSplit(fields, ", ");
    MANGOS_ASSERT(fieldTokens.size() != 0);

    va_list ap;
    char szQueryTail [MAX_QUERY_LEN];
    va_start(ap, queryTail);
    vsnprintf(szQueryTail, MAX_QUERY_LEN, queryTail, ap);
    va_end(ap);

    // query is delimited in input
    QueryResult* result = db.PQuery("SELECT %s FROM %s %s", fields, table, szQueryTail);
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            std::ostringstream ss;
            for (size_t i = 0; i < fieldTokens.size(); ++i)
            {
                std::string fieldValue = fields[i].GetCppString();
                db.escape_string(fieldValue);
                ss << (i != 0 ? " AND " : "") << fieldTokens[i] << " = '" << fieldValue << "'";
            }
            db.PExecute("DELETE FROM %s WHERE %s", table, ss.str().c_str());
        }
        while (result->NextRow());
        delete result;
    }
}

/**
 * @brief Cleans invalid instance bindings and orphaned respawn data from the database.
 */
void MapPersistentStateManager::CleanupInstances()
{
    BarGoLink bar(2);
    bar.step();

    // load reset times and clean expired instances
    m_Scheduler.LoadResetTimes();

    CharacterDatabase.BeginTransaction();
    sLog.outString("|>  Clean character/group - instance binds with invalid group/characters...");
    // clean character/group - instance binds with invalid group/characters
    _DelHelper(CharacterDatabase, "`character_instance`.`guid`, `instance`", "`character_instance`", "LEFT JOIN `characters` ON `character_instance`.`guid` = `characters`.`guid` WHERE `characters`.`guid` IS NULL");
    _DelHelper(CharacterDatabase, "`group_instance`.`leaderGuid`, `instance`", "`group_instance`", "LEFT JOIN `characters` ON `group_instance`.`leaderGuid` = `characters`.`guid` LEFT JOIN `groups` ON `group_instance`.`leaderGuid` = `groups`.`leaderGuid` WHERE `characters`.`guid` IS NULL OR `groups`.`leaderGuid` IS NULL");

    sLog.outString("|>  Clean instances that do not have any players or groups bound to them...");
    // clean instances that do not have any players or groups bound to them
    _DelHelper(CharacterDatabase, "`id`, `map`, `difficulty`", "`instance`", "LEFT JOIN `character_instance` ON `character_instance`.`instance` = `id` LEFT JOIN `group_instance` ON `group_instance`.`instance` = `id` WHERE `character_instance`.`instance` IS NULL AND `group_instance`.`instance` IS NULL");

    sLog.outString("|>  Clean invalid instance references in other tables...");
    // clean invalid instance references in other tables
    _DelHelper(CharacterDatabase, "`character_instance`.`guid`, `instance`", "`character_instance`", "LEFT JOIN `instance` ON `character_instance`.`instance` = `instance`.`id` WHERE `instance`.`id` IS NULL");
    _DelHelper(CharacterDatabase, "`group_instance`.`leaderGuid`, `instance`", "`group_instance`", "LEFT JOIN `instance` ON `group_instance`.`instance` = `instance`.`id` WHERE `instance`.`id` IS NULL");

    sLog.outString("|>  Clean unused respawn data...");
    // clean unused respawn data
    CharacterDatabase.Execute("DELETE FROM `creature_respawn` WHERE `instance` <> 0 AND `instance` NOT IN (SELECT `id` FROM `instance`)");
    CharacterDatabase.Execute("DELETE FROM `gameobject_respawn` WHERE `instance` <> 0 AND `instance` NOT IN (SELECT `id` FROM `instance`)");
    // execute transaction directly
    CharacterDatabase.CommitTransaction();

    bar.step();
    sLog.outString();
    sLog.outString(">> Instances cleaned up");
}

/**
 * @brief Renumbers instance ids to a compact contiguous range.
 */
void MapPersistentStateManager::PackInstances()
{
    // this routine renumbers player instance associations in such a way so they start from 1 and go up
    // TODO: this can be done a LOT more efficiently

    // obtain set of all associations
    std::set<uint32> InstanceSet;

    // all valid ids are in the instance table
    // any associations to ids not in this table are assumed to be
    // cleaned already in CleanupInstances
    QueryResult* result = CharacterDatabase.Query("SELECT `id` FROM `instance`");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            InstanceSet.insert(fields[0].GetUInt32());
        }
        while (result->NextRow());
        delete result;
    }

    BarGoLink bar(InstanceSet.size() + 1);
    bar.step();

    uint32 InstanceNumber = 1;
    // we do assume std::set is sorted properly on integer value
    for (std::set<uint32>::iterator i = InstanceSet.begin(); i != InstanceSet.end(); ++i)
    {
        if (*i != InstanceNumber)
        {
            CharacterDatabase.BeginTransaction();
            // remap instance id
            CharacterDatabase.PExecute("UPDATE `creature_respawn` SET `instance` = '%u' WHERE `instance` = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE `gameobject_respawn` SET `instance` = '%u' WHERE `instance` = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE `corpse` SET `instance` = '%u' WHERE `instance` = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE `character_instance` SET `instance` = '%u' WHERE `instance` = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE `instance` SET `id` = '%u' WHERE `id` = '%u'", InstanceNumber, *i);
            CharacterDatabase.PExecute("UPDATE `group_instance` SET `instance` = '%u' WHERE `instance` = '%u'", InstanceNumber, *i);
            // execute transaction synchronously
            CharacterDatabase.CommitTransaction();
        }

        ++InstanceNumber;
        bar.step();
    }

    sLog.outString(">> Instance numbers remapped, next instance id is %u", InstanceNumber);
    sLog.outString();
}

/**
 * @brief Deletes and erases a persistent state iterator from a holder.
 *
 * @param holder The state container.
 * @param itr The iterator to remove.
 */
void MapPersistentStateManager::_ResetSave(PersistentStateMap& holder, PersistentStateMap::iterator& itr)
{
    // unbind all players bound to the instance
    // do not allow UnbindInstance to automatically unload the InstanceSaves
    lock_instLists = true;
    delete itr->second;
    holder.erase(itr++);
    lock_instLists = false;
}

/**
 * @brief Resets a single instance state and removes its saved data.
 *
 * @param mapid The map id.
 * @param instanceId The instance id.
 */
void MapPersistentStateManager::_ResetInstance(uint32 mapid, uint32 instanceId)
{
    DEBUG_LOG("MapPersistentStateManager::_ResetInstance %u, %u", mapid, instanceId);

    PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.find(instanceId);
    if (itr != m_instanceSaveByInstanceId.end())
    {
        // delay reset until map unload for loaded map
        if (Map* iMap = itr->second->GetMap())
        {
            MANGOS_ASSERT(iMap->IsDungeon());

            ((DungeonMap*)iMap)->Reset(INSTANCE_RESET_RESPAWN_DELAY);
            return;
        }

        _ResetSave(m_instanceSaveByInstanceId, itr);
    }


    DeleteInstanceFromDB(instanceId);                       // even if state not loaded
}

struct MapPersistantStateResetWorker
{
    MapPersistantStateResetWorker() {};
    void operator()(Map* map)
    {
        ((DungeonMap*)map)->TeleportAllPlayersTo(TELEPORT_LOCATION_HOMEBIND);
        ((DungeonMap*)map)->Reset(INSTANCE_RESET_GLOBAL);
    }
};

struct MapPersistantStateWarnWorker
{
    MapPersistantStateWarnWorker(time_t _timeLeft) : timeLeft(_timeLeft)
    {};

    void operator()(Map* map)
    {
        ((DungeonMap*)map)->SendResetWarnings(timeLeft);
    }

    time_t timeLeft;
};

/**
 * @brief Resets or warns all instances for a map.
 *
 * @param mapid The map id.
 * @param warn True to send warnings instead of resetting.
 * @param timeLeft Seconds remaining until reset.
 */
void MapPersistentStateManager::_ResetOrWarnAll(uint32 mapid, Difficulty difficulty, bool warn, uint32 timeLeft)
{
    // global reset for all instances of the given map
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry->IsDungeon())
    {
        return;
    }

    time_t now = time(NULL);

    if (!warn)
    {
        // 'difficulty' is an INTERNAL mode, which is also what the
        // GetDifficulty() comparison below needs: DungeonPersistentState holds
        // internal modes, so a raw id here matched the wrong tier or none at all.
        MapDifficultyEntry const* mapDiff = GetMapDifficultyData(mapid, difficulty);
        if (!mapDiff || !mapDiff->RaidDuration)
        {
            sLog.outError("MapPersistentStateManager::ResetOrWarnAll: not valid difficulty or no reset delay for map %d", mapid);
            return;
        }

        // remove all binds for online player
        std::list<DungeonPersistentState *> unbindList;

        // note that we must build a list of states to unbind and then unbind them in two steps.  this is because the unbinding may
        // trigger the modification of the collection, which would invalidate the iterator and cause a crash.
        for (PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.begin(); itr != m_instanceSaveByInstanceId.end(); ++itr)
            if (itr->second->GetMapId() == mapid && itr->second->GetDifficulty() == difficulty)
            {
                unbindList.push_back((DungeonPersistentState *)itr->second);
            }

        for (auto i : unbindList)
        {
            i->UnbindThisState();
        }

        // reset maps, teleport player automaticaly to their homebinds and unload maps
        MapPersistantStateResetWorker worker;
        sMapMgr.DoForAllMapsWithMapId(mapid, worker);

        // delete them from the DB, even if not loaded
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `character_instance` USING `character_instance` LEFT JOIN `instance` ON `character_instance`.`instance` = `id` WHERE `map` = '%u'", mapid);
        CharacterDatabase.PExecute("DELETE FROM `group_instance` USING `group_instance` LEFT JOIN `instance` ON `group_instance`.`instance` = `id` WHERE `map` = '%u'", mapid);
        CharacterDatabase.PExecute("DELETE FROM `instance` WHERE `map` = '%u'", mapid);
        CharacterDatabase.CommitTransaction();

        // calculate the next reset time
        time_t next_reset = DungeonResetScheduler::CalculateNextResetTime(mapDiff, now + timeLeft);
        // update it in the DB
        CharacterDatabase.PExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE `mapid` = '%u' AND `difficulty` = '%u'", (uint64)next_reset, mapid, difficulty);
        return;
    }

    // note: this isn't fast but it's meant to be executed very rarely
    MapPersistantStateWarnWorker worker(timeLeft);
    sMapMgr.DoForAllMapsWithMapId(mapid, worker);
}

/**
 * @brief Collects statistics about loaded instance states and bindings.
 *
 * @param numStates Receives the number of dungeon states.
 * @param numBoundPlayers Receives the number of bound players.
 * @param numBoundGroups Receives the number of bound groups.
 */
void MapPersistentStateManager::GetStatistics(uint32& numStates, uint32& numBoundPlayers, uint32& numBoundGroups)
{
    numStates = 0;
    numBoundPlayers = 0;
    numBoundGroups = 0;

    // only instanceable maps have bounds
    for (PersistentStateMap::iterator itr = m_instanceSaveByInstanceId.begin(); itr != m_instanceSaveByInstanceId.end(); ++itr)
    {
        if (!itr->second->GetMapEntry()->IsDungeon())
        {
            continue;
        }

        ++numStates;
        numBoundPlayers += ((DungeonPersistentState*)itr->second)->GetPlayerCount();
        numBoundGroups += ((DungeonPersistentState*)itr->second)->GetGroupCount();
    }
}

/**
 * @brief Removes expired instances whose reset times have passed.
 *
 * @param t The cutoff time.
 */
void MapPersistentStateManager::_CleanupExpiredInstancesAtTime(time_t t)
{
    _DelHelper(CharacterDatabase, "id, map, instance.difficulty", "instance", "LEFT JOIN instance_reset ON mapid = map AND instance.difficulty =  instance_reset.difficulty WHERE (instance.resettime < '" UI64FMTD "' AND instance.resettime > '0') OR (NOT instance_reset.resettime IS NULL AND instance_reset.resettime < '" UI64FMTD "')", (uint64)t, (uint64)t);
}


/**
 * @brief Creates persistent states for all non-instanceable world maps.
 */
void MapPersistentStateManager::InitWorldMaps()
{
    MapPersistentState* state = NULL;                       // need any from created for shared pool state
    for (uint32 mapid = 0; mapid < sMapStore.GetNumRows(); ++mapid)
        if (MapEntry const* entry = sMapStore.LookupEntry(mapid))
            if (!entry->Instanceable())
            {
                state = AddPersistentState(entry, 0, REGULAR_DIFFICULTY, 0, false, true, false);
            }

    if (state)
    {
        state->InitPools();
    }
}

/**
 * @brief Loads creature respawn timers into persistent states.
 */
void MapPersistentStateManager::LoadCreatureRespawnTimes()
{
    // remove outdated data
    CharacterDatabase.DirectExecute("DELETE FROM `creature_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())");

    uint32 count = 0;

    //                                                     0       1              2      3           4             5            6
    QueryResult* result = CharacterDatabase.Query("SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `creature_respawn` LEFT JOIN `instance` ON `instance` = `id`");
    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 creature respawn time.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 loguid               = fields[0].GetUInt32();
        uint64 respawn_time         = fields[1].GetUInt64();
        uint32 mapId                = fields[2].GetUInt32();
        uint32 instanceId           = fields[3].GetUInt32();
        uint8 difficulty            = fields[4].GetUInt8();
        time_t resetTime            = (time_t)fields[5].GetUInt64();
        uint32 completedEncounters  = fields[6].GetUInt32();

        CreatureData const* data = sObjectMgr.GetCreatureData(loguid);
        if (!data)
        {
            continue;
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(data->mapid);
        if (!mapEntry)
        {
            continue;
        }

        if (instanceId)                                     // In instance - mapId must be data->mapid and mapEntry must be Instanceable
        {
            if (mapId != data->mapid || !mapEntry->Instanceable())
            {
                continue;
            }
        }
        else                                                // Not in instance, mapEntry must not be Instanceable
        {
            if (mapEntry->Instanceable())
            {
                continue;
            }
        }

        if (difficulty >= (!mapEntry->Instanceable() ? REGULAR_DIFFICULTY + 1 : (mapEntry->IsRaid() ? MAX_RAID_DIFFICULTY : MAX_DUNGEON_DIFFICULTY)))
        {
            continue;
        }

        MapPersistentState* state = AddPersistentState(mapEntry, instanceId, Difficulty(difficulty), resetTime, mapEntry->IsDungeon(), true, true, completedEncounters);
        if (!state)
        {
            continue;
        }

        state->SetCreatureRespawnTime(loguid, time_t(respawn_time));

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u creature respawn times", count);
    sLog.outString();
}

/**
 * @brief Loads gameobject respawn timers into persistent states.
 */
void MapPersistentStateManager::LoadGameobjectRespawnTimes()
{
    // remove outdated data
    CharacterDatabase.DirectExecute("DELETE FROM `gameobject_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())");

    uint32 count = 0;

    //                                                     0       1              2      3           4             5            6
    QueryResult* result = CharacterDatabase.Query("SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `gameobject_respawn` LEFT JOIN `instance` ON `instance` = `id`");

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 gameobject respawn time.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 loguid               = fields[0].GetUInt32();
        uint64 respawn_time         = fields[1].GetUInt64();
        uint32 mapId                = fields[2].GetUInt32();
        uint32 instanceId           = fields[3].GetUInt32();
        uint8 difficulty            = fields[4].GetUInt8();
        time_t resetTime            = (time_t)fields[5].GetUInt64();
        uint32 completedEncounters  = fields[6].GetUInt32();

        GameObjectData const* data = sObjectMgr.GetGOData(loguid);
        if (!data)
        {
            continue;
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(data->mapid);
        if (!mapEntry)
        {
            continue;
        }

        if (instanceId)                                     // In instance - mapId must be data->mapid and mapEntry must be Instanceable
        {
            if (mapId != data->mapid || !mapEntry->Instanceable())
            {
                continue;
            }
        }
        else                                                // Not in instance, mapEntry must not be Instanceable
        {
            if (mapEntry->Instanceable())
            {
                continue;
            }
        }

        if (difficulty >= (!mapEntry->Instanceable() ? REGULAR_DIFFICULTY + 1 : (mapEntry->IsRaid() ? MAX_RAID_DIFFICULTY : MAX_DUNGEON_DIFFICULTY)))
        {
            continue;
        }

        MapPersistentState* state = AddPersistentState(mapEntry, instanceId, Difficulty(difficulty), resetTime, mapEntry->IsDungeon(), true, true, completedEncounters);
        if (!state)
        {
            continue;
        }

        state->SetGORespawnTime(loguid, time_t(respawn_time));

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u gameobject respawn times", count);
    sLog.outString();
}
