/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "Channel.h"
#include "Opcodes.h"

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

static void test_full_channel_list()
{
    std::vector<MopChannelPackets::Member> members;
    members.push_back({UI64LIT(0x0102030405060708), 0x11223344, 0x05});
    members.push_back({UI64LIT(0xA1A2A3A4A5A6A7A8), 0x55667788, 0x09});

    WorldPacket packet;
    CHECK(MopChannelPackets::BuildList(packet, "Trade", 0x2C, members));
    CHECK(packet.GetOpcode() == SMSG_CHANNEL_LIST);
    CHECK(Equal(packet, {
        0x01,
        0x54, 0x72, 0x61, 0x64, 0x65, 0x00,
        0x2C,
        0x02, 0x00, 0x00, 0x00,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x44, 0x33, 0x22, 0x11,
        0x05,
        0xA8, 0xA7, 0xA6, 0xA5, 0xA4, 0xA3, 0xA2, 0xA1,
        0x88, 0x77, 0x66, 0x55,
        0x09
    }));
}

static void test_notify_tail_classes()
{
    WorldPacket noTail;
    CHECK(MopChannelPackets::BeginNotify(noTail, CHAT_WRONG_PASSWORD_NOTICE, "General"));
    CHECK(Equal(noTail, {
        0x04, 0x47, 0x65, 0x6E, 0x65, 0x72, 0x61, 0x6C, 0x00
    }));

    WorldPacket guidTail;
    CHECK(MopChannelPackets::BeginNotify(guidTail, CHAT_JOINED_NOTICE, "C"));
    MopChannelPackets::WriteGuidIdentity(
        guidTail, UI64LIT(0x0102030405060708), 0x11223344);
    CHECK(Equal(guidTail, {
        0x00, 0x43, 0x00,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x44, 0x33, 0x22, 0x11
    }));

    WorldPacket nameTail;
    CHECK(MopChannelPackets::BeginNotify(
        nameTail, CHAT_PLAYER_NOT_FOUND_NOTICE, "C"));
    CHECK(MopChannelPackets::WriteNameIdentity(nameTail, "Target", 0x55667788));
    CHECK(Equal(nameTail, {
        0x09, 0x43, 0x00,
        0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00,
        0x88, 0x77, 0x66, 0x55
    }));

    WorldPacket mode;
    CHECK(MopChannelPackets::BeginNotify(mode, CHAT_MODE_CHANGE_NOTICE, "C"));
    MopChannelPackets::WriteGuidIdentity(
        mode, UI64LIT(0x0102030405060708), 0x11223344);
    mode << uint8(0x02) << uint8(0x0A);
    CHECK(Equal(mode, {
        0x0C, 0x43, 0x00,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x44, 0x33, 0x22, 0x11, 0x02, 0x0A
    }));

    WorldPacket youJoined;
    CHECK(MopChannelPackets::BeginNotify(
        youJoined, CHAT_YOU_JOINED_NOTICE, "C"));
    youJoined << uint8(0x2C) << uint32(0x11223344) << uint32(0x55667788);
    CHECK(Equal(youJoined, {
        0x02, 0x43, 0x00, 0x2C,
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55
    }));

    WorldPacket youLeft;
    CHECK(MopChannelPackets::BeginNotify(
        youLeft, CHAT_YOU_LEFT_NOTICE, "C"));
    youLeft << uint32(0x11223344) << uint8(0x01);
    CHECK(Equal(youLeft, {
        0x03, 0x43, 0x00,
        0x44, 0x33, 0x22, 0x11, 0x01
    }));

    WorldPacket twoIdentities;
    CHECK(MopChannelPackets::BeginNotify(
        twoIdentities, CHAT_PLAYER_KICKED_NOTICE, "C"));
    MopChannelPackets::WriteGuidIdentity(
        twoIdentities, UI64LIT(0x0102030405060708), 0x11223344);
    MopChannelPackets::WriteGuidIdentity(
        twoIdentities, UI64LIT(0x1112131415161718), 0x55667788);
    CHECK(twoIdentities.size() == 3 + 2 * (8 + 4));
}

static void test_name_bounds()
{
    WorldPacket packet;
    CHECK(MopChannelPackets::BeginNotify(
        packet, CHAT_WRONG_PASSWORD_NOTICE, std::string(127, 'c')));
    CHECK(!MopChannelPackets::BeginNotify(
        packet, CHAT_WRONG_PASSWORD_NOTICE, std::string(128, 'c')));

    packet.clear();
    CHECK(MopChannelPackets::WriteNameIdentity(packet, std::string(512, 'n'), 1));
    packet.clear();
    CHECK(!MopChannelPackets::WriteNameIdentity(packet, std::string(513, 'n'), 1));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_full_channel_list();
    test_notify_tail_classes();
    test_name_bounds();

    if (g_fail)
        std::fprintf(stderr, "%d failure(s)\n", g_fail);
    return g_fail ? 1 : 0;
}
