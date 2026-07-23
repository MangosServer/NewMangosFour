/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "GossipDef.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool BytesEqual(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    if (packet.size() != expected.size())
        return false;

    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;

    return true;
}

static void TestHelloRequest()
{
    WorldPacket packet(CMSG_GOSSIP_HELLO, 6);
    uint8 const body[] = { 0xBA, 0x00, 0x07, 0x05, 0x06, 0x02 };
    packet.append(body, sizeof(body));

    CHECK(MopGossipPackets::ReadHello(packet) == ObjectGuid(UINT64_C(0x0007060004030001)));
}

static void TestGossipMessage()
{
    MopGossipPackets::Message message;
    message.sourceGuid = ObjectGuid(UINT64_C(0x0007060004030001));
    message.menuId = 0x01020304;
    message.titleTextId = 0xA1B2C3D4;

    MopGossipPackets::QuestItem quest;
    quest.questId = 0x99AABBCC;
    quest.icon = 7;
    quest.level = -2;
    quest.flags = 0x11223344;
    quest.flags2 = 0xDDEEFF00;
    quest.repeatable = true;
    quest.title = "Q";
    message.quests.push_back(quest);

    MopGossipPackets::GossipItem gossip;
    gossip.optionId = 0;
    gossip.icon = 7;
    gossip.coded = true;
    gossip.boxMoney = 0x10203040;
    gossip.message = "M";
    gossip.boxMessage = "B";
    message.gossipItems.push_back(gossip);

    WorldPacket packet;
    MopGossipPackets::BuildMessage(packet, message);

    CHECK(packet.GetOpcode() == SMSG_GOSSIP_MESSAGE);
    CHECK(BytesEqual(packet, {
        0x00, 0x00, 0x30, 0x0C, 0x80, 0x00, 0x0E, 0x00, 0x20, 0x03, 0x00,
        0x51, 0x44, 0x33, 0x22, 0x11, 0xFE, 0xFF, 0xFF, 0xFF, 0x07, 0x00,
        0x00, 0x00, 0xCC, 0xBB, 0xAA, 0x99, 0x00, 0xFF, 0xEE, 0xDD, 0x00,
        0x40, 0x30, 0x20, 0x10, 0x42, 0x00, 0x00, 0x00, 0x00, 0x01, 0x4D,
        0x07, 0x07, 0x05, 0x04, 0x03, 0x02, 0x01, 0x02, 0x06, 0x00, 0x00,
        0x00, 0x00, 0xD4, 0xC3, 0xB2, 0xA1
    }));
}

int main(int /*argc*/, char** /*argv*/)
{
    TestHelloRequest();
    TestGossipMessage();
    return g_fail ? 1 : 0;
}
