/**
 * MaNGOS Four — offline structural test for the MoP 5.4.8.18414 self-player
 * SMSG_UPDATE_OBJECT create-block serializer (MopUpdateObject::BuildSelfCreate).
 *
 * Scope: catch bit/byte-count regressions (exact serialized length, derived from
 * first principles) and value-block field/index/value regressions (decode the
 * fixed-size values block from the end of the packet). The movement bit *order*
 * was transcribed verbatim from the confirmed 18414 layout and is validated by
 * the live client gate; a same-spec round-trip of it would be circular.
 */

#include "MopUpdateObject.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <cstdio>
#include <cstring>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

namespace
{
    int NonZeroGuidBytes(uint64 g)
    {
        int n = 0;
        for (int i = 0; i < 8; ++i)
        {
            if (uint8(g >> (i * 8))) { ++n; }
        }
        return n;
    }

    MopUpdateObject::SelfPlayer MakeSelf()
    {
        MopUpdateObject::SelfPlayer e{};
        e.guid = 0x0000000000000010ULL;   // 16 -> only byte 0 non-zero
        e.mapId = 0;
        e.x = -8913.5f; e.y = 554.6f; e.z = 93.7f; e.o = 3.14f;
        e.moveTime = 0x11223344u;
        e.speedWalk = 1.0f; e.speedRun = 7.0f; e.speedRunBack = 4.5f;
        e.speedSwim = 4.7222f; e.speedSwimBack = 2.5f; e.speedFlight = 7.0f;
        e.speedFlightBack = 4.5f; e.speedTurn = 3.1415f; e.speedPitch = 3.1415f;
        e.race = 1; e.class_ = 2; e.gender = 0; e.powerType = 0;
        e.health = 100; e.maxHealth = 120;
        for (uint32 i = 0; i < MAX_STORED_POWERS; ++i) { e.power[i] = 50 + i; e.maxPower[i] = 60 + i; }
        e.level = 1; e.faction = 1; e.unitFlags = 0x00000008u;   // exercise a real unit flag through the serializer
        e.scale = 1.0f; e.boundingRadius = 0.388f; e.combatReach = 1.5f;
        e.displayId = 19724; e.nativeDisplayId = 19724;
        return e;
    }
}

// ACE (dragged in via 'game') rewrites main() and requires (int, char**).
int main(int /*argc*/, char** /*argv*/)
{
    // Binary-proved 18414 static-values grammar: minimal word count, mask words,
    // values in ascending field order, then the zero dynamic-field terminator.
    {
        const MopUpdateObject::StaticField fields[] =
        {
            { 0, 0x11223344u },
            { 31, 0x55667788u },
            { 32, 0x99AABBCCu },
            { 70, 0xDDEEFF00u },
        };

        ByteBuffer values;
        MopUpdateObject::AppendStaticValuesNoDynamic(values, fields, sizeof(fields) / sizeof(fields[0]));

        const uint8 expected[] =
        {
            0x03,
            0x01, 0x00, 0x00, 0x80,
            0x01, 0x00, 0x00, 0x00,
            0x40, 0x00, 0x00, 0x00,
            0x44, 0x33, 0x22, 0x11,
            0x88, 0x77, 0x66, 0x55,
            0xCC, 0xBB, 0xAA, 0x99,
            0x00, 0xFF, 0xEE, 0xDD,
            0x00,
        };
        CHECK(values.size() == sizeof(expected));
        CHECK(values.size() == sizeof(expected) && std::memcmp(values.contents(), expected, sizeof(expected)) == 0);
    }

    // Binary-proved stationary game-object movement subset: only the top-level
    // rotation and stationary-position bits, followed by XYZO and rotation64.
    {
        MopUpdateObject::StationaryGameObjectMovement movement{};
        movement.x = 1.0f;
        movement.y = 2.0f;
        movement.z = 3.0f;
        movement.o = 4.0f;
        movement.rotation = 0x1122334455667788ULL;

        ByteBuffer bytes;
        MopUpdateObject::AppendStationaryGameObjectMovement(bytes, movement);

        const uint8 expectedBits[] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x40 };
        CHECK(bytes.size() == sizeof(expectedBits) + 4 * sizeof(float) + sizeof(uint64));
        CHECK(bytes.size() >= sizeof(expectedBits) && std::memcmp(bytes.contents(), expectedBits, sizeof(expectedBits)) == 0);

        bytes.rpos(sizeof(expectedBits));
        float x, y, z, o;
        uint64 rotation;
        bytes >> x >> y >> z >> o >> rotation;
        CHECK(x == movement.x); CHECK(y == movement.y); CHECK(z == movement.z); CHECK(o == movement.o);
        CHECK(rotation == movement.rotation);
        CHECK(bytes.rpos() == bytes.size());
    }

    MopUpdateObject::SelfPlayer e = MakeSelf();

    // The binary-proved simple LIVING subset is reusable for ordinary units;
    // SELF is the sole top-level distinction for the receiving player.
    MopUpdateObject::SimpleLivingMovement living{};
    living.guid = e.guid;
    living.x = e.x; living.y = e.y; living.z = e.z; living.o = e.o;
    living.moveTime = e.moveTime;
    living.speedWalk = e.speedWalk; living.speedRun = e.speedRun; living.speedRunBack = e.speedRunBack;
    living.speedSwim = e.speedSwim; living.speedSwimBack = e.speedSwimBack;
    living.speedFlight = e.speedFlight; living.speedFlightBack = e.speedFlightBack;
    living.speedTurn = e.speedTurn; living.speedPitch = e.speedPitch;
    living.self = true;

    ByteBuffer selfMovement;
    MopUpdateObject::AppendSimpleLivingMovement(selfMovement, living);

    living.self = false;
    ByteBuffer creatureMovement;
    MopUpdateObject::AppendSimpleLivingMovement(creatureMovement, living);
    CHECK(selfMovement.size() == creatureMovement.size());
    CHECK(selfMovement.size() > 4);
    for (size_t i = 0; i < selfMovement.size() && i < creatureMovement.size(); ++i)
    {
        const uint8 delta = selfMovement.contents()[i] ^ creatureMovement.contents()[i];
        CHECK(delta == (i == 4 ? 0x40 : 0x00));
    }

    WorldPacket p;
    MopUpdateObject::BuildSelfCreate(p, e);

    const size_t movementOffset = 6 + (3 + NonZeroGuidBytes(e.guid));
    CHECK(p.size() >= movementOffset + selfMovement.size());
    CHECK(p.size() >= movementOffset + selfMovement.size() &&
        std::memcmp(p.contents() + movementOffset, selfMovement.contents(), selfMovement.size()) == 0);

    CHECK(p.GetOpcode() == SMSG_UPDATE_OBJECT);

    // --- exact length (movement bit-count + byte-count sanity) ---
    // header 6 + preamble (1 + [1 mask + nzgb] + 1) + movementBits ceil(104/8)=13
    //   + byteTail (nzgb guid bytes + 13 floats + 4 time) + values 114.
    // values 114 = 1 blockCount + 3*4 masks + 25 fields*4 + 1 dynamic-count.
    const int nzgb = NonZeroGuidBytes(e.guid);                 // 1
    const size_t expected = 6 + (3 + nzgb) + 13 + (nzgb + 13 * 4 + 4) + 114;
    CHECK(p.size() == expected);

    // --- header ---
    p.rpos(0);
    uint16 map; uint32 count;
    p >> map; p >> count;
    CHECK(map == e.mapId);
    CHECK(count == 1);

    // --- CREATE preamble ---
    uint8 updateType; p >> updateType;
    CHECK(updateType == 2);                                    // CREATE_OBJECT2
    uint8 gmask; p >> gmask;
    uint64 decGuid = 0;
    for (int i = 0; i < 8; ++i)
    {
        if (gmask & (1 << i)) { uint8 b; p >> b; decGuid |= uint64(b) << (i * 8); }
    }
    CHECK(decGuid == e.guid);
    uint8 typeId; p >> typeId;
    CHECK(typeId == 4);                                        // TYPEID_PLAYER

    // --- values block (fixed 114 bytes at the tail) ---
    CHECK(p.size() >= 114);
    p.rpos(p.size() - 114);
    uint8 blockCount; p >> blockCount;
    CHECK(blockCount == 3);
    uint32 mask[3];
    for (int i = 0; i < 3; ++i) { p >> mask[i]; }
    auto hasBit = [&](int idx) { return (mask[idx / 32] >> (idx % 32)) & 1u; };
    CHECK(hasBit(0)); CHECK(hasBit(1)); CHECK(hasBit(4)); CHECK(hasBit(7));
    CHECK(hasBit(30)); CHECK(hasBit(33)); CHECK(hasBit(55)); CHECK(hasBit(69)); CHECK(hasBit(70));
    CHECK(hasBit(34)); CHECK(hasBit(38)); CHECK(hasBit(40)); CHECK(hasBit(44)); // power1..5 / maxpower1..5 span
    CHECK(!hasBit(2)); CHECK(!hasBit(32));                     // spot-check unset bits

    uint32 f[25];
    for (int i = 0; i < 25; ++i) { p >> f[i]; }
    CHECK(f[0] == 16u);                                        // OBJECT_FIELD_GUID low
    CHECK(f[1] == 0u);                                         // OBJECT_FIELD_GUID high
    CHECK(f[2] == 25u);                                        // OBJECT_FIELD_TYPE
    CHECK(f[4] == (1u | (2u << 8) | (0u << 24)));              // SEX: race|class|gender
    CHECK(f[6] == 100u);                                       // HEALTH (idx 33)
    CHECK(f[7] == 50u);                                        // POWER1 (idx 34)
    CHECK(f[11] == 54u);                                       // POWER5 (idx 38)
    CHECK(f[12] == 120u);                                      // MAX_HEALTH (idx 39)
    CHECK(f[13] == 60u);                                       // MAXPOWER1 (idx 40)
    CHECK(f[17] == 64u);                                       // MAXPOWER5 (idx 44)
    CHECK(f[18] == 1u);                                        // LEVEL (idx 55)
    CHECK(f[19] == 1u);                                        // FACTION (idx 57)
    CHECK(f[20] == 0x8u);                                      // UNIT_FIELD_FLAGS (idx 61) passed through
    CHECK(f[23] == 19724u);                                    // DISPLAY_ID (idx 69)
    CHECK(f[24] == 19724u);                                    // NATIVE_DISPLAY_ID (idx 70)
    uint8 dyn; p >> dyn;
    CHECK(dyn == 0);                                           // no dynamic fields
    CHECK(p.rpos() == p.size());                               // consumed to the end

    if (g_failures == 0) { std::printf("ALL PASS\n"); return 0; }
    std::printf("%d FAILURES\n", g_failures);
    return 1;
}
