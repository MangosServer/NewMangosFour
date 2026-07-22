/*
 * MaNGOS Four — MoP 5.4.8.18414 SMSG_UPDATE_OBJECT self-player CREATE block.
 *
 * Dedicated 18414 serializer for the login self-create (Phase 6b). The inherited
 * ObjectUpdate.cpp writer is a pre-18414 layout and cannot be read by an
 * 18414 client; this builds the confirmed 18414 create-block instead. Scope for
 * this pass is the "essential-field bootstrap" — the minimal field set for the
 * client to leave the loading screen and stand in the world; the full field set
 * follows once the machinery is live-verified.
 *
 * The common header, block dispatch, classic packed GUID, static-values tail,
 * simple living movement and stationary game-object movement were recovered
 * from the 32-bit 18414 client reader. The self-player subset is additionally
 * live-client validated. Optional movement and dynamic-field branches remain
 * outside this serializer until recovered and tested separately.
 */
#ifndef MANGOS_MOP_UPDATE_OBJECT_H
#define MANGOS_MOP_UPDATE_OBJECT_H

#include "Common.h"
#include "SharedDefines.h"   // MAX_STORED_POWERS

class WorldPacket;
class ByteBuffer;

namespace MopUpdateObject
{
    struct StaticField
    {
        uint16 index;
        uint32 value;
    };

    /// Append the byte-aligned 18414 static-field mask and values followed by
    /// the zero dynamic-field terminator. Fields must be ordered by index.
    void AppendStaticValuesNoDynamic(ByteBuffer& out, StaticField const* fields, uint32 fieldCount);

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
