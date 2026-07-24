/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for the directly verified 5.4.8 quest turn-in packets.
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
        value >= 'A' ? uint8_t(value - 'A' + 10) :
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
    CHECK(packet.size() == expected.size());
    for (size_t index = 0; index < packet.size() &&
        index < expected.size(); ++index)
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

static uint32_t ReadU32(WorldPacket const& packet, size_t field)
{
    size_t const offset = field * 4;
    return uint32_t(packet[offset]) |
        (uint32_t(packet[offset + 1]) << 8) |
        (uint32_t(packet[offset + 2]) << 16) |
        (uint32_t(packet[offset + 3]) << 24);
}

static void TestCompleteQuestRequest()
{
    MopQuestGiverPackets::CompleteQuestRequest request;
    WorldPacket dense = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xFE, 0x80, 0x00, 0x02,
          0x03, 0x04, 0x05, 0x06, 0x09, 0x07 });
    CHECK(MopQuestGiverPackets::ParseCompleteQuest(dense, request));
    CHECK(request.questId == 0x12345678u);
    CHECK(request.questGiverGuid == UINT64_C(0x0807060504030201));
    CHECK(!request.completionContext);

    WorldPacket withContext = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xFF, 0x80, 0x00, 0x02,
          0x03, 0x04, 0x05, 0x06, 0x09, 0x07 });
    CHECK(MopQuestGiverPackets::ParseCompleteQuest(withContext, request));
    CHECK(request.completionContext);

    WorldPacket sparse = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x00 });
    CHECK(MopQuestGiverPackets::ParseCompleteQuest(sparse, request));
    CHECK(request.questGiverGuid == 0);

    WorldPacket truncated = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xFE, 0x80, 0x00 });
    CHECK(!MopQuestGiverPackets::ParseCompleteQuest(truncated, request));
    CHECK(truncated.rpos() == truncated.size());

    WorldPacket padding = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseCompleteQuest(padding, request));

    WorldPacket noncanonical = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x80, 0x00, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseCompleteQuest(noncanonical, request));
}

static void TestRequestRewardRequest()
{
    MopQuestGiverPackets::RequestRewardRequest request;
    WorldPacket dense = MakePacket(CMSG_QUESTGIVER_REQUEST_REWARD,
        { 0x78, 0x56, 0x34, 0x12, 0xFF, 0x05, 0x00, 0x09,
          0x06, 0x02, 0x03, 0x07, 0x04 });
    CHECK(MopQuestGiverPackets::ParseRequestReward(dense, request));
    CHECK(request.questId == 0x12345678u);
    CHECK(request.questGiverGuid == UINT64_C(0x0807060504030201));

    WorldPacket sparse = MakePacket(CMSG_QUESTGIVER_REQUEST_REWARD,
        { 0x78, 0x56, 0x34, 0x12, 0x00 });
    CHECK(MopQuestGiverPackets::ParseRequestReward(sparse, request));
    CHECK(request.questGiverGuid == 0);

    WorldPacket trailing = MakePacket(CMSG_QUESTGIVER_REQUEST_REWARD,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x00 });
    CHECK(!MopQuestGiverPackets::ParseRequestReward(trailing, request));

    WorldPacket noncanonical = MakePacket(CMSG_QUESTGIVER_REQUEST_REWARD,
        { 0x78, 0x56, 0x34, 0x12, 0x02, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseRequestReward(noncanonical, request));
}

static void TestChooseRewardRequest()
{
    MopQuestGiverPackets::ChooseRewardRequest request;
    WorldPacket dense = MakePacket(CMSG_QUESTGIVER_CHOOSE_REWARD,
        { 0xD4, 0xC3, 0xB2, 0xA1, 0x78, 0x56, 0x34, 0x12,
          0xFF, 0x03, 0x02, 0x07, 0x09, 0x00, 0x05, 0x06,
          0x04 });
    CHECK(MopQuestGiverPackets::ParseChooseReward(dense, request));
    CHECK(request.rewardItemId == 0xA1B2C3D4u);
    CHECK(request.questId == 0x12345678u);
    CHECK(request.questGiverGuid == UINT64_C(0x0807060504030201));

    WorldPacket sparse = MakePacket(CMSG_QUESTGIVER_CHOOSE_REWARD,
        { 0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12,
          0x00 });
    CHECK(MopQuestGiverPackets::ParseChooseReward(sparse, request));
    CHECK(request.rewardItemId == 0);
    CHECK(request.questGiverGuid == 0);

    WorldPacket truncated = MakePacket(CMSG_QUESTGIVER_CHOOSE_REWARD,
        { 0xD4, 0xC3, 0xB2, 0xA1, 0x78, 0x56, 0x34, 0x12,
          0xFF, 0x03 });
    CHECK(!MopQuestGiverPackets::ParseChooseReward(truncated, request));

    WorldPacket noncanonical = MakePacket(CMSG_QUESTGIVER_CHOOSE_REWARD,
        { 0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12,
          0x04, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseChooseReward(noncanonical, request));
}

static void TestResolveRewardChoiceByItemId()
{
    std::array<uint32, QUEST_REWARD_CHOICES_COUNT> choices =
        { 100, 200, 300, 0, 0, 0 };
    uint32 choice = 99;
    CHECK(MopQuestGiverPackets::ResolveRewardChoice(
        choices, 3, 200, choice));
    CHECK(choice == 1);
    CHECK(!MopQuestGiverPackets::ResolveRewardChoice(
        choices, 3, 2, choice));
    CHECK(!MopQuestGiverPackets::ResolveRewardChoice(
        choices, 3, 0, choice));

    choices = { 100, 0, 300, 0, 0, 0 };
    CHECK(MopQuestGiverPackets::ResolveRewardChoice(
        choices, 2, 300, choice));
    CHECK(choice == 2);

    choices.fill(0);
    CHECK(MopQuestGiverPackets::ResolveRewardChoice(
        choices, 0, 0, choice));
    CHECK(choice == 0);
    CHECK(!MopQuestGiverPackets::ResolveRewardChoice(
        choices, 0, 100, choice));
}

static void TestQuestgiverRequestItems()
{
    MopQuestGiverPackets::QuestRequestItems request;
    request.questGiverGuid =
        ObjectGuid(UINT64_C(0x0807060504030201));
    request.suggestedPlayers = 0x01020304;
    request.questFlags = 0x11121314;
    request.emoteDelay = 0x21222324;
    request.statusFlags = 0x5F;
    request.requiredMoney = 0x31323334;
    request.emote = 0x41424344;
    request.questId = 0x51525354;
    request.requiredCurrencyIds[0] = 0x61626364;
    request.requiredCurrencyCounts[0] = 0x71727374;
    request.requiredItemDisplayIds[0] = 0x81828384;
    request.requiredItemIds[0] = 0x91929394;
    request.requiredItemCounts[0] = 0xA1A2A3A4;
    request.title = "T";
    request.requestText = "R";

    WorldPacket packet;
    CHECK(MopQuestGiverPackets::BuildQuestRequestItems(packet, request));
    CHECK(packet.GetOpcode() == SMSG_QUESTGIVER_REQUEST_ITEMS);
    CheckBytes(packet, BytesFromHex(
        "0403020114131211242322215F0000003433323100000000"
        "00000000444342415453525100000B80400700001E000254"
        "64636261747372718483828194939291A4A3A2A105035204"
        "070906"));
}

static void PopulateOfferReward(
    MopQuestGiverPackets::QuestRewardOffer& offer)
{
    for (size_t index = 0; index < QUEST_REWARDS_COUNT; ++index)
    {
        offer.rewardItemIds[index] = uint32(100 + index);
        offer.rewardItemCounts[index] = uint32(200 + index);
        offer.rewardItemDisplayIds[index] = uint32(300 + index);
    }
    for (size_t index = 0; index < QUEST_REWARD_CHOICES_COUNT; ++index)
    {
        offer.rewardChoiceItemIds[index] = uint32(400 + index);
        offer.rewardChoiceItemCounts[index] = uint32(500 + index);
        offer.rewardChoiceItemDisplayIds[index] = uint32(600 + index);
    }
    for (size_t index = 0; index < QUEST_REWARD_CURRENCY_COUNT; ++index)
    {
        offer.rewardCurrencyIds[index] = uint32(700 + index);
        offer.rewardCurrencyCounts[index] = uint32(800 + index);
    }
    for (size_t index = 0; index < QUEST_REPUTATIONS_COUNT; ++index)
    {
        offer.rewardFactionIds[index] = uint32(900 + index);
        offer.rewardFactionValueIds[index] = uint32(1000 + index);
        offer.rewardFactionValueOverrides[index] = uint32(1100 + index);
    }
    offer.rewardChoiceItemCount = 1200;
    offer.questId = 1201;
    offer.rewardSpellCast = 1202;
    offer.rewardPackageItemId = 1203;
    offer.questTurnInPortrait = 1204;
    offer.rewardReputationMask = 1205;
    offer.bonusTalents = 1206;
    offer.rewardSkillId = 1207;
    offer.questFlags = 1208;
    offer.questFlags2 = 1209;
    offer.rewardXp = 1210;
    offer.characterTitleId = 1211;
    offer.rewardItemCount = 1212;
    offer.suggestedPlayers = 1213;
    offer.questTakerEntry = 1214;
    offer.questGiverPortrait = 1215;
    offer.rewardMoney = 1216;
    offer.rewardSpell = 1217;
    offer.rewardSkillPoints = 1218;
}

static void TestQuestgiverOfferRewardFixedFields()
{
    MopQuestGiverPackets::QuestRewardOffer offer;
    PopulateOfferReward(offer);

    WorldPacket packet;
    CHECK(MopQuestGiverPackets::BuildQuestOfferReward(packet, offer));
    CHECK(packet.GetOpcode() == SMSG_QUESTGIVER_OFFER_REWARD);

    std::vector<uint32_t> const expected =
    {
        202, 1201, 103, 602,
        900, 1000, 1100, 901, 1001, 1101, 902, 1002, 1102,
        903, 1003, 1103, 904, 1004, 1104,
        200, 203, 303, 101, 403, 603, 1200, 1202, 301, 505,
        604, 501, 600, 300, 1203, 1204, 201, 1205, 400,
        503, 504, 401, 1206, 1207,
        700, 800, 701, 801, 702, 802, 703, 803,
        1208, 1209, 1210, 1211, 402, 1212, 1213, 404, 1214,
        102, 500, 605, 1215, 1216, 405, 601, 502, 302, 1217,
        100, 1218
    };
    CHECK(expected.size() == 72);
    for (size_t index = 0; index < expected.size(); ++index)
    {
        CHECK(ReadU32(packet, index) == expected[index]);
    }
}

static void TestQuestgiverOfferRewardVariableFields()
{
    MopQuestGiverPackets::QuestRewardOffer offer;
    offer.questGiverGuid =
        ObjectGuid(UINT64_C(0x0807060504030201));
    offer.questTurnTargetName = "TN";
    offer.questTitle = "Q1";
    offer.offerRewardText = "OR";
    offer.questTurnTextWindow = "TI";
    offer.questGiverTargetName = "GN";
    offer.questGiverTextWindow = "GT";
    offer.autoLaunch = true;
    MopQuestGiverPackets::QuestEmote emote;
    emote.delay = 0x11121314;
    emote.emote = 0x21222324;
    offer.emotes.push_back(emote);

    WorldPacket packet;
    CHECK(MopQuestGiverPackets::BuildQuestOfferReward(packet, offer));
    CHECK(packet.size() == 327);
    for (size_t index = 0; index < 288; ++index)
    {
        CHECK(packet[index] == 0);
    }
    std::vector<uint8_t> const tail = BytesFromHex(
        "0080A00001C050200800BE544E51311413121124232221024F"
        "525449474E070347540009060405");
    for (size_t index = 0; index < tail.size(); ++index)
    {
        if (packet[288 + index] != tail[index])
        {
            std::fprintf(stderr,
                "OFFER TAIL offset=%zu actual=%02X expected=%02X\n",
                index, packet[288 + index], tail[index]);
            ++g_fail;
        }
    }
}

static void TestQuestCompletionNotifications()
{
    MopQuestGiverPackets::QuestRewardSummary summary;
    summary.bonusTalents = 0x01020304;
    summary.money = 0x11121314;
    summary.questId = 0x21222324;
    summary.experience = 0x31323334;

    WorldPacket reward;
    MopQuestGiverPackets::BuildQuestRewardSummary(reward, summary);
    CHECK(reward.GetOpcode() == SMSG_QUESTGIVER_QUEST_COMPLETE);
    CheckBytes(reward, BytesFromHex(
        "80040302011413121124232221000000003433323100000000"));

    WorldPacket update;
    MopQuestGiverPackets::BuildQuestUpdateComplete(
        update, 0xA1B2C3D4);
    CHECK(update.GetOpcode() == SMSG_QUESTUPDATE_COMPLETE);
    CheckBytes(update, BytesFromHex("D4C3B2A1"));
}

static void TestOpcodeValues()
{
    CHECK(uint32_t(CMSG_QUESTGIVER_COMPLETE_QUEST) == 0x0659u);
    CHECK(uint32_t(SMSG_QUESTGIVER_REQUEST_ITEMS) == 0x0277u);
    CHECK(uint32_t(CMSG_QUESTGIVER_REQUEST_REWARD) == 0x0378u);
    CHECK(uint32_t(SMSG_QUESTGIVER_OFFER_REWARD) == 0x074Fu);
    CHECK(uint32_t(CMSG_QUESTGIVER_CHOOSE_REWARD) == 0x07CBu);
    CHECK(uint32_t(SMSG_QUESTGIVER_QUEST_COMPLETE) == 0x0346u);
    CHECK(uint32_t(SMSG_QUESTUPDATE_COMPLETE) == 0x0776u);
}

int main(int /*argc*/, char** /*argv*/)
{
    TestCompleteQuestRequest();
    TestRequestRewardRequest();
    TestChooseRewardRequest();
    TestResolveRewardChoiceByItemId();
    TestQuestgiverRequestItems();
    TestQuestgiverOfferRewardFixedFields();
    TestQuestgiverOfferRewardVariableFields();
    TestQuestCompletionNotifications();
    TestOpcodeValues();

    if (g_fail != 0)
    {
        std::fprintf(stderr, "%d test(s) failed\n", g_fail);
        return 1;
    }

    std::puts("mop_questgiver_turnin_packets_test: PASS");
    return 0;
}
