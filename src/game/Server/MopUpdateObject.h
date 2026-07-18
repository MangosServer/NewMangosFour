/*
 * MaNGOS Four — MoP 5.4.8.18414 SMSG_UPDATE_OBJECT self-player CREATE block.
 *
 * Dedicated 18414 serializer for the login self-create (Phase 6b). The inherited
 * ObjectUpdate.cpp writer is Cata 4.3.4 (15595) layout and cannot be read by an
 * 18414 client; this builds the confirmed 18414 create-block instead. Scope for
 * this pass is the "essential-field bootstrap" — the minimal field set for the
 * client to leave the loading screen and stand in the world; the full field set
 * follows once the machinery is live-verified.
 *
 * Wire layout cross-confirmed SkyFire_548 + pandaria_5.4.8 + WowPacketParser
 * (V5_4_8_18291). Field indices from the 18414 update-field table (same source).
 */
#ifndef MANGOS_MOP_UPDATE_OBJECT_H
#define MANGOS_MOP_UPDATE_OBJECT_H

#include "Common.h"

class WorldPacket;

namespace MopUpdateObject
{
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
        uint32 health, maxHealth, power, maxPower;
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
