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
 * Independent byte fixtures for the 5.4.8.18414 respec confirmation exchange.
 */

#include "Player.h"
#include "MopWireCodec.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    if (packet.size() != expected.size())
        return false;
    return std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static void Append(WorldPacket& packet, std::vector<uint8> const& bytes)
{
    if (!bytes.empty())
        packet.append(bytes.data(), bytes.size());
}

static void test_outbound_zero_guid()
{
    WorldPacket packet(SMSG_RESPEC_WIPE_CONFIRM, 6);
    MopRespecPackets::BuildRespecWipeConfirm(packet, 0, 0, 0);
    CHECK(Equal(packet, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }));
}

static void test_outbound_nontrivial_guid()
{
    WorldPacket packet(SMSG_RESPEC_WIPE_CONFIRM, 14);
    MopRespecPackets::BuildRespecWipeConfirm(packet, 0x0123456789ABCDEFull, 0, 0x11223344u);
    CHECK(Equal(packet, {
        0xFF,
        0xCC, 0xEE,
        0x00,
        0x00, 0x88, 0xAA, 0x44, 0x22, 0x66,
        0x44, 0x33, 0x22, 0x11
    }));
}

static void test_inbound_zero_guid()
{
    WorldPacket packet(CMSG_CONFIRM_RESPEC_WIPE, 2);
    Append(packet, { 0x00, 0x00 });
    MopRespecPackets::ConfirmRespecWipe const confirm = MopRespecPackets::ReadConfirmRespecWipe(packet);
    CHECK(confirm.discriminator == 0);
    CHECK(confirm.trainerGuid == 0);
}

static void test_inbound_nontrivial_guid_and_discriminator()
{
    WorldPacket packet(CMSG_CONFIRM_RESPEC_WIPE, 10);
    Append(packet, {
        0x01,
        0xFF,
        0xAA, 0x66, 0xCC, 0x88, 0xEE, 0x44, 0x00, 0x22
    });
    MopRespecPackets::ConfirmRespecWipe const confirm = MopRespecPackets::ReadConfirmRespecWipe(packet);
    CHECK(confirm.discriminator == 1);
    CHECK(confirm.trainerGuid == 0x0123456789ABCDEFull);
}

static void test_postcrypt_header()
{
    CHECK(uint32(CMSG_CONFIRM_RESPEC_WIPE) == 0x0275u);
    CHECK(uint32(SMSG_RESPEC_WIPE_CONFIRM) == 0x02ABu);
    uint8 header[4] = {};
    CHECK(MopWire::BuildServerHeader(true, 6, SMSG_RESPEC_WIPE_CONFIRM, header));
    CHECK(header[0] == 0xAB && header[1] == 0xC2 && header[2] == 0x00 && header[3] == 0x00);
    CHECK(MopWire::BuildServerHeader(true, 14, SMSG_RESPEC_WIPE_CONFIRM, header));
    CHECK(header[0] == 0xAB && header[1] == 0xC2 && header[2] == 0x01 && header[3] == 0x00);
}

int main(int, char**)
{
    test_outbound_zero_guid();
    test_outbound_nontrivial_guid();
    test_inbound_zero_guid();
    test_inbound_nontrivial_guid_and_discriminator();
    test_postcrypt_header();
    if (g_fail)
        return 1;
    std::printf("mop_respec_packets: all checks passed\n");
    return 0;
}
