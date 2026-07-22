/*
 * MaNGOS Four — MoP 5.4.8.18414 object-update protocol primitives.
 * See MopUpdateObject.h. Layout transcribed (MaNGOS idiom, no code lift) from the
 * confirmed 18414 movement/values structure.
 */

#include "MopUpdateObject.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <cstring>
#include <vector>

namespace
{
    inline uint8 GuidByte(uint64 g, int i) { return uint8(g >> (i * 8)); }

    inline uint32 FloatBits(float f) { uint32 u; std::memcpy(&u, &f, 4); return u; }

    // Classic pack-guid (mask byte + present bytes), as used in the CREATE preamble.
    void AppendPackedGuid(ByteBuffer& out, uint64 guid)
    {
        uint8 mask = 0;
        uint8 bytes[8];
        int n = 0;
        for (int i = 0; i < 8; ++i)
        {
            uint8 b = GuidByte(guid, i);
            if (b)
            {
                mask |= uint8(1 << i);
                bytes[n++] = b;
            }
        }
        out << mask;
        if (n)
        {
            out.append(bytes, n);
        }
    }
}

uint16 MopUpdateObject::TranslateSelfInventoryIndex(uint16 legacyIndex)
{
    MANGOS_ASSERT(legacyIndex >= SelfInventorySourceStart &&
        legacyIndex < SelfInventorySourceStart + SelfInventoryFieldCount);
    return uint16(legacyIndex + 5);
}

uint32 MopUpdateObject::RepackUnitBytes0(uint32 legacyBytes0)
{
    return (legacyBytes0 & 0x0000FFFFu) |
        ((legacyBytes0 & 0xFF000000u) >> 8) |
        ((legacyBytes0 & 0x00FF0000u) << 8);
}

uint32 MopUpdateObject::TranslateUnitDynamicFlags(uint32 legacyFlags)
{
    return (legacyFlags & 0x000000FFu) << 1;
}

uint32 MopUpdateObject::TranslateGameObjectDynamic(uint32 legacyDynamic)
{
    return (legacyDynamic & 0xFFFF0000u) | ((legacyDynamic & 0x0000000Fu) << 1);
}

bool MopUpdateObject::CanUseSimpleUnitMovement(SimpleUnitEligibility const& eligibility)
{
    return !eligibility.isVehicle && !eligibility.isBoarded && !eligibility.hasTransport &&
        !eligibility.hasSpline && eligibility.movementFlags == 0 && eligibility.movementFlags2 == 0 &&
        !eligibility.hasOptionalMovement && !eligibility.hasAttackingTarget;
}

bool MopUpdateObject::CanUseStationaryGameObjectMovement(StationaryGameObjectEligibility const& eligibility)
{
    return eligibility.hasTemplate && !eligibility.isDestructibleBuilding && !eligibility.isTransport &&
        !eligibility.isBoarded && eligibility.hasStationaryPosition && eligibility.hasRotation &&
        !eligibility.hasUnsupportedMovement;
}

bool MopUpdateObject::CanUsePositionOnlyMovement(PositionOnlyEligibility const& eligibility)
{
    return !eligibility.isBoarded && eligibility.hasPosition && !eligibility.hasUnsupportedMovement;
}

bool MopUpdateObject::CanUseInventoryObject(InventoryObjectEligibility const& eligibility)
{
    return eligibility.hasTarget && eligibility.hasOwner && eligibility.ownerMatchesTarget;
}

void MopUpdateObject::AppendStaticValuesNoDynamic(ByteBuffer& out, StaticField const* fields, uint32 fieldCount)
{
    MANGOS_ASSERT(fields || fieldCount == 0);

    uint32 masks[63] = { 0 };
    uint8 blockCount = 0;
    uint16 previousIndex = 0;

    for (uint32 i = 0; i < fieldCount; ++i)
    {
        MANGOS_ASSERT(fields[i].index < 63 * 32);
        MANGOS_ASSERT(i == 0 || fields[i].index > previousIndex);

        previousIndex = fields[i].index;
        masks[fields[i].index / 32] |= uint32(1) << (fields[i].index % 32);
        blockCount = std::max<uint8>(blockCount, uint8(fields[i].index / 32 + 1));
    }

    out << blockCount;
    for (uint8 i = 0; i < blockCount; ++i)
    {
        out << masks[i];
    }

    for (uint32 i = 0; i < fieldCount; ++i)
    {
        out << fields[i].value;
    }

    out << uint8(0);
}

void MopUpdateObject::AppendEmptyMovement(ByteBuffer& out)
{
    out.WriteBits(0, 42);
    out.FlushBits();
}

void MopUpdateObject::AppendEmptyMovementCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
    StaticField const* fields, uint32 fieldCount)
{
    out << updateType;
    AppendPackedGuid(out, guid);
    out << typeId;
    AppendEmptyMovement(out);
    AppendStaticValuesNoDynamic(out, fields, fieldCount);
}

void MopUpdateObject::AppendInventoryCreateBlock(ByteBuffer& out, uint64 guid, uint8 typeId,
    uint32 const* values, uint32 valueCount)
{
    MANGOS_ASSERT(values);
    MANGOS_ASSERT((typeId == 1 && valueCount == ItemFieldCount) ||
        (typeId == 2 && valueCount == ContainerFieldCount));

    std::vector<StaticField> fields;
    fields.reserve(valueCount);
    for (uint16 i = 0; i < valueCount; ++i)
    {
        fields.push_back({ i, values[i] });
    }

    AppendEmptyMovementCreateBlock(out, 1, guid, typeId, fields.data(), uint32(fields.size()));
}

void MopUpdateObject::AppendInventoryValuesBlock(ByteBuffer& out, uint64 guid, uint8 typeId,
    StaticField const* fields, uint32 fieldCount)
{
    MANGOS_ASSERT(fields || fieldCount == 0);
    MANGOS_ASSERT(typeId == 1 || typeId == 2);
    const uint16 valueCount = typeId == 2 ? ContainerFieldCount : ItemFieldCount;
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        MANGOS_ASSERT(fields[i].index < valueCount);
    }
    AppendValuesBlock(out, guid, fields, fieldCount);
}

void MopUpdateObject::AppendSelfInventoryValuesBlock(ByteBuffer& out, uint64 guid,
    StaticField const* sourceFields, uint32 fieldCount)
{
    MANGOS_ASSERT(sourceFields || fieldCount == 0);

    std::vector<StaticField> fields;
    fields.reserve(fieldCount);
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        MANGOS_ASSERT(i == 0 || sourceFields[i - 1].index < sourceFields[i].index);
        fields.push_back({ TranslateSelfInventoryIndex(sourceFields[i].index), sourceFields[i].value });
    }

    AppendValuesBlock(out, guid, fields.data(), uint32(fields.size()));
}

void MopUpdateObject::AppendPositionOnlyMovement(ByteBuffer& out, PositionOnlyMovement const& movement)
{
    out.WriteBit(0);                 // game-object data
    out.WriteBit(0);                 // animation kits
    out.WriteBit(0);                 // living
    out.WriteBit(0);                 // scene-local script
    out.WriteBit(0);
    out.WriteBits(0, 22);            // transport frame count
    out.WriteBit(0);                 // vehicle
    out.WriteBit(0);
    out.WriteBit(0);
    out.WriteBit(0);                 // transport time
    out.WriteBit(0);                 // rotation
    out.WriteBit(0);
    out.WriteBit(0);                 // self
    out.WriteBit(0);                 // attacking target
    out.WriteBit(0);                 // scene object
    out.WriteBit(0);                 // scene pending instances
    out.WriteBit(0);
    out.WriteBit(0);                 // area trigger
    out.WriteBit(0);                 // game-object transport position
    out.WriteBit(0);                 // replace you
    out.WriteBit(1);                 // stationary position follows
    out.FlushBits();

    out << movement.y << movement.z << movement.o << movement.x;
}

void MopUpdateObject::AppendPositionOnlyCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
    PositionOnlyMovement const& movement, uint32 const* values, uint32 valueCount)
{
    MANGOS_ASSERT(values);
    MANGOS_ASSERT((typeId == 6 && valueCount == DynamicObjectFieldCount) ||
        (typeId == 7 && valueCount == CorpseFieldCount));

    std::vector<StaticField> fields;
    fields.reserve(valueCount);
    for (uint16 i = 0; i < valueCount; ++i)
    {
        fields.push_back({ i, values[i] });
    }

    out << updateType;
    AppendPackedGuid(out, guid);
    out << typeId;
    AppendPositionOnlyMovement(out, movement);
    AppendStaticValuesNoDynamic(out, fields.data(), uint32(fields.size()));
}

void MopUpdateObject::AppendPositionOnlyValuesBlock(ByteBuffer& out, uint64 guid, uint8 typeId,
    StaticField const* fields, uint32 fieldCount)
{
    MANGOS_ASSERT(fields || fieldCount == 0);
    MANGOS_ASSERT(typeId == 6 || typeId == 7);
    const uint16 valueCount = typeId == 7 ? CorpseFieldCount : DynamicObjectFieldCount;
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        MANGOS_ASSERT(fields[i].index < valueCount);
    }
    AppendValuesBlock(out, guid, fields, fieldCount);
}

void MopUpdateObject::AppendStationaryGameObjectMovement(ByteBuffer& out, StationaryGameObjectMovement const& movement)
{
    out.WriteBit(0);                 // game-object data
    out.WriteBit(0);                 // animation kits
    out.WriteBit(0);                 // living
    out.WriteBit(0);                 // scene-local script
    out.WriteBit(0);
    out.WriteBits(0, 22);            // transport frame count
    out.WriteBit(0);                 // vehicle
    out.WriteBit(0);
    out.WriteBit(0);
    out.WriteBit(0);                 // transport time
    out.WriteBit(1);                 // packed world rotation follows
    out.WriteBit(0);
    out.WriteBit(0);                 // self
    out.WriteBit(0);                 // attacking target
    out.WriteBit(0);                 // scene object
    out.WriteBit(0);                 // scene pending instances
    out.WriteBit(0);
    out.WriteBit(0);                 // area trigger
    out.WriteBit(0);                 // game-object transport position
    out.WriteBit(0);                 // replace you
    out.WriteBit(1);                 // stationary position follows
    out.FlushBits();

    out << movement.y << movement.z << movement.o << movement.x;
    out << movement.rotation;
}

void MopUpdateObject::AppendStationaryGameObjectCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
    StationaryGameObjectMovement const& movement, StaticField const* fields, uint32 fieldCount)
{
    out << updateType;
    AppendPackedGuid(out, guid);
    out << typeId;
    AppendStationaryGameObjectMovement(out, movement);
    AppendStaticValuesNoDynamic(out, fields, fieldCount);
}

void MopUpdateObject::AppendSimpleLivingMovement(ByteBuffer& out, SimpleLivingMovement const& movement)
{
    const uint64 g = movement.guid;

    out.WriteBit(0);                     // game-object data
    out.WriteBit(0);                     // animation kits
    out.WriteBit(1);                     // living
    out.WriteBit(0);                     // scene-local script
    out.WriteBit(0);
    out.WriteBits(0, 22);                // transport frame count
    out.WriteBit(0);                     // vehicle
    out.WriteBit(0);
    out.WriteBit(0);
    out.WriteBit(0);                     // transport time
    out.WriteBit(0);                     // rotation
    out.WriteBit(0);
    out.WriteBit(movement.self);         // self
    out.WriteBit(0);                     // attacking target
    out.WriteBit(0);                     // scene object
    out.WriteBit(0);                     // scene pending instances
    out.WriteBit(0);
    out.WriteBit(0);                     // area trigger
    out.WriteBit(0);                     // game-object transport position
    out.WriteBit(0);                     // replace you
    out.WriteBit(0);                     // living carries its own position

    out.WriteBit(GuidByte(g, 2) != 0);
    out.WriteBit(0);
    out.WriteBit(1);                     // pitch omitted
    out.WriteBit(0);                     // unit transport
    out.WriteBit(0);
    out.WriteBit(0);                     // movement time present
    out.WriteBit(GuidByte(g, 6) != 0);
    out.WriteBit(GuidByte(g, 4) != 0);
    out.WriteBit(GuidByte(g, 3) != 0);
    out.WriteBit(0);                     // orientation present
    out.WriteBit(1);                     // movement counter omitted
    out.WriteBit(GuidByte(g, 5) != 0);
    out.WriteBits(0, 22);                // forces
    out.WriteBit(1);                     // movement flags omitted
    out.WriteBits(0, 19);
    out.WriteBit(0);                     // fall data
    out.WriteBit(1);                     // spline elevation omitted
    out.WriteBit(0);                     // spline
    out.WriteBit(0);
    out.WriteBit(GuidByte(g, 0) != 0);
    out.WriteBit(GuidByte(g, 7) != 0);
    out.WriteBit(GuidByte(g, 1) != 0);
    out.WriteBit(1);                     // extra movement flags omitted
    out.FlushBits();

    out.WriteByteSeq(GuidByte(g, 4));
    out << movement.speedFlight;
    out.WriteByteSeq(GuidByte(g, 2));
    out.WriteByteSeq(GuidByte(g, 1));
    out << movement.speedTurn;
    out << movement.moveTime;
    out << movement.speedRunBack;
    out.WriteByteSeq(GuidByte(g, 7));
    out << movement.speedPitch;
    out << movement.x;
    out << movement.o;
    out << movement.speedWalk;
    out << movement.y;
    out << movement.speedFlightBack;
    out.WriteByteSeq(GuidByte(g, 3));
    out.WriteByteSeq(GuidByte(g, 5));
    out.WriteByteSeq(GuidByte(g, 6));
    out.WriteByteSeq(GuidByte(g, 0));
    out << movement.speedSwimBack;
    out << movement.speedRun;
    out << movement.speedSwim;
    out << movement.z;
}

void MopUpdateObject::AppendSimpleLivingCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
    SimpleLivingMovement const& movement, StaticField const* fields, uint32 fieldCount)
{
    MANGOS_ASSERT(movement.guid == guid);
    out << updateType;
    AppendPackedGuid(out, guid);
    out << typeId;
    AppendSimpleLivingMovement(out, movement);
    AppendStaticValuesNoDynamic(out, fields, fieldCount);
}

void MopUpdateObject::AppendValuesBlock(ByteBuffer& out, uint64 guid, StaticField const* fields, uint32 fieldCount)
{
    out << uint8(0);
    AppendPackedGuid(out, guid);
    AppendStaticValuesNoDynamic(out, fields, fieldCount);
}

void MopUpdateObject::BuildDestroyObject(WorldPacket& out, uint64 guid, bool animation)
{
    const uint8 bytes[8] =
    {
        GuidByte(guid, 0), GuidByte(guid, 1), GuidByte(guid, 2), GuidByte(guid, 3),
        GuidByte(guid, 4), GuidByte(guid, 5), GuidByte(guid, 6), GuidByte(guid, 7),
    };

    out.Initialize(SMSG_DESTROY_OBJECT, 10);
    out.WriteBit(bytes[3] != 0);
    out.WriteBit(bytes[2] != 0);
    out.WriteBit(bytes[4] != 0);
    out.WriteBit(bytes[1] != 0);
    out.WriteBit(animation);
    out.WriteBit(bytes[7] != 0);
    out.WriteBit(bytes[0] != 0);
    out.WriteBit(bytes[6] != 0);
    out.WriteBit(bytes[5] != 0);
    out.FlushBits();
    out.WriteByteSeq(bytes[0]);
    out.WriteByteSeq(bytes[4]);
    out.WriteByteSeq(bytes[7]);
    out.WriteByteSeq(bytes[2]);
    out.WriteByteSeq(bytes[6]);
    out.WriteByteSeq(bytes[3]);
    out.WriteByteSeq(bytes[1]);
    out.WriteByteSeq(bytes[5]);
}

void MopUpdateObject::BuildSelfCreate(WorldPacket& out, const SelfPlayer& e)
{
    out.Initialize(SMSG_UPDATE_OBJECT);

    // ---- packet header ----
    out << uint16(e.mapId);
    out << uint32(1);                    // one update block, no out-of-range section

    SimpleLivingMovement movement{};
    movement.guid = e.guid;
    movement.x = e.x;
    movement.y = e.y;
    movement.z = e.z;
    movement.o = e.o;
    movement.moveTime = e.moveTime;
    movement.speedWalk = e.speedWalk;
    movement.speedRun = e.speedRun;
    movement.speedRunBack = e.speedRunBack;
    movement.speedSwim = e.speedSwim;
    movement.speedSwimBack = e.speedSwimBack;
    movement.speedFlight = e.speedFlight;
    movement.speedFlightBack = e.speedFlightBack;
    movement.speedTurn = e.speedTurn;
    movement.speedPitch = e.speedPitch;
    movement.self = true;
    // ---- values block (essential 18414 fields, ascending index order) ----
    // UNIT_FIELD_SEX (renamed BYTES_0): race, class, power type, gender.
    const uint32 sex = uint32(e.race) | (uint32(e.class_) << 8) |
        (uint32(e.powerType) << 16) | (uint32(e.gender) << 24);

    const StaticField fields[] =
    {
        {  0, uint32(e.guid & 0xFFFFFFFFu) },   // OBJECT_FIELD_GUID low
        {  1, uint32(e.guid >> 32) },           // OBJECT_FIELD_GUID high
        {  4, 25u },                            // OBJECT_FIELD_TYPE (OBJECT|UNIT|PLAYER)
        {  7, FloatBits(e.scale) },             // OBJECT_FIELD_SCALE
        { 30, sex },                            // UNIT_FIELD_SEX  (OBJECT_END+0x16)
        { 31, uint32(e.powerType) },            // UNIT_FIELD_DISPLAY_POWER (+0x17)
        { 33, e.health },                       // UNIT_FIELD_HEALTH (+0x19)
        { 34, e.power[0] },                     // UNIT_FIELD_POWER1 (+0x1A)
        { 35, e.power[1] },                     // UNIT_FIELD_POWER2 (+0x1B)
        { 36, e.power[2] },                     // UNIT_FIELD_POWER3 (+0x1C)
        { 37, e.power[3] },                     // UNIT_FIELD_POWER4 (+0x1D)
        { 38, e.power[4] },                     // UNIT_FIELD_POWER5 (+0x1E)
        { 39, e.maxHealth },                    // UNIT_FIELD_MAX_HEALTH (+0x1F)
        { 40, e.maxPower[0] },                  // UNIT_FIELD_MAXPOWER1 (+0x20)
        { 41, e.maxPower[1] },                  // UNIT_FIELD_MAXPOWER2 (+0x21)
        { 42, e.maxPower[2] },                  // UNIT_FIELD_MAXPOWER3 (+0x22)
        { 43, e.maxPower[3] },                  // UNIT_FIELD_MAXPOWER4 (+0x23)
        { 44, e.maxPower[4] },                  // UNIT_FIELD_MAXPOWER5 (+0x24)
        { 55, uint32(e.level) },                // UNIT_FIELD_LEVEL (+0x2F)
        { 57, e.faction },                      // UNIT_FIELD_FACTION_TEMPLATE (+0x31)
        { 61, e.unitFlags },                    // UNIT_FIELD_FLAGS (+0x35)
        { 67, FloatBits(e.boundingRadius) },    // UNIT_FIELD_BOUNDING_RADIUS (+0x3B)
        { 68, FloatBits(e.combatReach) },       // UNIT_FIELD_COMBAT_REACH (+0x3C)
        { 69, e.displayId },                    // UNIT_FIELD_DISPLAY_ID (+0x3D)
        { 70, e.nativeDisplayId },              // UNIT_FIELD_NATIVE_DISPLAY_ID (+0x3E)
    };
    AppendSimpleLivingCreateBlock(out, 2, e.guid, 4, movement, fields,
        uint32(sizeof(fields) / sizeof(fields[0])));
}
