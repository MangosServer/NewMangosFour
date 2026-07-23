/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    return packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static void test_compact_statuses()
{
    WorldPacket accepted;
    CHECK(MopTradePackets::BuildStatus(accepted, TRADE_STATUS_ACCEPTED));
    CHECK(accepted.GetOpcode() == SMSG_TRADE_STATUS);
    CHECK(Equal(accepted, { 0x18 }));

    MopTradePackets::StatusData failedData;
    failedData.flag = true;
    failedData.first = 0x11223344;
    failedData.second = 0x55667788;
    WorldPacket failed;
    CHECK(MopTradePackets::BuildStatus(failed, TRADE_STATUS_FAILED, failedData));
    CHECK(Equal(failed, {
        0x02,
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55
    }));

    MopTradePackets::StatusData initiatedData;
    initiatedData.first = 0xA1B2C3D4;
    WorldPacket initiated;
    CHECK(MopTradePackets::BuildStatus(
        initiated, TRADE_STATUS_INITIATED, initiatedData));
    CHECK(Equal(initiated, {
        0x08, 0xD4, 0xC3, 0xB2, 0xA1
    }));

    MopTradePackets::StatusData currencyData;
    currencyData.first = 0x01020304;
    currencyData.second = 0xA0B0C0D0;
    WorldPacket currency;
    CHECK(MopTradePackets::BuildStatus(
        currency, TRADE_STATUS_CURRENCY_NOT_TRADABLE, currencyData));
    CHECK(Equal(currency, {
        0x0C,
        0x04, 0x03, 0x02, 0x01,
        0xD0, 0xC0, 0xB0, 0xA0
    }));

    WorldPacket notEnoughCurrency;
    CHECK(MopTradePackets::BuildStatus(
        notEnoughCurrency, TRADE_STATUS_NOT_ENOUGH_CURRENCY, currencyData));
    CHECK(Equal(notEnoughCurrency, {
        0x5C,
        0x04, 0x03, 0x02, 0x01,
        0xD0, 0xC0, 0xB0, 0xA0
    }));

    MopTradePackets::StatusData slotData;
    slotData.slot = 0xA5;
    WorldPacket wrongRealm;
    CHECK(MopTradePackets::BuildStatus(
        wrongRealm, TRADE_STATUS_WRONG_REALM, slotData));
    CHECK(Equal(wrongRealm, { 0x58, 0xA5 }));

    WorldPacket notOnTapList;
    CHECK(MopTradePackets::BuildStatus(
        notOnTapList, TRADE_STATUS_NOT_ON_TAPLIST, slotData));
    CHECK(Equal(notOnTapList, { 0x7C, 0xA5 }));

    MopTradePackets::StatusData proposalData;
    proposalData.guid = UI64LIT(0x0100020300040506);
    WorldPacket proposed;
    CHECK(MopTradePackets::BuildStatus(
        proposed, TRADE_STATUS_PROPOSED, proposalData));
    CHECK(Equal(proposed, {
        0x61, 0xEC,
        0x05, 0x04, 0x00, 0x03, 0x02, 0x07
    }));

    WorldPacket zeroGuidProposal;
    CHECK(MopTradePackets::BuildStatus(
        zeroGuidProposal, TRADE_STATUS_PROPOSED));
    CHECK(Equal(zeroGuidProposal, { 0x60, 0x00 }));
}

static MopTradePackets::UpdateData MakeUpdate()
{
    MopTradePackets::UpdateData update;
    update.transaction = 0x11223344;
    update.currency = 0x55667788;
    update.spell = 0x99AABBCC;
    update.side = 0xDD;
    update.money = UI64LIT(0x0102030405060708);
    update.slotCountA = 0x11223345;
    update.sequence = 0x55667789;
    update.slotCountB = 0x99AABBCD;
    return update;
}

static void test_extended_zero_items()
{
    WorldPacket packet;
    CHECK(MopTradePackets::BuildUpdate(packet, MakeUpdate()));
    CHECK(packet.GetOpcode() == SMSG_TRADE_STATUS_EXTENDED);
    CHECK(Equal(packet, {
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99,
        0xDD,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x45, 0x33, 0x22, 0x11,
        0x89, 0x77, 0x66, 0x55,
        0xCD, 0xBB, 0xAA, 0x99,
        0x00, 0x00, 0x00
    }));
}

static void test_extended_unwrapped_item()
{
    MopTradePackets::UpdateData update = MakeUpdate();
    MopTradePackets::ItemData item;
    item.notWrapped = true;
    item.creatorGuid = UI64LIT(0x0102000405000708);
    item.giftCreatorGuid = UI64LIT(0xA1A2A300A5A600A8);
    item.hasLock = true;
    item.maxDurability = 0x11111111;
    item.unknown0 = 0x22222222;
    item.reforge = 0x33333333;
    item.permanentEnchant = 0x44444444;
    item.durability = 0x55555555;
    item.gemEnchant = {{ 0x66666666, 0x77777777, 0x88888888 }};
    item.randomProperty = 0x99999999;
    item.spellCharges = 0xAAAAAAAA;
    item.suffixFactor = 0xBBBBBBBB;
    item.slot = 6;
    item.itemId = 0xCCCCCCCC;
    item.stackCount = 0xDDDDDDDD;
    update.items.push_back(item);

    WorldPacket packet;
    CHECK(MopTradePackets::BuildUpdate(packet, update));
    CHECK(Equal(packet, {
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99,
        0xDD,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x45, 0x33, 0x22, 0x11,
        0x89, 0x77, 0x66, 0x55,
        0xCD, 0xBB, 0xAA, 0x99,
        0x00, 0x00, 0x1E, 0xFD, 0x74,
        0x04,
        0x11, 0x11, 0x11, 0x11,
        0x22, 0x22, 0x22, 0x22,
        0x33, 0x33, 0x33, 0x33,
        0x06, 0x00, 0x03, 0x09,
        0x44, 0x44, 0x44, 0x44,
        0x55, 0x55, 0x55, 0x55,
        0x66, 0x66, 0x66, 0x66,
        0x77, 0x77, 0x77, 0x77,
        0x88, 0x88, 0x88, 0x88,
        0x99, 0x99, 0x99, 0x99,
        0xAA, 0xAA, 0xAA, 0xAA,
        0xBB, 0xBB, 0xBB, 0xBB,
        0x05,
        0x06,
        0xA2, 0xA7, 0xA4,
        0xCC, 0xCC, 0xCC, 0xCC,
        0xA0, 0xA9,
        0xDD, 0xDD, 0xDD, 0xDD,
        0xA3
    }));
}

static void test_extended_wrapped_item()
{
    MopTradePackets::UpdateData update;
    MopTradePackets::ItemData item;
    item.notWrapped = false;
    item.creatorGuid = UI64LIT(0x0102000405000708);
    item.giftCreatorGuid = UI64LIT(0xA1A2A300A5A600A8);
    item.hasLock = true;
    item.maxDurability = 0x11111111;
    item.slot = 2;
    item.itemId = 0x12345678;
    item.stackCount = 0x90ABCDEF;
    update.items.push_back(item);

    WorldPacket packet;
    CHECK(MopTradePackets::BuildUpdate(packet, update));
    CHECK(Equal(packet, {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x16, 0xE8,
        0x02,
        0xA2, 0xA7, 0xA4,
        0x78, 0x56, 0x34, 0x12,
        0xA0, 0xA9,
        0xEF, 0xCD, 0xAB, 0x90,
        0xA3
    }));
}

static MopTradePackets::ItemData MakeWrappedItem(uint8 slot)
{
    MopTradePackets::ItemData item;
    item.notWrapped = false;
    item.slot = slot;
    item.itemId = 1000 + slot;
    item.stackCount = 1;
    return item;
}

static void test_extended_bounds()
{
    MopTradePackets::UpdateData seven;
    for (uint8 slot = 0; slot < 7; ++slot)
        seven.items.push_back(MakeWrappedItem(slot));
    WorldPacket boundary;
    CHECK(MopTradePackets::BuildUpdate(boundary, seven));
    CHECK(boundary.size() == 107);

    MopTradePackets::UpdateData tooMany = seven;
    tooMany.items.push_back(MakeWrappedItem(0));
    WorldPacket countRejected;
    CHECK(!MopTradePackets::BuildUpdate(countRejected, tooMany));
    CHECK(countRejected.empty());

    MopTradePackets::UpdateData invalidSlot;
    invalidSlot.items.push_back(MakeWrappedItem(7));
    WorldPacket slotRejected;
    CHECK(!MopTradePackets::BuildUpdate(slotRejected, invalidSlot));
    CHECK(slotRejected.empty());

    MopTradePackets::UpdateData duplicate;
    duplicate.items.push_back(MakeWrappedItem(3));
    duplicate.items.push_back(MakeWrappedItem(3));
    WorldPacket duplicateRejected;
    CHECK(!MopTradePackets::BuildUpdate(duplicateRejected, duplicate));
    CHECK(duplicateRejected.empty());
}

int main(int /*argc*/, char** /*argv*/)
{
    test_compact_statuses();
    test_extended_zero_items();
    test_extended_unwrapped_item();
    test_extended_wrapped_item();
    test_extended_bounds();
    return g_fail ? 1 : 0;
}
