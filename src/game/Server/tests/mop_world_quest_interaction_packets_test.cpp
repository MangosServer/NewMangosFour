/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for directly verified 5.4.8 world and quest interaction
 * packets.
 */

#include "Player.h"
#include "GossipDef.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket MakePacket(OpcodesList opcode,
    std::initializer_list<uint8_t> bytes)
{
    WorldPacket packet(opcode, bytes.size());
    for (uint8_t byte : bytes)
    {
        packet << byte;
    }
    return packet;
}

static void CheckBytes(WorldPacket const& packet,
    std::initializer_list<uint8_t> expected)
{
    CHECK(packet.size() == expected.size());
    size_t index = 0;
    for (uint8_t byte : expected)
    {
        if (index < packet.size())
        {
            CHECK(packet[index] == byte);
        }
        ++index;
    }
}

static void TestAreaTriggerRequest()
{
    MopAreaTriggerPackets::Request request;

    WorldPacket entered = MakePacket(CMSG_AREATRIGGER,
        { 0x78, 0x56, 0x34, 0x12, 0xC0 });
    CHECK(MopAreaTriggerPackets::ParseRequest(entered, request));
    CHECK(request.triggerId == 0x12345678u);
    CHECK(request.entered);

    WorldPacket left = MakePacket(CMSG_AREATRIGGER,
        { 0xEF, 0xCD, 0xAB, 0x90, 0x80 });
    CHECK(MopAreaTriggerPackets::ParseRequest(left, request));
    CHECK(request.triggerId == 0x90ABCDEFu);
    CHECK(!request.entered);

    WorldPacket truncated = MakePacket(CMSG_AREATRIGGER,
        { 0x78, 0x56, 0x34, 0x12 });
    CHECK(!MopAreaTriggerPackets::ParseRequest(truncated, request));
    CHECK(truncated.rpos() == truncated.size());

    WorldPacket trailing = MakePacket(CMSG_AREATRIGGER,
        { 0x78, 0x56, 0x34, 0x12, 0xC0, 0x00 });
    CHECK(!MopAreaTriggerPackets::ParseRequest(trailing, request));
    CHECK(trailing.rpos() == trailing.size());

    WorldPacket missingFlag = MakePacket(CMSG_AREATRIGGER,
        { 0x78, 0x56, 0x34, 0x12, 0x40 });
    CHECK(!MopAreaTriggerPackets::ParseRequest(missingFlag, request));
    CHECK(missingFlag.rpos() == missingFlag.size());

    WorldPacket nonzeroPadding = MakePacket(CMSG_AREATRIGGER,
        { 0x78, 0x56, 0x34, 0x12, 0xC1 });
    CHECK(!MopAreaTriggerPackets::ParseRequest(nonzeroPadding, request));
    CHECK(nonzeroPadding.rpos() == nonzeroPadding.size());
}

static void TestAreaTriggerNoCorpse()
{
    WorldPacket packet;
    MopAreaTriggerPackets::BuildNoCorpse(packet);
    CHECK(packet.GetOpcode() == SMSG_AREA_TRIGGER_NO_CORPSE);
    CHECK(packet.empty());
}

static void TestQuestgiverStatusMultipleRequest()
{
    WorldPacket empty(CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY, 0);
    CHECK(MopQuestStatusPackets::ParseMultipleStatusQuery(empty));

    WorldPacket trailing = MakePacket(CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY,
        { 0x00 });
    CHECK(!MopQuestStatusPackets::ParseMultipleStatusQuery(trailing));
    CHECK(trailing.rpos() == trailing.size());
}

static void TestQuestgiverStatusMultipleResponse()
{
    WorldPacket empty;
    std::vector<MopQuestStatusPackets::StatusEntry> entries;
    CHECK(MopQuestStatusPackets::BuildMultipleStatus(empty, entries));
    CHECK(empty.GetOpcode() == SMSG_QUESTGIVER_STATUS_MULTIPLE);
    CheckBytes(empty, { 0x00, 0x00, 0x00 });

    MopQuestStatusPackets::StatusEntry zero;
    entries.push_back(zero);
    WorldPacket zeroPacket;
    CHECK(MopQuestStatusPackets::BuildMultipleStatus(zeroPacket, entries));
    CheckBytes(zeroPacket,
        { 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00 });

    entries[0].guid = ObjectGuid(UINT64_C(0x8877665544332211));
    entries[0].status = 0x00000006u;
    WorldPacket dense;
    CHECK(MopQuestStatusPackets::BuildMultipleStatus(dense, entries));
    CheckBytes(dense,
        { 0x00, 0x00, 0x0F, 0xF8, 0x76, 0x32, 0x89, 0x67,
          0x54, 0x06, 0x00, 0x00, 0x00, 0x23, 0x45, 0x10 });

    entries[0].guid = ObjectGuid(UINT64_C(0x8800000000000011));
    entries[0].status = 0x12345678u;
    WorldPacket sparse;
    CHECK(MopQuestStatusPackets::BuildMultipleStatus(sparse, entries));
    CheckBytes(sparse,
        { 0x00, 0x00, 0x0A, 0x20, 0x89, 0x78, 0x56, 0x34, 0x12, 0x10 });
}

static void TestNpcTextRequest()
{
    MopNpcTextPackets::Request request;
    WorldPacket dense = MakePacket(CMSG_NPC_TEXT_QUERY,
        { 0x78, 0x56, 0x34, 0x12, 0xFF, 0x54, 0x10, 0x32,
          0x67, 0x23, 0x89, 0x45, 0x76 });
    CHECK(MopNpcTextPackets::ParseRequest(dense, request));
    CHECK(request.textId == 0x12345678u);
    CHECK(request.sourceGuid == ObjectGuid(UINT64_C(0x8877665544332211)));

    WorldPacket zeroGuid = MakePacket(CMSG_NPC_TEXT_QUERY,
        { 0xEF, 0xCD, 0xAB, 0x90, 0x00 });
    CHECK(MopNpcTextPackets::ParseRequest(zeroGuid, request));
    CHECK(request.textId == 0x90ABCDEFu);
    CHECK(request.sourceGuid.IsEmpty());

    WorldPacket truncated = MakePacket(CMSG_NPC_TEXT_QUERY,
        { 0x78, 0x56, 0x34, 0x12, 0xFF, 0x54 });
    CHECK(!MopNpcTextPackets::ParseRequest(truncated, request));
    CHECK(truncated.rpos() == truncated.size());

    WorldPacket trailing = MakePacket(CMSG_NPC_TEXT_QUERY,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x00 });
    CHECK(!MopNpcTextPackets::ParseRequest(trailing, request));
    CHECK(trailing.rpos() == trailing.size());
}

static void TestNpcTextResponse()
{
    MopNpcTextPackets::Response response;
    response.textId = 0x12345678u;

    WorldPacket missing;
    MopNpcTextPackets::BuildResponse(missing, response);
    CHECK(missing.GetOpcode() == SMSG_NPC_TEXT_UPDATE);
    CheckBytes(missing,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00 });

    response.found = true;
    response.probabilities[0] = 1.0f;
    response.broadcastTextIds[0] = 0x01020304u;
    WorldPacket found;
    MopNpcTextPackets::BuildResponse(found, response);
    CHECK(found.size() == 73);
    CHECK(found[0] == 0x78 && found[1] == 0x56 &&
        found[2] == 0x34 && found[3] == 0x12);
    CHECK(found[4] == 0x40 && found[5] == 0x00 &&
        found[6] == 0x00 && found[7] == 0x00);
    CHECK(found[8] == 0x00 && found[9] == 0x00 &&
        found[10] == 0x80 && found[11] == 0x3F);
    for (size_t index = 12; index < 40; ++index)
    {
        CHECK(found[index] == 0x00);
    }
    CHECK(found[40] == 0x04 && found[41] == 0x03 &&
        found[42] == 0x02 && found[43] == 0x01);
    for (size_t index = 44; index < 72; ++index)
    {
        CHECK(found[index] == 0x00);
    }
    CHECK(found[72] == 0x80);
}

static void TestOpcodeValues()
{
    CHECK(uint32_t(CMSG_AREATRIGGER) == 0x1C44u);
    CHECK(uint32_t(SMSG_AREA_TRIGGER_NO_CORPSE) == 0x089Eu);
    CHECK(uint32_t(CMSG_NPC_TEXT_QUERY) == 0x0287u);
    CHECK(uint32_t(SMSG_NPC_TEXT_UPDATE) == 0x140Au);
    CHECK(uint32_t(CMSG_QUESTGIVER_STATUS_MULTIPLE_QUERY) == 0x02F1u);
    CHECK(uint32_t(SMSG_QUESTGIVER_STATUS_MULTIPLE) == 0x06CEu);
}

int main(int /*argc*/, char** /*argv*/)
{
    TestAreaTriggerRequest();
    TestAreaTriggerNoCorpse();
    TestQuestgiverStatusMultipleRequest();
    TestQuestgiverStatusMultipleResponse();
    TestNpcTextRequest();
    TestNpcTextResponse();
    TestOpcodeValues();

    if (g_fail != 0)
    {
        std::fprintf(stderr, "%d test(s) failed\n", g_fail);
        return 1;
    }

    std::puts("mop_world_quest_interaction_packets_test: PASS");
    return 0;
}
