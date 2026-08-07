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
    /// Smallest magnitude the 18414 client will accept for a movement speed.
    ///
    /// sub_45B733 compares with sub_409DD6(a, b, 0.00000023841858), i.e. an
    /// approximately-equal test at 2^-22, and sub_768D2F rejects the create when
    /// any of the nine speeds is approximately equal to zero. The bound is
    /// exclusive (`eps > fabs(a - b)` fails the comparison), so exactly epsilon
    /// is already accepted; the floor below sits an order of magnitude clear of
    /// it so no later rounding can walk back across.
    float const MIN_WIRE_SPEED = 0.000001f;

    /// Force a speed into the range the client's create validator accepts.
    ///
    /// Speeds are conceptually non-negative, so this floors rather than
    /// preserving the sign: creature_template ships negative denormals
    /// (-3.72738e-21 on 55151, -2.97773e-20 on 61928) which are meaningless as
    /// speeds and would be rejected on magnitude anyway.
    float SanitizeWireSpeed(float speed)
    {
        return speed < MIN_WIRE_SPEED ? MIN_WIRE_SPEED : speed;
    }

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
    static_assert(PLAYER_FIELD_BUYBACK_PRICE_1 == MopUpdateObject::SelfBuybackSourceStart &&
        PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + 12 ==
            MopUpdateObject::SelfBuybackSourceStart + MopUpdateObject::SelfBuybackFieldCount,
        "self buyback translation must cover the legacy price and timestamp arrays");
    static_assert(PLAYER_EXPLORED_ZONES_1 == MopUpdateObject::SelfExploredSourceStart &&
        PLAYER_REST_STATE_EXPERIENCE == MopUpdateObject::SelfExploredSourceEnd,
        "explored-zone and rested-pool projection must stay contiguous and anchored");
    static_assert(PLAYER_QUEST_LOG_1_1 == MopUpdateObject::SelfQuestLogSourceStart &&
        PLAYER_QUEST_LOG_50_5 + 1 ==
            MopUpdateObject::SelfQuestLogSourceStart + MopUpdateObject::SelfQuestLogFieldCount,
        "self quest-log translation must cover all fifty legacy five-word slots");
    static_assert(MAX_QUEST_OFFSET == MopUpdateObject::SelfQuestLogSourceStride,
        "legacy quest slot stride must match the projection's source stride");
    static_assert(PLAYER_QUEST_LOG_2_1 - PLAYER_QUEST_LOG_1_1 ==
        MopUpdateObject::SelfQuestLogSourceStride,
        "legacy quest slots must remain contiguous at the projected stride");

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
        // Current target, as a two-word GUID at CGUnitData::target. Without it
        // a client cannot show what a creature is attacking, which is what
        // drives target-of-target on a targeted mob.
        add(22, object.GetUInt32Value(UNIT_FIELD_TARGET));
        add(23, object.GetUInt32Value(UNIT_FIELD_TARGET + 1));
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
        uint32 transportTime, std::vector<MopUpdateObject::StaticField>& fields)
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
        uint16 pathProgress = 0xFFFFu;
        if (gameObject->GetGoType() == GAMEOBJECT_TYPE_MO_TRANSPORT)
        {
            // The dynamic high word is the route clock normalized to 16 bits,
            // not the TaxiPath identifier. The client combines it with LEVEL
            // (the route period) and the transport-time movement branch.
            uint32 const period = object.GetUInt32Value(GAMEOBJECT_LEVEL);
            if (period != 0)
            {
                float const timer = float(transportTime % period);
                pathProgress = uint16(timer / float(period) * 65535.0f + 0.5f);
            }
        }
        uint32 const legacyDynamic = (uint32(pathProgress) << 16) | dynamicLow;
        uint32 dynamic = MopUpdateObject::TranslateGameObjectDynamic(legacyDynamic);

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
        // Byte 1 of this field is the GAMEOBJECT_TYPE. Omitting it left every
        // gameobject looking like type 0 (DOOR) to the client, which routes the
        // display record's model name into the M2 cache. That cache rejects any
        // extension other than .m2/.mdl/.mdx and returns null, and the caller
        // releases the null handle without checking it. WMO-backed types read
        // their type byte here to take the branch that skips the M2 load
        // entirely, so this field is what keeps them off that path.
        add(18, object.GetUInt32Value(GAMEOBJECT_BYTES_1));
        // Build 18414 has one additional gameobject value slot. Retail type-15
        // creates include it as zero, producing the 0x000FFFFF values mask.
        add(19, 0);
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
            else if (sourceIndex == UNIT_FIELD_FLAGS)
            {
                value = MopUpdateObject::ProjectPlayerUnitFlags(value);
            }
            if (!omitZero || value != 0)
            {
                fields.push_back({ targetIndex, value });
            }
        };

        // Object block: only guid, type and scale. m_data (2/3), m_entryID (5)
        // and m_dynamicFlags (6) are deliberately NOT sent - the player's own
        // self create does not carry them either, there is no corresponding
        // incremental translation (so a guild change would go stale), index 5
        // is zero for players, and index 6 has viewer-relative semantics that
        // this builder cannot evaluate.
        add(0, object.GetUInt32Value(OBJECT_FIELD_GUID));
        add(1, object.GetUInt32Value(OBJECT_FIELD_GUID + 1));
        add(4, 0x19u); // OBJECT | UNIT | PLAYER
        addTranslated(OBJECT_FIELD_SCALE_X);
        // Current target, so an observer's UI can show target-of-target and
        // who is targeting them. Emitted unconditionally: zero is the real
        // "no target" value and an observer that only saw the create would
        // otherwise start from whatever the client defaults to.
        addTranslated(UNIT_FIELD_TARGET);
        addTranslated(uint16(UNIT_FIELD_TARGET + 1));
        addTranslated(UNIT_FIELD_BYTES_0);
        // Byte 3 of the packed bytes0 word is carried separately at 31, the
        // same split TranslateSelfPlayerFields performs for the owner. The
        // observer incremental path emits both 30 and 31 for a change to this
        // source, so a power-type change does not leave 31 stale.
        add(31, (object.GetUInt32Value(UNIT_FIELD_BYTES_0) >> 24) & 0xFFu);
        addTranslated(UNIT_FIELD_HEALTH);
        // Powers and max powers. The creature create and the player's own self
        // create have always carried these; the observer create did not, which
        // left a watcher with no power values for a targeted player.
        for (uint16 i = 0; i < 5; ++i)
        {
            addTranslated(uint16(UNIT_FIELD_POWER1 + i));
        }
        addTranslated(UNIT_FIELD_MAXHEALTH);
        for (uint16 i = 0; i < 5; ++i)
        {
            addTranslated(uint16(UNIT_FIELD_MAXPOWER1 + i));
        }
        addTranslated(UNIT_FIELD_LEVEL);
        addTranslated(UNIT_FIELD_FACTIONTEMPLATE);
        for (uint16 i = 0; i < 3; ++i)
        {
            addTranslated(uint16(UNIT_VIRTUAL_ITEM_SLOT_ID + i), true);
        }
        // Unit flags, so an observer sees another player's combat, stun,
        // fear and silence state. Emitted unconditionally: zero is a real
        // value here (a player standing idle out of combat), and an observer
        // that only ever saw the create would otherwise start from whatever
        // the client defaults to rather than from the truth.
        addTranslated(UNIT_FIELD_FLAGS);
        // flags2 carries genuinely visible state such as feign death and
        // transforms, and is already in updateVisualBits.
        //
        // Deliberately NOT sent alongside it:
        //  - 63 auraState is viewer-relative. The creature projection masks
        //    AURA_STATE_CONFLAGRATE for viewers who are not the relevant
        //    caster; this builder has no target and would send the raw word.
        //  - 64/65 attack timers are stored as FLOAT (Player uses
        //    SetFloatValue), so reading them as uint32 here would ship the
        //    IEEE-754 bit pattern rather than milliseconds. The creature path
        //    converts explicitly; until that conversion and a proven need
        //    exist, they stay out.
        //  - 66 ranged attack time is classified PRIVATE.
        addTranslated(UNIT_FIELD_FLAGS_2);
        // Model geometry. Both the creature create and the owner's own create
        // carry these; without them a watcher has no bounding radius or combat
        // reach for the player it is rendering.
        addTranslated(UNIT_FIELD_BOUNDINGRADIUS);
        addTranslated(UNIT_FIELD_COMBATREACH);
        addTranslated(UNIT_FIELD_DISPLAYID);
        addTranslated(UNIT_FIELD_NATIVEDISPLAYID);
        addTranslated(UNIT_FIELD_MOUNTDISPLAYID, true);
        // Stand state and animation tier. Emitted unconditionally because zero
        // is the ordinary standing value and an observer that never received
        // it renders the player from whatever the client defaults to. Without
        // this a watcher cannot see another player sit, kneel, or hold a
        // looping state emote - the emote state at 89 below arrives correctly
        // but the client will not play it.
        addTranslated(UNIT_FIELD_BYTES_1);
        // Emote state is deliberately NOT sent here.
        //
        // The 18414 client starts a player state-emote animation from a change
        // to this field, not from its initial create value. Carrying it in the
        // create seeds the client's cached value, after which every later
        // update repeats the same number and is a no-op - which is why sending
        // it again, in the same packet or a later one, never made the player
        // animate. GridNotifiers sends it as a standalone update immediately
        // after this create instead, so the client observes a real 0 -> N
        // transition. See VisibleNotifier::Notify.
        //
        // Creatures are unaffected: BuildMopUnitStaticFields still carries the
        // field, and a creature create demonstrably does animate.
        // The packed appearance words. These must be in the CREATE, not left
        // to the changed-value path: Object::ClearUpdateMask drops the change
        // flags on entering the world, and PLAYER_BYTES in particular never
        // changes again afterwards, so an observer that only ever saw the
        // create would otherwise never learn this player's hair, facial hair
        // or gender at all. Zero is meaningful for all three (male, sober,
        // no arena faction), so they are emitted unconditionally.
        addTranslated(PLAYER_BYTES);
        addTranslated(PLAYER_BYTES_2);
        addTranslated(PLAYER_BYTES_3);
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

        // Every speed on the wire must be far enough from zero for the client to
        // accept the block. Its create validator (sub_768D2F, reached from
        // sub_769816 via sub_7691A4 when the LIVING bit is set) tests all nine
        // speeds with an approximately-equal-to-zero comparison at epsilon
        // 2.3841858e-7 = 2^-22, and rejects the object if any of them is nearer
        // to zero than that. A rejected create returns 0 from sub_79DC30, which
        // makes the block loop in sub_79E087 BREAK -- so the object is lost and
        // so is every later block in the same packet, silently, with no reply.
        //
        // Zero is not hypothetical and is not always wrong data. A totem SHOULD
        // have SpeedWalk 0, because a totem does not walk, and Unit::UpdateSpeed
        // multiplies straight through it (UnitSpeed.cpp:246) with no validation:
        //
        //     3968 Sentry Totem, 5923/5924 Cleansing Totem, 5926 Frost
        //     Resistance Totem, 7467 Nature Resistance Totem, 15803 Tranquil Air
        //     Totem, 17539 Totem of Wrath, 30527 Training Dummy -- SpeedWalk 0
        //
        // plus 55151 Rumpus Brute, 57421 Mothran and 61928 Sik'thik Guardian,
        // which carry corrupt denormals (-3.7e-21, 3.1e-30). 64 rows in
        // creature_template have a non-positive walk or run speed. A player
        // gaining sight of any of them lost the rest of that update packet.
        //
        // SetSpeedRate also clamps a negative rate to exactly 0.0f
        // (UnitSpeed.cpp:284), so a -100% snare reaches here as a true zero too.
        //
        // Clamping HERE rather than in UpdateSpeed is deliberate: the unit keeps
        // its real speed rate and still does not move, and the correction cannot
        // be bypassed by a future producer, because this is the only place the
        // create block is filled in.
        movement.speedWalk = SanitizeWireSpeed(unit->GetSpeed(MOVE_WALK));
        movement.speedRun = SanitizeWireSpeed(unit->GetSpeed(MOVE_RUN));
        movement.speedRunBack = SanitizeWireSpeed(unit->GetSpeed(MOVE_RUN_BACK));
        movement.speedSwim = SanitizeWireSpeed(unit->GetSpeed(MOVE_SWIM));
        movement.speedSwimBack = SanitizeWireSpeed(unit->GetSpeed(MOVE_SWIM_BACK));
        movement.speedFlight = SanitizeWireSpeed(unit->GetSpeed(MOVE_FLIGHT));
        movement.speedFlightBack = SanitizeWireSpeed(unit->GetSpeed(MOVE_FLIGHT_BACK));
        movement.speedTurn = SanitizeWireSpeed(unit->GetSpeed(MOVE_TURN_RATE));
        movement.speedPitch = SanitizeWireSpeed(unit->GetSpeed(MOVE_PITCH_RATE));
        movement.self = false;

        MovementInfo const& movementInfo = unit->m_movementInfo;
        if (!movementInfo.GetTransportGuid().IsEmpty())
        {
            Position const* transportPosition = movementInfo.GetTransportPos();
            movement.transportGuid = movementInfo.GetTransportGuid().GetRawValue();
            movement.transportX = transportPosition->x;
            movement.transportY = transportPosition->y;
            movement.transportZ = transportPosition->z;
            movement.transportO = transportPosition->o;
            movement.transportTime = movementInfo.GetTransportTime();
            movement.transportTime2 = movementInfo.GetTransportTime2();
            movement.transportTime3 = movementInfo.GetTransportTime3();
            movement.transportSeat = movementInfo.GetTransportSeat();
            movement.hasTransportTime2 = movementInfo.GetStatusInfo().hasTransportTime2;
            movement.hasTransportTime3 = movementInfo.GetStatusInfo().hasTransportTime3;
        }

        if (GetTypeId() == TYPEID_PLAYER)
        {
            fields.reserve(53);
            BuildMopObserverPlayerStaticFields(*this, fields);
        }
        else
        {
            // 49: BuildMopUnitStaticFields emits 47 plus the two target words.
            fields.reserve(49);
            BuildMopUnitStaticFields(*this, target, fields);
        }
        MopUpdateObject::AppendSimpleLivingCreateBlock(data->GetBuffer(), updateType, guid,
            m_objectTypeId, movement, fields.data(), uint32(fields.size()));
    }
    else
    {
        GameObject const* gameObject = static_cast<GameObject const*>(this);
        bool const isMoTransport = gameObject->GetGoType() == GAMEOBJECT_TYPE_MO_TRANSPORT;
        MopUpdateObject::StationaryGameObjectMovement movement{};
        // MO_TRANSPORT route coordinates are client-interpolated. Its create
        // snapshot supplies the local origin and the shared route clock.
        movement.x = isMoTransport ? 0.0f : gameObject->GetPositionX();
        movement.y = isMoTransport ? 0.0f : gameObject->GetPositionY();
        movement.z = isMoTransport ? 0.0f : gameObject->GetPositionZ();
        movement.o = gameObject->GetOrientation();
        movement.transportTime = isMoTransport ? GameTime::GetGameTimeMS() : 0;
        movement.rotation = uint64(gameObject->GetPackedWorldRotation());
        movement.isTransport = isMoTransport;

        fields.reserve(20);
        BuildMopGameObjectStaticFields(*this, target, movement.transportTime, fields);
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
        bool const isMoTransport = gameObject->GetGoType() == GAMEOBJECT_TYPE_MO_TRANSPORT;
        if (gameObject->IsTransport() && !isMoTransport)
        {
            return false;
        }
        uint16 supportedFlags = UPDATEFLAG_HAS_POSITION | UPDATEFLAG_ROTATION;
        if (isMoTransport)
        {
            supportedFlags |= UPDATEFLAG_TRANSPORT;
        }
        MopUpdateObject::StationaryGameObjectEligibility eligibility{};
        eligibility.hasTemplate = gameObject->GetGOInfo() != NULL;
        eligibility.isTransport = isMoTransport;
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
    // Snapshot policy: the projected create block carries only a stationary
    // position snapshot (no movement flags, no spline data, no attack
    // target), so a unit that is merely moving or fighting can still be
    // created from that snapshot -- the already-ported 18414
    // SMSG_MONSTER_MOVE stream animates it afterwards (and the current
    // spline is re-sent on visibility gain, see Unit::SendCurrentSplineTo).
    // Gating on these states made every wandering, patrolling or fighting
    // creature invisible. Only genuinely unprojectable states (vehicles,
    // vehicle boarding and exotic movement extras) still block the create.
    // A legacy MO_TRANSPORT parent is representable by the optional unit-
    // transport fields in AppendSimpleLivingMovement and is therefore allowed.
    eligibility.hasSpline = false;
    eligibility.movementFlags = 0;
    eligibility.hasAttackingTarget = false;
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
                MopUpdateObject::SelfSkillFieldCount +
                MopUpdateObject::SelfBuybackFieldCount +
                MopUpdateObject::SelfQuestLogFieldCount);
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
            addIfChanged(UNIT_FIELD_MOUNTDISPLAYID);
            // Emote state. Ordered here because the serializer requires
            // ascending legacy indices and 83 falls between the mount display
            // id at 65 and PLAYER_FLAGS at 157.
            addIfChanged(UNIT_NPC_EMOTESTATE);
            // PLAYER_FLAGS_GHOST lives here. Until this was projected the
            // client was never told the character had died, so nothing
            // downstream of death worked: no release dialog, therefore no
            // CMSG_REPOP_REQUEST at all, and both release and .revive had no
            // state to act on. Ordered after the native display id and ahead
            // of the quest log because the serializer requires ascending
            // legacy indices.
            addIfChanged(PLAYER_FLAGS);
            // Rest state lives in byte 3 of PLAYER_BYTES_2 and changes when the
            // player enters or leaves an inn - i.e. always AFTER login, so the
            // login seed alone would never deliver a single transition and
            // GetRestState() would go stale the moment it mattered. Drunkenness
            // (byte 1 of PLAYER_BYTES_3) and barber-shop appearance changes have
            // the same shape. Ordered between PLAYER_FLAGS at 157 and the quest
            // log at 166 to keep the legacy indices ascending.
            addIfChanged(PLAYER_BYTES);
            addIfChanged(PLAYER_BYTES_2);
            addIfChanged(PLAYER_BYTES_3);

            // QuestLogFrame renders a slot only once the client holds its
            // quest id, so an accepted quest never appears until these private
            // fields are projected. Ordered ahead of the visible-item feed
            // because the serializer requires ascending legacy indices.
            //
            // Fed a whole slot at a time. The serializer emits all fifteen
            // 18414 words of any slot it sees, so supplying only the changed
            // word would clear the rest of that slot on the client.
            for (uint16 slot = 0; slot < MopUpdateObject::SelfQuestLogSlotCount; ++slot)
            {
                const uint16 base = uint16(MopUpdateObject::SelfQuestLogSourceStart +
                    slot * MopUpdateObject::SelfQuestLogSourceStride);
                bool slotChanged = false;
                for (uint16 word = 0; word < MopUpdateObject::SelfQuestLogSourceStride; ++word)
                {
                    if (m_changedValues[base + word])
                    {
                        slotChanged = true;
                        break;
                    }
                }
                if (!slotChanged)
                {
                    continue;
                }
                for (uint16 word = 0; word < MopUpdateObject::SelfQuestLogSourceStride; ++word)
                {
                    fields.push_back({ uint16(base + word), m_uint32Values[base + word] });
                }
            }

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
            // Explored zones and the rested-XP pool. Ordered after the skill
            // block and before buyback to keep legacy indices ascending.
            for (uint16 i = MopUpdateObject::SelfExploredSourceStart;
                 i <= MopUpdateObject::SelfExploredSourceEnd; ++i)
            {
                addIfChanged(i);
            }
            // The 18414 merchant UI requires the private price/timestamp
            // arrays as well as the buyback item GUIDs before it lists a slot.
            for (uint16 i = MopUpdateObject::SelfBuybackSourceStart;
                 i < MopUpdateObject::SelfBuybackSourceStart +
                     MopUpdateObject::SelfBuybackFieldCount; ++i)
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
                // The packed word splits across two client indices. The create
                // seeds both 30 and 31, so the incremental must maintain both
                // or a power-type change (SetPowerType writes byte 3 of this
                // same source) would leave 31 permanently stale for observers.
                // Pushed in ascending order: AppendStaticValuesNoDynamic
                // asserts that projected indices ascend.
                fields.push_back({ 30, MopUpdateObject::RepackUnitBytes0(value) });
                fields.push_back({ 31, (value >> 24) & 0xFFu });
                continue;
            }
            if (i == UNIT_FIELD_FLAGS)
            {
                value = MopUpdateObject::ProjectPlayerUnitFlags(value);
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
        fields.reserve(20);
        GameObject const* gameObject = static_cast<GameObject const*>(this);
        uint32 const transportTime = gameObject->GetGoType() == GAMEOBJECT_TYPE_MO_TRANSPORT ?
            GameTime::GetGameTimeMS() : 0;
        BuildMopGameObjectStaticFields(*this, target, transportTime, fields);
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
