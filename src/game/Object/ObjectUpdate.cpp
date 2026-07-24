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

#include "Object.h"
#include "Item.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Creature.h"
#include "Player.h"
#include "Vehicle.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"
#include "MapManager.h"
#include "Log.h"
#include "Transports.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "VMapFactory.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "movement/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"
#include "MopUpdateObject.h"
#include <vector>

#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#include "ElunaConfig.h"
#include "ElunaEventMgr.h"
#endif /* ENABLE_ELUNA */

/**
 * @file ObjectUpdate.cpp
 * @brief Cohesion split of Object.cpp -- Object update-data serialization: create/values update-block building, movement-block packing, update masks and value (de)serialization. Same Object class; no behaviour change. CMake file(GLOB Object/*.cpp) picks this file up automatically; Object.h is unchanged.
 */

namespace
{
    static_assert(ITEM_END == MopUpdateObject::ItemFieldCount,
        "18414 Item direct-copy range must remain fields 0..68");
    static_assert(CONTAINER_END == MopUpdateObject::ContainerFieldCount,
        "18414 Container direct-copy range must remain fields 0..141");
    static_assert(DYNAMICOBJECT_END == 14 && DYNAMICOBJECT_END == MopUpdateObject::DynamicObjectFieldCount,
        "18414 DynamicObject direct-copy range must remain fields 0..13");
    static_assert(CORPSE_END == 36 && CORPSE_END == MopUpdateObject::CorpseFieldCount,
        "18414 Corpse direct-copy range must remain fields 0..35");
    static_assert(MOVEFLAG_WALK_MODE == MopUpdateObject::SimpleLivingWalkModeFlag,
        "18414 simple LIVING walk-mode flag must match the gameplay movement flag");
    static_assert(PLAYER_FIELD_INV_SLOT_HEAD == MopUpdateObject::SelfInventorySourceStart,
        "self inventory translation must start at local field 960");
    static_assert(PLAYER_FIELD_VENDORBUYBACK_SLOT_1 + 24 ==
        MopUpdateObject::SelfInventorySourceStart + MopUpdateObject::SelfInventoryFieldCount,
        "self inventory translation must end before local field 1132");
    static_assert(OBJECT_FIELD_SCALE_X == 7 && UNIT_FIELD_BYTES_0 == 26 &&
        UNIT_FIELD_HEALTH == 28 && UNIT_FIELD_POWER1 == 29 &&
        UNIT_FIELD_POWER5 == 33 && UNIT_FIELD_MAXHEALTH == 34 &&
        UNIT_FIELD_MAXPOWER1 == 35 && UNIT_FIELD_MAXPOWER5 == 39 &&
        UNIT_FIELD_LEVEL == 50 && UNIT_FIELD_FACTIONTEMPLATE == 51 &&
        UNIT_VIRTUAL_ITEM_SLOT_ID == 52 && UNIT_FIELD_FLAGS == 55 &&
        UNIT_FIELD_BOUNDINGRADIUS == 61 && UNIT_FIELD_COMBATREACH == 62 &&
        UNIT_FIELD_DISPLAYID == 63 &&
        UNIT_FIELD_NATIVEDISPLAYID == 64 && UNIT_FIELD_MOUNTDISPLAYID == 65,
        "observer Player Unit-field projection assumes the legacy 17538 indices");
    static_assert(PLAYER_VISIBLE_ITEM_1_ENTRYID == MopUpdateObject::ObserverVisibleItemSourceStart &&
        PLAYER_CHOSEN_TITLE == MopUpdateObject::ObserverVisibleItemSourceStart +
            MopUpdateObject::ObserverVisibleItemFieldCount &&
        MopUpdateObject::ObserverVisibleItemTargetStart == 921,
        "observer Player visible-item projection must remain local 916..953 to target 921..958");
    static_assert(PLAYER_FIELD_COINAGE == 1142 && PLAYER_XP == 1144 &&
        PLAYER_NEXT_LEVEL_XP == 1145,
        "self progression projection assumes the legacy 17538 Player indices");
    static_assert(PLAYER_SKILL_LINEID_0 == MopUpdateObject::SelfSkillSourceStart &&
        PLAYER_SKILL_TALENT_0 + 64 ==
            MopUpdateObject::SelfSkillSourceStart + MopUpdateObject::SelfSkillFieldCount,
        "self skill translation must cover all seven legacy 64-word arrays");

    bool CanBuildMopInventoryObject(Object const& object, Player* target)
    {
        if (object.GetTypeId() != TYPEID_ITEM && object.GetTypeId() != TYPEID_CONTAINER)
        {
            return false;
        }

        Item const* item = static_cast<Item const*>(&object);
        ObjectGuid const& owner = item->GetOwnerGuid();
        MopUpdateObject::InventoryObjectEligibility eligibility{};
        eligibility.hasTarget = target != NULL;
        eligibility.hasOwner = !owner.IsEmpty();
        eligibility.ownerMatchesTarget = target && owner == target->GetObjectGuid();
        return MopUpdateObject::CanUseInventoryObject(eligibility);
    }

    void BuildMopUnitStaticFields(Object const& object, Player* target,
        std::vector<MopUpdateObject::StaticField>& fields)
    {
        Unit const* unit = static_cast<Unit const*>(&object);
        Creature* creature = const_cast<Creature*>(static_cast<Creature const*>(&object));
        auto add = [&fields](uint16 index, uint32 value)
        {
            fields.push_back({ index, value });
        };

        uint32 dynamicFlags = object.GetUInt32Value(UNIT_DYNAMIC_FLAGS);
        if (!creature->loot.isLooted() && !(dynamicFlags & UNIT_DYNFLAG_LOOTABLE))
        {
            creature->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
            dynamicFlags |= UNIT_DYNFLAG_LOOTABLE;
        }

        static_assert(UNIT_DYNFLAG_LOOTABLE == 0x0001 &&
            UNIT_DYNFLAG_TAPPED == 0x0004 &&
            UNIT_DYNFLAG_TAPPED_BY_PLAYER == 0x0008,
            "18414 observer projection assumes the inherited dynamic-flag bits");
        MopUpdateObject::UnitDynamicFlagView dynamicFlagView{};
        dynamicFlagView.hasLootRecipient = creature->HasLootRecipient();
        dynamicFlagView.tappedByViewer = creature->IsTappedBy(target);
        dynamicFlagView.allowedToLoot = target->isAllowedToLoot(creature);

        uint32 bytes0 = object.GetUInt32Value(UNIT_FIELD_BYTES_0);
        uint32 unitFlags = object.GetUInt32Value(UNIT_FIELD_FLAGS);
        if (target->isGameMaster())
        {
            unitFlags &= ~UNIT_FLAG_NOT_SELECTABLE;
        }

        uint32 auraState = object.GetUInt32Value(UNIT_FIELD_AURASTATE);
        if (unit->HasAuraState(AURA_STATE_CONFLAGRATE) &&
            !unit->HasAuraStateForCaster(AURA_STATE_CONFLAGRATE, target->GetObjectGuid()))
        {
            auraState &= ~(uint32(1) << (AURA_STATE_CONFLAGRATE - 1));
        }

        uint32 npcFlags = object.GetUInt32Value(UNIT_NPC_FLAGS);
        if (!target->canSeeSpellClickOn(creature))
        {
            npcFlags &= ~UNIT_NPC_FLAG_SPELLCLICK;
        }
        if ((npcFlags & UNIT_NPC_FLAG_TRAINER) && !creature->IsTrainerOf(target, false))
        {
            npcFlags &= ~(UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_TRAINER_CLASS | UNIT_NPC_FLAG_TRAINER_PROFESSION);
        }
        if ((npcFlags & UNIT_NPC_FLAG_STABLEMASTER) && target->getClass() != CLASS_HUNTER)
        {
            npcFlags &= ~UNIT_NPC_FLAG_STABLEMASTER;
        }

        add(0, object.GetUInt32Value(OBJECT_FIELD_GUID));
        add(1, object.GetUInt32Value(OBJECT_FIELD_GUID + 1));
        add(2, object.GetUInt32Value(OBJECT_FIELD_DATA));
        add(3, object.GetUInt32Value(OBJECT_FIELD_DATA + 1));
        add(4, object.GetUInt32Value(OBJECT_FIELD_TYPE));
        add(5, object.GetUInt32Value(OBJECT_FIELD_ENTRY));
        add(6, MopUpdateObject::TranslateUnitDynamicFlagsForViewer(
            dynamicFlags, dynamicFlagView));
        add(7, object.GetUInt32Value(OBJECT_FIELD_SCALE_X));
        add(30, MopUpdateObject::RepackUnitBytes0(bytes0));
        add(31, (bytes0 >> 24) & 0xFFu);
        add(32, object.GetUInt32Value(UNIT_OVERRIDE_DISPLAY_POWER_ID));
        add(33, object.GetUInt32Value(UNIT_FIELD_HEALTH));
        for (uint16 i = 0; i < 5; ++i) add(uint16(34 + i), object.GetUInt32Value(UNIT_FIELD_POWER1 + i));
        add(39, object.GetUInt32Value(UNIT_FIELD_MAXHEALTH));
        for (uint16 i = 0; i < 5; ++i) add(uint16(40 + i), object.GetUInt32Value(UNIT_FIELD_MAXPOWER1 + i));
        add(55, object.GetUInt32Value(UNIT_FIELD_LEVEL));
        add(57, object.GetUInt32Value(UNIT_FIELD_FACTIONTEMPLATE));
        for (uint16 i = 0; i < 3; ++i) add(uint16(58 + i), object.GetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + i));
        add(61, unitFlags);
        add(62, object.GetUInt32Value(UNIT_FIELD_FLAGS_2));
        add(63, auraState);
        add(64, uint32(std::max(0.0f, object.GetFloatValue(UNIT_FIELD_BASEATTACKTIME))));
        add(65, uint32(std::max(0.0f, object.GetFloatValue(UNIT_FIELD_BASEATTACKTIME + 1))));
        add(66, uint32(std::max(0.0f, object.GetFloatValue(UNIT_FIELD_RANGEDATTACKTIME))));
        add(67, object.GetUInt32Value(UNIT_FIELD_BOUNDINGRADIUS));
        add(68, object.GetUInt32Value(UNIT_FIELD_COMBATREACH));
        add(69, object.GetUInt32Value(UNIT_FIELD_DISPLAYID));
        add(70, object.GetUInt32Value(UNIT_FIELD_NATIVEDISPLAYID));
        add(71, object.GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID));
        add(76, object.GetUInt32Value(UNIT_FIELD_BYTES_1));
        add(86, object.GetUInt32Value(UNIT_CREATED_BY_SPELL));
        add(87, npcFlags);
        add(88, object.GetUInt32Value(UNIT_NPC_FLAGS + 1));
        add(89, object.GetUInt32Value(UNIT_NPC_EMOTESTATE));
        add(154, object.GetUInt32Value(UNIT_FIELD_HOVERHEIGHT));
        add(155, object.GetUInt32Value(UNIT_FIELD_MIN_ITEM_LEVEL));
        add(156, object.GetUInt32Value(UNIT_FIELD_MAXITEMLEVEL));
    }

    void BuildMopGameObjectStaticFields(Object const& object, Player* target,
        std::vector<MopUpdateObject::StaticField>& fields)
    {
        GameObject const* gameObject = static_cast<GameObject const*>(&object);
        auto add = [&fields](uint16 index, uint32 value)
        {
            fields.push_back({ index, value });
        };

        uint16 dynamicLow = 0;
        if (gameObject->ActivateToQuest(target) || target->isGameMaster())
        {
            switch (gameObject->GetGoType())
            {
                case GAMEOBJECT_TYPE_QUESTGIVER:
                    dynamicLow = GO_DYNFLAG_LO_ACTIVATE;
                    break;
                case GAMEOBJECT_TYPE_CHEST:
                case GAMEOBJECT_TYPE_GENERIC:
                case GAMEOBJECT_TYPE_SPELL_FOCUS:
                case GAMEOBJECT_TYPE_GOOBER:
                    dynamicLow = GO_DYNFLAG_LO_ACTIVATE | GO_DYNFLAG_LO_SPARKLE;
                    break;
                default:
                    break;
            }
        }
        uint32 dynamic = MopUpdateObject::TranslateGameObjectDynamic(0xFFFF0000u | dynamicLow);

        add(0, object.GetUInt32Value(OBJECT_FIELD_GUID));
        add(1, object.GetUInt32Value(OBJECT_FIELD_GUID + 1));
        add(2, object.GetUInt32Value(OBJECT_FIELD_DATA));
        add(3, object.GetUInt32Value(OBJECT_FIELD_DATA + 1));
        add(4, object.GetUInt32Value(OBJECT_FIELD_TYPE));
        add(5, object.GetUInt32Value(OBJECT_FIELD_ENTRY));
        add(6, dynamic);
        add(7, object.GetUInt32Value(OBJECT_FIELD_SCALE_X));
        add(8, object.GetUInt32Value(OBJECT_FIELD_CREATED_BY));
        add(9, object.GetUInt32Value(OBJECT_FIELD_CREATED_BY + 1));
        add(10, object.GetUInt32Value(GAMEOBJECT_DISPLAYID));
        add(11, object.GetUInt32Value(GAMEOBJECT_FLAGS));
        for (uint16 i = 0; i < 4; ++i) add(uint16(12 + i), object.GetUInt32Value(GAMEOBJECT_PARENTROTATION + i));
        add(16, object.GetUInt32Value(GAMEOBJECT_FACTION));
        add(17, object.GetUInt32Value(GAMEOBJECT_LEVEL));
    }

    void BuildMopObserverPlayerStaticFields(Object const& object,
        std::vector<MopUpdateObject::StaticField>& fields)
    {
        auto add = [&fields](uint16 index, uint32 value)
        {
            fields.push_back({ index, value });
        };
        auto addTranslated = [&object, &fields](uint16 sourceIndex, bool omitZero = false)
        {
            uint16 targetIndex = 0;
            MANGOS_ASSERT(MopUpdateObject::TranslateObserverPlayerIndex(sourceIndex, targetIndex));
            uint32 value = object.GetUInt32Value(sourceIndex);
            if (sourceIndex == UNIT_FIELD_BYTES_0)
            {
                value = MopUpdateObject::RepackUnitBytes0(value);
            }
            if (!omitZero || value != 0)
            {
                fields.push_back({ targetIndex, value });
            }
        };

        add(0, object.GetUInt32Value(OBJECT_FIELD_GUID));
        add(1, object.GetUInt32Value(OBJECT_FIELD_GUID + 1));
        add(4, 0x19u); // OBJECT | UNIT | PLAYER
        addTranslated(OBJECT_FIELD_SCALE_X);
        addTranslated(UNIT_FIELD_BYTES_0);
        addTranslated(UNIT_FIELD_HEALTH);
        addTranslated(UNIT_FIELD_MAXHEALTH);
        addTranslated(UNIT_FIELD_LEVEL);
        addTranslated(UNIT_FIELD_FACTIONTEMPLATE);
        for (uint16 i = 0; i < 3; ++i)
        {
            addTranslated(uint16(UNIT_VIRTUAL_ITEM_SLOT_ID + i), true);
        }
        addTranslated(UNIT_FIELD_DISPLAYID);
        addTranslated(UNIT_FIELD_NATIVEDISPLAYID);
        addTranslated(UNIT_FIELD_MOUNTDISPLAYID, true);
        for (uint16 i = 0; i < MopUpdateObject::ObserverVisibleItemFieldCount; ++i)
        {
            addTranslated(uint16(MopUpdateObject::ObserverVisibleItemSourceStart + i), true);
        }
    }
}

/**
 * @brief Force immediate update transmission to all viewers
 *
 * Sends all pending update changes immediately rather than waiting
 * for the next update tick. This is used for urgent updates that
 * must be visible immediately (e.g., combat state changes).
 *
 * The method builds update data for all nearby players and sends
 * it immediately, then removes the object from the pending update list.
 */
void Object::SendForcedObjectUpdate()
{
    if (!m_inWorld || !m_objectUpdated)
    {
        return;
    }

    UpdateDataMapType update_players;

    BuildUpdateData(update_players);
    RemoveFromClientUpdateList();

    WorldPacket packet;                                     // here we allocate a std::vector with a size of 0x10000
    for (UpdateDataMapType::iterator iter = update_players.begin(); iter != update_players.end(); ++iter)
    {
        iter->second.BuildPacket(&packet);
        iter->first->GetSession()->SendPacket(&packet);
        packet.clear();                                     // clean the string
    }
}

/**
 * @brief Build create update block for player
 * @param data Update data buffer
 * @param target Target player
 *
 * Builds the update packet data needed to create this object
 * for the specified player. Includes movement data and
 * all update field values.
 */
void Object::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (CanBuildMopInventoryObject(*this, target))
    {
        const uint32 valueCount = GetTypeId() == TYPEID_CONTAINER ?
            MopUpdateObject::ContainerFieldCount : MopUpdateObject::ItemFieldCount;
        MANGOS_ASSERT(m_valuesCount == valueCount);
        MopUpdateObject::AppendInventoryCreateBlock(data->GetBuffer(),
            GetObjectGuid().GetRawValue(), m_objectTypeId, m_uint32Values, valueCount);
        data->AddUpdateBlock();
        return;
    }

    if (!target || !CanBuildMopCreateUpdate())
    {
        return;
    }

    uint8 updateType = m_itsNewObject ? UPDATETYPE_CREATE_OBJECT2 : UPDATETYPE_CREATE_OBJECT;
    uint64 guid = GetObjectGuid().GetRawValue();
    std::vector<MopUpdateObject::StaticField> fields;

    if (GetTypeId() == TYPEID_DYNAMICOBJECT || GetTypeId() == TYPEID_CORPSE)
    {
        WorldObject const* worldObject = static_cast<WorldObject const*>(this);
        MopUpdateObject::PositionOnlyMovement movement{};
        movement.x = worldObject->GetPositionX();
        movement.y = worldObject->GetPositionY();
        movement.z = worldObject->GetPositionZ();
        movement.o = worldObject->GetOrientation();

        const uint32 valueCount = GetTypeId() == TYPEID_CORPSE ?
            MopUpdateObject::CorpseFieldCount : MopUpdateObject::DynamicObjectFieldCount;
        MANGOS_ASSERT(m_valuesCount == valueCount);
        MopUpdateObject::AppendPositionOnlyCreateBlock(data->GetBuffer(), updateType, guid,
            m_objectTypeId, movement, m_uint32Values, valueCount);
    }
    else if (GetTypeId() == TYPEID_UNIT || GetTypeId() == TYPEID_PLAYER)
    {
        Unit const* unit = static_cast<Unit const*>(this);
        MopUpdateObject::SimpleLivingMovement movement{};
        movement.guid = guid;
        movement.x = unit->GetPositionX();
        movement.y = unit->GetPositionY();
        movement.z = unit->GetPositionZ();
        movement.o = unit->GetOrientation();
        movement.moveTime = GameTime::GetGameTimeMS();
        movement.speedWalk = unit->GetSpeed(MOVE_WALK);
        movement.speedRun = unit->GetSpeed(MOVE_RUN);
        movement.speedRunBack = unit->GetSpeed(MOVE_RUN_BACK);
        movement.speedSwim = unit->GetSpeed(MOVE_SWIM);
        movement.speedSwimBack = unit->GetSpeed(MOVE_SWIM_BACK);
        movement.speedFlight = unit->GetSpeed(MOVE_FLIGHT);
        movement.speedFlightBack = unit->GetSpeed(MOVE_FLIGHT_BACK);
        movement.speedTurn = unit->GetSpeed(MOVE_TURN_RATE);
        movement.speedPitch = unit->GetSpeed(MOVE_PITCH_RATE);
        movement.self = false;

        if (GetTypeId() == TYPEID_PLAYER)
        {
            fields.reserve(53);
            BuildMopObserverPlayerStaticFields(*this, fields);
        }
        else
        {
            fields.reserve(47);
            BuildMopUnitStaticFields(*this, target, fields);
        }
        MopUpdateObject::AppendSimpleLivingCreateBlock(data->GetBuffer(), updateType, guid,
            m_objectTypeId, movement, fields.data(), uint32(fields.size()));
    }
    else
    {
        GameObject const* gameObject = static_cast<GameObject const*>(this);
        MopUpdateObject::StationaryGameObjectMovement movement{};
        movement.x = gameObject->GetPositionX();
        movement.y = gameObject->GetPositionY();
        movement.z = gameObject->GetPositionZ();
        movement.o = gameObject->GetOrientation();
        movement.rotation = uint64(gameObject->GetPackedWorldRotation());

        fields.reserve(18);
        BuildMopGameObjectStaticFields(*this, target, fields);
        MopUpdateObject::AppendStationaryGameObjectCreateBlock(data->GetBuffer(), updateType, guid,
            m_objectTypeId, movement, fields.data(), uint32(fields.size()));
    }
    data->AddUpdateBlock();
}

bool Object::CanBuildMopCreateUpdate() const
{
    if (GetTypeId() == TYPEID_DYNAMICOBJECT || GetTypeId() == TYPEID_CORPSE)
    {
        uint16 const supportedFlags = UPDATEFLAG_HAS_POSITION;
        WorldObject const* worldObject = static_cast<WorldObject const*>(this);
        MopUpdateObject::PositionOnlyEligibility eligibility{};
        eligibility.isBoarded = worldObject->IsBoarded();
        eligibility.hasPosition = (m_updateFlag & UPDATEFLAG_HAS_POSITION) != 0;
        eligibility.hasUnsupportedMovement = (m_updateFlag & ~supportedFlags) != 0;
        return MopUpdateObject::CanUsePositionOnlyMovement(eligibility);
    }

    if (GetTypeId() == TYPEID_GAMEOBJECT)
    {
        GameObject const* gameObject = static_cast<GameObject const*>(this);
        uint16 const supportedFlags = UPDATEFLAG_HAS_POSITION | UPDATEFLAG_ROTATION;
        MopUpdateObject::StationaryGameObjectEligibility eligibility{};
        eligibility.hasTemplate = gameObject->GetGOInfo() != NULL;
        eligibility.isDestructibleBuilding = eligibility.hasTemplate &&
            gameObject->GetGoType() == GAMEOBJECT_TYPE_DESTRUCTIBLE_BUILDING;
        eligibility.isTransport = gameObject->IsTransport();
        eligibility.isBoarded = gameObject->IsBoarded();
        eligibility.hasStationaryPosition = (m_updateFlag & UPDATEFLAG_HAS_POSITION) != 0;
        eligibility.hasRotation = (m_updateFlag & UPDATEFLAG_ROTATION) != 0;
        eligibility.hasUnsupportedMovement = (m_updateFlag & ~supportedFlags) != 0;
        return MopUpdateObject::CanUseStationaryGameObjectMovement(eligibility);
    }

    if (GetTypeId() != TYPEID_UNIT && GetTypeId() != TYPEID_PLAYER)
    {
        return false;
    }

    Unit const* unit = static_cast<Unit const*>(this);
    if (GetTypeId() == TYPEID_PLAYER && static_cast<Player const*>(this)->GetTransport() != NULL)
    {
        return false;
    }
    MovementInfo const& movement = unit->m_movementInfo;
    MovementInfo::StatusInfo const& status = movement.GetStatusInfo();
    MopUpdateObject::SimpleUnitEligibility eligibility{};
    eligibility.isVehicle = unit->IsVehicle();
    eligibility.isBoarded = unit->IsBoarded();
    eligibility.hasTransport = !movement.GetTransportGuid().IsEmpty();
    eligibility.hasSpline = unit->IsSplineEnabled();
    eligibility.movementFlags = uint32(movement.GetMovementFlags());
    eligibility.movementFlags2 = uint32(movement.GetMovementFlags2());
    eligibility.hasOptionalMovement = status.hasFallData || status.hasFallDirection || status.hasOrientation ||
        status.hasPitch || status.hasSpline || status.hasSplineElevation || status.hasTimeStamp ||
        status.hasTransportTime2 || status.hasTransportTime3 || movement.GetUnknownBit148() ||
        movement.GetUnknownBit149() || movement.GetUnknownBit172() || !movement.GetMovementForceIds().empty() ||
        movement.HasUnknownUInt32();
    eligibility.hasAttackingTarget = unit->getVictim() != NULL;
    return MopUpdateObject::CanUseSimpleUnitMovement(eligibility);
}

/**
 * @brief Send create update to player
 * @param player Target player
 *
 * Sends the create update packet to the specified player,
 * causing the object to appear in their game world.
 */
bool Object::SendCreateUpdateToPlayer(Player* player)
{
    if (!player)
    {
        return false;
    }

    // send create update to player
    UpdateData upd(player->GetMapId());
    WorldPacket packet;

    BuildCreateUpdateBlockForPlayer(&upd, player);
    if (!upd.HasData())
    {
        return false;
    }
    upd.BuildPacket(&packet);
    player->GetSession()->SendPacket(&packet);
    return true;
}

/**
 * @brief Build values update block for player
 * @param data Update data buffer
 * @param target Target player
 *
 * Builds the update packet data for changed field values
 * to send to the specified player.
 */
void Object::BuildValuesUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (GetTypeId() == TYPEID_ITEM || GetTypeId() == TYPEID_CONTAINER)
    {
        if (!CanBuildMopInventoryObject(*this, target))
        {
            return;
        }

        const uint16 valueCount = GetTypeId() == TYPEID_CONTAINER ?
            MopUpdateObject::ContainerFieldCount : MopUpdateObject::ItemFieldCount;
        std::vector<MopUpdateObject::StaticField> fields;
        fields.reserve(valueCount);
        for (uint16 i = 0; i < valueCount; ++i)
        {
            if (m_changedValues[i])
            {
                fields.push_back({ i, m_uint32Values[i] });
            }
        }
        if (!fields.empty())
        {
            MopUpdateObject::AppendInventoryValuesBlock(data->GetBuffer(),
                GetObjectGuid().GetRawValue(), m_objectTypeId, fields.data(), uint32(fields.size()));
            data->AddUpdateBlock();
        }
        return;
    }

    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (!target)
        {
            return;
        }

        std::vector<MopUpdateObject::StaticField> fields;
        if (target == static_cast<Player const*>(this))
        {
            fields.reserve(25 + MopUpdateObject::ObserverVisibleItemFieldCount +
                MopUpdateObject::SelfInventoryFieldCount +
                MopUpdateObject::SelfSkillFieldCount);
            auto addIfChanged = [this, &fields](uint16 sourceIndex)
            {
                if (m_changedValues[sourceIndex])
                {
                    fields.push_back({ sourceIndex, m_uint32Values[sourceIndex] });
                }
            };

            addIfChanged(OBJECT_FIELD_SCALE_X);
            addIfChanged(UNIT_FIELD_BYTES_0);
            addIfChanged(UNIT_FIELD_HEALTH);
            for (uint16 i = 0; i < 5; ++i)
            {
                addIfChanged(uint16(UNIT_FIELD_POWER1 + i));
            }
            addIfChanged(UNIT_FIELD_MAXHEALTH);
            for (uint16 i = 0; i < 5; ++i)
            {
                addIfChanged(uint16(UNIT_FIELD_MAXPOWER1 + i));
            }
            addIfChanged(UNIT_FIELD_LEVEL);
            addIfChanged(UNIT_FIELD_FACTIONTEMPLATE);
            addIfChanged(UNIT_FIELD_FLAGS);
            addIfChanged(UNIT_FIELD_BOUNDINGRADIUS);
            addIfChanged(UNIT_FIELD_COMBATREACH);
            addIfChanged(UNIT_FIELD_DISPLAYID);
            addIfChanged(UNIT_FIELD_NATIVEDISPLAYID);

            // Local equipment changes use the same public 18414 visible-item
            // projection as updates sent to nearby observers.
            for (uint16 i = MopUpdateObject::ObserverVisibleItemSourceStart;
                 i < MopUpdateObject::ObserverVisibleItemSourceStart +
                     MopUpdateObject::ObserverVisibleItemFieldCount; ++i)
            {
                addIfChanged(i);
            }

            for (uint16 i = MopUpdateObject::SelfInventorySourceStart;
                 i < MopUpdateObject::SelfInventorySourceStart + MopUpdateObject::SelfInventoryFieldCount; ++i)
            {
                if (m_changedValues[i])
                {
                    fields.push_back({ i, m_uint32Values[i] });
                }
            }
            addIfChanged(PLAYER_FIELD_COINAGE);
            addIfChanged(PLAYER_FIELD_COINAGE + 1);
            addIfChanged(PLAYER_XP);
            addIfChanged(PLAYER_NEXT_LEVEL_XP);
            // The 18414 client checks local skill-line/rank state before it
            // permits a language-selecting chat packet to leave the client.
            for (uint16 i = MopUpdateObject::SelfSkillSourceStart;
                 i < MopUpdateObject::SelfSkillSourceStart +
                     MopUpdateObject::SelfSkillFieldCount; ++i)
            {
                addIfChanged(i);
            }
            if (!fields.empty())
            {
                MopUpdateObject::AppendSelfPlayerValuesBlock(data->GetBuffer(),
                    GetObjectGuid().GetRawValue(), fields.data(), uint32(fields.size()));
                data->AddUpdateBlock();
            }
            return;
        }

        fields.reserve(50);
        for (uint16 i = 0; i < PLAYER_END_NOT_SELF; ++i)
        {
            uint16 targetIndex;
            if (!m_changedValues[i] || !MopUpdateObject::TranslateObserverPlayerIndex(i, targetIndex))
            {
                continue;
            }
            uint32 value = m_uint32Values[i];
            if (i == UNIT_FIELD_BYTES_0)
            {
                value = MopUpdateObject::RepackUnitBytes0(value);
            }
            fields.push_back({ targetIndex, value });
        }
        if (!fields.empty())
        {
            MopUpdateObject::AppendValuesBlock(data->GetBuffer(), GetObjectGuid().GetRawValue(),
                fields.data(), uint32(fields.size()));
            data->AddUpdateBlock();
        }
        return;
    }

    if (GetTypeId() == TYPEID_DYNAMICOBJECT || GetTypeId() == TYPEID_CORPSE)
    {
        if (!target || !CanBuildMopCreateUpdate())
        {
            return;
        }

        const uint16 valueCount = GetTypeId() == TYPEID_CORPSE ?
            MopUpdateObject::CorpseFieldCount : MopUpdateObject::DynamicObjectFieldCount;
        MANGOS_ASSERT(m_valuesCount == valueCount);
        std::vector<MopUpdateObject::StaticField> fields;
        fields.reserve(valueCount);
        for (uint16 i = 0; i < valueCount; ++i)
        {
            if (m_changedValues[i])
            {
                fields.push_back({ i, m_uint32Values[i] });
            }
        }
        if (!fields.empty())
        {
            MopUpdateObject::AppendPositionOnlyValuesBlock(data->GetBuffer(),
                GetObjectGuid().GetRawValue(), m_objectTypeId, fields.data(), uint32(fields.size()));
            data->AddUpdateBlock();
        }
        return;
    }

    if (!target || (GetTypeId() != TYPEID_UNIT && GetTypeId() != TYPEID_GAMEOBJECT) ||
        (GetTypeId() == TYPEID_GAMEOBJECT && !CanBuildMopCreateUpdate()))
    {
        return;
    }

    std::vector<MopUpdateObject::StaticField> fields;
    if (GetTypeId() == TYPEID_UNIT)
    {
        fields.reserve(47);
        BuildMopUnitStaticFields(*this, target, fields);
    }
    else
    {
        fields.reserve(18);
        BuildMopGameObjectStaticFields(*this, target, fields);
    }
    MopUpdateObject::AppendValuesBlock(data->GetBuffer(), GetObjectGuid().GetRawValue(),
        fields.data(), uint32(fields.size()));
    data->AddUpdateBlock();
}

/**
 * @brief Build out of range update block
 * @param data Update data buffer
 *
 * Adds this object's GUID to the out-of-range list,
 * indicating it should be removed from the client's view.
 */
void Object::BuildOutOfRangeUpdateBlock(UpdateData* data) const
{
    data->AddOutOfRangeGUID(GetObjectGuid());
}

/**
 * @brief Destroy object for player
 * @param target Target player
 *
 * Sends a destroy packet to the specified player,
 * removing this object from their game world.
 */
void Object::DestroyForPlayer(Player* target, bool anim) const
{
    MANGOS_ASSERT(target);

    WorldPacket data;
    MopUpdateObject::BuildDestroyObject(data, GetObjectGuid().GetRawValue(), anim);
    target->GetSession()->SendPacket(&data);
}

/**
 * @brief Build movement update block
 * @param data Byte buffer to write to
 * @param updateFlags Update flags
 *
 * Builds the movement data portion of the update packet.
 * Includes position, orientation, movement flags, and speeds
 * for living objects, or just position for static objects.
 */
void Object::BuildMovementUpdate(ByteBuffer* data, uint16 updateFlags) const
{
    ObjectGuid Guid = GetObjectGuid();

    data->WriteBit(false);
    data->WriteBit(false);
    data->WriteBit(updateFlags & UPDATEFLAG_ROTATION);
    data->WriteBit(updateFlags & UPDATEFLAG_ANIM_KITS);               // AnimKits
    data->WriteBit(updateFlags & UPDATEFLAG_HAS_ATTACKING_TARGET);
    data->WriteBit(updateFlags & UPDATEFLAG_SELF);
    data->WriteBit(updateFlags & UPDATEFLAG_VEHICLE);
    data->WriteBit(updateFlags & UPDATEFLAG_LIVING);
    data->WriteBits(0, 24);                                     // Byte Counter
    data->WriteBit(false);
    data->WriteBit(updateFlags & UPDATEFLAG_POSITION);                // flags & UPDATEFLAG_HAS_POSITION Game Object Position
    data->WriteBit(updateFlags & UPDATEFLAG_HAS_POSITION);            // Stationary Position
    data->WriteBit(updateFlags & UPDATEFLAG_TRANSPORT_ARR);
    data->WriteBit(false);
    data->WriteBit(updateFlags & UPDATEFLAG_TRANSPORT);

    bool hasTransport = false,
        isSplineEnabled = false,
        hasPitch = false,
        hasFallData = false,
        hasFallDirection = false,
        hasElevation = false,
        hasOrientation = !isType(TYPEMASK_ITEM),
        hasTimeStamp = true,
        hasTransportTime2 = false,
        hasTransportTime3 = false;

    if (isType(TYPEMASK_UNIT))
    {
        Unit const* unit = (Unit const*)this;
        hasTransport = !unit->m_movementInfo.GetTransportGuid().IsEmpty();
        isSplineEnabled = unit->IsSplineEnabled();

        if (GetTypeId() == TYPEID_PLAYER)
        {
            // use flags received from client as they are more correct
            hasPitch = unit->m_movementInfo.GetStatusInfo().hasPitch;
            hasFallData = unit->m_movementInfo.GetStatusInfo().hasFallData;
            hasFallDirection = unit->m_movementInfo.GetStatusInfo().hasFallDirection;
            hasElevation = unit->m_movementInfo.GetStatusInfo().hasSplineElevation;
            hasTransportTime2 = unit->m_movementInfo.GetStatusInfo().hasTransportTime2;
            hasTransportTime3 = unit->m_movementInfo.GetStatusInfo().hasTransportTime3;
        }
        else
        {
            hasPitch = unit->m_movementInfo.HasMovementFlag(MovementFlags(MOVEFLAG_SWIMMING | MOVEFLAG_FLYING)) ||
                            unit->m_movementInfo.HasMovementFlag2(MOVEFLAG2_ALLOW_PITCHING);
            hasFallData = unit->m_movementInfo.HasMovementFlag2(MOVEFLAG2_INTERP_TURNING);
            hasFallDirection = unit->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLING);
            hasElevation = unit->m_movementInfo.HasMovementFlag(MOVEFLAG_SPLINE_ELEVATION);
        }
    }

    if (updateFlags & UPDATEFLAG_LIVING)
    {
        Unit const* unit = (Unit const*)this;

        data->WriteBit(!unit->m_movementInfo.GetMovementFlags());
        data->WriteBit(!hasOrientation);

        data->WriteGuidMask<7, 3, 2>(Guid);

        if (unit->m_movementInfo.GetMovementFlags())
        {
            data->WriteBits(unit->m_movementInfo.GetMovementFlags(), 30);
        }

        data->WriteBit(false);
        data->WriteBit(!hasPitch);
        data->WriteBit(isSplineEnabled);
        data->WriteBit(hasFallData);
        data->WriteBit(!hasElevation);
        data->WriteGuidMask<5>(Guid);
        data->WriteBit(hasTransport);
        data->WriteBit(!hasTimeStamp);

        if (hasTransport)
        {
            ObjectGuid tGuid = unit->m_movementInfo.GetTransportGuid();

            data->WriteGuidMask<1>(tGuid);
            data->WriteBit(hasTransportTime2);
            data->WriteGuidMask<4, 0, 6>(tGuid);
            data->WriteBit(hasTransportTime3);
            data->WriteGuidMask<7, 5, 3, 2>(tGuid);
        }

        data->WriteGuidMask<4>(Guid);

        if (isSplineEnabled)
        {
            Movement::PacketBuilder::WriteCreateBits(*unit->movespline, *data);
        }

        data->WriteGuidMask<6>(Guid);

        if (hasFallData)
        {
            data->WriteBit(hasFallDirection);
        }

        data->WriteGuidMask<0, 1>(Guid);
        data->WriteBit(false);    // Unknown 4.3.3
        data->WriteBit(!unit->m_movementInfo.GetMovementFlags2());

        if (unit->m_movementInfo.GetMovementFlags2())
        {
            data->WriteBits(unit->m_movementInfo.GetMovementFlags2(), 12);
        }
    }

    // used only with GO's, placeholder
    if (updateFlags & UPDATEFLAG_POSITION)
    {
        ObjectGuid transGuid;
        data->WriteGuidMask<5>(transGuid);
        data->WriteBit(hasTransportTime3);
        data->WriteGuidMask<0, 3, 6, 1, 4, 2>(transGuid);
        data->WriteBit(hasTransportTime2);
        data->WriteGuidMask<7>(transGuid);
    }

    if (updateFlags & UPDATEFLAG_HAS_ATTACKING_TARGET)
    {
        ObjectGuid guid;
        if (Unit* victim = ((Unit*)this)->getVictim())
        {
            guid = victim->GetObjectGuid();
        }

        data->WriteGuidMask<2, 7, 0, 4, 5, 6, 1, 3>(guid);
    }

    if (updateFlags & UPDATEFLAG_ANIM_KITS)
    {
        data->WriteBit(true);   // hasAnimKit0 == false
        data->WriteBit(true);   // hasAnimKit1 == false
        data->WriteBit(true);   // hasAnimKit2 == false
    }

    data->FlushBits();

    if (updateFlags & UPDATEFLAG_LIVING)
    {
        Unit const* unit = (Unit const*)this;

        data->WriteGuidBytes<4>(Guid);

        *data << float(unit->GetSpeed(MOVE_RUN_BACK));

        if (hasFallData)
        {
            if (hasFallDirection)
            {
                // 15595 client reads horizontal speed, then jump direction sin/cos
                *data << float(unit->m_movementInfo.GetJumpInfo().xyspeed);
                *data << float(unit->m_movementInfo.GetJumpInfo().sinAngle);
                *data << float(unit->m_movementInfo.GetJumpInfo().cosAngle);
            }

            *data << uint32(unit->m_movementInfo.GetFallTime());
            *data << float(unit->m_movementInfo.GetJumpInfo().velocity);
        }

        *data << float(unit->GetSpeed(MOVE_SWIM_BACK));

        if (hasElevation)
        {
            *data << float(unit->m_movementInfo.GetSplineElevation());
        }

        if (isSplineEnabled)
        {
            Movement::PacketBuilder::WriteCreateBytes(*unit->movespline, *data);
        }

        *data << float(unit->GetPositionZ());
        data->WriteGuidBytes<5>(Guid);

        if (hasTransport)
        {
            ObjectGuid tGuid = unit->m_movementInfo.GetTransportGuid();

            data->WriteGuidBytes<5, 7>(tGuid);
            *data << uint32(unit->m_movementInfo.GetTransportTime());
            *data << float(NormalizeOrientation(unit->m_movementInfo.GetTransportPos()->o));

            if (hasTransportTime2)
            {
                *data << uint32(unit->m_movementInfo.GetTransportTime2());
            }

            *data << float(unit->m_movementInfo.GetTransportPos()->y);
            *data << float(unit->m_movementInfo.GetTransportPos()->x);
            data->WriteGuidBytes<3>(tGuid);
            *data << float(unit->m_movementInfo.GetTransportPos()->z);
            data->WriteGuidBytes<0>(tGuid);

            if (hasTransportTime3)
            {
                *data << uint32(unit->m_movementInfo.GetFallTime());
            }

            *data << int8(unit->m_movementInfo.GetTransportSeat());
            data->WriteGuidBytes<1, 6, 2, 4>(tGuid);
        }

        *data << float(unit->GetPositionX());
        *data << float(unit->GetSpeed(MOVE_PITCH_RATE));
        data->WriteGuidBytes<3, 0>(Guid);
        *data << float(unit->GetSpeed(MOVE_SWIM));
        *data << float(unit->GetPositionY());
        data->WriteGuidBytes<7, 1, 2>(Guid);
        *data << float(unit->GetSpeed(MOVE_WALK));

        *data << uint32(GameTime::GetGameTimeMS());

        *data << float(unit->GetSpeed(MOVE_FLIGHT_BACK));
        data->WriteGuidBytes<6>(Guid);
        *data << float(unit->GetSpeed(MOVE_TURN_RATE));

        if (hasOrientation)
        {
            *data << float(NormalizeOrientation(unit->GetOrientation()));
        }

        *data << float(unit->GetSpeed(MOVE_RUN));

        if (hasPitch)
        {
            *data << float(unit->m_movementInfo.GetPitch());
        }

        *data << float(unit->GetSpeed(MOVE_FLIGHT));
    }

    if (updateFlags & UPDATEFLAG_VEHICLE)
    {
        *data << float(NormalizeOrientation(((WorldObject*)this)->GetOrientation()));
        *data << uint32(((Unit*)this)->GetVehicleInfo()->GetVehicleEntry()->ID); // vehicle id
    }

    // used only with GO's, placeholder
    if (updateFlags & UPDATEFLAG_POSITION)
    {
        ObjectGuid transGuid;

        data->WriteGuidBytes<0, 5>(transGuid);
        if (hasTransportTime3)
        {
            *data << uint32(0);
        }

        data->WriteGuidBytes<3>(transGuid);
        *data << float(0.0f);   // x offset
        data->WriteGuidBytes<4, 6, 1>(transGuid);
        *data << uint32(0);     // transport time
        *data << float(0.0f);   // y offset
        data->WriteGuidBytes<2, 7>(transGuid);
        *data << float(0.0f);   // z offset
        *data << int8(-1);      // transport seat
        *data << float(0.0f);   // o offset

        if (hasTransportTime2)
        {
            *data << uint32(0);
        }
    }

    if (updateFlags & UPDATEFLAG_ROTATION)
    {
        *data << int64(((GameObject*)this)->GetPackedWorldRotation());
    }

    if (updateFlags & UPDATEFLAG_TRANSPORT_ARR)
    {
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << uint8(0);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
        *data << float(0.0f);
    }

    if (updateFlags & UPDATEFLAG_HAS_POSITION)
    {
        *data << float(NormalizeOrientation(((WorldObject*)this)->GetOrientation()));
        *data << float(((WorldObject*)this)->GetPositionX());
        *data << float(((WorldObject*)this)->GetPositionY());
        *data << float(((WorldObject*)this)->GetPositionZ());
    }

    if (updateFlags & UPDATEFLAG_HAS_ATTACKING_TARGET)
    {
        ObjectGuid guid;
        if (Unit* victim = ((Unit*)this)->getVictim())
        {
            guid = victim->GetObjectGuid();
        }

        data->WriteGuidBytes<4, 0, 3, 5, 7, 6, 2, 1>(guid);
    }

    if (updateFlags & UPDATEFLAG_TRANSPORT)
    {
        *data << uint32(GameTime::GetGameTimeMS());           // ms time
    }
}

/**
 * @brief Build values update data
 * @param updatetype Update type (create or values)
 * @param data Byte buffer to write to
 * @param updateMask Update mask indicating which fields changed
 * @param target Target player
 *
 * Builds the actual field value data for the update packet.
 * Handles special cases for gameobjects and units.
 */
void Object::BuildValuesUpdate(uint8 updatetype, ByteBuffer* data, UpdateMask* updateMask, Player* target) const
{
    if (!target)
    {
        return;
    }

    uint32 valuesCount = m_valuesCount;
    if (GetTypeId() == TYPEID_PLAYER && target != this)
    {
        valuesCount = PLAYER_END_NOT_SELF;
    }

    bool IsActivateToQuest = false;
    bool IsPerCasterAuraState = false;

    if (updatetype == UPDATETYPE_CREATE_OBJECT || updatetype == UPDATETYPE_CREATE_OBJECT2)
    {
        if (isType(TYPEMASK_GAMEOBJECT) && !((GameObject*)this)->IsTransport())
        {
            if (((GameObject*)this)->ActivateToQuest(target) || target->isGameMaster())
            {
                IsActivateToQuest = true;
            }

            updateMask->SetBit(GAMEOBJECT_DYNAMIC);
        }
        else if (isType(TYPEMASK_UNIT))
        {
            if (((Unit*)this)->HasAuraState(AURA_STATE_CONFLAGRATE))
            {
                IsPerCasterAuraState = true;
                updateMask->SetBit(UNIT_FIELD_AURASTATE);
            }
        }
    }
    else                                                    // case UPDATETYPE_VALUES
    {
        if (isType(TYPEMASK_GAMEOBJECT) && !((GameObject*)this)->IsTransport())
        {
            if (((GameObject*)this)->ActivateToQuest(target) || target->isGameMaster())
            {
                IsActivateToQuest = true;
            }

            updateMask->SetBit(GAMEOBJECT_DYNAMIC);
            updateMask->SetBit(GAMEOBJECT_BYTES_1);         // why do we need this here?
        }
        else if (isType(TYPEMASK_UNIT))
        {
            if (((Unit*)this)->HasAuraState(AURA_STATE_CONFLAGRATE))
            {
                IsPerCasterAuraState = true;
                updateMask->SetBit(UNIT_FIELD_AURASTATE);
            }
        }
    }

    MANGOS_ASSERT(updateMask && updateMask->GetCount() == m_valuesCount);

    *data << (uint8)updateMask->GetBlockCount();
    data->append(updateMask->GetMask(), updateMask->GetLength());

    // 2 specialized loops for speed optimization in non-unit case
    if (isType(TYPEMASK_UNIT))                              // unit (creature/player) case
    {
        for (uint16 index = 0; index < valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                if (index == UNIT_NPC_FLAGS)
                {
                    uint32 appendValue = m_uint32Values[index];

                    if (GetTypeId() == TYPEID_UNIT)
                    {
                        if (!target->canSeeSpellClickOn((Creature*)this))
                        {
                            appendValue &= ~UNIT_NPC_FLAG_SPELLCLICK;
                        }

                        if (appendValue & UNIT_NPC_FLAG_TRAINER)
                        {
                            if (!((Creature*)this)->IsTrainerOf(target, false))
                            {
                                appendValue &= ~(UNIT_NPC_FLAG_TRAINER | UNIT_NPC_FLAG_TRAINER_CLASS | UNIT_NPC_FLAG_TRAINER_PROFESSION);
                            }
                        }

                        if (appendValue & UNIT_NPC_FLAG_STABLEMASTER)
                        {
                            if (target->getClass() != CLASS_HUNTER)
                            {
                                appendValue &= ~UNIT_NPC_FLAG_STABLEMASTER;
                            }
                        }
                    }

                    *data << uint32(appendValue);
                }
                else if (index == UNIT_FIELD_AURASTATE)
                {
                    if (IsPerCasterAuraState)
                    {
                        // IsPerCasterAuraState set if related pet caster aura state set already
                        if (((Unit*)this)->HasAuraStateForCaster(AURA_STATE_CONFLAGRATE, target->GetObjectGuid()))
                        {
                            *data << m_uint32Values[index];
                        }
                        else
                        {
                            *data << (m_uint32Values[index] & ~(1 << (AURA_STATE_CONFLAGRATE - 1)));
                        }
                    }
                    else
                    {
                        *data << m_uint32Values[index];
                    }
                }
                // FIXME: Some values at server stored in float format but must be sent to client in uint32 format
                else if (index >= UNIT_FIELD_BASEATTACKTIME && index <= UNIT_FIELD_RANGEDATTACKTIME)
                {
                    // convert from float to uint32 and send
                    *data << uint32(m_floatValues[index] < 0 ? 0 : m_floatValues[index]);
                }

                // there are some float values which may be negative or can't get negative due to other checks
                else if ((index >= UNIT_FIELD_NEGSTAT0 && index <= UNIT_FIELD_NEGSTAT4) ||
                         (index >= UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE  && index <= (UNIT_FIELD_RESISTANCEBUFFMODSPOSITIVE + 6)) ||
                         (index >= UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE  && index <= (UNIT_FIELD_RESISTANCEBUFFMODSNEGATIVE + 6)) ||
                         (index >= UNIT_FIELD_POSSTAT0 && index <= UNIT_FIELD_POSSTAT4))
                {
                    *data << uint32(m_floatValues[index]);
                }

                // Gamemasters should be always able to select units - remove not selectable flag
                else if (index == UNIT_FIELD_FLAGS && target->isGameMaster())
                {
                    *data << (m_uint32Values[index] & ~UNIT_FLAG_NOT_SELECTABLE);
                }
                /* Hide loot animation for players that aren't permitted to loot the corpse */
                else if (index == UNIT_DYNAMIC_FLAGS && GetTypeId() == TYPEID_UNIT)
                {
                    uint32 send_value = m_uint32Values[index];

                    /* Initiate pointer to creature so we can check loot */
                    if (Creature* my_creature = (Creature*)this)
                        /* If the creature is NOT fully looted */
                        if (!my_creature->loot.isLooted())
                            /* If the lootable flag is NOT set */
                            if (!(send_value & UNIT_DYNFLAG_LOOTABLE))
                            {
                                /* Update it on the creature */
                                my_creature->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                                /* Update it in the packet */
                                send_value = send_value | UNIT_DYNFLAG_LOOTABLE;
                            }

                    /* If we're not allowed to loot the target, destroy the lootable flag */
                    if (!target->isAllowedToLoot((Creature*)this))
                        if (send_value & UNIT_DYNFLAG_LOOTABLE)
                        {
                            send_value = send_value & ~UNIT_DYNFLAG_LOOTABLE;
                        }

                    /* If we are allowed to loot it and mob is tapped by us, destroy the tapped flag */
                    bool is_tapped = target->IsTappedByMeOrMyGroup((Creature*)this);

                    /* If the creature has tapped flag but is tapped by us, remove the flag */
                    if (send_value & UNIT_DYNFLAG_TAPPED && is_tapped)
                    {
                        send_value = send_value & ~UNIT_DYNFLAG_TAPPED;
                    }

                    *data << send_value;
                }
                else
                {
                    // send in current format (float as float, uint32 as uint32)
                    *data << m_uint32Values[index];
                }
            }
        }
    }
    else if (isType(TYPEMASK_GAMEOBJECT))                   // gameobject case
    {
        for (uint16 index = 0; index < valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                // send in current format (float as float, uint32 as uint32)
                if (index == GAMEOBJECT_DYNAMIC)
                {
                    // GAMEOBJECT_TYPE_DUNGEON_DIFFICULTY can have lo flag = 2
                    //      most likely related to "can enter map" and then should be 0 if can not enter

                    if (IsActivateToQuest)
                    {
                        switch (((GameObject*)this)->GetGoType())
                        {
                            case GAMEOBJECT_TYPE_QUESTGIVER:
                                // GO also seen with GO_DYNFLAG_LO_SPARKLE explicit, relation/reason unclear (192861)
                                *data << uint16(GO_DYNFLAG_LO_ACTIVATE);
                                *data << uint16(-1);
                                break;
                            case GAMEOBJECT_TYPE_CHEST:
                            case GAMEOBJECT_TYPE_GENERIC:
                            case GAMEOBJECT_TYPE_SPELL_FOCUS:
                            case GAMEOBJECT_TYPE_GOOBER:
                                *data << uint16(GO_DYNFLAG_LO_ACTIVATE | GO_DYNFLAG_LO_SPARKLE);
                                *data << uint16(-1);
                                break;
                            default:
                                // unknown, not happen.
                                *data << uint16(0);
                                *data << uint16(-1);
                                break;
                        }
                    }
                    else
                    {
                        // disable quest object
                        *data << uint16(0);
                        *data << uint16(-1);
                    }
                }
                else if (index == GAMEOBJECT_BYTES_1)
                {
                    if (((GameObject*)this)->GetGOInfo()->type == GAMEOBJECT_TYPE_TRANSPORT)
                    {
                        *data << uint32(m_uint32Values[index] | GO_STATE_TRANSPORT_SPEC);
                    }
                    else
                    {
                        *data << uint32(m_uint32Values[index]);
                    }
                }
                else
                {
                    *data << m_uint32Values[index];          // other cases
                }
            }
        }
    }
    else                                                    // other objects case (no special index checks)
    {
        for (uint16 index = 0; index < valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                // send in current format (float as float, uint32 as uint32)
                *data << m_uint32Values[index];
            }
        }
    }

    // 18414 always follows the static update-field values with the dynamic-
    // field presence-mask word count. Four does not model dynamic update fields
    // yet, so terminate the section explicitly with a zero count.
    *data << uint8(0);
}

/**
 * @brief Clear update mask
 * @param remove If true, remove from client update list
 *
 * Clears all changed value flags and optionally removes
 * the object from the pending update list.
 */
void Object::ClearUpdateMask(bool remove)
{
    if (m_uint32Values)
    {
        for (uint16 index = 0; index < m_valuesCount; ++index)
        {
            m_changedValues[index] = false;
        }
    }

    if (m_objectUpdated)
    {
        if (remove)
        {
            RemoveFromClientUpdateList();
        }
        m_objectUpdated = false;
    }
}

/**
 * @brief Load values from data string
 * @param data Data string to load from
 * @return True if successful
 *
 * Loads update field values from a character data string.
 * Used when loading objects from database.
 */
bool Object::LoadValues(const char* data)
{
    if (!m_uint32Values)
    {
        _InitValues();
    }

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != m_valuesCount)
    {
        return false;
    }

    Tokens::iterator iter;
    int index;
    for (iter = tokens.begin(), index = 0; index < m_valuesCount; ++iter, ++index)
    {
        m_uint32Values[index] = std::stoul((*iter).c_str());
    }

    return true;
}

/**
 * @brief Set update bits in mask
 * @param updateMask Update mask to modify
 * @param target Target player (unused)
 *
 * Sets bits in the update mask for all fields that have changed.
 */
void Object::_SetUpdateBits(UpdateMask* updateMask, Player* target) const
{
    uint32 valuesCount = m_valuesCount;
    if (GetTypeId() == TYPEID_PLAYER && target != this)
    {
        valuesCount = PLAYER_END_NOT_SELF;
    }

    for (uint16 index = 0; index < valuesCount; ++index )
        if (m_changedValues[index])
        {
            updateMask->SetBit(index);
        }
}

/**
 * @brief Set create bits in mask
 * @param updateMask Update mask to modify
 * @param target Target player (unused)
 *
 * Sets bits in the update mask for all non-zero fields.
 * Used when creating a new object for a player.
 */
void Object::_SetCreateBits(UpdateMask* updateMask, Player* target) const
{
    uint32 valuesCount = m_valuesCount;
    if (GetTypeId() == TYPEID_PLAYER && target != this)
    {
        valuesCount = PLAYER_END_NOT_SELF;
    }

    for (uint16 index = 0; index < valuesCount; ++index)
        if (GetUInt32Value(index) != 0)
        {
            updateMask->SetBit(index);
        }
}
