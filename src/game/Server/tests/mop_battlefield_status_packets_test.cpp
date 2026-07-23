/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Independent 5.4.8.18414 fixtures for the battlefield-status response family.
 */

#include "BattleGround.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace
{
    void CheckPacket(WorldPacket const& actual, WorldPacket const& expected, uint32 opcode)
    {
        CHECK(actual.GetOpcode() == opcode);
        CHECK(actual.size() == expected.size());
        if (actual.size() == expected.size())
            for (size_t i = 0; i < actual.size(); ++i)
                CHECK(actual.contents()[i] == expected.contents()[i]);
    }

    WorldPacket BuildNoneFixture(MopBattleGroundPackets::BattlefieldStatusNone const& s)
    {
        WorldPacket out(SMSG_BATTLEFIELD_STATUS);
        out << s.unusedTime << s.id << s.queueSlot;
        out.WriteGuidMask<2, 0, 4, 5, 3, 7, 1, 6>(s.playerGuid);
        out.WriteGuidBytes<3, 5, 6, 4, 2, 0, 1, 7>(s.playerGuid);
        return out;
    }

    WorldPacket BuildQueuedFixture(MopBattleGroundPackets::BattlefieldStatusQueued const& s)
    {
        WorldPacket out(SMSG_BATTLEFIELD_STATUS_QUEUED);
        out.WriteGuidMask<1, 5>(s.battlefieldGuid);
        out.WriteGuidMask<0>(s.playerGuid);
        out.WriteGuidMask<7>(s.battlefieldGuid);
        out.WriteBit(s.rated);
        out.WriteBit(s.asGroup);
        out.WriteBit(s.eligible);
        out.WriteGuidMask<6, 4, 1, 3, 7, 5>(s.playerGuid);
        out.WriteBit(s.suspended);
        out.WriteGuidMask<3, 0, 2>(s.battlefieldGuid);
        out.WriteGuidMask<2>(s.playerGuid);
        out.WriteGuidMask<6, 4>(s.battlefieldGuid);
        out.FlushBits();

        out.WriteGuidBytes<4>(s.battlefieldGuid);
        out.WriteGuidBytes<6>(s.playerGuid);
        out << s.id;
        out.WriteGuidBytes<5>(s.battlefieldGuid);
        out << s.estimatedWaitTime;
        out.WriteGuidBytes<3>(s.battlefieldGuid);
        out.WriteGuidBytes<3, 4, 0>(s.playerGuid);
        out << s.minLevel;
        out.WriteGuidBytes<0>(s.battlefieldGuid);
        out << s.unusedTime << s.premadeSize;
        out.WriteGuidBytes<1>(s.battlefieldGuid);
        out << s.timeWaited;
        out.WriteGuidBytes<7>(s.battlefieldGuid);
        out << s.queueSlot;
        out.WriteGuidBytes<2>(s.playerGuid);
        out.WriteGuidBytes<6>(s.battlefieldGuid);
        out << s.maxLevel;
        out.WriteGuidBytes<2>(s.battlefieldGuid);
        out.WriteGuidBytes<5, 1>(s.playerGuid);
        out << s.clientInstanceId;
        out.WriteGuidBytes<7>(s.playerGuid);
        return out;
    }

    WorldPacket BuildConfirmationFixture(MopBattleGroundPackets::BattlefieldStatusConfirmation const& s)
    {
        WorldPacket out(SMSG_BATTLEFIELD_STATUS_NEEDCONFIRMATION);
        out.WriteGuidMask<7, 5, 4, 1>(s.playerGuid);
        out.WriteGuidMask<3, 2>(s.battlefieldGuid);
        out.WriteBit(!s.hasRole);
        out.WriteGuidMask<0>(s.battlefieldGuid);
        out.WriteGuidMask<0, 6>(s.playerGuid);
        out.WriteGuidMask<7, 4, 1>(s.battlefieldGuid);
        out.WriteBit(s.rated);
        out.WriteGuidMask<2>(s.playerGuid);
        out.WriteGuidMask<6>(s.battlefieldGuid);
        out.WriteGuidMask<3>(s.playerGuid);
        out.WriteGuidMask<5>(s.battlefieldGuid);
        out.FlushBits();

        out.WriteGuidBytes<1>(s.playerGuid);
        out.WriteGuidBytes<1>(s.battlefieldGuid);
        out.WriteGuidBytes<2>(s.playerGuid);
        out << s.premadeSize << s.queueSlot;
        if (s.hasRole)
            out << s.role;
        out << s.clientInstanceId;
        out.WriteGuidBytes<6, 7>(s.battlefieldGuid);
        out << s.unusedTime;
        out.WriteGuidBytes<7>(s.playerGuid);
        out << s.maxLevel;
        out.WriteGuidBytes<4>(s.playerGuid);
        out.WriteGuidBytes<2, 4>(s.battlefieldGuid);
        out << s.expirationTime << s.minLevel;
        out.WriteGuidBytes<3>(s.playerGuid);
        out.WriteGuidBytes<0>(s.battlefieldGuid);
        out.WriteGuidBytes<5, 6>(s.playerGuid);
        out << s.id;
        out.WriteGuidBytes<3>(s.battlefieldGuid);
        out.WriteGuidBytes<0>(s.playerGuid);
        out.WriteGuidBytes<5>(s.battlefieldGuid);
        out << s.mapId;
        return out;
    }

    WorldPacket BuildActiveFixture(MopBattleGroundPackets::BattlefieldStatusActive const& s)
    {
        WorldPacket out(SMSG_BATTLEFIELD_STATUS_ACTIVE);
        out.WriteGuidMask<0>(s.playerGuid);
        out.WriteGuidMask<3>(s.battlefieldGuid);
        out.WriteGuidMask<3>(s.playerGuid);
        out.WriteGuidMask<2>(s.battlefieldGuid);
        out.WriteGuidMask<2>(s.playerGuid);
        out.WriteGuidMask<5, 1>(s.battlefieldGuid);
        out.WriteGuidMask<7>(s.playerGuid);
        out.WriteBit(s.locked);
        out.WriteGuidMask<6>(s.playerGuid);
        out.WriteGuidMask<0>(s.battlefieldGuid);
        out.WriteBit(s.alliance);
        out.WriteGuidMask<6, 7, 4>(s.battlefieldGuid);
        out.WriteGuidMask<1, 4, 5>(s.playerGuid);
        out.WriteBit(s.rated);
        out.FlushBits();

        out.WriteGuidBytes<3>(s.playerGuid);
        out << s.unusedTime << s.remainingTime;
        out.WriteGuidBytes<7, 5>(s.battlefieldGuid);
        out.WriteGuidBytes<1>(s.playerGuid);
        out.WriteGuidBytes<6>(s.battlefieldGuid);
        out << s.elapsedTime << s.maxLevel;
        out.WriteGuidBytes<1, 2>(s.battlefieldGuid);
        out << s.queueSlot;
        out.WriteGuidBytes<4>(s.playerGuid);
        out << s.minLevel;
        out.WriteGuidBytes<6>(s.playerGuid);
        out << s.mapId;
        out.WriteGuidBytes<0, 5, 7>(s.playerGuid);
        out.WriteGuidBytes<4>(s.battlefieldGuid);
        out << s.clientInstanceId;
        out.WriteGuidBytes<2>(s.playerGuid);
        out << s.premadeSize << s.id;
        out.WriteGuidBytes<3, 0>(s.battlefieldGuid);
        return out;
    }

    WorldPacket BuildFailedFixture(MopBattleGroundPackets::BattlefieldStatusFailed const& s)
    {
        WorldPacket out(SMSG_BATTLEFIELD_STATUS_FAILED);
        out << s.joinTime << s.id << s.queueSlot << s.result;
        out.WriteGuidMask<7>(s.clientLookupGuid);
        out.WriteGuidMask<2, 7>(s.battlefieldGuid);
        out.WriteGuidMask<5>(s.clientLookupGuid);
        out.WriteGuidMask<2>(s.playerGuid);
        out.WriteGuidMask<6>(s.battlefieldGuid);
        out.WriteGuidMask<7, 3>(s.playerGuid);
        out.WriteGuidMask<0, 3>(s.battlefieldGuid);
        out.WriteGuidMask<4>(s.clientLookupGuid);
        out.WriteGuidMask<1>(s.playerGuid);
        out.WriteGuidMask<0>(s.clientLookupGuid);
        out.WriteGuidMask<0>(s.playerGuid);
        out.WriteGuidMask<2>(s.clientLookupGuid);
        out.WriteGuidMask<4>(s.battlefieldGuid);
        out.WriteGuidMask<4>(s.playerGuid);
        out.WriteGuidMask<1>(s.battlefieldGuid);
        out.WriteGuidMask<3>(s.clientLookupGuid);
        out.WriteGuidMask<5>(s.battlefieldGuid);
        out.WriteGuidMask<1>(s.clientLookupGuid);
        out.WriteGuidMask<6, 5>(s.playerGuid);
        out.WriteGuidMask<6>(s.clientLookupGuid);
        out.FlushBits();

        out.WriteGuidBytes<1, 2, 7>(s.clientLookupGuid);
        out.WriteGuidBytes<6, 0>(s.battlefieldGuid);
        out.WriteGuidBytes<5, 0>(s.playerGuid);
        out.WriteGuidBytes<1, 7>(s.battlefieldGuid);
        out.WriteGuidBytes<6>(s.playerGuid);
        out.WriteGuidBytes<0>(s.clientLookupGuid);
        out.WriteGuidBytes<5>(s.battlefieldGuid);
        out.WriteGuidBytes<6>(s.clientLookupGuid);
        out.WriteGuidBytes<1>(s.playerGuid);
        out.WriteGuidBytes<2>(s.battlefieldGuid);
        out.WriteGuidBytes<7, 2, 3>(s.playerGuid);
        out.WriteGuidBytes<5>(s.clientLookupGuid);
        out.WriteGuidBytes<4>(s.playerGuid);
        out.WriteGuidBytes<3>(s.clientLookupGuid);
        out.WriteGuidBytes<3>(s.battlefieldGuid);
        out.WriteGuidBytes<4>(s.clientLookupGuid);
        out.WriteGuidBytes<4>(s.battlefieldGuid);
        return out;
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    ObjectGuid player(UINT64_C(0x8070605040302010));
    ObjectGuid battlefield(UINT64_C(0x1020304050607080));
    ObjectGuid lookup(UINT64_C(0x8877665544332211));

    MopBattleGroundPackets::BattlefieldStatusNone none;
    none.playerGuid = player;
    none.unusedTime = 0x11223344;
    none.id = 0x55667788;
    none.queueSlot = 2;
    WorldPacket nonePacket;
    MopBattleGroundPackets::BuildBattlefieldStatusNone(nonePacket, none);
    CheckPacket(nonePacket, BuildNoneFixture(none), SMSG_BATTLEFIELD_STATUS);

    MopBattleGroundPackets::BattlefieldStatusQueued queued;
    queued.playerGuid = player;
    queued.battlefieldGuid = battlefield;
    queued.rated = true;
    queued.asGroup = true;
    queued.eligible = false;
    queued.suspended = true;
    queued.id = 0x01020304;
    queued.estimatedWaitTime = 0x11121314;
    queued.unusedTime = 0x191A1B1C;
    queued.timeWaited = 0x21222324;
    queued.queueSlot = 1;
    queued.clientInstanceId = 0x31323334;
    queued.minLevel = 85;
    queued.premadeSize = 3;
    queued.maxLevel = 90;
    WorldPacket queuedPacket;
    MopBattleGroundPackets::BuildBattlefieldStatusQueued(queuedPacket, queued);
    CheckPacket(queuedPacket, BuildQueuedFixture(queued), SMSG_BATTLEFIELD_STATUS_QUEUED);

    MopBattleGroundPackets::BattlefieldStatusConfirmation confirmation;
    confirmation.playerGuid = player;
    confirmation.battlefieldGuid = battlefield;
    confirmation.rated = true;
    confirmation.hasRole = true;
    confirmation.role = 4;
    confirmation.premadeSize = 2;
    confirmation.queueSlot = 2;
    confirmation.clientInstanceId = 0x41424344;
    confirmation.unusedTime = 0x51525354;
    confirmation.maxLevel = 90;
    confirmation.expirationTime = 0x61626364;
    confirmation.minLevel = 85;
    confirmation.id = 0x71727374;
    confirmation.mapId = 0x81828384;
    WorldPacket confirmationPacket;
    MopBattleGroundPackets::BuildBattlefieldStatusConfirmation(confirmationPacket, confirmation);
    CheckPacket(confirmationPacket, BuildConfirmationFixture(confirmation), SMSG_BATTLEFIELD_STATUS_NEEDCONFIRMATION);

    MopBattleGroundPackets::BattlefieldStatusActive active;
    active.playerGuid = player;
    active.battlefieldGuid = battlefield;
    active.locked = true;
    active.alliance = true;
    active.rated = true;
    active.unusedTime = 0x01020304;
    active.remainingTime = 0x11121314;
    active.elapsedTime = 0x21222324;
    active.maxLevel = 90;
    active.queueSlot = 2;
    active.minLevel = 85;
    active.mapId = 0x31323334;
    active.clientInstanceId = 0x41424344;
    active.premadeSize = 5;
    active.id = 0x51525354;
    WorldPacket activePacket;
    MopBattleGroundPackets::BuildBattlefieldStatusActive(activePacket, active);
    CheckPacket(activePacket, BuildActiveFixture(active), SMSG_BATTLEFIELD_STATUS_ACTIVE);

    MopBattleGroundPackets::BattlefieldStatusFailed failed;
    failed.playerGuid = player;
    failed.battlefieldGuid = battlefield;
    failed.clientLookupGuid = lookup;
    failed.joinTime = 0x01020304;
    failed.id = 0x11121314;
    failed.queueSlot = 2;
    failed.result = 0x21222324;
    WorldPacket failedPacket;
    MopBattleGroundPackets::BuildBattlefieldStatusFailed(failedPacket, failed);
    CheckPacket(failedPacket, BuildFailedFixture(failed), SMSG_BATTLEFIELD_STATUS_FAILED);

    if (g_fail)
        return 1;
    std::puts("mop_battlefield_status_packets_test: PASS");
    return 0;
}
