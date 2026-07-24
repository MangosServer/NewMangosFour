/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "Group.h"
#include "LootMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket Packet(OpcodesList opcode, std::initializer_list<uint8> body)
{
    WorldPacket packet(opcode, body.size());
    for (uint8 value : body)
    {
        packet << value;
    }
    return packet;
}

static void CheckPacket(WorldPacket const& packet, OpcodesList opcode,
    std::initializer_list<uint8> expected)
{
    CHECK(packet.GetOpcode() == opcode);
    CHECK(packet.size() == expected.size());
    size_t index = 0;
    for (uint8 value : expected)
    {
        CHECK(index < packet.size());
        if (index < packet.size())
        {
            CHECK(packet[index] == value);
        }
        ++index;
    }
}

static void TestLootRequest()
{
    WorldPacket dense = Packet(CMSG_LOOT,
        { 0xFF, 0x05, 0x07, 0x00, 0x06, 0x04, 0x03, 0x09, 0x02 });
    uint64 guid = 0;
    CHECK(MopLootPackets::ParseLootRequest(dense, guid));
    CHECK(guid == UINT64_C(0x0807060504030201));
    CHECK(dense.rpos() == dense.size());

    WorldPacket sparse = Packet(CMSG_LOOT, { 0x18, 0x10, 0x89 });
    CHECK(MopLootPackets::ParseLootRequest(sparse, guid));
    CHECK(guid == UINT64_C(0x8800000000000011));

    WorldPacket zero = Packet(CMSG_LOOT, { 0x00 });
    CHECK(MopLootPackets::ParseLootRequest(zero, guid));
    CHECK(guid == 0);
}

static void TestLootRequestRejectsMalformedBodies()
{
    uint64 guid = 0;
    WorldPacket truncated = Packet(CMSG_LOOT, { 0xFF, 0x05 });
    CHECK(!MopLootPackets::ParseLootRequest(truncated, guid));

    WorldPacket trailing = Packet(CMSG_LOOT,
        { 0x00, 0x99 });
    CHECK(!MopLootPackets::ParseLootRequest(trailing, guid));
}

static void TestLootReleaseRequest()
{
    WorldPacket dense = Packet(CMSG_LOOT_RELEASE,
        { 0xFF, 0x10, 0x76, 0x54, 0x32, 0x67, 0x45, 0x89, 0x23 });
    uint64 guid = 0;
    CHECK(MopLootPackets::ParseLootReleaseRequest(dense, guid));
    CHECK(guid == UINT64_C(0x8877665544332211));

    WorldPacket sparse = Packet(CMSG_LOOT_RELEASE, { 0x88, 0x10, 0x89 });
    CHECK(MopLootPackets::ParseLootReleaseRequest(sparse, guid));
    CHECK(guid == UINT64_C(0x8800000000000011));
}

static void TestLootReleaseRequestRejectsMalformedBodies()
{
    uint64 guid = 0;
    WorldPacket truncated = Packet(CMSG_LOOT_RELEASE, { 0xFF, 0x10 });
    CHECK(!MopLootPackets::ParseLootReleaseRequest(truncated, guid));

    WorldPacket trailing = Packet(CMSG_LOOT_RELEASE,
        { 0x00, 0x99 });
    CHECK(!MopLootPackets::ParseLootReleaseRequest(trailing, guid));
}

static void TestLootMoneyRequest()
{
    WorldPacket empty(CMSG_LOOT_MONEY, 0);
    CHECK(MopLootPackets::ParseLootMoneyRequest(empty));

    WorldPacket trailing = Packet(CMSG_LOOT_MONEY, { 0x99 });
    CHECK(!MopLootPackets::ParseLootMoneyRequest(trailing));
}

static void TestAutostoreLootItemRequest()
{
    std::vector<MopLootPackets::LootTakeEntry> entries;
    WorldPacket dense = Packet(CMSG_AUTOSTORE_LOOT_ITEM,
        {
            0x00, 0x00, 0x03, 0xFE,
            0x11, 0x51, 0x21, 0x81, 0x71, 0x61, 0x41, 0x31, 0x05
        });
    CHECK(MopLootPackets::ParseAutostoreLootItemRequest(dense, entries));
    CHECK(entries.size() == 1);
    if (entries.size() == 1)
    {
        CHECK(entries[0].lootGuid == UINT64_C(0x8070605040302010));
        CHECK(entries[0].lootListId == 5);
    }

    WorldPacket sparse = Packet(CMSG_AUTOSTORE_LOOT_ITEM,
        { 0x00, 0x00, 0x02, 0xC0, 0x11, 0x81, 0x05 });
    CHECK(MopLootPackets::ParseAutostoreLootItemRequest(sparse, entries));
    CHECK(entries.size() == 1);
    if (entries.size() == 1)
    {
        CHECK(entries[0].lootGuid == UINT64_C(0x8000000000000010));
    }
}

static void TestAutostoreLootItemRejectsMalformedBodies()
{
    std::vector<MopLootPackets::LootTakeEntry> entries;

    WorldPacket zeroCount = Packet(CMSG_AUTOSTORE_LOOT_ITEM,
        { 0x00, 0x00, 0x00 });
    CHECK(!MopLootPackets::ParseAutostoreLootItemRequest(zeroCount, entries));

    // The client collector never constructs more than 50 entries. Reject the
    // 23-bit wire maximum before allocating or reading per-entry masks.
    WorldPacket overClientLimit = Packet(CMSG_AUTOSTORE_LOOT_ITEM,
        { 0x00, 0x00, 0x66 });
    CHECK(!MopLootPackets::ParseAutostoreLootItemRequest(overClientLimit, entries));

    WorldPacket truncated = Packet(CMSG_AUTOSTORE_LOOT_ITEM,
        { 0x00, 0x00, 0x03, 0xFE, 0x11 });
    CHECK(!MopLootPackets::ParseAutostoreLootItemRequest(truncated, entries));

    WorldPacket trailing = Packet(CMSG_AUTOSTORE_LOOT_ITEM,
        {
            0x00, 0x00, 0x03, 0xFE,
            0x11, 0x51, 0x21, 0x81, 0x71, 0x61, 0x41, 0x31, 0x05, 0x99
        });
    CHECK(!MopLootPackets::ParseAutostoreLootItemRequest(trailing, entries));
}

static void TestGroupLootVoteRequest()
{
    MopGroupLootPackets::VoteRequest request;
    WorldPacket dense = Packet(CMSG_LOOT_ROLL,
        {
            0x05, 0x02, 0xFF,
            0x00, 0x02, 0x09, 0x05, 0x03, 0x07, 0x04, 0x06
        });
    CHECK(MopGroupLootPackets::ParseVoteRequest(dense, request));
    CHECK(request.lootGuid == UINT64_C(0x0807060504030201));
    CHECK(request.lootListId == 5);
    CHECK(request.rollType == 2);
    CHECK(dense.rpos() == dense.size());

    WorldPacket sparse = Packet(CMSG_LOOT_ROLL,
        { 0x07, 0x01, 0x90, 0x10, 0x89 });
    CHECK(MopGroupLootPackets::ParseVoteRequest(sparse, request));
    CHECK(request.lootGuid == UINT64_C(0x8800000000000011));
    CHECK(request.lootListId == 7);
    CHECK(request.rollType == 1);
}

static void TestGroupLootVoteRejectsMalformedBodies()
{
    MopGroupLootPackets::VoteRequest request;

    WorldPacket truncated = Packet(CMSG_LOOT_ROLL,
        { 0x05, 0x02, 0xFF, 0x00 });
    CHECK(!MopGroupLootPackets::ParseVoteRequest(truncated, request));

    WorldPacket trailing = Packet(CMSG_LOOT_ROLL,
        { 0x05, 0x02, 0x00, 0x99 });
    CHECK(!MopGroupLootPackets::ParseVoteRequest(trailing, request));
}

static MopLootPackets::LootItem DenseGroupLootItem()
{
    MopLootPackets::LootItem item;
    item.itemId = 0x55667788;
    item.displayInfoId = 0xDDEEFF00;
    item.count = 2;
    item.randomPropertyId = int32(0x99AABBCC);
    item.randomSuffix = 0x11223344;
    item.optionalByte = 5;
    item.hasOptionalByte = true;
    item.lootListId = 7;
    item.hasLootListId = true;
    item.unknown = 3;
    item.slotType = 4;
    item.canTradeToTapList = true;
    item.situ = { 0xDD, 0xCC, 0xBB, 0xAA };
    return item;
}

static void TestGroupLootStartRoll()
{
    MopGroupLootPackets::StartRoll start;
    start.lootGuid = UINT64_C(0x0807060504030201);
    start.mapId = 0x0A0B0C0D;
    start.itemSlot = 7;
    start.offeredVoteMask = 0x0F;
    start.durationMs = 60000;
    start.item = DenseGroupLootItem();

    WorldPacket packet;
    CHECK(MopGroupLootPackets::BuildStartRoll(packet, start));
    CheckPacket(packet, SMSG_LOOT_START_ROLL,
        {
            0x8F, 0xF6, 0x09, 0x44, 0x33, 0x22, 0x11, 0x07,
            0x0D, 0x0C, 0x0B, 0x0A, 0xCC, 0xBB, 0xAA, 0x99,
            0x07, 0x04, 0x00, 0x05, 0x02, 0x04, 0x00, 0x00,
            0x00, 0xDD, 0xCC, 0xBB, 0xAA, 0x88, 0x77, 0x66,
            0x55, 0x0F, 0x02, 0x00, 0x00, 0x00, 0x07, 0x60,
            0xEA, 0x00, 0x00, 0x05, 0x06, 0x00, 0xFF, 0xEE,
            0xDD, 0x03
        });
}

static void TestGroupLootRollUpdate()
{
    MopGroupLootPackets::RollUpdate update;
    update.lootGuid = UINT64_C(0x0807060504030201);
    update.participantGuid = UINT64_C(0x1817161514131211);
    update.rollNumber = 100;
    update.itemSlot = 7;
    update.rollType = 1;
    update.autoPass = true;
    update.item = DenseGroupLootItem();

    WorldPacket packet;
    CHECK(MopGroupLootPackets::BuildRollUpdate(packet, update));
    CheckPacket(packet, SMSG_LOOT_ROLL,
        {
            0xF7, 0x7F, 0xF3, 0x80, 0x09, 0x16, 0x00, 0x07,
            0x05, 0x07, 0x00, 0xFF, 0xEE, 0xDD, 0x44, 0x33,
            0x22, 0x11, 0xCC, 0xBB, 0xAA, 0x99, 0x04, 0x00,
            0x00, 0x00, 0xDD, 0xCC, 0xBB, 0xAA, 0x03, 0x04,
            0x12, 0x02, 0x88, 0x77, 0x66, 0x55, 0x14, 0x64,
            0x00, 0x00, 0x00, 0x05, 0x06, 0x13, 0x10, 0x01,
            0x19, 0x02, 0x00, 0x00, 0x00, 0x17, 0x15
        });
}

static void TestGroupLootRollWinner()
{
    MopGroupLootPackets::RollWinner winner;
    winner.lootGuid = UINT64_C(0x0807060504030201);
    winner.winnerGuid = UINT64_C(0x1817161514131211);
    winner.rollNumber = 100;
    winner.itemSlot = 7;
    winner.rollType = 1;
    winner.item = DenseGroupLootItem();

    WorldPacket packet;
    CHECK(MopGroupLootPackets::BuildRollWinner(packet, winner));
    CheckPacket(packet, SMSG_LOOT_ROLL_WON,
        {
            0xFF, 0x7F, 0x37, 0x04, 0x00, 0x00, 0x00, 0xDD,
            0xCC, 0xBB, 0xAA, 0x00, 0xFF, 0xEE, 0xDD, 0x12,
            0x09, 0x44, 0x33, 0x22, 0x11, 0x17, 0x05, 0x19,
            0x03, 0x02, 0x00, 0x15, 0x64, 0x00, 0x00, 0x00,
            0xCC, 0xBB, 0xAA, 0x99, 0x06, 0x13, 0x14, 0x01,
            0x16, 0x07, 0x04, 0x88, 0x77, 0x66, 0x55, 0x07,
            0x10, 0x02, 0x00, 0x00, 0x00, 0x05
        });
}

static void TestGroupLootAllPassed()
{
    MopGroupLootPackets::AllPassed passed;
    passed.lootGuid = UINT64_C(0x0807060504030201);
    passed.itemSlot = 7;
    passed.item = DenseGroupLootItem();

    WorldPacket packet;
    CHECK(MopGroupLootPackets::BuildAllPassed(packet, passed));
    CheckPacket(packet, SMSG_LOOT_ALL_PASSED,
        {
            0x3F, 0xCF, 0x06, 0x07, 0x00, 0x02, 0x00, 0x00,
            0x00, 0x05, 0x02, 0x05, 0x00, 0xFF, 0xEE, 0xDD,
            0x03, 0x44, 0x33, 0x22, 0x11, 0x04, 0x00, 0x00,
            0x00, 0xDD, 0xCC, 0xBB, 0xAA, 0x04, 0x88, 0x77,
            0x66, 0x55, 0xCC, 0xBB, 0xAA, 0x99, 0x07, 0x09
        });
}

static MopLootPackets::LootResponse BaseLootResponse()
{
    MopLootPackets::LootResponse response;
    response.sourceGuid = UINT64_C(0x0807060504030201);
    response.lootGuid = UINT64_C(0x1817161514131211);
    response.hasLootType = true;
    response.lootType = 1;
    response.success = true;
    return response;
}

static void TestEmptyLootResponse()
{
    MopLootPackets::LootResponse response = BaseLootResponse();
    WorldPacket packet;

    CHECK(MopLootPackets::BuildLootResponse(packet, response));
    CheckPacket(packet, SMSG_LOOT_RESPONSE,
        {
            0xA0, 0x00, 0x01, 0xFF, 0x60, 0x00, 0x03, 0xFC,
            0x12, 0x19, 0x07, 0x15, 0x14, 0x01, 0x04, 0x17,
            0x02, 0x05, 0x13, 0x00, 0x10, 0x06, 0x09, 0x03, 0x16
        });
}

static void TestDenseItemLootResponse()
{
    MopLootPackets::LootResponse response = BaseLootResponse();
    MopLootPackets::LootItem item;
    item.slotType = 4;
    item.canTradeToTapList = true;
    item.hasOptionalByte = true;
    item.optionalByte = 5;
    item.hasLootListId = true;
    item.lootListId = 7;
    item.unknown = 3;
    item.randomSuffix = 0x11223344;
    item.count = 2;
    item.itemId = 0x55667788;
    item.situ = { 0xDD, 0xCC, 0xBB, 0xAA };
    item.randomPropertyId = int32(0x99AABBCC);
    item.displayInfoId = 0xDDEEFF00;
    response.items.push_back(item);

    WorldPacket packet;
    CHECK(MopLootPackets::BuildLootResponse(packet, response));
    CheckPacket(packet, SMSG_LOOT_RESPONSE,
        {
            0xA0, 0x00, 0x01, 0xFF, 0x60, 0x00, 0x07, 0x27, 0xFC,
            0x44, 0x33, 0x22, 0x11,
            0x02, 0x00, 0x00, 0x00,
            0x88, 0x77, 0x66, 0x55,
            0x04, 0x00, 0x00, 0x00, 0xDD, 0xCC, 0xBB, 0xAA,
            0x05,
            0xCC, 0xBB, 0xAA, 0x99,
            0x07,
            0x00, 0xFF, 0xEE, 0xDD,
            0x12, 0x19, 0x07, 0x15, 0x14, 0x01, 0x04, 0x17,
            0x02, 0x05, 0x13, 0x00, 0x10, 0x06, 0x09, 0x03, 0x16
        });
}

static void TestDenseCurrencyLootResponse()
{
    MopLootPackets::LootResponse response = BaseLootResponse();
    MopLootPackets::LootCurrency currency;
    currency.slotType = 4;
    currency.amount = 0x11223344;
    currency.lootListId = 7;
    currency.currencyId = 0x55667788;
    response.currencies.push_back(currency);

    WorldPacket packet;
    CHECK(MopLootPackets::BuildLootResponse(packet, response));
    CheckPacket(packet, SMSG_LOOT_RESPONSE,
        {
            0xA0, 0x00, 0x03, 0xFF, 0x60, 0x00, 0x03, 0xCF, 0x80,
            0x12, 0x19, 0x07, 0x15, 0x14, 0x01, 0x04, 0x17,
            0x44, 0x33, 0x22, 0x11, 0x07, 0x88, 0x77, 0x66, 0x55,
            0x02, 0x05, 0x13, 0x00, 0x10, 0x06, 0x09, 0x03, 0x16
        });
}

static void TestMoneyAndGroupLootResponse()
{
    MopLootPackets::LootResponse response = BaseLootResponse();
    response.money = 0x11223344;
    response.hasLootMethod = true;
    response.lootMethod = 2;
    response.hasLootThreshold = true;
    response.lootThreshold = 4;

    WorldPacket packet;
    CHECK(MopLootPackets::BuildLootResponse(packet, response));
    CheckPacket(packet, SMSG_LOOT_RESPONSE,
        {
            0x20, 0x00, 0x01, 0xFB, 0x60, 0x00, 0x03, 0xF4,
            0x04,
            0x12,
            0x44, 0x33, 0x22, 0x11,
            0x19, 0x07, 0x15, 0x14,
            0x02, 0x01,
            0x04, 0x17,
            0x02, 0x05, 0x13, 0x00, 0x10, 0x06, 0x09, 0x03, 0x16
        });
}

static void TestLootResponseRejectsUnsafeProducerValues()
{
    {
        MopLootPackets::LootResponse response = BaseLootResponse();
        response.items.resize(256);
        WorldPacket packet;
        CHECK(!MopLootPackets::BuildLootResponse(packet, response));
    }
    {
        MopLootPackets::LootResponse response = BaseLootResponse();
        MopLootPackets::LootItem item;
        item.slotType = 8;
        response.items.push_back(item);
        WorldPacket packet;
        CHECK(!MopLootPackets::BuildLootResponse(packet, response));
    }
    {
        MopLootPackets::LootResponse response = BaseLootResponse();
        MopLootPackets::LootItem item;
        item.situ.resize(MopLootPackets::MAX_SITU_BYTES + 1);
        response.items.push_back(item);
        WorldPacket packet;
        CHECK(!MopLootPackets::BuildLootResponse(packet, response));
    }
}

static void TestLootReleaseResponse()
{
    WorldPacket packet;
    MopLootPackets::BuildLootReleaseResponse(packet,
        UINT64_C(0x8877665544332211), UINT64_C(0x18F7E6D5C4B3A291));
    CheckPacket(packet, SMSG_LOOT_RELEASE_RESPONSE,
        {
            0xFF, 0xFF, 0x23, 0xA3, 0x32, 0x67, 0xE7, 0x19,
            0xC5, 0x10, 0xB2, 0x90, 0x45, 0x76, 0xF6, 0x54,
            0xD4, 0x89
        });

    MopLootPackets::BuildLootReleaseResponse(packet,
        UINT64_C(0x8800000000000011), UINT64_C(0x00F700000000A200));
    CheckPacket(packet, SMSG_LOOT_RELEASE_RESPONSE,
        { 0x14, 0x05, 0xA3, 0x10, 0xF6, 0x89 });
}

static void TestLootMoneyNotify()
{
    WorldPacket packet;
    MopLootPackets::BuildLootMoneyNotify(packet, 0x11223344, false);
    CheckPacket(packet, SMSG_LOOT_MONEY_NOTIFY,
        { 0x00, 0x44, 0x33, 0x22, 0x11 });

    MopLootPackets::BuildLootMoneyNotify(packet, 0x11223344, true);
    CheckPacket(packet, SMSG_LOOT_MONEY_NOTIFY,
        { 0x80, 0x44, 0x33, 0x22, 0x11 });
}

static void TestLootClearMoney()
{
    WorldPacket packet;
    MopLootPackets::BuildLootClearMoney(packet, UINT64_C(0x8070605040302010));
    CheckPacket(packet, SMSG_LOOT_CLEAR_MONEY,
        { 0xFF, 0x11, 0x51, 0x31, 0x81, 0x21, 0x61, 0x41, 0x71 });

    MopLootPackets::BuildLootClearMoney(packet, UINT64_C(0x8000000000000010));
    CheckPacket(packet, SMSG_LOOT_CLEAR_MONEY, { 0x41, 0x11, 0x81 });
}

static void TestLootRemoved()
{
    WorldPacket packet;
    MopLootPackets::BuildLootRemoved(packet,
        UINT64_C(0x8070605040302010), UINT64_C(0x01F1E1D1C1B1A191), 5);
    CheckPacket(packet, SMSG_LOOT_REMOVED,
        {
            0xFF, 0xFF, 0xA0, 0x81, 0x00, 0x90, 0x71, 0x31,
            0xE0, 0xC0, 0xB0, 0x11, 0x61, 0x21, 0x05, 0xF0,
            0x41, 0x51, 0xD0
        });
}

int main(int /*argc*/, char** /*argv*/)
{
    TestLootRequest();
    TestLootRequestRejectsMalformedBodies();
    TestLootReleaseRequest();
    TestLootReleaseRequestRejectsMalformedBodies();
    TestLootMoneyRequest();
    TestAutostoreLootItemRequest();
    TestAutostoreLootItemRejectsMalformedBodies();
    TestGroupLootVoteRequest();
    TestGroupLootVoteRejectsMalformedBodies();
    TestGroupLootStartRoll();
    TestGroupLootRollUpdate();
    TestGroupLootRollWinner();
    TestGroupLootAllPassed();
    TestEmptyLootResponse();
    TestDenseItemLootResponse();
    TestDenseCurrencyLootResponse();
    TestMoneyAndGroupLootResponse();
    TestLootResponseRejectsUnsafeProducerValues();
    TestLootReleaseResponse();
    TestLootMoneyNotify();
    TestLootClearMoney();
    TestLootRemoved();
    return g_fail ? 1 : 0;
}
