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

#include <algorithm>
#include <set>
#include <vector>

#include "DBCEnums.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "GameEventMgr.h"
#include "Group.h"
#include "LFGMgr.h"
#include "Object.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "SharedDefines.h"
#include "WorldSession.h"

INSTANTIATE_SINGLETON_1(LFGMgr);

LFGMgr::LFGMgr()
{
    m_proposalId = 0;
}

LFGMgr::~LFGMgr()
{
    m_dailyAny.clear();
    m_dailyTBCHeroic.clear();
    m_dailyLKNormal.clear();
    m_dailyLKHeroic.clear();

    m_playerData.clear();
    m_queueSet.clear();

    m_playerStatusMap.clear();
    m_groupStatusMap.clear();
    m_groupSet.clear();
    m_proposalMap.clear();

    m_roleCheckMap.clear();

    m_bootStatusMap.clear();

    m_tankWaitTime.clear();
    m_healerWaitTime.clear();
    m_dpsWaitTime.clear();
    m_avgWaitTime.clear();
}

void LFGMgr::Update()
{
    //todo: remove old queues, proposals & boot votes

    // remove old role checks
    RemoveOldRoleChecks();

    // go through a waitTimeMap::iterator for each wait map and update times based on player count
    for (waitTimeMap::iterator tankItr = m_tankWaitTime.begin(); tankItr != m_tankWaitTime.end(); ++tankItr)
    {
        LFGWait waitInfo = tankItr->second;
        if (waitInfo.doAverage)
        {
            int32 lastTime = waitInfo.previousTime;
            int32 thisTime = waitInfo.time;

            // average of the two join times
            waitInfo.time = (thisTime + lastTime) / 2;

            // now set what was just the current wait time to the previous time for a later calculation
            waitInfo.previousTime = thisTime;
            waitInfo.doAverage = false;

            tankItr->second = waitInfo;
        }
    }
    for (waitTimeMap::iterator healItr = m_healerWaitTime.begin(); healItr != m_healerWaitTime.end(); ++healItr)
    {
        LFGWait waitInfo = healItr->second;
        if (waitInfo.doAverage)
        {
            int32 lastTime = waitInfo.previousTime;
            int32 thisTime = waitInfo.time;

            // average of the two join times
            waitInfo.time = (thisTime + lastTime) / 2;

            // now set what was just the current wait time to the previous time for a later calculation
            waitInfo.previousTime = thisTime;
            waitInfo.doAverage = false;

            healItr->second = waitInfo;
        }
    }
    for (waitTimeMap::iterator dpsItr = m_dpsWaitTime.begin(); dpsItr != m_dpsWaitTime.end(); ++dpsItr)
    {
        LFGWait waitInfo = dpsItr->second;
        if (waitInfo.doAverage)
        {
            int32 lastTime = waitInfo.previousTime;
            int32 thisTime = waitInfo.time;

            // average of the two join times
            waitInfo.time = (thisTime + lastTime) / 2;

            // now set what was just the current wait time to the previous time for a later calculation
            waitInfo.previousTime = thisTime;
            waitInfo.doAverage = false;

            dpsItr->second = waitInfo;
        }
    }
    for (waitTimeMap::iterator avgItr = m_avgWaitTime.begin(); avgItr != m_avgWaitTime.end(); ++avgItr)
    {
        LFGWait waitInfo = avgItr->second;
        if (waitInfo.doAverage)
        {
            int32 lastTime = waitInfo.previousTime;
            int32 thisTime = waitInfo.time;

            // average of the two join times
            waitInfo.time = (thisTime + lastTime) / 2;

            // now set what was just the current wait time to the previous time for a later calculation
            waitInfo.previousTime = thisTime;
            waitInfo.doAverage = false;

            avgItr->second = waitInfo;
        }
    }

    // Queue System
    FindQueueMatches();
    SendQueueStatus();
}


ItemRewards LFGMgr::GetDungeonItemRewards(uint32 dungeonId, DungeonTypes type)
{
    ItemRewards rewards;
    LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(dungeonId);
    if (dungeon)
    {
        uint32 minLevel = dungeon->MinLevel;
        uint32 maxLevel = dungeon->MaxLevel;
        uint32 avgLevel = (minLevel+maxLevel)/2; // otherwise there are issues

        DungeonFinderItemsMap const& itemBuffer = sObjectMgr.GetDungeonFinderItemsMap();
        for (DungeonFinderItemsMap::const_iterator it = itemBuffer.begin(); it != itemBuffer.end(); ++it)
        {
            DungeonFinderItems itemCache = it->second;
            if (itemCache.dungeonType == type)
            {
                // should only be one of this inequality in the map
                if ((avgLevel >= itemCache.minLevel) && (avgLevel <= itemCache.maxLevel))
                {
                    rewards.itemId = itemCache.itemReward;
                    rewards.itemAmount = itemCache.itemAmount;
                    return rewards;
                }
            }
        }
    }
    return rewards;
}

DungeonTypes LFGMgr::GetDungeonType(uint32 dungeonId)
{
    LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(dungeonId);
    if (dungeon)
    {
        // DungeonTypes classifies FIVE-MAN dungeons for daily-reward purposes -- see
        // RegisterPlayerDaily and the reward selection in LFGMgrProposal -- and has no raid member
        // at all. A raid row must therefore not be classified here.
        //
        // That needs saying because translating the difficulty made this newly wrong. Raid rows
        // carry raw DifficultyID 3 (10-normal) and 4 (25-normal), which translate to internal 0 and
        // 1, exactly the values the tests below look for. So every TBC and WotLK raid would come out
        // of here typed DUNGEON_TBC / DUNGEON_WOTLK or their heroic variants -- a 25-man raid counted
        // as a normal 5-man dungeon for daily rewards. Before the translation, raw 3 and 4 matched
        // neither constant and fell out as DUNGEON_UNKNOWN by accident.
        if (dungeon->TypeID == LFG_TYPE_RAID)
        {
            return DUNGEON_UNKNOWN;
        }

        // LfgDungeons.dbc carries a RAW client DifficultyID; the DUNGEON_DIFFICULTY_* constants
        // are internal modes. Comparing them directly made raw 1 (5-man normal) equal
        // DUNGEON_DIFFICULTY_HEROIC, which is 1, while raw 2 (5-man heroic) matched neither
        // constant and fell through to DUNGEON_UNKNOWN -- so every TBC and WotLK dungeon was
        // typed as heroic or as unknown, never as normal.
        int32 const mode = ToInternalDifficulty(dungeon->DifficultyID);

        switch (dungeon->ExpansionLevel)
        {
            case 0:
                return DUNGEON_CLASSIC;
            case 1:
            {
                if (mode == int32(DUNGEON_DIFFICULTY_NORMAL))
                {
                    return DUNGEON_TBC;
                }
                else if (mode == int32(DUNGEON_DIFFICULTY_HEROIC))
                {
                    return DUNGEON_TBC_HEROIC;
                }
                return DUNGEON_UNKNOWN;                     // was a fall-through into case 2
            }
            case 2:
            {
                if (mode == int32(DUNGEON_DIFFICULTY_NORMAL))
                {
                    return DUNGEON_WOTLK;
                }
                else if (mode == int32(DUNGEON_DIFFICULTY_HEROIC))
                {
                    return DUNGEON_WOTLK_HEROIC;
                }
                return DUNGEON_UNKNOWN;                     // was a fall-through into default
            }
            default:
                return DUNGEON_UNKNOWN;
        }
    }
    return DUNGEON_UNKNOWN;
}

void LFGMgr::RegisterPlayerDaily(uint32 guidLow, DungeonTypes dungeon)
{
    switch (dungeon)
    {
        case DUNGEON_CLASSIC:
        case DUNGEON_TBC:
            m_dailyAny.insert(guidLow);
            break;
        case DUNGEON_TBC_HEROIC:
            m_dailyTBCHeroic.insert(guidLow);
            break;
        case DUNGEON_WOTLK:
            m_dailyLKNormal.insert(guidLow);
            break;
        case DUNGEON_WOTLK_HEROIC:
            m_dailyLKHeroic.insert(guidLow);
            break;
        default:
            break;
    }
}

bool LFGMgr::HasPlayerDoneDaily(uint32 guidLow, DungeonTypes dungeon)
{
    switch (dungeon)
    {
        case DUNGEON_CLASSIC:
        case DUNGEON_TBC:
            return (m_dailyAny.find(guidLow) != m_dailyAny.end()) ? true : false;
        case DUNGEON_TBC_HEROIC:
            return (m_dailyTBCHeroic.find(guidLow) != m_dailyTBCHeroic.end()) ? true : false;
        case DUNGEON_WOTLK:
            return (m_dailyLKNormal.find(guidLow) != m_dailyLKNormal.end()) ? true : false;
        case DUNGEON_WOTLK_HEROIC:
            return (m_dailyLKHeroic.find(guidLow) != m_dailyLKHeroic.end()) ? true : false;
        default:
            return false;
    }
    return false;
}

void LFGMgr::ResetDailyRecords()
{
    m_dailyAny.clear();
    m_dailyTBCHeroic.clear();
    m_dailyLKNormal.clear();
    m_dailyLKHeroic.clear();
}

bool LFGMgr::IsSeasonActive(uint32 dungeonId)
{
    switch (dungeonId)
    {
        case 285:
            return IsHolidayActive(HOLIDAY_HALLOWS_END);
        case 286:
            return IsHolidayActive(HOLIDAY_FIRE_FESTIVAL);
        case 287:
            return IsHolidayActive(HOLIDAY_BREWFEST);
        case 288:
            return IsHolidayActive(HOLIDAY_LOVE_IS_IN_THE_AIR);
        default:
            return false;
    }
    return false;
}

dungeonEntries LFGMgr::FindRandomDungeonsForPlayer(uint32 level, uint8 expansion)
{
    dungeonEntries randomDungeons;

    // go through the dungeon dbc and select the applicable dungeons
    for (uint32 id = 0; id < sLfgDungeonsStore.GetNumRows(); ++id)
    {
        LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(id);
        if (dungeon)
        {
            if ( (dungeon->TypeID == LFG_TYPE_RANDOM_DUNGEON)
                || (IsSeasonal(dungeon->Flags) && IsSeasonActive(dungeon->ID)) )
                if ((uint8)dungeon->ExpansionLevel <= expansion && dungeon->MinLevel <= level
                    && dungeon->MaxLevel >= level)
                    randomDungeons[dungeon->ID] = dungeon->Entry();
        }
    }
    return randomDungeons;
}

/**
 * @brief The `dungeonfinder_requirements` row for an LfgDungeons entry, or NULL.
 *
 * This is the third DBC-to-world-table join that crosses the difficulty key spaces, and it
 * has to translate for the same reason the other two do.
 *
 * LfgDungeons.dbc carries a RAW client DifficultyID; `dungeonfinder_requirements`.`difficulty`
 * is an INTERNAL 0-based mode. Confirmed against the shipped world data rather than assumed:
 * every five-man map with two rows carries 0 and 1 -- Opening of the Dark Portal 269, The Forge
 * of Souls 632, Trial of the Champion 650, Pit of Saron 658 and Halls of Reflection 668 -- and
 * Icecrown Citadel 631 carries 2 and 3. In the raw space those would be 1/2 and 5/6.
 *
 * 269 is the clearest exhibit of the five and was missing from an earlier version of this list:
 * its difficulty-1 row is the Black Morass heroic attunement, so a table keyed on raw ids would
 * have had to carry it at 2.
 *
 * Passing the raw id shifted every lookup by one tier, in both directions:
 *
 *   an LFG NORMAL row (raw 1) fetched the (map, 1) row, which is the HEROIC requirement, so a
 *   player was held to the heroic item level and attunement quest to queue for normal content.
 *   Item level and quests specifically: no five-man row in the table carries an achievement --
 *   the only two that do are Icecrown Citadel's (631, 2) and (631, 3), a raid map that never
 *   reaches this lookup because LFG_FORBIDDEN_RAID short-circuits above. The live five-man
 *   gates are min_item_level 180 on 12 rows, 200 on 6 and 219 on 2;
 *   an LFG HEROIC row (raw 2) asked for (map, 2), which for a five-man is CHALLENGE and has no
 *   row at all, so the real heroic requirement was skipped entirely.
 *
 * Too strict where it should be lenient and absent where it should bite.
 *
 * It is PRE-EXISTING, not something this branch caused. An earlier version of this comment
 * claimed the row-ordinal indexing had made the lookup miss on noise and that correcting the
 * index turned it systematic. That is false, and measurably so: this function enumerates
 * 0..GetNumRows()-1 and reads MapID, DifficultyID and TypeID off ONE row pointer, so with 343
 * rows and no duplicate ids both index modes visit every row exactly once and the same 33 rows
 * reach the requirements table with the same wrong tier key either way. The claim was carried
 * over from a genuinely index-sensitive site -- CreateDungeonGroup, which looks a row up by a
 * client-supplied id -- where it does hold.
 *
 * It is fixed here because this is the branch that separates the two key spaces, not because
 * the branch created it.
 *
 * A row whose tier has no internal mode yields NULL, which reads as "no requirement". That is
 * the pre-existing behaviour for a missing row and is safe here: JoinLFG refuses such a slot
 * outright at admission, so nothing untranslatable reaches a queue on the strength of it.
 */
static DungeonFinderRequirements const* GetDungeonFinderRequirementsFor(LfgDungeonsEntry const* dungeon)
{
    int32 const mode = ToInternalDifficulty(dungeon->DifficultyID);
    if (mode < 0)
    {
        return NULL;
    }

    return sObjectMgr.GetDungeonFinderRequirements(uint32(dungeon->MapID), uint32(mode));
}

dungeonForbidden LFGMgr::FindRandomDungeonsNotForPlayer(Player* plr)
{
    uint32 level = plr->getLevel();
    uint8 expansion = plr->GetSession()->Expansion();

    dungeonForbidden randomDungeons;

    for (uint32 id = 0; id < sLfgDungeonsStore.GetNumRows(); ++id)
    {
        LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(id);
        if (dungeon)
        {
            uint32 forbiddenReason = 0;

            if ((uint8)dungeon->ExpansionLevel > expansion)
            {
                forbiddenReason = (uint32)LFG_FORBIDDEN_EXPANSION;
            }
            else if (dungeon->TypeID == LFG_TYPE_RAID)
            {
                forbiddenReason = (uint32)LFG_FORBIDDEN_RAID;
            }
            else if (dungeon->MinLevel > level)
            {
                forbiddenReason = (uint32)LFG_FORBIDDEN_LOW_LEVEL;
            }
            else if (dungeon->MaxLevel < level)
            {
                forbiddenReason = (uint32)LFG_FORBIDDEN_HIGH_LEVEL;
            }
            else if (IsSeasonal(dungeon->Flags) && !IsSeasonActive(dungeon->ID)) // check pointers/function args
            {
                forbiddenReason = (uint32)LFG_FORBIDDEN_NOT_IN_SEASON;
            }
            else if (DungeonFinderRequirements const* req = GetDungeonFinderRequirementsFor(dungeon))
            {
                if (req->minItemLevel && (plr->GetEquipGearScore(false,false) < req->minItemLevel))
                {
                    forbiddenReason = (uint32)LFG_FORBIDDEN_LOW_GEAR_SCORE;
                }
                else if (req->achievement && !plr->GetAchievementMgr().HasAchievement(req->achievement))
                {
                    forbiddenReason = (uint32)LFG_FORBIDDEN_MISSING_ACHIEVEMENT;
                }
                else if (plr->GetTeam() == ALLIANCE && req->allianceQuestId && !plr->GetQuestRewardStatus(req->allianceQuestId))
                {
                    forbiddenReason = (uint32)LFG_FORBIDDEN_QUEST_INCOMPLETE;
                }
                else if (plr->GetTeam() == HORDE && req->hordeQuestId && !plr->GetQuestRewardStatus(req->hordeQuestId))
                {
                    forbiddenReason = (uint32)LFG_FORBIDDEN_QUEST_INCOMPLETE;
                }
                else
                    if (req->item)
                    {
                        if (!plr->HasItemCount(req->item, 1) && (!req->item2 || !plr->HasItemCount(req->item2, 1)))
                        {
                            forbiddenReason = LFG_FORBIDDEN_MISSING_ITEM;
                        }
                    }
                    else if (req->item2 && !plr->HasItemCount(req->item2, 1))
                    {
                        forbiddenReason = LFG_FORBIDDEN_MISSING_ITEM;
                    }
            }

            if (forbiddenReason)
            {
                randomDungeons[dungeon->Entry()] = forbiddenReason;
            }
        }
    }
    return randomDungeons;
}

namespace
{
    // The client's LFD frame offers four INDEPENDENT checkboxes -- FrameXML/LFDFrame.lua
    // calls SetLFGRoles(leader, tank, healer, dps) -- so the mask that arrives on the
    // wire routinely carries several roles at once. A player who ticked tank AND dps is
    // willing to fill either, not neither.
    //
    // Every consumer here used to switch on the exact value of (mask & ~LEADER), matching
    // only 0x02/0x04/0x08. A hybrid therefore counted as zero of everything: solo hybrids
    // merged into a full-size entry that still reported every role missing and could
    // neither complete nor merge again, and a premade containing one hybrid failed its
    // role check outright and was ejected.
    //
    // Assigning each player exactly one of the roles they offered needs backtracking, not
    // a greedy pass: given a tank-only player and a tank-or-healer player, handing the
    // tank slot to the hybrid first strands the specialist even though a valid assignment
    // exists.
    //
    // The search is MEMOISED, and that is not an optimisation. Plain backtracking is
    // exponential in the number of players, and this is not a five-man-only path: raid
    // finder rows ask for 2/6/17 and flexible raid for 0/0/25, so a 25-player entry of
    // hybrids would explore on the order of 3^25 states and hang the world thread --
    // LFGMgr::Update runs on it. Keying failures on (index, remaining quota) collapses
    // that to at most (players+1) x (tank+1) x (healer+1) x (damage+1) states, a few
    // thousand even for the largest shipped composition.
    struct RoleQuota
    {
        uint8 tank;
        uint8 healer;
        uint8 damage;

        uint32 Total() const { return uint32(tank) + healer + damage; }
    };

    /// Pack (index, remaining quota) into one key for the failure memo.
    uint64 RoleStateKey(size_t index, RoleQuota const& remaining)
    {
        return (uint64(index) << 24)
             | (uint64(remaining.tank) << 16)
             | (uint64(remaining.healer) << 8)
             | uint64(remaining.damage);
    }

    bool AssignRolesRecursive(std::vector<uint8> const& masks, size_t index, RoleQuota remaining,
                              RoleQuota& leftover, std::set<uint64>& deadEnds)
    {
        if (index == masks.size())
        {
            leftover = remaining;   // what is still open once everyone present is placed
            return true;
        }

        // Already proved unsatisfiable from this exact state.
        uint64 const key = RoleStateKey(index, remaining);
        if (deadEnds.find(key) != deadEnds.end())
        {
            return false;
        }

        static uint8 const candidates[3] = { PLAYER_ROLE_TANK, PLAYER_ROLE_HEALER, PLAYER_ROLE_DAMAGE };

        for (uint8 i = 0; i < 3; ++i)
        {
            uint8 const role = candidates[i];
            if (!(masks[index] & role))
            {
                continue;
            }

            uint8* slot = (role == PLAYER_ROLE_TANK) ? &remaining.tank
                        : (role == PLAYER_ROLE_HEALER) ? &remaining.healer
                        : &remaining.damage;

            if (!*slot)
            {
                continue;
            }

            --(*slot);
            if (AssignRolesRecursive(masks, index + 1, remaining, leftover, deadEnds))
            {
                return true;
            }
            ++(*slot);
        }

        deadEnds.insert(key);
        return false;
    }

    /// Can every player fill exactly one of the roles they offered, within the dungeon's caps?
    /// On success `leftover` receives the roles still open, which is what the queue
    /// advertises as "needed" and what the completion test reads.
    bool RolesFitQuota(roleMap const& roles, RoleQuota const& quota, RoleQuota& leftover)
    {
        if (roles.size() > quota.Total())
        {
            return false;
        }

        std::vector<uint8> masks;
        masks.reserve(roles.size());

        for (roleMap::const_iterator it = roles.begin(); it != roles.end(); ++it)
        {
            uint8 const offered = uint8(it->second & ~PLAYER_ROLE_LEADER);
            if (!offered)
            {
                return false;   // no role ticked at all -- cannot be placed
            }

            masks.push_back(offered);
        }

        // Least-flexible player first, so the search prunes early.
        std::sort(masks.begin(), masks.end(), [](uint8 a, uint8 b)
        {
            uint8 popA = uint8((a & 2 ? 1 : 0) + (a & 4 ? 1 : 0) + (a & 8 ? 1 : 0));
            uint8 popB = uint8((b & 2 ? 1 : 0) + (b & 4 ? 1 : 0) + (b & 8 ? 1 : 0));
            return popA < popB;
        });

        leftover = quota;

        std::set<uint64> deadEnds;
        return AssignRolesRecursive(masks, 0, quota, leftover, deadEnds);
    }

    /// The role composition a dungeon actually wants, straight off its DBC row.
    ///
    /// Not every queueable row is a 1/1/3 five-man: the shipped LfgDungeons.dbc carries
    /// 0/0/3 scenarios, 0/0/1 solo content, 2/6/17 raid finder and 0/0/25 flexible raid.
    /// Reading the row instead of assuming NORMAL_* is what lets those queue at all.
    bool GetDungeonQuota(std::set<uint32> const& dungeonList, RoleQuota& quota)
    {
        if (dungeonList.empty())
        {
            return false;
        }

        LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(*dungeonList.begin());
        if (!dungeon)
        {
            return false;
        }

        quota.tank   = uint8(dungeon->Count_tank);
        quota.healer = uint8(dungeon->Count_healer);
        quota.damage = uint8(dungeon->Count_damage);

        return quota.Total() != 0;
    }
}

bool LFGMgr::RolesAreValidForDungeons(roleMap const& roles, std::set<uint32> const& dungeonList)
{
    RoleQuota quota;
    if (!GetDungeonQuota(dungeonList, quota))
    {
        return false;
    }

    RoleQuota leftover;
    return RolesFitQuota(roles, quota, leftover);
}

void LFGMgr::UpdateNeededRoles(ObjectGuid guid, LFGPlayers* information)
{
    // The role composition comes from the DUNGEON, not from a difficulty test.
    //
    // This previously read `if (dungeon->DifficultyID == DUNGEON_DIFFICULTY_NORMAL)`,
    // comparing a RAW client DifficultyID against the internal 0-based enum -- two
    // different key spaces at 5.4.8. The shipped LfgDungeons.dbc carries no queueable
    // row with DifficultyID 0 at all (TypeID 1 has {1,2,7,11,12,14}, TypeID 6 has
    // {1,2,11,12}), so the branch NEVER fired and the needed-role counts stayed at zero
    // for every entry. With zeros, RoleMapsAreCompatible computed (3-0)+(3-0) = 6 > 3
    // and refused every pair, so the matchmaker formed nothing at all.
    RoleQuota quota;
    if (!GetDungeonQuota(information->dungeonList, quota))
    {
        m_playerData[guid] = *information;
        return;
    }

    RoleQuota leftover;
    if (!RolesFitQuota(information->currentRoles, quota, leftover))
    {
        // No assignment places everyone present -- the entry is over-subscribed on some
        // role. Report the dungeon's full requirement so it advertises as unsatisfiable
        // rather than wrapping a uint8 and claiming 254 damage slots are free.
        leftover = quota;
    }

    // The resolver already clamped these: they count down from the quota as players are
    // placed and can never go below zero, so the old `1 - tankCount` uint8 wrap that
    // turned a two-tank party into "254 more tanks welcome" cannot recur.
    information->neededTanks   = leftover.tank;
    information->neededHealers = leftover.healer;
    information->neededDps     = leftover.damage;

    m_playerData[guid] = *information;
}

void LFGMgr::AddToQueue(ObjectGuid guid)
{
    LFGPlayers* information = GetPlayerOrPartyData(guid);
    if (!information)
    {
        return;
    }

    // This will be necessary for finding matches in the queue
    UpdateNeededRoles(guid, information);

    // put info into wait time maps for starters
    for (roleMap::iterator it = information->currentRoles.begin(); it != information->currentRoles.end(); ++it)
    {
        AddToWaitMap(it->second, information->dungeonList);
    }

    // just in case someone's already been in the queue.
    queueSet::iterator qItr = m_queueSet.find(guid);
    if (qItr == m_queueSet.end())
    {
        m_queueSet.insert(guid);
    }
}

void LFGMgr::RemoveFromQueue(ObjectGuid guid)
{
    m_queueSet.erase(guid);

    //todo - might need to implement a removefromwaitmap function
}

void LFGMgr::AddToWaitMap(uint8 role, std::set<uint32> dungeons)
{
    // use withoutLeader for switch operator
    uint8 withoutLeader = role;
    withoutLeader &= ~PLAYER_ROLE_LEADER;

    switch (withoutLeader)
    {
        case PLAYER_ROLE_TANK:
        {
            for (std::set<uint32>::iterator itr = dungeons.begin(); itr != dungeons.end(); ++itr)
            {
                waitTimeMap::iterator it = m_tankWaitTime.find(*itr);
                if (it != m_tankWaitTime.end())
                {
                    // Increment current player count by one
                    ++it->second.playerCount;
                }
                else
                {
                    LFGWait waitInfo(QUEUE_DEFAULT_TIME, -1, 1, false);
                    m_tankWaitTime[*itr] = waitInfo;
                }
            }
        } break;
        case PLAYER_ROLE_HEALER:
        {
            for (std::set<uint32>::iterator itr = dungeons.begin(); itr != dungeons.end(); ++itr)
            {
                waitTimeMap::iterator it = m_healerWaitTime.find(*itr);
                if (it != m_healerWaitTime.end())
                {
                    // Increment current player count by one
                    ++it->second.playerCount;
                }
                else
                {
                    LFGWait waitInfo(QUEUE_DEFAULT_TIME, -1, 1, false);
                    m_healerWaitTime[*itr] = waitInfo;
                }
            }
        } break;
        case PLAYER_ROLE_DAMAGE:
        {
            for (std::set<uint32>::iterator itr = dungeons.begin(); itr != dungeons.end(); ++itr)
            {
                waitTimeMap::iterator it = m_dpsWaitTime.find(*itr);
                if (it != m_dpsWaitTime.end())
                {
                    // Increment current player count by one
                    ++it->second.playerCount;
                }
                else
                {
                    LFGWait waitInfo(QUEUE_DEFAULT_TIME, -1, 1, false);
                    m_dpsWaitTime[*itr] = waitInfo;
                }
            }
        } break;
        default:
            break;
    }

    // insert the average time regardless of role
    for (std::set<uint32>::iterator itr = dungeons.begin(); itr != dungeons.end(); ++itr)
    {
        waitTimeMap::iterator it = m_avgWaitTime.find(*itr);
        if (it != m_avgWaitTime.end())
        {
            ++it->second.playerCount;
        }
        else
        {
            LFGWait waitInfo(QUEUE_DEFAULT_TIME, -1, 1, false);
            m_avgWaitTime[*itr] = waitInfo;
        }
    }
}

bool LFGMgr::TryFormGroup(ObjectGuid guid)
{
    LFGPlayers* entry = GetPlayerOrPartyData(guid);
    if (!entry || entry->currentState != LFG_STATE_QUEUED)
    {
        return false;
    }

    if (entry->neededTanks || entry->neededHealers || entry->neededDps)
    {
        return false;
    }

    SendDungeonProposal(entry);

    // Out of the queue the moment a proposal exists for it. Without this the entry is
    // still LFG_STATE_QUEUED next tick, gets matched again, and fires a fresh proposal
    // -- and a new SMSG_LFG_PROPOSAL_UPDATE -- every single tick, forever.
    m_queueSet.erase(guid);
    m_playerData.erase(guid);
    return true;
}

void LFGMgr::FindQueueMatches()
{
    // Snapshot: MergeGroups and TryFormGroup both erase from m_queueSet, and erasing the
    // element an active iterator points at is UB.
    queueSet const snapshot = m_queueSet;

    for (queueSet::const_iterator itr = snapshot.begin(); itr != snapshot.end(); ++itr)
    {
        // An entry can be absorbed or dequeued by an earlier iteration of this same pass.
        if (m_queueSet.find(*itr) == m_queueSet.end())
        {
            continue;
        }

        FindSpecificQueueMatches(*itr);

        // A party that arrives already complete -- the common premade-of-five case --
        // is never merged with anything, so the completion test inside MergeGroups
        // never sees it. Without this check such a group waits in the queue forever.
        TryFormGroup(*itr);
    }
}

void LFGMgr::FindSpecificQueueMatches(ObjectGuid guid)
{
    uint64 rawGuid = guid.GetRawValue();
    LFGPlayers* queueInfo = GetPlayerOrPartyData(guid);
    if (queueInfo)
    {
        // compare to everyone else in queue for compatibility
        // after a match is found call UpdateNeededRoles
        // Use the roleMap to store player guid/role information; merge into queueInfo struct & delete other struct/map entry
        queueSet const snapshot = m_queueSet;

        for (queueSet::const_iterator itr = snapshot.begin(); itr != snapshot.end(); ++itr)
        {
            if (*itr == guid)
            {
                continue;
            }

            // Absorbed by an earlier merge in this same pass, or dequeued by a proposal.
            if (m_queueSet.find(*itr) == m_queueSet.end())
            {
                continue;
            }

            // Re-read: MergeGroups mutates the entry we are accumulating into.
            queueInfo = GetPlayerOrPartyData(guid);
            if (!queueInfo)
            {
                return;
            }

            LFGPlayers* matchInfo = GetPlayerOrPartyData(*itr);
            if (matchInfo)
            {
                // 1. iterate through queueInfo's dungeon set and search the matchInfo for a matching entry.
                // 2. if an(y) entry is found, great and proceed!
                // 2a. if an entry is found and the amounts of players-to-roles are compatible, make
                //     a new map of only the inter-compatible dungeons and use that if the other checks pass
                // 3. Regardless of outcome, after the end of calculations send a LFGQueueStatus packet
                bool fullyCompatible = false;
                std::set<uint32> compatibleDungeons;

                for (std::set<uint32>::iterator dItr = matchInfo->dungeonList.begin(); dItr != matchInfo->dungeonList.end(); ++dItr)
                {
                    if (queueInfo->dungeonList.find(*dItr) != queueInfo->dungeonList.end())
                    {
                        compatibleDungeons.insert(*dItr);
                    }
                }

                if (!compatibleDungeons.empty())
                {
                    // check for player / role count and also team compatibility
                    // if function returns true, then merge groups into one
                    if (RoleMapsAreCompatible(queueInfo, matchInfo, compatibleDungeons) && MatchesAreOfSameTeam(queueInfo, matchInfo))
                    {
                        MergeGroups(guid, *itr, compatibleDungeons);
                    }
                }
            }
        }
    }
}

bool LFGMgr::RoleMapsAreCompatible(LFGPlayers* groupOne, LFGPlayers* groupTwo,
                                   std::set<uint32> const& compatibleDungeons)
{
    // When this is called we already know the dungeons overlap, so just focus on roles.
    //
    // The question is simply: if these two entries were one, could every player in the
    // union fill a distinct slot the dungeon actually has? Asking the resolver directly
    // replaces the old per-role arithmetic, which recovered "present" as
    // (NORMAL_X - neededX) and so inherited every uint8 wrap in neededX -- a two-tank
    // party gave (1-255) + (1-0) = -254, which passed the cap test and merged a party
    // that could never complete.
    //
    // It also drops the hardcoded 1/1/3/5, which is wrong for the 108 of 247 queueable
    // TypeID 1 rows that are scenarios (0/0/3), solo content (0/0/1), raid finder
    // (2/6/17) or flexible raid (0/0/25).
    RoleQuota quota;
    if (!GetDungeonQuota(compatibleDungeons, quota))
    {
        return false;
    }

    if ((groupOne->currentRoles.size() + groupTwo->currentRoles.size()) > quota.Total())
    {
        return false;
    }

    roleMap combined = groupOne->currentRoles;
    for (roleMap::const_iterator it = groupTwo->currentRoles.begin(); it != groupTwo->currentRoles.end(); ++it)
    {
        combined[it->first] = it->second;
    }

    // A player present in both entries collapses to one key, so the union can be smaller
    // than the sum -- that is the duplicate-membership case and it must not merge.
    if (combined.size() != groupOne->currentRoles.size() + groupTwo->currentRoles.size())
    {
        return false;
    }

    RoleQuota leftover;
    return RolesFitQuota(combined, quota, leftover);
}

bool LFGMgr::MatchesAreOfSameTeam(LFGPlayers* groupOne, LFGPlayers* groupTwo)
{
    // we should safely be able to compare any two players from each struct to
    // determine compatibility
    roleMap::iterator it1 = groupOne->currentRoles.begin();
    roleMap::iterator it2 = groupTwo->currentRoles.begin();

    // now we find the players from the maps
    Player* pPlayer1 = sObjectAccessor.FindPlayer(it1->first);
    Player* pPlayer2 = sObjectAccessor.FindPlayer(it2->first);

    if (!pPlayer1 || !pPlayer2)
    {
        return false;
    }

    // todo: disable this if a config option is set
    if (pPlayer1->GetTeamId() == pPlayer2->GetTeamId())
    {
        return true;
    }

    return false;
}

void LFGMgr::MergeGroups(ObjectGuid guidOne, ObjectGuid guidTwo, std::set<uint32> compatibleDungeons)
{
    // merge into the entry for rawGuidOne, then see if they are
    // able to enter the dungeon at this point or not
    LFGPlayers* mainGroup   = GetPlayerOrPartyData(guidOne);
    LFGPlayers* bufferGroup = GetPlayerOrPartyData(guidTwo);

    if (!mainGroup || !bufferGroup)
    {
        return;
    }

    // update the dungeon selection with the compatible ones
    mainGroup->dungeonList.clear();
    mainGroup->dungeonList = compatibleDungeons;

    // move players / roles into a single roleMap
    for (roleMap::iterator it = bufferGroup->currentRoles.begin(); it != bufferGroup->currentRoles.end(); ++it)
    {
        mainGroup->currentRoles[it->first] = it->second;
    }

    // update the role count / needed role info
    UpdateNeededRoles(guidOne, mainGroup);

    // being safe
    //mainGroup = GetPlayerOrPartyData(rawGuidOne);

    // Both containers, or guidTwo lingers in m_queueSet pointing at data that no
    // longer exists. That stale entry is not merely a leak: the merged-away
    // player still reads LFG_STATE_QUEUED, SendQueueStatus keys off m_playerData
    // so their client never hears again, and a re-queue skips JoinLFG's
    // duplicate cleanup (it is guarded on existing data) -- leaving that player
    // live in a fresh solo entry AND still listed in the merged entry's roles,
    // which can produce two proposals for the same person.
    m_queueSet.erase(guidTwo);
    m_playerData.erase(guidTwo);

    // Completion is decided after the absorbed entry is gone, so the proposal is built
    // from one consistent view and TryFormGroup can dequeue the survivor safely.
    TryFormGroup(guidOne);
}

void LFGMgr::SendQueueStatus()
{
    // First we should get the current time
    time_t timeNow = time(0);

    // Check who is listed as being in the queue
    for (queueSet::iterator itr = m_queueSet.begin(); itr != m_queueSet.end(); ++itr)
    {
        // make sure it's not a false entry
        LFGPlayers* queueInfo = GetPlayerOrPartyData(*itr);
        if (queueInfo && queueInfo->currentState == LFG_STATE_QUEUED)
        {
            for (roleMap::iterator rItr = queueInfo->currentRoles.begin(); rItr != queueInfo->currentRoles.end(); ++rItr)
            {
                if (Player* pPlayer = sObjectAccessor.FindPlayer(rItr->first))
                {
                    uint32 dungeonId = *queueInfo->dungeonList.begin();

                    LFGQueueStatus status;
                    status.queueGuid = itr->GetRawValue();
                    status.dungeonID        = dungeonId;
                    status.neededTanks      = queueInfo->neededTanks;
                    status.neededHeals      = queueInfo->neededHealers;
                    status.neededDps        = queueInfo->neededDps;
                    status.timeSpentInQueue = uint32(timeNow - queueInfo->joinedTime);
                    status.joinTime = uint32(queueInfo->joinedTime);

                    int32 playerWaitTime;

                    // strip leader flag from role
                    uint8 withoutLeader = rItr->second;
                    withoutLeader &= ~PLAYER_ROLE_LEADER;

                    switch (withoutLeader)
                    {
                        case PLAYER_ROLE_TANK:
                            playerWaitTime = m_tankWaitTime[dungeonId].time;
                            break;
                        case PLAYER_ROLE_HEALER:
                            playerWaitTime = m_healerWaitTime[dungeonId].time;
                            break;
                        case PLAYER_ROLE_DAMAGE:
                            playerWaitTime = m_dpsWaitTime[dungeonId].time;
                            break;
                        default:
                            playerWaitTime = m_avgWaitTime[dungeonId].time;
                            break;
                    }

                    status.playerAvgWaitTime = playerWaitTime;
                    status.dpsAvgWaitTime    = m_dpsWaitTime[dungeonId].time;
                    status.healerAvgWaitTime = m_healerWaitTime[dungeonId].time;
                    status.tankAvgWaitTime   = m_tankWaitTime[dungeonId].time;
                    status.avgWaitTime       = m_avgWaitTime[dungeonId].time;

                    // Send packet to client
                    pPlayer->GetSession()->SendLfgQueueStatus(status);
                }
            }
        }
    }
}

uint32 LFGMgr::GetDungeonEntry(uint32 ID)
{
    LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(ID);
    if (dungeon)
    {
        return dungeon->Entry();
    }
    else
    {
        return 0;
    }
}

uint8 LFGMgr::GetDungeonCategory(uint32 ID)
{
    LfgDungeonsEntry const* dungeon = sLfgDungeonsStore.LookupEntry(ID);
    return dungeon ? uint8(dungeon->Subtype) : 0;
}
