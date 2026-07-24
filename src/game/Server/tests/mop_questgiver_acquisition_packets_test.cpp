/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for directly verified 5.4.8 quest acquisition packets.
 */

#include "GossipDef.h"
#include "Opcodes.h"
#include "WorldPacket.h"

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

static uint8_t HexNibble(char value)
{
    return value >= 'a' ? uint8_t(value - 'a' + 10) :
        uint8_t(value - '0');
}

static std::vector<uint8_t> BytesFromHex(char const* hex)
{
    std::vector<uint8_t> bytes;
    while (*hex != '\0')
    {
        bytes.push_back(uint8_t((HexNibble(hex[0]) << 4) |
            HexNibble(hex[1])));
        hex += 2;
    }
    return bytes;
}

static void CheckBytes(WorldPacket const& packet,
    std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        std::fprintf(stderr, "SIZE actual=%zu expected=%zu\n",
            packet.size(), expected.size());
        ++g_fail;
    }
    for (size_t index = 0; index < expected.size() &&
        index < packet.size(); ++index)
    {
        if (packet[index] != expected[index])
        {
            std::fprintf(stderr,
                "BYTE offset=%zu actual=%02X expected=%02X\n",
                index, packet[index], expected[index]);
            ++g_fail;
        }
    }
}

static void TestQuestgiverHello()
{
    uint64 guid = 0;
    WorldPacket dense = MakePacket(CMSG_QUESTGIVER_HELLO,
        { 0xFF, 0x04, 0x03, 0x09, 0x05, 0x06, 0x00, 0x07, 0x02 });
    CHECK(MopQuestGiverPackets::ParseHello(dense, guid));
    CHECK(guid == UINT64_C(0x0807060504030201));

    WorldPacket sparse = MakePacket(CMSG_QUESTGIVER_HELLO,
        { 0x01, 0x03 });
    CHECK(MopQuestGiverPackets::ParseHello(sparse, guid));
    CHECK(guid == UINT64_C(0x0000000000000002));

    WorldPacket truncated = MakePacket(CMSG_QUESTGIVER_HELLO,
        { 0xFF, 0x04 });
    CHECK(!MopQuestGiverPackets::ParseHello(truncated, guid));
    CHECK(truncated.rpos() == truncated.size());

    WorldPacket trailing = MakePacket(CMSG_QUESTGIVER_HELLO,
        { 0x00, 0x00 });
    CHECK(!MopQuestGiverPackets::ParseHello(trailing, guid));
    CHECK(trailing.rpos() == trailing.size());

    WorldPacket noncanonical = MakePacket(CMSG_QUESTGIVER_HELLO,
        { 0x01, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseHello(noncanonical, guid));
    CHECK(noncanonical.rpos() == noncanonical.size());
}

static void TestQuestgiverQueryQuest()
{
    MopQuestGiverPackets::QueryQuestRequest request;
    WorldPacket dense = MakePacket(CMSG_QUESTGIVER_QUERY_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xFF, 0x80, 0x02, 0x00,
          0x04, 0x09, 0x07, 0x03, 0x05, 0x06 });
    CHECK(MopQuestGiverPackets::ParseQueryQuest(dense, request));
    CHECK(request.questId == 0x12345678u);
    CHECK(request.questGiverGuid == UINT64_C(0x0807060504030201));

    WorldPacket sparse = MakePacket(CMSG_QUESTGIVER_QUERY_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x10, 0x80, 0x03 });
    CHECK(MopQuestGiverPackets::ParseQueryQuest(sparse, request));
    CHECK(request.questGiverGuid == UINT64_C(0x0000000000000002));

    WorldPacket truncated = MakePacket(CMSG_QUESTGIVER_QUERY_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xFF, 0x80, 0x02 });
    CHECK(!MopQuestGiverPackets::ParseQueryQuest(truncated, request));
    CHECK(truncated.rpos() == truncated.size());

    WorldPacket trailing = MakePacket(CMSG_QUESTGIVER_QUERY_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x80, 0x00 });
    CHECK(!MopQuestGiverPackets::ParseQueryQuest(trailing, request));
    CHECK(trailing.rpos() == trailing.size());

    WorldPacket missingFlag = MakePacket(CMSG_QUESTGIVER_QUERY_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x00 });
    CHECK(!MopQuestGiverPackets::ParseQueryQuest(missingFlag, request));
    CHECK(missingFlag.rpos() == missingFlag.size());

    WorldPacket nonzeroPadding = MakePacket(CMSG_QUESTGIVER_QUERY_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x81 });
    CHECK(!MopQuestGiverPackets::ParseQueryQuest(nonzeroPadding, request));
    CHECK(nonzeroPadding.rpos() == nonzeroPadding.size());

    WorldPacket noncanonical = MakePacket(CMSG_QUESTGIVER_QUERY_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x10, 0x80, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseQueryQuest(noncanonical, request));
    CHECK(noncanonical.rpos() == noncanonical.size());
}

static void TestQuestgiverAcceptQuest()
{
    MopQuestGiverPackets::AcceptQuestRequest request;
    WorldPacket dense = MakePacket(CMSG_QUESTGIVER_ACCEPT_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xFF, 0x80, 0x67, 0x54,
          0x10, 0x23, 0x76, 0x32, 0x45, 0x89 });
    CHECK(MopQuestGiverPackets::ParseAcceptQuest(dense, request));
    CHECK(request.questId == 0x12345678u);
    CHECK(request.questGiverGuid == UINT64_C(0x8877665544332211));
    CHECK(request.questDetailsAcceptFlag);

    WorldPacket denseFlagClear = MakePacket(CMSG_QUESTGIVER_ACCEPT_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xDF, 0x80, 0x67, 0x54,
          0x10, 0x23, 0x76, 0x32, 0x45, 0x89 });
    CHECK(MopQuestGiverPackets::ParseAcceptQuest(denseFlagClear, request));
    CHECK(!request.questDetailsAcceptFlag);

    WorldPacket sparse = MakePacket(CMSG_QUESTGIVER_ACCEPT_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x68, 0x00, 0x10, 0x89 });
    CHECK(MopQuestGiverPackets::ParseAcceptQuest(sparse, request));
    CHECK(request.questGiverGuid == UINT64_C(0x8800000000000011));
    CHECK(request.questDetailsAcceptFlag);

    WorldPacket truncated = MakePacket(CMSG_QUESTGIVER_ACCEPT_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xFF, 0x80, 0x67 });
    CHECK(!MopQuestGiverPackets::ParseAcceptQuest(truncated, request));
    CHECK(truncated.rpos() == truncated.size());

    WorldPacket trailing = MakePacket(CMSG_QUESTGIVER_ACCEPT_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x00, 0x00 });
    CHECK(!MopQuestGiverPackets::ParseAcceptQuest(trailing, request));
    CHECK(trailing.rpos() == trailing.size());

    WorldPacket nonzeroPadding = MakePacket(CMSG_QUESTGIVER_ACCEPT_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseAcceptQuest(nonzeroPadding, request));
    CHECK(nonzeroPadding.rpos() == nonzeroPadding.size());

    WorldPacket noncanonical = MakePacket(CMSG_QUESTGIVER_ACCEPT_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x40, 0x00, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseAcceptQuest(noncanonical, request));
    CHECK(noncanonical.rpos() == noncanonical.size());
}

static void TestQuestgiverQuestList()
{
    MopQuestGiverPackets::QuestList list;
    list.questGiverGuid = ObjectGuid(UINT64_C(0x0102030405060708));
    list.greeting = "Hello";

    MopQuestGiverPackets::QuestListEntry entry;
    entry.questId = 12345;
    entry.level = 1;
    entry.title = "Test Quest";
    list.quests.push_back(entry);

    WorldPacket packet;
    CHECK(MopQuestGiverPackets::BuildQuestList(packet, list));
    CHECK(packet.GetOpcode() == SMSG_QUESTGIVER_QUEST_LIST);
    CheckBytes(packet, BytesFromHex(
        "0000000000000000805c0000815f060903000000000039300000"
        "5465737420517565737400000000000000000100000002040748"
        "656c6c6f05"));
}

static void TestQuestgiverQuestDetails()
{
    MopQuestGiverPackets::QuestDetails details;
    details.questGiverGuid =
        ObjectGuid(UINT64_C(0x0102030405060708));
    details.questId = 12345;
    details.suggestedPlayers = 1;
    details.questGiverTextWindow = "NPC";
    details.questTitle = "Test Quest";
    details.questGiverTargetName = "Guide";
    details.questDetails = "Do the thing.";
    details.questObjectives = "Complete it.";

    WorldPacket packet;
    CHECK(MopQuestGiverPackets::BuildQuestDetails(packet, details));
    CHECK(packet.GetOpcode() == SMSG_QUESTGIVER_QUEST_DETAILS);

    std::vector<uint8_t> expected(354, 0);
    expected[88] = 0x39;
    expected[89] = 0x30;
    expected[92] = 0x01;
    std::vector<uint8_t> const tail = BytesFromHex(
        "4020180a00000305a80008068000000000001847756964655465"
        "737420517565737403436f6d706c6574652069742e4e5043446f"
        "20746865207468696e672e00040902060705");
    for (size_t index = 0; index < tail.size(); ++index)
    {
        expected[284 + index] = tail[index];
    }
    CheckBytes(packet, expected);
}

static uint32_t ReadU32(WorldPacket const& packet, size_t field)
{
    size_t const offset = field * 4;
    return uint32_t(packet[offset]) |
        (uint32_t(packet[offset + 1]) << 8) |
        (uint32_t(packet[offset + 2]) << 16) |
        (uint32_t(packet[offset + 3]) << 24);
}

static void TestQuestDetailsFixedFieldOrder()
{
    MopQuestGiverPackets::QuestDetails details;
    for (size_t index = 0; index < QUEST_REWARDS_COUNT; ++index)
    {
        details.rewardItemIds[index] = uint32_t(100 + index);
        details.rewardItemCounts[index] = uint32_t(200 + index);
        details.rewardItemDisplayIds[index] = uint32_t(300 + index);
    }
    for (size_t index = 0; index < QUEST_REWARD_CHOICES_COUNT; ++index)
    {
        details.rewardChoiceItemIds[index] = uint32_t(400 + index);
        details.rewardChoiceItemCounts[index] = uint32_t(500 + index);
        details.rewardChoiceItemDisplayIds[index] =
            uint32_t(600 + index);
    }
    for (size_t index = 0; index < QUEST_REWARD_CURRENCY_COUNT; ++index)
    {
        details.rewardCurrencyIds[index] = uint32_t(700 + index);
        details.rewardCurrencyCounts[index] = uint32_t(800 + index);
    }
    for (size_t index = 0; index < QUEST_REPUTATIONS_COUNT; ++index)
    {
        details.rewardFactionIds[index] = uint32_t(900 + index);
        details.rewardFactionValueIds[index] = uint32_t(1000 + index);
        details.rewardFactionValueOverrides[index] =
            uint32_t(1100 + index);
    }
    details.rewardChoiceItemCount = 1200;
    details.questGiverPortrait = 1201;
    details.questId = 1202;
    details.suggestedPlayers = 1203;
    details.bonusTalents = 1204;
    details.rewardSkillId = 1205;
    details.rewardXp = 1206;
    details.rewardReputationMask = 1207;
    details.rewardSpell = 1208;
    details.questFlags = 1209;
    details.characterTitleId = 1210;
    details.rewardOrRequiredMoney = 1211;
    details.questFlags2 = 1212;
    details.rewardSpellCastOrUnknown = 1213;
    details.rewardItemCount = 1214;
    details.rewardSkillPoints = 1215;
    details.rewardPackageItemId = 1216;
    details.questTurnInPortrait = 1217;

    WorldPacket packet;
    CHECK(MopQuestGiverPackets::BuildQuestDetails(packet, details));
    std::vector<uint32_t> const expected =
    {
        203, 604, 402,
        800, 700, 801, 701, 802, 702, 803, 703,
        1200, 502, 201, 605, 200, 303, 400, 503, 1201,
        603, 100, 1202, 1203, 600, 504, 505, 1204, 501, 602,
        1000, 1100, 900, 1001, 1101, 901, 1002, 1102, 902,
        1003, 1103, 903, 1004, 1104, 904,
        103, 1205, 1206, 1207, 302, 101, 401, 405, 1208,
        1209, 1210, 102, 1211, 202, 1212, 1213, 403, 1214,
        1215, 300, 404, 1216, 500, 301, 601, 1217
    };
    CHECK(expected.size() == 71);
    for (size_t index = 0; index < expected.size(); ++index)
    {
        CHECK(ReadU32(packet, index) == expected[index]);
    }
}

static void TestQuestDetailsVariableRecords()
{
    MopQuestGiverPackets::QuestDetails details;
    MopQuestGiverPackets::QuestObjective objective;
    objective.type = 7;
    objective.id = 0x11223344;
    objective.amount = -5;
    objective.objectId = 0x55667788;
    details.objectives.push_back(objective);

    MopQuestGiverPackets::QuestEmote emote;
    emote.delay = 0x12345678;
    emote.emote = 0x90ABCDEF;
    details.emotes.push_back(emote);
    details.learnSpells.push_back(0x0BADF00D);

    WorldPacket packet;
    CHECK(MopQuestGiverPackets::BuildQuestDetails(packet, details));
    CHECK(packet.size() == 328);

    packet.rpos(284);
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(packet.ReadBits(8) == 0);
    CHECK(!packet.ReadBit());
    CHECK(packet.ReadBits(10) == 0);
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(packet.ReadBits(9) == 0);
    CHECK(packet.ReadBits(21) == 1);
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(packet.ReadBits(8) == 0);
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(packet.ReadBits(10) == 0);
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(!packet.ReadBit());
    CHECK(packet.ReadBits(12) == 0);
    CHECK(packet.ReadBits(22) == 1);
    CHECK(packet.ReadBits(20) == 1);
    CHECK(packet.ReadBits(12) == 0);
    CHECK(packet.rpos() == 303);

    uint8_t parsedType = 0;
    uint32_t parsedId = 0;
    int32_t parsedAmount = 0;
    uint32_t parsedObjectId = 0;
    uint32_t parsedDelay = 0;
    uint32_t parsedEmote = 0;
    uint32_t parsedSpell = 0;
    packet >> parsedType >> parsedId >> parsedAmount >> parsedObjectId >>
        parsedDelay >> parsedEmote >> parsedSpell;
    CHECK(parsedType == objective.type);
    CHECK(parsedId == objective.id);
    CHECK(parsedAmount == objective.amount);
    CHECK(parsedObjectId == objective.objectId);
    CHECK(parsedDelay == emote.delay);
    CHECK(parsedEmote == emote.emote);
    CHECK(parsedSpell == details.learnSpells[0]);
    CHECK(packet.rpos() == packet.size());
}

static void TestOpcodeValues()
{
    CHECK(uint32_t(CMSG_QUESTGIVER_HELLO) == 0x02DBu);
    CHECK(uint32_t(SMSG_QUESTGIVER_QUEST_LIST) == 0x02D4u);
    CHECK(uint32_t(CMSG_QUESTGIVER_QUERY_QUEST) == 0x12F0u);
    CHECK(uint32_t(SMSG_QUESTGIVER_QUEST_DETAILS) == 0x134Cu);
    CHECK(uint32_t(CMSG_QUESTGIVER_ACCEPT_QUEST) == 0x06D1u);
}

int main(int /*argc*/, char** /*argv*/)
{
    TestQuestgiverHello();
    TestQuestgiverQueryQuest();
    TestQuestgiverAcceptQuest();
    TestQuestgiverQuestList();
    TestQuestgiverQuestDetails();
    TestQuestDetailsFixedFieldOrder();
    TestQuestDetailsVariableRecords();
    TestOpcodeValues();

    if (g_fail != 0)
    {
        std::fprintf(stderr, "%d test(s) failed\n", g_fail);
        return 1;
    }

    std::puts("mop_questgiver_acquisition_packets_test: PASS");
    return 0;
}
