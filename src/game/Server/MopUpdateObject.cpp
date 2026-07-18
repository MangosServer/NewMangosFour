/*
 * MaNGOS Four — MoP 5.4.8.18414 SMSG_UPDATE_OBJECT self-player CREATE block.
 * See MopUpdateObject.h. Layout transcribed (MaNGOS idiom, no code lift) from the
 * confirmed 18414 movement/values structure.
 */

#include "MopUpdateObject.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <cstring>

namespace
{
    inline uint8 GuidByte(uint64 g, int i) { return uint8(g >> (i * 8)); }

    inline uint32 FloatBits(float f) { uint32 u; std::memcpy(&u, &f, 4); return u; }

    // Classic pack-guid (mask byte + present bytes), as used in the CREATE preamble.
    void AppendPackedGuid(WorldPacket& out, uint64 guid)
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

void MopUpdateObject::BuildSelfCreate(WorldPacket& out, const SelfPlayer& e)
{
    out.Initialize(SMSG_UPDATE_OBJECT);

    // ---- packet header ----
    out << uint16(e.mapId);
    out << uint32(1);                    // one update block, no out-of-range section

    // ---- CREATE block preamble ----
    out << uint8(2);                     // UPDATETYPE_CREATE_OBJECT2
    AppendPackedGuid(out, e.guid);
    out << uint8(4);                     // TYPEID_PLAYER

    const uint64 g = e.guid;

    // ---- movement block: top-level 21-slot updateflags ladder ----
    // Stationary living self-player: LIVING + SELF set, everything else clear.
    out.WriteBit(0);                     // has gameobject
    out.WriteBit(0);                     // anim kits
    out.WriteBit(1);                     // LIVING
    out.WriteBit(0);                     // scene local script data
    out.WriteBit(0);
    out.WriteBits(0, 22);                // transport frame count
    out.WriteBit(0);                     // vehicle
    out.WriteBit(0);                     // unk 1044
    out.WriteBit(0);
    out.WriteBit(0);                     // transport
    out.WriteBit(0);                     // rotation
    out.WriteBit(0);
    out.WriteBit(1);                     // SELF (this object is the receiving player)
    out.WriteBit(0);                     // has target
    out.WriteBit(0);                     // scene object
    out.WriteBit(0);                     // scene pending instances
    out.WriteBit(0);
    out.WriteBit(0);                     // areatrigger
    out.WriteBit(0);                     // go transport position
    out.WriteBit(0);                     // replace you
    out.WriteBit(0);                     // stationary position (living carries its own)

    // ---- LIVING sub-ladder (movementFlags=0, no transport/spline/fall/pitch) ----
    out.WriteBit(GuidByte(g, 2) != 0);
    out.WriteBit(0);
    out.WriteBit(1);                     // !hasPitch
    out.WriteBit(0);                     // hasUnitTransport
    out.WriteBit(0);
    out.WriteBit(0);                     // !movementInfo.time (time is present)
    out.WriteBit(GuidByte(g, 6) != 0);
    out.WriteBit(GuidByte(g, 4) != 0);
    out.WriteBit(GuidByte(g, 3) != 0);
    out.WriteBit(0);                     // fuzzyEq(o, 0) -> false (o != 0)
    out.WriteBit(1);                     // !movementCounter (counter == 0)
    out.WriteBit(GuidByte(g, 5) != 0);
    out.WriteBits(0, 22);                // Forces
    out.WriteBit(1);                     // !movementFlags (flags == 0)
    out.WriteBits(0, 19);
    out.WriteBit(0);                     // hasFallData
    // movementFlags == 0 -> no 30-bit flags
    out.WriteBit(1);                     // !hasSplineElevation
    out.WriteBit(0);                     // hasSpline
    out.WriteBit(0);
    out.WriteBit(GuidByte(g, 0) != 0);
    out.WriteBit(GuidByte(g, 7) != 0);
    out.WriteBit(GuidByte(g, 1) != 0);
    // no spline
    out.WriteBit(1);                     // !movementFlagsExtra (extra == 0)
    // no fall data, no extra flags

    out.FlushBits();

    // ---- LIVING byte tail (order per 18414 layout) ----
    out.WriteByteSeq(GuidByte(g, 4));
    out << float(e.speedFlight);
    // movementCounter == 0 -> skip
    out.WriteByteSeq(GuidByte(g, 2));
    // no fall data
    out.WriteByteSeq(GuidByte(g, 1));
    out << float(e.speedTurn);
    out << uint32(e.moveTime);           // movementInfo.time
    out << float(e.speedRunBack);
    // no spline elevation
    out.WriteByteSeq(GuidByte(g, 7));
    out << float(e.speedPitch);
    out << float(e.x);
    // no pitch
    out << float(e.o);                   // o != 0 -> orientation present
    out << float(e.speedWalk);
    out << float(e.y);
    out << float(e.speedFlightBack);
    out.WriteByteSeq(GuidByte(g, 3));
    out.WriteByteSeq(GuidByte(g, 5));
    out.WriteByteSeq(GuidByte(g, 6));
    out.WriteByteSeq(GuidByte(g, 0));
    out << float(e.speedSwimBack);
    out << float(e.speedRun);
    out << float(e.speedSwim);
    out << float(e.z);                   // positionZMinusOffset

    // ---- values block (essential 18414 fields, ascending index order) ----
    // UNIT_FIELD_SEX (renamed BYTES_0): byte0 race, byte1 class, byte3 gender.
    const uint32 sex = uint32(e.race) | (uint32(e.class_) << 8) | (uint32(e.gender) << 24);

    struct FV { uint16 idx; uint32 val; };
    const FV fields[] =
    {
        {  0, uint32(e.guid & 0xFFFFFFFFu) },   // OBJECT_FIELD_GUID low
        {  1, uint32(e.guid >> 32) },           // OBJECT_FIELD_GUID high
        {  4, 25u },                            // OBJECT_FIELD_TYPE (OBJECT|UNIT|PLAYER)
        {  7, FloatBits(e.scale) },             // OBJECT_FIELD_SCALE
        { 30, sex },                            // UNIT_FIELD_SEX  (OBJECT_END+0x16)
        { 31, uint32(e.powerType) },            // UNIT_FIELD_DISPLAY_POWER (+0x17)
        { 33, e.health },                       // UNIT_FIELD_HEALTH (+0x19)
        { 34, e.power },                        // UNIT_FIELD_POWER[0] (+0x1A)
        { 39, e.maxHealth },                    // UNIT_FIELD_MAX_HEALTH (+0x1F)
        { 40, e.maxPower },                     // UNIT_FIELD_MAX_POWER[0] (+0x20)
        { 55, uint32(e.level) },                // UNIT_FIELD_LEVEL (+0x2F)
        { 57, e.faction },                      // UNIT_FIELD_FACTION_TEMPLATE (+0x31)
        { 61, e.unitFlags },                    // UNIT_FIELD_FLAGS (+0x35)
        { 67, FloatBits(e.boundingRadius) },    // UNIT_FIELD_BOUNDING_RADIUS (+0x3B)
        { 68, FloatBits(e.combatReach) },       // UNIT_FIELD_COMBAT_REACH (+0x3C)
        { 69, e.displayId },                    // UNIT_FIELD_DISPLAY_ID (+0x3D)
        { 70, e.nativeDisplayId },              // UNIT_FIELD_NATIVE_DISPLAY_ID (+0x3E)
    };
    const int nf = int(sizeof(fields) / sizeof(fields[0]));

    const uint8 blockCount = uint8(fields[nf - 1].idx / 32 + 1);   // 3
    uint32 mask[8] = { 0 };
    for (int i = 0; i < nf; ++i)
    {
        mask[fields[i].idx / 32] |= (1u << (fields[i].idx % 32));
    }

    out << uint8(blockCount);
    for (int i = 0; i < blockCount; ++i)
    {
        out << uint32(mask[i]);
    }
    for (int i = 0; i < nf; ++i)
    {
        out << uint32(fields[i].val);
    }

    // ---- dynamic-fields block (none for a player) ----
    out << uint8(0);
}
