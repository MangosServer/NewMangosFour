/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * clients including 5.4.8.18414.
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Independent 5.4.8.18414 fixtures for CMSG_CAST_SPELL.
 */

#include "Spell.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace
{
    static void CheckBytes(WorldPacket const& packet, std::initializer_list<uint8> expected)
    {
        CHECK(packet.size() == expected.size());
        if (packet.size() != expected.size())
            return;

        size_t index = 0;
        for (uint8 byte : expected)
            CHECK(packet.contents()[index++] == byte);
    }

    struct MovementFixture
    {
        uint64 guid = UINT64_C(0x8070605040302010);
        uint64 transportGuid = UINT64_C(0x1020304050607080);
        bool hasTransport = true;
        bool hasTransportTime2 = true;
        bool hasTransportTime3 = true;
        bool hasOrientation = true;
        bool hasSplineElevation = true;
        bool hasPitch = true;
        bool hasMovementFlags = true;
        bool hasTimestamp = true;
        bool hasUnknownUInt32 = true;
        bool hasFallData = true;
        bool hasFallDirection = true;
        bool hasMovementFlags2 = true;
        uint32 forceCount = 2;
        bool writeForceIds = true;
    };

    struct CastFixture
    {
        bool hasTargetString = true;
        bool hasCastCount = true;
        bool hasSource = true;
        bool hasDestination = true;
        bool hasSpellId = true;
        bool hasTargetMask = true;
        bool hasElevation = true;
        bool hasGlyphIndex = true;
        bool hasMovement = true;
        bool hasMissileSpeed = true;
        bool hasCastFlags = true;
        uint8 researchCount = 2;

        uint64 targetGuid = UINT64_C(0x8070605040302010);
        uint64 itemGuid = UINT64_C(0x1020304050607080);
        uint64 sourceTransportGuid = UINT64_C(0x0011220033440055);
        uint64 destinationTransportGuid = UINT64_C(0x660077880099AA00);
        uint32 targetMask = TARGET_FLAG_UNIT | TARGET_FLAG_ITEM |
            TARGET_FLAG_SOURCE_LOCATION | TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_STRING;
        uint8 castFlags = 0x15;
        std::string targetString = "Target";
        float missileSpeed = 19.5f;
        float elevation = -0.75f;
        uint8 castCount = 7;
        uint32 spellId = 0x11223344u;
        uint32 glyphIndex = 0x55667788u;
        MovementFixture movement;
    };

    static void WriteMovementBits(WorldPacket& packet, MovementFixture const& movement)
    {
        packet.WriteBits(movement.forceCount, 22);
        packet.WriteBit(false);
        packet.WriteGuidMask<4>(ObjectGuid(movement.guid));
        packet.WriteBit(movement.hasTransport);
        if (movement.hasTransport)
        {
            packet.WriteBit(movement.hasTransportTime2);
            packet.WriteGuidMask<7, 4, 1, 0, 6, 3, 5>(ObjectGuid(movement.transportGuid));
            packet.WriteBit(movement.hasTransportTime3);
            packet.WriteGuidMask<2>(ObjectGuid(movement.transportGuid));
        }
        packet.WriteBit(false);
        packet.WriteGuidMask<7>(ObjectGuid(movement.guid));
        packet.WriteBit(!movement.hasOrientation);
        packet.WriteGuidMask<6>(ObjectGuid(movement.guid));
        packet.WriteBit(!movement.hasSplineElevation);
        packet.WriteBit(!movement.hasPitch);
        packet.WriteGuidMask<0>(ObjectGuid(movement.guid));
        packet.WriteBit(false);
        packet.WriteBit(!movement.hasMovementFlags);
        packet.WriteBit(!movement.hasTimestamp);
        packet.WriteBit(!movement.hasUnknownUInt32);
        if (movement.hasMovementFlags)
            packet.WriteBits(0x01234567u, 30);
        packet.WriteGuidMask<1, 3, 2, 5>(ObjectGuid(movement.guid));
        packet.WriteBit(movement.hasFallData);
        if (movement.hasFallData)
            packet.WriteBit(movement.hasFallDirection);
        packet.WriteBit(!movement.hasMovementFlags2);
        if (movement.hasMovementFlags2)
            packet.WriteBits(0x1234u, 13);
    }

    static void WriteMovementBytes(WorldPacket& packet, MovementFixture const& movement)
    {
        ObjectGuid guid(movement.guid);
        ObjectGuid transportGuid(movement.transportGuid);

        packet << float(1.25f);
        packet.WriteGuidBytes<0>(guid);
        if (movement.hasTransport)
        {
            packet.WriteGuidBytes<2>(transportGuid);
            packet << int8(3);
            packet.WriteGuidBytes<3, 7>(transportGuid);
            packet << float(4.25f);
            packet.WriteGuidBytes<5>(transportGuid);
            if (movement.hasTransportTime3)
                packet << uint32(0x01020304u);
            packet << float(5.25f) << float(6.25f);
            packet.WriteGuidBytes<6, 1>(transportGuid);
            packet << float(7.25f);
            packet.WriteGuidBytes<4>(transportGuid);
            if (movement.hasTransportTime2)
                packet << uint32(0x11121314u);
            packet.WriteGuidBytes<0>(transportGuid);
            packet << uint32(0x21222324u);
        }
        packet.WriteGuidBytes<5>(guid);
        if (movement.hasFallData)
        {
            packet << uint32(0x31323334u) << float(8.25f);
            if (movement.hasFallDirection)
                packet << float(9.25f) << float(10.25f) << float(11.25f);
        }
        if (movement.hasSplineElevation)
            packet << float(12.25f);
        packet.WriteGuidBytes<6>(guid);
        if (movement.hasUnknownUInt32)
            packet << uint32(0x41424344u);
        packet.WriteGuidBytes<4>(guid);
        if (movement.hasOrientation)
            packet << float(13.25f);
        if (movement.hasTimestamp)
            packet << uint32(0x51525354u);
        packet.WriteGuidBytes<1>(guid);
        if (movement.hasPitch)
            packet << float(14.25f);
        packet.WriteGuidBytes<3>(guid);
        if (movement.writeForceIds)
            for (uint32 i = 0; i < movement.forceCount; ++i)
                packet << uint32(0x61626364u + i);
        packet << float(15.25f);
        packet.WriteGuidBytes<7>(guid);
        packet << float(16.25f);
        packet.WriteGuidBytes<2>(guid);
    }

    static WorldPacket BuildFixture(CastFixture const& fixture)
    {
        WorldPacket packet(CMSG_CAST_SPELL, 256);
        ObjectGuid targetGuid(fixture.targetGuid);
        ObjectGuid itemGuid(fixture.itemGuid);
        ObjectGuid sourceGuid(fixture.sourceTransportGuid);
        ObjectGuid destinationGuid(fixture.destinationTransportGuid);

        packet.WriteBit(false);
        packet.WriteBit(!fixture.hasTargetString);
        packet.WriteBit(false);
        packet.WriteBit(!fixture.hasCastCount);
        packet.WriteBit(fixture.hasSource);
        packet.WriteBit(fixture.hasDestination);
        packet.WriteBit(!fixture.hasSpellId);
        packet.WriteBits(fixture.researchCount, 2);
        packet.WriteBit(!fixture.hasTargetMask);
        packet.WriteBit(!fixture.hasElevation);
        for (uint8 i = 0; i < fixture.researchCount; ++i)
            packet.WriteBits(i + 1, 2);
        packet.WriteBit(!fixture.hasGlyphIndex);
        packet.WriteBit(fixture.hasMovement);
        packet.WriteBit(!fixture.hasMissileSpeed);
        packet.WriteBit(!fixture.hasCastFlags);
        packet.WriteGuidMask<5, 4, 2, 7, 1, 6, 3, 0>(targetGuid);
        if (fixture.hasDestination)
            packet.WriteGuidMask<1, 3, 5, 0, 2, 6, 7, 4>(destinationGuid);
        if (fixture.hasMovement)
            WriteMovementBits(packet, fixture.movement);
        packet.WriteGuidMask<1, 0, 7, 4, 6, 5, 3, 2>(itemGuid);
        if (fixture.hasSource)
            packet.WriteGuidMask<4, 5, 3, 0, 7, 1, 6, 2>(sourceGuid);
        if (fixture.hasTargetMask)
            packet.WriteBits(fixture.targetMask, 20);
        if (fixture.hasCastFlags)
            packet.WriteBits(fixture.castFlags, 5);
        if (fixture.hasTargetString)
            packet.WriteBits(fixture.targetString.size(), 7);
        packet.FlushBits();

        for (uint8 i = 0; i < fixture.researchCount; ++i)
            packet << uint32(0x70000000u + i) << uint32(0x71000000u + i);
        if (fixture.hasMovement)
            WriteMovementBytes(packet, fixture.movement);
        packet.WriteGuidBytes<4, 2, 1, 5, 7, 3, 6, 0>(itemGuid);
        if (fixture.hasDestination)
        {
            packet.WriteGuidBytes<2>(destinationGuid);
            packet << float(21.25f);
            packet.WriteGuidBytes<4, 1, 0, 3>(destinationGuid);
            packet << float(22.25f);
            packet.WriteGuidBytes<7>(destinationGuid);
            packet << float(23.25f);
            packet.WriteGuidBytes<5, 6>(destinationGuid);
        }
        packet.WriteGuidBytes<3, 4, 7, 6, 2, 0, 1, 5>(targetGuid);
        if (fixture.hasSource)
        {
            packet << float(31.25f);
            packet.WriteGuidBytes<5, 1, 7, 6>(sourceGuid);
            packet << float(32.25f);
            packet.WriteGuidBytes<3, 2, 0, 4>(sourceGuid);
            packet << float(33.25f);
        }
        if (fixture.hasTargetString)
            packet.append(fixture.targetString.data(), fixture.targetString.size());
        if (fixture.hasMissileSpeed)
            packet << fixture.missileSpeed;
        if (fixture.hasElevation)
            packet << fixture.elevation;
        if (fixture.hasCastCount)
            packet << fixture.castCount;
        if (fixture.hasSpellId)
            packet << fixture.spellId;
        if (fixture.hasGlyphIndex)
            packet << fixture.glyphIndex;
        return packet;
    }

    static bool Read(WorldPacket& packet, MopSpellPackets::CastSpellRequest& request)
    {
        return MopSpellPackets::ReadCastSpellRequest(packet, request);
    }

    static void CheckDense(CastFixture const& fixture)
    {
        WorldPacket packet = BuildFixture(fixture);
        MopSpellPackets::CastSpellRequest request;
        CHECK(Read(packet, request));
        CHECK(packet.rpos() == packet.size());
        CHECK(request.castCount == fixture.castCount);
        CHECK(request.spellId == fixture.spellId);
        CHECK(request.glyphIndex == fixture.glyphIndex);
        CHECK(request.castFlags == fixture.castFlags);
        CHECK(request.targetMask == fixture.targetMask);
        CHECK(request.targetGuid.GetRawValue() == fixture.targetGuid);
        CHECK(request.itemTargetGuid.GetRawValue() == fixture.itemGuid);
        CHECK(request.sourceTransportGuid.GetRawValue() == fixture.sourceTransportGuid);
        CHECK(request.destinationTransportGuid.GetRawValue() == fixture.destinationTransportGuid);
        CHECK(request.targetString == fixture.targetString);
        CHECK(request.missileSpeed == fixture.missileSpeed);
        CHECK(request.elevation == fixture.elevation);
        CHECK(request.sourceX == 32.25f && request.sourceY == 31.25f && request.sourceZ == 33.25f);
        CHECK(request.destinationX == 21.25f && request.destinationY == 22.25f && request.destinationZ == 23.25f);
    }

    static WorldPacket BuildSpellStartFixture(MopSpellPackets::SpellStartPacket const& spell)
    {
        WorldPacket packet(SMSG_SPELL_START, 256);
        ObjectGuid unknownGuid;

        packet.WriteBits(0, 24);
        packet.WriteGuidMask<5>(spell.casterGuid);
        packet.WriteBit(true);
        packet.WriteBit(false);
        packet.WriteGuidMask<4>(spell.casterUnitGuid);
        packet.WriteGuidMask<2>(spell.casterGuid);
        packet.WriteBits(spell.runeCooldownCount, 3);
        packet.WriteGuidMask<2, 6>(spell.casterUnitGuid);
        packet.WriteBits(0, 25);
        packet.WriteBits(0, 13);
        packet.WriteGuidMask<4>(spell.casterGuid);
        packet.WriteBits(0, 24);
        packet.WriteGuidMask<7>(spell.casterUnitGuid);
        packet.WriteBit(spell.hasSourceLocation);
        packet.WriteBits(spell.hasPredictedPower ? 1 : 0, 21);
        packet.WriteGuidMask<3, 0, 1, 7, 2, 6, 4, 5>(spell.itemTargetGuid);
        packet.WriteBit(!spell.hasElevation);
        packet.WriteBit(!spell.hasTargetString);
        packet.WriteBit(!spell.hasAmmoInventoryType);
        packet.WriteBit(spell.hasDestinationLocation);
        packet.WriteBit(true);
        packet.WriteGuidMask<3>(spell.casterGuid);
        if (spell.hasDestinationLocation)
            packet.WriteGuidMask<1, 6, 2, 7, 0, 3, 5, 4>(spell.destinationTransportGuid);
        packet.WriteBit(!spell.hasAmmoDisplayId);
        if (spell.hasSourceLocation)
            packet.WriteGuidMask<4, 3, 5, 1, 7, 0, 6, 2>(spell.sourceTransportGuid);
        packet.WriteBit(false);
        packet.WriteGuidMask<6>(spell.casterGuid);
        packet.WriteGuidMask<2, 1, 7, 6, 0, 5, 3, 4>(unknownGuid);
        packet.WriteBit(spell.targetMask == 0);
        if (spell.targetMask)
            packet.WriteBits(spell.targetMask, 20);
        packet.WriteGuidMask<1>(spell.casterGuid);
        packet.WriteBit(!spell.hasPredictedHeal);
        packet.WriteBit(true);
        packet.WriteBit(!spell.hasCastSchoolImmunities);
        packet.WriteGuidMask<5>(spell.casterUnitGuid);
        packet.WriteBit(false);
        packet.WriteBits(0, 20);
        packet.WriteGuidMask<1, 4, 6, 7, 5, 3, 0, 2>(spell.targetGuid);
        packet.WriteGuidMask<0>(spell.casterGuid);
        packet.WriteGuidMask<3>(spell.casterUnitGuid);
        packet.WriteBit(true);
        if (spell.hasTargetString)
            packet.WriteBits(spell.targetString.size(), 7);
        packet.WriteBit(!spell.hasCastImmunities);
        packet.WriteGuidMask<1>(spell.casterUnitGuid);
        packet.WriteBit(spell.hasVisualChain);
        packet.WriteGuidMask<7>(spell.casterGuid);
        packet.WriteBit(!spell.hasPredictedType);
        packet.WriteGuidMask<0>(spell.casterUnitGuid);
        packet.FlushBits();

        packet.WriteGuidBytes<1, 7, 6, 0, 4, 2, 3, 5>(spell.itemTargetGuid);
        packet.WriteGuidBytes<4, 5, 1, 7, 6, 3, 2, 0>(spell.targetGuid);
        packet << spell.castTime;
        packet.WriteGuidBytes<4, 5, 3, 2, 1, 6, 7, 0>(unknownGuid);
        if (spell.hasDestinationLocation)
        {
            packet.WriteGuidBytes<4, 0, 5, 7, 1, 2, 3>(spell.destinationTransportGuid);
            packet << spell.destinationY << spell.destinationZ;
            packet.WriteGuidBytes<6>(spell.destinationTransportGuid);
            packet << spell.destinationX;
        }
        if (spell.hasSourceLocation)
        {
            packet.WriteGuidBytes<0, 5, 4, 7, 3, 6>(spell.sourceTransportGuid);
            packet << spell.sourceX;
            packet.WriteGuidBytes<2>(spell.sourceTransportGuid);
            packet << spell.sourceZ;
            packet.WriteGuidBytes<1>(spell.sourceTransportGuid);
            packet << spell.sourceY;
        }
        packet.WriteGuidBytes<4>(spell.casterGuid);
        if (spell.hasCastSchoolImmunities)
            packet << spell.castSchoolImmunities;
        packet.WriteGuidBytes<2>(spell.casterGuid);
        if (spell.hasCastImmunities)
            packet << spell.castImmunities;
        if (spell.hasVisualChain)
            packet << spell.visualChainFirst << spell.visualChainSecond;
        if (spell.hasPredictedPower)
            packet << spell.predictedPower << spell.predictedPowerType;
        packet << spell.castFlags;
        packet.WriteGuidBytes<5, 7, 1>(spell.casterGuid);
        packet << spell.castCount;
        packet.WriteGuidBytes<7, 0>(spell.casterUnitGuid);
        packet.WriteGuidBytes<6, 0>(spell.casterGuid);
        packet.WriteGuidBytes<1>(spell.casterUnitGuid);
        if (spell.hasAmmoInventoryType)
            packet << spell.ammoInventoryType;
        if (spell.hasPredictedHeal)
            packet << spell.predictedHeal;
        packet.WriteGuidBytes<6, 3>(spell.casterUnitGuid);
        packet << spell.spellId;
        if (spell.hasAmmoDisplayId)
            packet << spell.ammoDisplayId;
        packet.WriteGuidBytes<4, 5, 2>(spell.casterUnitGuid);
        if (spell.hasTargetString)
            packet.append(spell.targetString.data(), spell.targetString.size());
        if (spell.hasPredictedType)
            packet << spell.predictedType;
        packet.WriteGuidBytes<3>(spell.casterGuid);
        if (spell.hasElevation)
            packet << spell.elevation;
        for (uint8 i = 0; i < spell.runeCooldownCount; ++i)
            packet << spell.runeCooldowns[i];
        return packet;
    }

    static WorldPacket BuildSpellGoFixture(MopSpellPackets::SpellGoPacket const& spell)
    {
        WorldPacket packet(SMSG_SPELL_GO, 512);
        ObjectGuid auxiliaryGuid;

        packet.WriteGuidMask<2>(spell.casterUnitGuid);
        packet.WriteBit(!spell.hasAmmoInventoryType);
        packet.WriteBit(spell.hasSourceLocation);
        packet.WriteGuidMask<2>(spell.casterGuid);
        if (spell.hasSourceLocation)
            packet.WriteGuidMask<3, 7, 4, 2, 0, 6, 1, 5>(spell.sourceTransportGuid);
        packet.WriteGuidMask<6>(spell.casterGuid);
        packet.WriteBit(!spell.hasDestinationTrailingByte);
        packet.WriteGuidMask<7>(spell.casterUnitGuid);
        packet.WriteBits(0, 20);
        packet.WriteBits(spell.misses.size(), 25);
        packet.WriteBits(spell.misses.size(), 24);
        packet.WriteGuidMask<1>(spell.casterUnitGuid);
        packet.WriteGuidMask<0>(spell.casterGuid);
        packet.WriteBits(0, 13);
        for (MopSpellPackets::SpellGoMiss const& miss : spell.misses)
            packet.WriteGuidMask<1, 3, 6, 4, 5, 2, 0, 7>(miss.guid);
        packet.WriteGuidMask<5>(spell.casterUnitGuid);
        packet.WriteBit(false);
        packet.WriteBit(false);
        packet.WriteBit(!spell.hasTargetString);
        packet.WriteGuidMask<7, 2, 1, 3, 6, 0, 5, 4>(spell.itemTargetGuid);
        packet.WriteGuidMask<7>(spell.casterGuid);
        packet.WriteGuidMask<0, 6, 5, 7, 4, 2, 3, 1>(spell.targetGuid);
        packet.WriteBit(!spell.hasRuneStateBefore);
        packet.WriteBits(spell.hasPredictedPower ? 1 : 0, 21);
        packet.WriteGuidMask<1>(spell.casterGuid);
        packet.WriteBit(!spell.hasPredictedType);
        packet.WriteBit(spell.targetMask == 0);
        packet.WriteGuidMask<3>(spell.casterUnitGuid);
        if (spell.hasTargetString)
            packet.WriteBits(spell.targetString.size(), 7);
        packet.WriteBit(!spell.hasPredictedHeal);
        packet.WriteBit(false);
        packet.WriteBit(!spell.hasCastImmunities);
        packet.WriteGuidMask<6>(spell.casterUnitGuid);
        packet.WriteBit(false);
        packet.WriteBit(spell.hasVisualChain);
        packet.WriteGuidMask<7, 6, 1, 2, 0, 5, 3, 4>(auxiliaryGuid);
        packet.WriteBit(!spell.hasDelay);
        packet.WriteBit(!spell.hasCastSchoolImmunities);
        packet.WriteBits(spell.runeCooldownCount, 3);
        packet.WriteGuidMask<0>(spell.casterUnitGuid);
        for (MopSpellPackets::SpellGoMiss const& miss : spell.misses)
        {
            packet.WriteBits(miss.reason, 4);
            if (miss.reason == SPELL_MISS_REFLECT)
                packet.WriteBits(miss.reflectResult, 4);
        }
        if (spell.targetMask)
            packet.WriteBits(spell.targetMask, 20);
        packet.WriteBit(!spell.hasElevation);
        packet.WriteBit(!spell.hasRuneStateAfter);
        packet.WriteGuidMask<4>(spell.casterGuid);
        packet.WriteBit(!spell.hasAmmoDisplayId);
        packet.WriteBit(spell.hasDestinationLocation);
        packet.WriteGuidMask<5>(spell.casterGuid);
        packet.WriteBits(spell.hitGuids.size(), 24);
        if (spell.hasDestinationLocation)
            packet.WriteGuidMask<0, 3, 2, 1, 4, 5, 6, 7>(spell.destinationTransportGuid);
        packet.WriteGuidMask<4>(spell.casterUnitGuid);
        for (ObjectGuid const& hit : spell.hitGuids)
            packet.WriteGuidMask<2, 7, 1, 6, 4, 5, 0, 3>(hit);
        packet.WriteGuidMask<3>(spell.casterGuid);
        packet.FlushBits();

        packet.WriteGuidBytes<5, 2, 1, 6, 0, 3, 4, 7>(spell.targetGuid);
        packet.WriteGuidBytes<5, 2, 0, 6, 7, 3, 1, 4>(spell.itemTargetGuid);
        packet.WriteGuidBytes<2>(spell.casterGuid);
        for (ObjectGuid const& hit : spell.hitGuids)
            packet.WriteGuidBytes<0, 6, 2, 7, 5, 4, 3, 1>(hit);
        packet.WriteGuidBytes<6, 2, 7, 1, 4, 3, 5, 0>(auxiliaryGuid);
        if (spell.hasDelay)
            packet << spell.delay;
        packet << spell.timestamp;
        for (MopSpellPackets::SpellGoMiss const& miss : spell.misses)
            packet.WriteGuidBytes<4, 2, 0, 6, 7, 5, 1, 3>(miss.guid);
        if (spell.hasDestinationLocation)
        {
            packet << spell.destinationZ << spell.destinationY;
            packet.WriteGuidBytes<4, 5, 7, 6, 1, 2>(spell.destinationTransportGuid);
            packet << spell.destinationX;
            packet.WriteGuidBytes<0, 3>(spell.destinationTransportGuid);
        }
        packet.WriteGuidBytes<6>(spell.casterGuid);
        packet.WriteGuidBytes<7>(spell.casterUnitGuid);
        packet.WriteGuidBytes<1>(spell.casterGuid);
        if (spell.hasVisualChain)
            packet << spell.visualChainFirst << spell.visualChainSecond;
        packet << spell.castFlags;
        if (spell.hasSourceLocation)
        {
            packet.WriteGuidBytes<2>(spell.sourceTransportGuid);
            packet << spell.sourceY << spell.sourceX;
            packet.WriteGuidBytes<6, 5, 1, 7>(spell.sourceTransportGuid);
            packet << spell.sourceZ;
            packet.WriteGuidBytes<3, 0, 4>(spell.sourceTransportGuid);
        }
        packet.WriteGuidBytes<6>(spell.casterUnitGuid);
        if (spell.hasPredictedType)
            packet << spell.predictedType;
        packet.WriteGuidBytes<4>(spell.casterGuid);
        if (spell.hasCastSchoolImmunities)
            packet << spell.castSchoolImmunities;
        if (spell.hasCastImmunities)
            packet << spell.castImmunities;
        if (spell.hasAmmoDisplayId)
            packet << spell.ammoDisplayId;
        packet.WriteGuidBytes<1>(spell.casterUnitGuid);
        if (spell.hasAmmoInventoryType)
            packet << spell.ammoInventoryType;
        if (spell.hasPredictedPower)
            packet << spell.predictedPowerType << spell.predictedPower;
        if (spell.hasRuneStateAfter)
            packet << spell.runeStateAfter;
        for (uint8 i = 0; i < spell.runeCooldownCount; ++i)
            packet << spell.runeCooldowns[i];
        if (spell.hasRuneStateBefore)
            packet << spell.runeStateBefore;
        packet.WriteGuidBytes<0>(spell.casterGuid);
        if (spell.hasDestinationTrailingByte)
            packet << spell.destinationTrailingByte;
        if (spell.hasPredictedHeal)
            packet << spell.predictedHeal;
        packet << spell.castCount;
        packet.WriteGuidBytes<5>(spell.casterGuid);
        packet.WriteGuidBytes<2>(spell.casterUnitGuid);
        packet.WriteGuidBytes<3>(spell.casterGuid);
        packet.WriteGuidBytes<5>(spell.casterUnitGuid);
        if (spell.hasTargetString)
            packet.append(spell.targetString.data(), spell.targetString.size());
        packet << spell.spellId;
        if (spell.hasElevation)
            packet << spell.elevation;
        packet.WriteGuidBytes<0, 3, 4>(spell.casterUnitGuid);
        packet.WriteGuidBytes<7>(spell.casterGuid);
        return packet;
    }
}

static void test_dense_and_guid_permutations()
{
    CastFixture fixture;
    CheckDense(fixture);

    fixture.targetGuid = UINT64_C(0x0007000500030001);
    fixture.itemGuid = UINT64_C(0x8000600040002000);
    fixture.sourceTransportGuid = UINT64_C(0x0011003300550077);
    fixture.destinationTransportGuid = UINT64_C(0x8800660044002200);
    fixture.movement.guid = UINT64_C(0x1000300050007000);
    fixture.movement.transportGuid = UINT64_C(0x0020004000600080);
    CheckDense(fixture);
}

static void test_sparse_defaults()
{
    CastFixture fixture;
    fixture.hasTargetString = fixture.hasCastCount = fixture.hasSource = false;
    fixture.hasDestination = fixture.hasSpellId = fixture.hasTargetMask = false;
    fixture.hasElevation = fixture.hasGlyphIndex = fixture.hasMovement = false;
    fixture.hasMissileSpeed = fixture.hasCastFlags = false;
    fixture.researchCount = 0;
    fixture.targetGuid = fixture.itemGuid = 0;

    WorldPacket packet = BuildFixture(fixture);
    MopSpellPackets::CastSpellRequest request;
    CHECK(Read(packet, request));
    CHECK(packet.rpos() == packet.size());
    CHECK(request.castCount == 0);
    CHECK(request.spellId == 0);
    CHECK(request.glyphIndex == 0);
    CHECK(request.castFlags == 0);
    CHECK(request.targetMask == 0);
    CHECK(!request.targetGuid);
    CHECK(!request.itemTargetGuid);
    CHECK(request.targetString.empty());
    CHECK(request.missileSpeed == 0.0f);
    CHECK(request.elevation == 0.0f);
}

static void test_missile_and_elevation_presence_are_not_swapped()
{
    CastFixture fixture;
    fixture.hasMissileSpeed = true;
    fixture.hasElevation = false;
    WorldPacket missile = BuildFixture(fixture);
    MopSpellPackets::CastSpellRequest missileRequest;
    CHECK(Read(missile, missileRequest));
    CHECK(missileRequest.missileSpeed == fixture.missileSpeed);
    CHECK(missileRequest.elevation == 0.0f);

    fixture.hasMissileSpeed = false;
    fixture.hasElevation = true;
    WorldPacket elevation = BuildFixture(fixture);
    MopSpellPackets::CastSpellRequest elevationRequest;
    CHECK(Read(elevation, elevationRequest));
    CHECK(elevationRequest.missileSpeed == 0.0f);
    CHECK(elevationRequest.elevation == fixture.elevation);
}

static void test_hostile_movement_count_rejected()
{
    CastFixture fixture;
    fixture.researchCount = 0;
    fixture.movement.forceCount = (1u << 22) - 1u;
    fixture.movement.writeForceIds = false;
    WorldPacket packet = BuildFixture(fixture);
    MopSpellPackets::CastSpellRequest request;
    CHECK(!Read(packet, request));
    CHECK(packet.rpos() == packet.size());
}

static void test_every_truncated_dense_prefix_and_trailing_byte_rejected()
{
    WorldPacket complete = BuildFixture(CastFixture());
    std::vector<uint8> bytes(complete.contents(), complete.contents() + complete.size());

    for (size_t size = 0; size < bytes.size(); ++size)
    {
        WorldPacket truncated(CMSG_CAST_SPELL, size);
        if (size)
            truncated.append(bytes.data(), size);
        MopSpellPackets::CastSpellRequest request;
        CHECK(!Read(truncated, request));
        CHECK(truncated.rpos() == truncated.size());
    }

    WorldPacket trailing(CMSG_CAST_SPELL, bytes.size() + 1);
    trailing.append(bytes.data(), bytes.size());
    trailing << uint8(0xCC);
    MopSpellPackets::CastSpellRequest request;
    CHECK(!Read(trailing, request));
    CHECK(trailing.rpos() == trailing.size());
}

static void test_declared_string_larger_than_remaining_rejected()
{
    CastFixture fixture;
    fixture.hasMovement = false;
    WorldPacket full = BuildFixture(fixture);
    std::vector<uint8> bytes(full.contents(), full.contents() + full.size());
    bytes.resize(bytes.size() - fixture.targetString.size() - sizeof(float) * 2 -
        sizeof(uint8) - sizeof(uint32) * 2);

    WorldPacket truncated(CMSG_CAST_SPELL, bytes.size());
    truncated.append(bytes.data(), bytes.size());
    MopSpellPackets::CastSpellRequest request;
    CHECK(!Read(truncated, request));
    CHECK(truncated.rpos() == truncated.size());
}

static void test_opcode_is_framable()
{
    CHECK(uint32(CMSG_CAST_SPELL) == 0x0206u);
    CHECK(uint32(CMSG_CAST_SPELL) < uint32(OPCODE_TABLE_SIZE));
}

static void test_cast_failure_result_translation()
{
    CHECK(MopSpellPackets::ToClientCastResult(SPELL_FAILED_CANT_STEALTH) == 22u);
    CHECK(MopSpellPackets::ToClientCastResult(SPELL_FAILED_CASTER_AURASTATE) == 24u);
    CHECK(MopSpellPackets::ToClientCastResult(SPELL_FAILED_TARGET_IN_COMBAT) == 119u);
    CHECK(MopSpellPackets::ToClientCastResult(SPELL_FAILED_TARGET_IS_PLAYER) == 121u);
    CHECK(MopSpellPackets::ToClientCastResult(SPELL_FAILED_BM_OR_INVISGOD) == 164u);
    CHECK(MopSpellPackets::ToClientCastResult(SPELL_FAILED_EXPERT_RIDING_REQUIREMENT) == 167u);
    CHECK(MopSpellPackets::ToClientCastResult(SPELL_FAILED_ARTISAN_RIDING_REQUIREMENT) == 168u);
    CHECK(MopSpellPackets::ToClientCastResult(SPELL_FAILED_NOT_IDLE) == 174u);
    CHECK(MopSpellPackets::ToClientCastResult(SPELL_FAILED_NOT_IN_LFG_DUNGEON) == 211u);
    CHECK(MopSpellPackets::ToClientCastResult(SPELL_FAILED_UNKNOWN) == 254u);
}

static void test_cast_failure_wire_layout()
{
    MopSpellPackets::CastFailedArguments arguments;
    WorldPacket packet;

    MopSpellPackets::BuildCastFailed(packet, 0x11223344u, SPELL_FAILED_CASTER_AURASTATE, 7, false, arguments);
    CHECK(packet.GetOpcode() == SMSG_CAST_FAILED);
    CheckBytes(packet, { 0x44, 0x33, 0x22, 0x11, 0x18, 0x00, 0x00, 0x00, 0x07, 0xC0 });

    arguments.hasArg10 = true;
    arguments.arg10 = 0x55667788u;
    MopSpellPackets::BuildCastFailed(packet, 0x11223344u, SPELL_FAILED_CASTER_AURASTATE, 7, false, arguments);
    CheckBytes(packet, {
        0x44, 0x33, 0x22, 0x11, 0x18, 0x00, 0x00, 0x00, 0x07, 0x40,
        0x88, 0x77, 0x66, 0x55
    });

    arguments.hasArg10 = false;
    arguments.hasArg18 = true;
    arguments.arg18 = 0x99AABBCCu;
    MopSpellPackets::BuildCastFailed(packet, 0x11223344u, SPELL_FAILED_CASTER_AURASTATE, 7, false, arguments);
    CheckBytes(packet, {
        0x44, 0x33, 0x22, 0x11, 0x18, 0x00, 0x00, 0x00, 0x07, 0x80,
        0xCC, 0xBB, 0xAA, 0x99
    });

    arguments.hasArg10 = true;
    arguments.arg10 = 0x55667788u;
    MopSpellPackets::BuildCastFailed(packet, 0x11223344u, SPELL_FAILED_CASTER_AURASTATE, 7, false, arguments);
    CheckBytes(packet, {
        0x44, 0x33, 0x22, 0x11, 0x18, 0x00, 0x00, 0x00, 0x07, 0x00,
        0x88, 0x77, 0x66, 0x55, 0xCC, 0xBB, 0xAA, 0x99
    });
}

static void test_pet_cast_failure_wire_layout()
{
    MopSpellPackets::CastFailedArguments arguments;
    WorldPacket packet;

    MopSpellPackets::BuildCastFailed(packet, 0x11223344u, SPELL_FAILED_CASTER_AURASTATE, 7, true, arguments);
    CHECK(packet.GetOpcode() == SMSG_PET_CAST_FAILED);
    CheckBytes(packet, { 0x18, 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11, 0x07, 0xC0 });

    arguments.hasArg10 = true;
    arguments.arg10 = 0x55667788u;
    MopSpellPackets::BuildCastFailed(packet, 0x11223344u, SPELL_FAILED_CASTER_AURASTATE, 7, true, arguments);
    CheckBytes(packet, {
        0x18, 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11, 0x07, 0x80,
        0x88, 0x77, 0x66, 0x55
    });

    arguments.hasArg10 = false;
    arguments.hasArg18 = true;
    arguments.arg18 = 0x99AABBCCu;
    MopSpellPackets::BuildCastFailed(packet, 0x11223344u, SPELL_FAILED_CASTER_AURASTATE, 7, true, arguments);
    CheckBytes(packet, {
        0x18, 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11, 0x07, 0x40,
        0xCC, 0xBB, 0xAA, 0x99
    });

    arguments.hasArg10 = true;
    arguments.arg10 = 0x55667788u;
    MopSpellPackets::BuildCastFailed(packet, 0x11223344u, SPELL_FAILED_CASTER_AURASTATE, 7, true, arguments);
    CheckBytes(packet, {
        0x18, 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11, 0x07, 0x00,
        0x88, 0x77, 0x66, 0x55, 0xCC, 0xBB, 0xAA, 0x99
    });
}

static void test_spell_start_wire_layout()
{
    MopSpellPackets::SpellStartPacket spell;
    spell.casterGuid = ObjectGuid(UINT64_C(0x8070605040302010));
    spell.casterUnitGuid = ObjectGuid(UINT64_C(0x1020304050607080));
    spell.targetGuid = ObjectGuid(UINT64_C(0x0011220033440055));
    spell.itemTargetGuid = ObjectGuid(UINT64_C(0x660077880099AA00));
    spell.hasSourceLocation = true;
    spell.sourceTransportGuid = ObjectGuid(UINT64_C(0x8877665544332211));
    spell.sourceX = 1.25f;
    spell.sourceY = 2.25f;
    spell.sourceZ = 3.25f;
    spell.hasDestinationLocation = true;
    spell.destinationTransportGuid = ObjectGuid(UINT64_C(0x1122334455667788));
    spell.destinationX = 4.25f;
    spell.destinationY = 5.25f;
    spell.destinationZ = 6.25f;
    spell.targetMask = TARGET_FLAG_UNIT | TARGET_FLAG_ITEM |
        TARGET_FLAG_SOURCE_LOCATION | TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_STRING;
    spell.hasTargetString = true;
    spell.targetString = "Target";
    spell.castTime = 0x01020304u;
    spell.castFlags = 0x11223344u;
    spell.castCount = 7;
    spell.spellId = 0x55667788u;
    spell.hasPredictedPower = true;
    spell.predictedPower = -123;
    spell.predictedPowerType = 3;
    spell.hasCastSchoolImmunities = true;
    spell.castSchoolImmunities = 0xA1A2A3A4u;
    spell.hasCastImmunities = true;
    spell.castImmunities = 0xB1B2B3B4u;
    spell.hasVisualChain = true;
    spell.visualChainFirst = 0xC1C2C3C4u;
    spell.visualChainSecond = 0xD1D2D3D4u;
    spell.hasAmmoInventoryType = true;
    spell.ammoInventoryType = 2;
    spell.hasAmmoDisplayId = true;
    spell.ammoDisplayId = 0xE1E2E3E4u;
    spell.hasPredictedHeal = true;
    spell.predictedHeal = 0xF1F2F3F4u;
    spell.hasPredictedType = true;
    spell.predictedType = 1;
    spell.hasElevation = true;
    spell.elevation = -0.75f;
    spell.runeCooldownCount = 2;
    spell.runeCooldowns[0] = 0x11;
    spell.runeCooldowns[1] = 0x22;

    WorldPacket expected = BuildSpellStartFixture(spell);
    WorldPacket actual;
    CHECK(MopSpellPackets::BuildSpellStart(actual, spell));
    CHECK(actual.GetOpcode() == SMSG_SPELL_START);
    CHECK(actual.size() == expected.size());
    CHECK(actual.size() == 0 || std::memcmp(actual.contents(), expected.contents(), actual.size()) == 0);

    MopSpellPackets::SpellStartPacket sparse;
    sparse.castTime = 25;
    sparse.castFlags = CAST_FLAG_HAS_TRAJECTORY;
    sparse.castCount = 1;
    sparse.spellId = 133;
    expected = BuildSpellStartFixture(sparse);
    CHECK(MopSpellPackets::BuildSpellStart(actual, sparse));
    CHECK(actual.size() == expected.size());
    CHECK(actual.size() == 0 || std::memcmp(actual.contents(), expected.contents(), actual.size()) == 0);

    sparse.hasTargetString = true;
    expected = BuildSpellStartFixture(sparse);
    CHECK(MopSpellPackets::BuildSpellStart(actual, sparse));
    CHECK(actual.size() == expected.size());
    CHECK(actual.size() == 0 || std::memcmp(actual.contents(), expected.contents(), actual.size()) == 0);

    sparse.hasTargetString = true;
    sparse.targetString.assign(128, 'x');
    CHECK(!MopSpellPackets::BuildSpellStart(actual, sparse));
    sparse.hasTargetString = false;
    sparse.targetString.clear();
    sparse.targetMask = 0x100000;
    CHECK(!MopSpellPackets::BuildSpellStart(actual, sparse));
    sparse.targetMask = 0;
    sparse.runeCooldownCount = 8;
    CHECK(!MopSpellPackets::BuildSpellStart(actual, sparse));
}

static void test_spell_go_wire_layout()
{
    MopSpellPackets::SpellGoPacket spell;
    spell.casterGuid = ObjectGuid(UINT64_C(0x8070605040302010));
    spell.casterUnitGuid = ObjectGuid(UINT64_C(0x1020304050607080));
    spell.targetGuid = ObjectGuid(UINT64_C(0x0011220033440055));
    spell.itemTargetGuid = ObjectGuid(UINT64_C(0x660077880099AA00));
    spell.hitGuids.push_back(ObjectGuid(UINT64_C(0x8877665544332211)));
    spell.hitGuids.push_back(ObjectGuid(UINT64_C(0x1122334455667788)));
    MopSpellPackets::SpellGoMiss reflect;
    reflect.guid = ObjectGuid(UINT64_C(0xA1A2A3A4A5A6A7A8));
    reflect.reason = SPELL_MISS_REFLECT;
    reflect.reflectResult = SPELL_MISS_RESIST;
    spell.misses.push_back(reflect);
    MopSpellPackets::SpellGoMiss resist;
    resist.guid = ObjectGuid(UINT64_C(0xB1B2B3B4B5B6B7B8));
    resist.reason = SPELL_MISS_RESIST;
    spell.misses.push_back(resist);
    spell.hasSourceLocation = true;
    spell.sourceTransportGuid = ObjectGuid(UINT64_C(0xC1C2C3C4C5C6C7C8));
    spell.sourceX = 1.25f;
    spell.sourceY = 2.25f;
    spell.sourceZ = 3.25f;
    spell.hasDestinationLocation = true;
    spell.destinationTransportGuid = ObjectGuid(UINT64_C(0xD1D2D3D4D5D6D7D8));
    spell.destinationX = 4.25f;
    spell.destinationY = 5.25f;
    spell.destinationZ = 6.25f;
    spell.hasDestinationTrailingByte = true;
    spell.destinationTrailingByte = 7;
    spell.targetMask = TARGET_FLAG_UNIT | TARGET_FLAG_ITEM |
        TARGET_FLAG_SOURCE_LOCATION | TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_STRING;
    spell.hasTargetString = true;
    spell.targetString = "Target";
    spell.hasDelay = true;
    spell.delay = 0x01020304u;
    spell.timestamp = 0x11121314u;
    spell.castFlags = 0x21222324u;
    spell.castCount = 9;
    spell.spellId = 0x31323334u;
    spell.hasPredictedPower = true;
    spell.predictedPowerType = 3;
    spell.predictedPower = -456;
    spell.hasRuneStateBefore = true;
    spell.runeStateBefore = 0x15;
    spell.hasRuneStateAfter = true;
    spell.runeStateAfter = 0x2A;
    spell.runeCooldownCount = 2;
    spell.runeCooldowns[0] = 0x33;
    spell.runeCooldowns[1] = 0x44;
    spell.hasPredictedType = true;
    spell.predictedType = 1;
    spell.hasPredictedHeal = true;
    spell.predictedHeal = 0x41424344u;
    spell.hasCastSchoolImmunities = true;
    spell.castSchoolImmunities = 0x51525354u;
    spell.hasCastImmunities = true;
    spell.castImmunities = 0x61626364u;
    spell.hasVisualChain = true;
    spell.visualChainFirst = 0x71727374u;
    spell.visualChainSecond = 0x81828384u;
    spell.hasAmmoInventoryType = true;
    spell.ammoInventoryType = 2;
    spell.hasAmmoDisplayId = true;
    spell.ammoDisplayId = 0x91929394u;
    spell.hasElevation = true;
    spell.elevation = -0.75f;

    WorldPacket expected = BuildSpellGoFixture(spell);
    WorldPacket actual;
    CHECK(MopSpellPackets::BuildSpellGo(actual, spell));
    CHECK(actual.GetOpcode() == SMSG_SPELL_GO);
    CHECK(actual.size() == expected.size());
    CHECK(actual.size() == 0 || std::memcmp(actual.contents(), expected.contents(), actual.size()) == 0);

    MopSpellPackets::SpellGoPacket sparse;
    sparse.timestamp = 25;
    sparse.castFlags = CAST_FLAG_UNKNOWN9;
    sparse.castCount = 1;
    sparse.spellId = 133;
    expected = BuildSpellGoFixture(sparse);
    CHECK(MopSpellPackets::BuildSpellGo(actual, sparse));
    CHECK(actual.size() == expected.size());
    CHECK(actual.size() == 0 || std::memcmp(actual.contents(), expected.contents(), actual.size()) == 0);

    sparse.hasTargetString = true;
    sparse.targetString.assign(128, 'x');
    CHECK(!MopSpellPackets::BuildSpellGo(actual, sparse));
    sparse.hasTargetString = false;
    sparse.targetString.clear();
    sparse.targetMask = 0x100000;
    CHECK(!MopSpellPackets::BuildSpellGo(actual, sparse));
    sparse.targetMask = 0;
    sparse.runeCooldownCount = 8;
    CHECK(!MopSpellPackets::BuildSpellGo(actual, sparse));
    sparse.runeCooldownCount = 0;
    MopSpellPackets::SpellGoMiss invalidMiss;
    invalidMiss.reason = 0x10;
    sparse.misses.push_back(invalidMiss);
    CHECK(!MopSpellPackets::BuildSpellGo(actual, sparse));
    sparse.misses[0].reason = SPELL_MISS_REFLECT;
    sparse.misses[0].reflectResult = 0x10;
    CHECK(!MopSpellPackets::BuildSpellGo(actual, sparse));
}

static void test_category_cooldown_request_and_response()
{
    WorldPacket emptyRequest(CMSG_REQUEST_CATEGORY_COOLDOWNS, 0);
    CHECK(MopSpellPackets::ReadCategoryCooldownRequest(emptyRequest));
    CHECK(emptyRequest.rpos() == emptyRequest.size());

    WorldPacket trailingRequest(CMSG_REQUEST_CATEGORY_COOLDOWNS, 1);
    trailingRequest << uint8(0xAA);
    CHECK(!MopSpellPackets::ReadCategoryCooldownRequest(trailingRequest));
    CHECK(trailingRequest.rpos() == trailingRequest.size());

    WorldPacket emptyResponse;
    std::vector<MopSpellPackets::CategoryCooldown> emptyRecords;
    MopSpellPackets::BuildCategoryCooldown(emptyResponse, emptyRecords);
    CHECK(emptyResponse.GetOpcode() == SMSG_CATEGORY_COOLDOWN);
    CheckBytes(emptyResponse, { 0x00, 0x00, 0x00 });

    WorldPacket response;
    std::vector<MopSpellPackets::CategoryCooldown> records = {
        { -25, 0x11223344u },
        { 1000, 0xAABBCCDDu },
    };
    MopSpellPackets::BuildCategoryCooldown(response, records);
    CHECK(response.GetOpcode() == SMSG_CATEGORY_COOLDOWN);
    CheckBytes(response, {
        0x00, 0x00, 0x10,
        0xE7, 0xFF, 0xFF, 0xFF, 0x44, 0x33, 0x22, 0x11,
        0xE8, 0x03, 0x00, 0x00, 0xDD, 0xCC, 0xBB, 0xAA,
    });

    CHECK(uint32(CMSG_REQUEST_CATEGORY_COOLDOWNS) == 0x1203u);
    CHECK(uint32(SMSG_CATEGORY_COOLDOWN) == 0x01DBu);
    CHECK(uint32(CMSG_REQUEST_CATEGORY_COOLDOWNS) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_CATEGORY_COOLDOWN) < uint32(OPCODE_TABLE_SIZE));
}

int main(int, char**)
{
    test_dense_and_guid_permutations();
    test_sparse_defaults();
    test_missile_and_elevation_presence_are_not_swapped();
    test_hostile_movement_count_rejected();
    test_every_truncated_dense_prefix_and_trailing_byte_rejected();
    test_declared_string_larger_than_remaining_rejected();
    test_opcode_is_framable();
    test_cast_failure_result_translation();
    test_cast_failure_wire_layout();
    test_pet_cast_failure_wire_layout();
    test_spell_start_wire_layout();
    test_spell_go_wire_layout();
    test_category_cooldown_request_and_response();

    if (g_fail)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_fail);
        return 1;
    }
    std::puts("mop_spell_cast_packets_test: PASS");
    return 0;
}
