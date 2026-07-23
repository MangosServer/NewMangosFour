/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
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

/*
 * MaNGOS Four — MoP 5.4.8.18414 object-update protocol primitives.
 *
 * The inherited ObjectUpdate.cpp writer is a pre-18414 layout and cannot be
 * read by an 18414 client. This module owns the shared, binary-proved wire
 * grammar; gameplay code selects eligible objects and supplies semantic fields.
 *
 * The common header, block dispatch, classic packed GUID, static-values tail,
 * simple living movement, stationary game-object movement, values tail and
 * destroy body were recovered from the 32-bit 18414 client reader. The
 * self-player subset is additionally live-client validated. Optional movement
 * and dynamic-field branches remain outside this serializer until recovered.
 */
#ifndef MANGOS_MOP_UPDATE_OBJECT_H
#define MANGOS_MOP_UPDATE_OBJECT_H

#include "Common.h"
#include "SharedDefines.h"   // MAX_STORED_POWERS

class WorldPacket;
class ByteBuffer;

namespace MopUpdateObject
{
    static constexpr uint16 SelfInventorySourceStart = 960;
    static constexpr uint16 SelfInventoryFieldCount = 172;
    static constexpr uint16 ObserverVisibleItemSourceStart = 916;
    static constexpr uint16 ObserverVisibleItemTargetStart = 921;
    static constexpr uint16 ObserverVisibleItemFieldCount = 38;
    static constexpr uint16 ItemFieldCount = 69;
    static constexpr uint16 ContainerFieldCount = 142;
    static constexpr uint16 DynamicObjectFieldCount = 14;
    static constexpr uint16 CorpseFieldCount = 36;
    static constexpr uint32 SimpleLivingWalkModeFlag = 0x00000100u;

    uint16 TranslateSelfInventoryIndex(uint16 legacyIndex);

    /// Translate one field from Four's legacy Player storage to the narrow
    /// public observer projection proved for 18414. Returns false for every
    /// private or otherwise unsupported Player field.
    bool TranslateObserverPlayerIndex(uint16 legacyIndex, uint16& targetIndex);

    struct StaticField
    {
        uint16 index;
        uint32 value;
    };

    /// Append the byte-aligned 18414 static-field mask and values followed by
    /// the zero dynamic-field terminator. Fields must be ordered by index.
    void AppendStaticValuesNoDynamic(ByteBuffer& out, StaticField const* fields, uint32 fieldCount);

    /// Append the mandatory all-zero 18414 movement ladder used by objects
    /// with no movement branches (Item and Container).
    void AppendEmptyMovement(ByteBuffer& out);

    void AppendEmptyMovementCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
        StaticField const* fields, uint32 fieldCount);

    /// Append an Item or Container CREATE_OBJECT block. Values are copied
    /// directly from legacy fields 0..68 or 0..141 respectively.
    void AppendInventoryCreateBlock(ByteBuffer& out, uint64 guid, uint8 typeId,
        uint32 const* values, uint32 valueCount);

    /// Append changed Item or Container VALUES, retaining changed-to-zero
    /// fields and enforcing the direct-copy field range for the object type.
    void AppendInventoryValuesBlock(ByteBuffer& out, uint64 guid, uint8 typeId,
        StaticField const* fields, uint32 fieldCount);

    struct InventoryObjectEligibility
    {
        bool hasTarget;
        bool hasOwner;
        bool ownerMatchesTarget;
    };

    bool CanUseInventoryObject(InventoryObjectEligibility const& eligibility);

    /// Translate ordered legacy self-inventory fields from [960,1132) to
    /// 18414 [965,1137) and append one VALUES block. Zero values are retained.
    void AppendSelfInventoryValuesBlock(ByteBuffer& out, uint64 guid,
        StaticField const* sourceFields, uint32 fieldCount);

    /// Translate the binary-proved self-player Unit projection plus the
    /// existing inventory range into one ordered 18414 VALUES block.
    /// Changed-to-zero values are retained.
    void AppendSelfPlayerValuesBlock(ByteBuffer& out, uint64 guid,
        StaticField const* sourceFields, uint32 fieldCount);

    /// Convert the legacy [race,class,gender,power] packed value to the 18414
    /// [race,class,power,gender] layout.
    uint32 RepackUnitBytes0(uint32 legacyBytes0);

    /// Translate the eight defined legacy Unit dynamic flags past the 18414
    /// DISABLE_CLIENT_SIDE bit. Unknown legacy bits are deliberately dropped.
    uint32 TranslateUnitDynamicFlags(uint32 legacyFlags);

    /// Preserve the legacy high-half interaction sentinel while translating
    /// the four defined low GameObject flags past target bit zero.
    uint32 TranslateGameObjectDynamic(uint32 legacyDynamic);

    struct SimpleUnitEligibility
    {
        bool isVehicle;
        bool isBoarded;
        bool hasTransport;
        bool hasSpline;
        uint32 movementFlags;
        uint32 movementFlags2;
        bool hasOptionalMovement;
        bool hasAttackingTarget;
    };

    /// True only when the proved narrow LIVING layout can represent the Unit.
    /// WALK_MODE is accepted for stationary objects because the 18414 CREATE
    /// apply path does not consume the decoded raw movement-flags field.
    bool CanUseSimpleUnitMovement(SimpleUnitEligibility const& eligibility);

    struct StationaryGameObjectEligibility
    {
        bool hasTemplate;
        bool isDestructibleBuilding;
        bool isTransport;
        bool isBoarded;
        bool hasStationaryPosition;
        bool hasRotation;
        bool hasUnsupportedMovement;
    };

    bool CanUseStationaryGameObjectMovement(StationaryGameObjectEligibility const& eligibility);

    struct PositionOnlyEligibility
    {
        bool isBoarded;
        bool hasPosition;
        bool hasUnsupportedMovement;
    };

    /// True only for the proved stationary-position-only movement subset.
    bool CanUsePositionOnlyMovement(PositionOnlyEligibility const& eligibility);

    struct PositionOnlyMovement
    {
        float x;
        float y;
        float z;
        float o;
    };

    /// Append the DynamicObject/Corpse position-only movement subset.
    void AppendPositionOnlyMovement(ByteBuffer& out, PositionOnlyMovement const& movement);

    /// Append a DynamicObject or Corpse CREATE block with a direct-copy full
    /// static-field snapshot and no dynamic fields.
    void AppendPositionOnlyCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
        PositionOnlyMovement const& movement, uint32 const* values, uint32 valueCount);

    /// Append changed DynamicObject or Corpse VALUES using direct field indices.
    void AppendPositionOnlyValuesBlock(ByteBuffer& out, uint64 guid, uint8 typeId,
        StaticField const* fields, uint32 fieldCount);

    struct StationaryGameObjectMovement
    {
        float x;
        float y;
        float z;
        float o;
        uint64 rotation;
    };

    /// Append the narrow 18414 stationary game-object movement subset. This
    /// deliberately excludes transports, animation kits, targets and scenes.
    void AppendStationaryGameObjectMovement(ByteBuffer& out, StationaryGameObjectMovement const& movement);

    void AppendStationaryGameObjectCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
        StationaryGameObjectMovement const& movement, StaticField const* fields, uint32 fieldCount);

    struct SimpleLivingMovement
    {
        uint64 guid;
        float x;
        float y;
        float z;
        float o;
        uint32 moveTime;
        float speedWalk;
        float speedRun;
        float speedRunBack;
        float speedSwim;
        float speedSwimBack;
        float speedFlight;
        float speedFlightBack;
        float speedTurn;
        float speedPitch;
        bool self;
    };

    /// Append the proved no-flags/no-transport/no-fall/no-spline living subset.
    void AppendSimpleLivingMovement(ByteBuffer& out, SimpleLivingMovement const& movement);

    /// Append one complete simple-living CREATE block to an UpdateData buffer.
    void AppendSimpleLivingCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
        SimpleLivingMovement const& movement, StaticField const* fields, uint32 fieldCount);

    /// Append one complete VALUES block to an UpdateData buffer.
    void AppendValuesBlock(ByteBuffer& out, uint64 guid, StaticField const* fields, uint32 fieldCount);

    /// Build the complete 18414 SMSG_DESTROY_OBJECT body.
    void BuildDestroyObject(WorldPacket& out, uint64 guid, bool animation);

    /// Everything the essential self-create block needs, pulled from the Player
    /// via semantic accessors at the call site (so this stays decoupled from
    /// Four's current 17538 update-field storage layout).
    struct SelfPlayer
    {
        uint64 guid;
        uint16 mapId;
        float x, y, z, o;
        uint32 moveTime;                 // movementInfo.time (non-zero)

        // movement speeds
        float speedWalk, speedRun, speedRunBack, speedSwim, speedSwimBack;
        float speedFlight, speedFlightBack, speedTurn, speedPitch;

        // values (18414 UNIT/OBJECT fields)
        uint8  race, class_, gender, powerType;
        uint32 health, maxHealth;
        // All class power slots (dense-indexed like the server's UNIT_FIELD_POWER1..5).
        // The client reads the displayed bar from the slot its ChrClassXPowerTypes maps
        // the display power to (slot 0 for the primary); populating every slot avoids that
        // assumption and keeps any secondary resource (chi/combo/etc.) correct.
        uint32 power[MAX_STORED_POWERS], maxPower[MAX_STORED_POWERS];
        uint8  level;
        uint32 faction;
        uint32 unitFlags;
        float  scale, boundingRadius, combatReach;
        uint32 displayId, nativeDisplayId;
    };

    /// Build a complete SMSG_UPDATE_OBJECT packet holding one CREATE_OBJECT2
    /// block for the self player (living, stationary at login).
    void BuildSelfCreate(WorldPacket& out, const SelfPlayer& e);
}

#endif // MANGOS_MOP_UPDATE_OBJECT_H
