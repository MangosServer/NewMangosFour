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

/**
 * Independent byte fixtures for the 5.4.8.18414 stable-pet exchange.
 */

#include "WorldSession.h"
#include "MopWireCodec.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    return packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static void Append(WorldPacket& packet, std::vector<uint8> const& bytes)
{
    if (!bytes.empty())
        packet.append(bytes.data(), bytes.size());
}

static MopStablePackets::StablePetRecord MakeRecord(uint32 slot = 0,
    std::string const& name = "")
{
    MopStablePackets::StablePetRecord record;
    record.entry = 1;
    record.level = 2;
    record.state = 1;
    record.modelId = 3;
    record.name = name;
    record.petNumber = 4;
    record.slot = slot;
    return record;
}

static void test_request_guid_fixtures()
{
    WorldPacket zero(CMSG_REQUEST_STABLED_PETS, 1);
    Append(zero, { 0x00 });
    CHECK(MopStablePackets::ReadStableListRequest(zero) == 0);

    WorldPacket nontrivial(CMSG_REQUEST_STABLED_PETS, 9);
    Append(nontrivial, {
        0xFF,
        0xEE, 0x44, 0x00, 0xCC, 0xAA, 0x88, 0x66, 0x22
    });
    CHECK(MopStablePackets::ReadStableListRequest(nontrivial) ==
        0x0123456789ABCDEFull);
}

static void test_empty_list_fixture()
{
    WorldPacket packet;
    CHECK(MopStablePackets::BuildPetStableList(packet, 0, {}));
    CHECK(packet.GetOpcode() == SMSG_PET_STABLE_LIST);
    CHECK(Equal(packet, { 0x00, 0x00, 0x00, 0x00 }));
}

static void test_two_record_list_fixture()
{
    MopStablePackets::StablePetRecord first;
    first.entry = 0x11223344u;
    first.level = 0x55667788u;
    first.state = 1;
    first.modelId = 0x99AABBCCu;
    first.name = "A";
    first.petNumber = 0xDDEEFF00u;
    first.slot = 4;

    MopStablePackets::StablePetRecord second;
    second.entry = 0x01020304u;
    second.level = 0x05060708u;
    second.state = 2;
    second.modelId = 0x0A0B0C0Du;
    second.name = "Cat";
    second.petNumber = 0x10203040u;
    second.slot = 54;

    WorldPacket packet;
    CHECK(MopStablePackets::BuildPetStableList(packet,
        0x0123456789ABCDEFull, { first, second }));
    CHECK(Equal(packet, {
        0xFF, 0x00, 0x00, 0x40, 0x20, 0x60,
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0x01,
        0xCC, 0xBB, 0xAA, 0x99,
        0x41,
        0x00, 0xFF, 0xEE, 0xDD,
        0x04, 0x00, 0x00, 0x00,
        0x04, 0x03, 0x02, 0x01,
        0x08, 0x07, 0x06, 0x05,
        0x02,
        0x0D, 0x0C, 0x0B, 0x0A,
        0x43, 0x61, 0x74,
        0x40, 0x30, 0x20, 0x10,
        0x36, 0x00, 0x00, 0x00,
        0x88, 0x44, 0x00, 0xAA, 0xEE, 0x66, 0xCC, 0x22
    }));
}

static void test_list_bounds()
{
    std::vector<MopStablePackets::StablePetRecord> records;
    for (uint32 slot = 0; slot < 55; ++slot)
        records.push_back(MakeRecord(slot));

    WorldPacket accepted;
    CHECK(MopStablePackets::BuildPetStableList(accepted, 0, records));
    CHECK(accepted.GetOpcode() == SMSG_PET_STABLE_LIST);

    WorldPacket rejected(SMSG_STABLE_RESULT, 1);
    rejected << uint8(0xA5);
    records.push_back(MakeRecord(0));
    CHECK(!MopStablePackets::BuildPetStableList(rejected, 0, records));
    CHECK(rejected.GetOpcode() == SMSG_STABLE_RESULT);
    CHECK(Equal(rejected, { 0xA5 }));

    WorldPacket name255;
    CHECK(MopStablePackets::BuildPetStableList(name255, 0,
        { MakeRecord(0, std::string(255, 'x')) }));

    WorldPacket name256(SMSG_STABLE_RESULT, 1);
    name256 << uint8(0xA6);
    CHECK(!MopStablePackets::BuildPetStableList(name256, 0,
        { MakeRecord(0, std::string(256, 'x')) }));
    CHECK(name256.GetOpcode() == SMSG_STABLE_RESULT);
    CHECK(Equal(name256, { 0xA6 }));

    WorldPacket slot54;
    CHECK(MopStablePackets::BuildPetStableList(slot54, 0,
        { MakeRecord(54) }));

    WorldPacket slot55(SMSG_STABLE_RESULT, 1);
    slot55 << uint8(0xA7);
    CHECK(!MopStablePackets::BuildPetStableList(slot55, 0,
        { MakeRecord(55) }));
    CHECK(slot55.GetOpcode() == SMSG_STABLE_RESULT);
    CHECK(Equal(slot55, { 0xA7 }));

    WorldPacket duplicateSlot(SMSG_STABLE_RESULT, 1);
    duplicateSlot << uint8(0xA8);
    CHECK(!MopStablePackets::BuildPetStableList(duplicateSlot, 0,
        { MakeRecord(7), MakeRecord(7) }));
    CHECK(duplicateSlot.GetOpcode() == SMSG_STABLE_RESULT);
    CHECK(Equal(duplicateSlot, { 0xA8 }));
}

static void test_result_values()
{
    for (uint8 value : { uint8(0), uint8(8), uint8(12), uint8(255) })
    {
        WorldPacket packet;
        MopStablePackets::BuildStableResult(packet, value);
        CHECK(packet.GetOpcode() == SMSG_STABLE_RESULT);
        CHECK(Equal(packet, { value }));
    }
}

static void test_postcrypt_headers()
{
    CHECK(uint32(CMSG_REQUEST_STABLED_PETS) == 0x02CAu);
    CHECK(uint32(SMSG_PET_STABLE_LIST) == 0x1613u);
    CHECK(uint32(SMSG_STABLE_RESULT) == 0x14BEu);

    uint8 header[4] = {};
    CHECK(MopWire::BuildServerHeader(true, 4, SMSG_PET_STABLE_LIST, header));
    CHECK(header[0] == 0x13 && header[1] == 0x96 &&
        header[2] == 0x00 && header[3] == 0x00);
    CHECK(MopWire::BuildServerHeader(true, 1, SMSG_STABLE_RESULT, header));
    CHECK(header[0] == 0xBE && header[1] == 0x34 &&
        header[2] == 0x00 && header[3] == 0x00);
}

int main(int, char**)
{
    test_request_guid_fixtures();
    test_empty_list_fixture();
    test_two_record_list_fixture();
    test_list_bounds();
    test_result_values();
    test_postcrypt_headers();
    if (g_fail)
        return 1;
    std::printf("mop_stable_packets: all checks passed\n");
    return 0;
}
