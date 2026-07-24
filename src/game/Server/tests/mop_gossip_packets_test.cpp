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

static WorldPacket MakePacket(OpcodesList opcode,
    std::vector<uint8> const& body)
{
    WorldPacket packet(opcode, body.size());
    if (!body.empty())
        packet.append(body.data(), body.size());
    return packet;
}

static void TestSelectOptionRequest()
{
    std::vector<uint8> const body = {
        0xD4, 0xC3, 0xB2, 0xA1, 0x78, 0x56, 0x34, 0x12,
        0xFE, 0x05, 0x09, 0x05, 0x04, 0x06, 0x00, 0x07,
        0x58, 0x59, 0x02, 0x03
    };
    WorldPacket packet = MakePacket(CMSG_GOSSIP_SELECT_OPTION, body);
    MopGossipPackets::SelectOptionRequest request;
    CHECK(MopGossipPackets::ParseSelectOption(packet, request));
    CHECK(request.optionId == 0xA1B2C3D4);
    CHECK(request.menuId == 0x12345678);
    CHECK(request.sourceGuid == ObjectGuid(UINT64_C(0x0807060504030201)));
    CHECK(request.code == "XY");
    CHECK(packet.rpos() == packet.size());

    std::vector<uint8> const sparseBody = {
        0x04, 0x03, 0x02, 0x01, 0xDD, 0xCC, 0xBB, 0xAA,
        0x40, 0x01, 0x10, 0x32
    };
    WorldPacket sparse = MakePacket(CMSG_GOSSIP_SELECT_OPTION, sparseBody);
    CHECK(MopGossipPackets::ParseSelectOption(sparse, request));
    CHECK(request.optionId == 0x01020304);
    CHECK(request.menuId == 0xAABBCCDD);
    CHECK(request.sourceGuid == ObjectGuid(UINT64_C(0x0000000000330011)));
    CHECK(request.code.empty());

    std::vector<uint8> const zeroBody = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    WorldPacket zero = MakePacket(CMSG_GOSSIP_SELECT_OPTION, zeroBody);
    CHECK(MopGossipPackets::ParseSelectOption(zero, request));
    CHECK(request.sourceGuid.IsEmpty());
    CHECK(request.code.empty());

    for (size_t length = 0; length < body.size(); ++length)
    {
        WorldPacket truncated(CMSG_GOSSIP_SELECT_OPTION, length);
        if (length != 0)
            truncated.append(body.data(), length);
        CHECK(!MopGossipPackets::ParseSelectOption(truncated, request));
        CHECK(truncated.rpos() == truncated.size());
    }

    std::vector<uint8> trailingBody = body;
    trailingBody.push_back(0);
    WorldPacket trailing = MakePacket(CMSG_GOSSIP_SELECT_OPTION, trailingBody);
    CHECK(!MopGossipPackets::ParseSelectOption(trailing, request));
    CHECK(trailing.rpos() == trailing.size());

    std::vector<uint8> badLengthBody = body;
    badLengthBody[9] = 0x07; // declares three code bytes, but carries two
    WorldPacket badLength = MakePacket(CMSG_GOSSIP_SELECT_OPTION, badLengthBody);
    CHECK(!MopGossipPackets::ParseSelectOption(badLength, request));
    CHECK(badLength.rpos() == badLength.size());

    std::vector<uint8> noncanonicalBody = body;
    noncanonicalBody[10] = 1; // a present byte must not decode to zero
    WorldPacket noncanonical =
        MakePacket(CMSG_GOSSIP_SELECT_OPTION, noncanonicalBody);
    CHECK(!MopGossipPackets::ParseSelectOption(noncanonical, request));
    CHECK(noncanonical.rpos() == noncanonical.size());

    std::vector<uint8> embeddedNullBody = body;
    embeddedNullBody[16] = 0;
    WorldPacket embeddedNull =
        MakePacket(CMSG_GOSSIP_SELECT_OPTION, embeddedNullBody);
    CHECK(!MopGossipPackets::ParseSelectOption(embeddedNull, request));
    CHECK(embeddedNull.rpos() == embeddedNull.size());
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

static void TestPointOfInterest()
{
    MopGossipPackets::PointOfInterest point;
    point.flags = 0x11223344;
    point.x = 1.5f;
    point.y = -2.25f;
    point.icon = 0x55667788;
    point.data = 0xAABBCCDD;
    point.name = "P";

    WorldPacket packet;
    CHECK(MopGossipPackets::BuildPointOfInterest(packet, point));
    CHECK(packet.GetOpcode() == SMSG_GOSSIP_POI);
    CHECK(BytesEqual(packet, {
        0x44, 0x33, 0x22, 0x11,
        0x00, 0x00, 0xC0, 0x3F,
        0x00, 0x00, 0x10, 0xC0,
        0x88, 0x77, 0x66, 0x55,
        0xDD, 0xCC, 0xBB, 0xAA,
        0x50, 0x00
    }));

    point.name.assign(63, 'N');
    CHECK(MopGossipPackets::BuildPointOfInterest(packet, point));
    CHECK(packet.size() == 84);
    CHECK(packet.contents()[83] == 0);

    point.name.push_back('N');
    CHECK(!MopGossipPackets::BuildPointOfInterest(packet, point));
}

int main(int /*argc*/, char** /*argv*/)
{
    TestHelloRequest();
    TestSelectOptionRequest();
    TestGossipMessage();
    TestPointOfInterest();
    return g_fail ? 1 : 0;
}
