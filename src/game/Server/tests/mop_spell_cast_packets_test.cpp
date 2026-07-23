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
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace
{
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

int main(int, char**)
{
    test_dense_and_guid_permutations();
    test_sparse_defaults();
    test_missile_and_elevation_presence_are_not_swapped();
    test_hostile_movement_count_rejected();
    test_every_truncated_dense_prefix_and_trailing_byte_rejected();
    test_declared_string_larger_than_remaining_rejected();
    test_opcode_is_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_fail);
        return 1;
    }
    std::puts("mop_spell_cast_packets_test: PASS");
    return 0;
}
