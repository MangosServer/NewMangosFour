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
 * @file ObjectMgrInstanceData.cpp
 * @brief Cohesion split of ObjectMgr.cpp -- instance encounter, group and arena-team loaders.
 *        Same `ObjectMgr` class; no behaviour change. CMake
 *        `file(GLOB Object/*.cpp)` picks this file up automatically;
 *        ObjectMgr.h is unchanged.
 */

#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "SQLStorages.h"
#include "DBCStores.h"
#include "Group.h"
#include "LFGMgr.h"

/**
 * @brief Gets instance template data by map id.
 *
 * @param map The instance map id.
 * @return The instance template, or null if missing.
 */
InstanceTemplate const* ObjectMgr::GetInstanceTemplate(uint32 map) { return sInstanceTemplate.LookupEntry<InstanceTemplate>(map); }
WorldTemplate const* ObjectMgr::GetWorldTemplate(uint32 map) { return sWorldTemplate.LookupEntry<WorldTemplate>(map); }

/* ********************************************************************************************* */
/* *                                Loading Functions                                            */
/* ********************************************************************************************* */
/**
 * @brief Loads groups, group members, and group instance bindings from the database.
 */
void ObjectMgr::LoadGroups()
{
    // -- loading groups --
    uint32 count = 0;
    //                                                     0           1                2             3             4                5        6        7        8        9        10       11       12       13           14            15                16            17
    QueryResult* result = CharacterDatabase.Query("SELECT `mainTank`, `mainAssistant`, `lootMethod`, `looterGuid`, `lootThreshold`, `icon1`, `icon2`, `icon3`, `icon4`, `icon5`, `icon6`, `icon7`, `icon8`, `groupType`, `difficulty`, `raiddifficulty`, `leaderGuid`, `groupId` FROM `groups`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u group definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        ++count;
        Group* group = new Group;
        if (!group->LoadGroupFromDB(fields))
        {
            group->Disband();
            delete group;
            continue;
        }
        AddGroup(group);
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u group definitions", count);
    sLog.outString();

    // -- loading members --
    count = 0;
    //                                       0           1          2         3
    result = CharacterDatabase.Query("SELECT `memberGuid`, `assistant`, `subgroup`, `groupId` FROM `group_member` ORDER BY `groupId`");
    if (!result)
    {
        BarGoLink bar2(1);
        bar2.step();
    }
    else
    {
        Group* group = NULL;                                // used as cached pointer for avoid relookup group for each member

        BarGoLink bar2(result->GetRowCount());
        do
        {
            bar2.step();
            Field* fields = result->Fetch();
            ++count;

            uint32 memberGuidlow = fields[0].GetUInt32();
            ObjectGuid memberGuid = ObjectGuid(HIGHGUID_PLAYER, memberGuidlow);
            bool   assistent     = fields[1].GetBool();
            uint8  subgroup      = fields[2].GetUInt8();
            uint32 groupId       = fields[3].GetUInt32();
            if (!group || group->GetId() != groupId)
            {
                group = GetGroupById(groupId);
                if (!group)
                {
                    sLog.outErrorDb("Incorrect entry in group_member table : no group with Id %d for member %s!",
                                    groupId, memberGuid.GetString().c_str());
                    CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `memberGuid` = '%u'", memberGuidlow);
                    continue;
                }
            }

            if (!group->LoadMemberFromDB(memberGuidlow, subgroup, assistent))
            {
                sLog.outErrorDb("Incorrect entry in group_member table : member %s can not be added to group (Id: %u)!",
                                memberGuid.GetString().c_str(), groupId);
                CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `memberGuid` = '%u'", memberGuidlow);
            }
        }
        while (result->NextRow());
        delete result;
    }

    // clean groups
    // TODO: maybe delete from the DB before loading in this case
    for (GroupMap::iterator itr = mGroupMap.begin(); itr != mGroupMap.end();)
    {
        // Mirror RemoveMember's own survival threshold rather than assuming two.
        //
        // This loop predates the branch and was correct while a one-member group could
        // never reach startup: the logout path dissolved it. It no longer does -- an LFG
        // group survives logout deliberately, so a run that bled down to a single member
        // persists to the database and loads here.
        //
        // Disbanding it at this point defeats the whole restart-survival feature, and
        // silently: it runs BEFORE the instance-bind loop, so RestoreDungeonGroup is
        // never called for the group, and before the demotion sweep, which then cannot
        // see it either. The player logs back in inside the instance with no group, no
        // LFG block, no eye and no teleport out -- exactly the stranding this branch
        // exists to prevent.
        //
        // 1 < 1 is false, so a single-member LFG or battleground group now survives to
        // the bind loop and is either restored or demoted there.
        uint32 const minMembers = (itr->second->isBGGroup() || itr->second->isLFGGroup()) ? 1u : 2u;
        if (itr->second->GetMembersCount() < minMembers)
        {
            itr->second->Disband();
            delete itr->second;
            mGroupMap.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }

    // -- loading instances --
    count = 0;
    result = CharacterDatabase.Query(
                 //                        0             1      2           3                       4             5
                 "SELECT `group_instance`.`leaderGuid`, `map`, `instance`, `permanent`, `instance`.`difficulty`, `resettime`, "
                 // 6
                 "(SELECT COUNT(*) FROM `character_instance` WHERE `guid` = `group_instance`.`leaderGuid` AND `instance` = `group_instance`.`instance` AND `permanent` = 1 LIMIT 1), "
                 // 7                              8
                 " `groups`.`groupId`, `instance`.`encountersMask` "
                 "FROM `group_instance` LEFT JOIN `instance` ON `instance` = `id` LEFT JOIN `groups` ON `groups`.`leaderGUID` = `group_instance`.`leaderGUID` ORDER BY `leaderGuid`"
             );

    if (!result)
    {
        BarGoLink bar2(1);
        bar2.step();
    }
    else
    {
        Group* group = NULL;                                // used as cached pointer for avoid relookup group for each member

        BarGoLink bar2(result->GetRowCount());
        do
        {
            bar2.step();
            Field* fields = result->Fetch();
            ++count;

            uint32 leaderGuidLow = fields[0].GetUInt32();
            uint32 mapId = fields[1].GetUInt32();
            Difficulty diff = (Difficulty)fields[4].GetUInt8();
            uint32 groupId = fields[7].GetUInt32();

            if (!group || group->GetId() != groupId)
            {
                // find group id in map by leader low guid
                group = GetGroupById(groupId);
                if (!group)
                {
                    sLog.outErrorDb("Incorrect entry in group_instance table : no group with leader %d", leaderGuidLow);
                    continue;
                }
            }

            MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
            if (!mapEntry || !mapEntry->IsDungeon())
            {
                sLog.outErrorDb("Incorrect entry in group_instance table : no dungeon map %d", mapId);
                continue;
            }

            if (diff >= (mapEntry->IsRaid() ? MAX_RAID_DIFFICULTY : MAX_DUNGEON_DIFFICULTY))
            {
                sLog.outErrorDb("Wrong dungeon difficulty use in group_instance table: %d", diff + 1);
                diff = REGULAR_DIFFICULTY;                  // default for both difficaly types
            }

            DungeonPersistentState* state = (DungeonPersistentState*)sMapPersistentStateMgr.AddPersistentState(mapEntry, fields[2].GetUInt32(), Difficulty(diff), (time_t)fields[5].GetUInt64(), (fields[6].GetUInt32() == 0), true, true, fields[8].GetUInt32());
            group->BindToInstance(state, fields[3].GetBool(), true);

            // Nothing in LFGMgr is persisted, so a dungeon-finder party that was inside its
            // instance when the world went down comes back with the group and the bind but
            // no LFG status -- which empties SMSG_GROUP_LIST's LFG block and takes the eye,
            // both teleport options and the Vote Kick gate away from the client. Rebuild it
            // here, where the bind's map, difficulty and encounter mask are all in hand.
            sLFGMgr.RestoreDungeonGroup(group, mapId, uint32(diff), fields[8].GetUInt32());
        }
        while (result->NextRow());
        delete result;
    }

    sLog.outString(">> Loaded %u group-instance binds total", count);
    sLog.outString();

    // Any group still flagged as a finder run but WITHOUT a restored status has outlived
    // its instance, and must be demoted rather than left in between.
    //
    // RestoreDungeonGroup above rebuilds a run's LFG status from its bind. A group whose
    // bind is gone -- an ordinary dungeon instance expires two hours after it is created,
    // so this is the normal outcome of leaving a party assembled overnight -- never
    // reaches that call at all, because the loop iterates binds. It comes back with
    // GROUPTYPE_LFD set and no status behind it.
    //
    // Left alone the client gets neither behaviour: SMSG_GROUP_LIST omits the LFG block,
    // so the eye and the teleport options disappear, while every server-side
    // isLFGGroup() test still says finder group -- which is why TeleportPlayer refuses
    // with "has no LFG status" instead of moving anyone. Demote here, once, where the
    // binds have all been processed and the answer is finally knowable.
    {
        uint32 demoted = 0;
        for (GroupMap::const_iterator itr = mGroupMap.begin(); itr != mGroupMap.end(); ++itr)
        {
            Group* group = itr->second;
            if (!group || !group->isLFGGroup())
            {
                continue;
            }

            // Exactly the predicate Group::SendUpdate uses to decide whether to emit an
            // LFG block, so a group is demoted precisely when the client would otherwise
            // have been sent nothing and left in the half-state.
            if (sLFGMgr.GetGroupDungeonEntry(group->GetObjectGuid()) != 0)
            {
                continue;                                   // a live run, restored above
            }

            sLog.outString("Group %u was a dungeon finder group whose instance no longer "
                           "exists; converting it to an ordinary party.", group->GetId());
            group->ClearLfgGroup();
            ++demoted;
        }

        if (demoted)
        {
            sLog.outString(">> Demoted %u finder group(s) with no surviving instance", demoted);
            sLog.outString();
        }
    }

    sLog.outString(">> Loaded %u group members total", count);
    sLog.outString();
}

void ObjectMgr::LoadInstanceEncounters()
{
    m_DungeonEncounters.clear();         // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `creditType`, `creditEntry`, `lastEncounterDungeon` FROM `instance_encounters`");

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 Instance Encounters. DB table `instance_encounters` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();
        DungeonEncounterEntry const* dungeonEncounter = sDungeonEncounterStore.LookupEntry(entry);

        if (!dungeonEncounter)
        {
            sLog.outErrorDb("Table `instance_encounters` has an invalid encounter id %u, skipped!", entry);
            continue;
        }

        uint8 creditType = fields[1].GetUInt8();
        uint32 creditEntry = fields[2].GetUInt32();
        switch (creditType)
        {
            case ENCOUNTER_CREDIT_KILL_CREATURE:
            {
                CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(creditEntry);
                if (!cInfo)
                {
                    sLog.outErrorDb("Table `instance_encounters` has an invalid creature (entry %u) linked to the encounter %u (%s), skipped!", creditEntry, entry, dungeonEncounter->Name_lang[0]);
                    continue;
                }
                break;
            }
            case ENCOUNTER_CREDIT_CAST_SPELL:
            {
                if (!sSpellStore.LookupEntry(creditEntry))
                {
                    // skip spells that aren't in dbc for now
                    // sLog.outErrorDb("Table `instance_encounters` has an invalid spell (entry %u) linked to the encounter %u (%s), skipped!", creditEntry, entry, dungeonEncounter->encounterName[0]);
                    continue;
                }
                break;
            }
            default:
                sLog.outErrorDb("Table `instance_encounters` has an invalid credit type (%u) for encounter %u (%s), skipped!", creditType, entry, dungeonEncounter->Name_lang[0]);
                continue;
        }
        uint32 lastEncounterDungeon = fields[3].GetUInt32();

        m_DungeonEncounters.insert(DungeonEncounterMap::value_type(creditEntry, new DungeonEncounter(dungeonEncounter, EncounterCreditType(creditType), creditEntry, lastEncounterDungeon)));
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %zu Instance Encounters", m_DungeonEncounters.size());
}
