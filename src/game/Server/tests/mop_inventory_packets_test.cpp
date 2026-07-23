/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "Item.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket Packet(OpcodesList opcode, std::initializer_list<uint8> body)
{
    WorldPacket packet(opcode, body.size());
    for (uint8 value : body)
        packet << value;
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
            CHECK(packet[index] == value);
        ++index;
    }
}

static void TestSwapInvItemDense()
{
    WorldPacket packet = Packet(CMSG_SWAP_INV_ITEM,
        { 0x21, 0x22, 0x80, 0x32, 0x31, 0x42, 0x41 });
    MopItemPackets::SwapInvItemRequest request;

    CHECK(MopItemPackets::ParseSwapInvItem(packet, request));
    CHECK(request.sourceSlot == 0x21);
    CHECK(request.destinationSlot == 0x22);
    CHECK(request.updates[0].bag == 0x31);
    CHECK(request.updates[0].slot == 0x32);
    CHECK(request.updates[1].bag == 0x41);
    CHECK(request.updates[1].slot == 0x42);
    CHECK(packet.rpos() == packet.size());
}

static void TestSwapInvItemSparse()
{
    WorldPacket packet = Packet(CMSG_SWAP_INV_ITEM,
        { 0x00, 0x01, 0x9C, 0x01 });
    MopItemPackets::SwapInvItemRequest request;

    CHECK(MopItemPackets::ParseSwapInvItem(packet, request));
    CHECK(request.sourceSlot == 0x00);
    CHECK(request.destinationSlot == 0x01);
    CHECK(request.updates[0].bag == 0x00);
    CHECK(request.updates[0].slot == 0x01);
    CHECK(request.updates[1].bag == 0x00);
    CHECK(request.updates[1].slot == 0x00);
    CHECK(packet.rpos() == packet.size());
}

static void TestSwapInvItemRejectsMalformedBodies()
{
    {
        WorldPacket packet = Packet(CMSG_SWAP_INV_ITEM,
            { 0x21, 0x22, 0x40 });
        MopItemPackets::SwapInvItemRequest request;
        CHECK(!MopItemPackets::ParseSwapInvItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_SWAP_INV_ITEM,
            { 0x21, 0x22, 0x81, 0x32, 0x31, 0x42, 0x41 });
        MopItemPackets::SwapInvItemRequest request;
        CHECK(!MopItemPackets::ParseSwapInvItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_SWAP_INV_ITEM,
            { 0x21, 0x22, 0x80, 0x32 });
        MopItemPackets::SwapInvItemRequest request;
        CHECK(!MopItemPackets::ParseSwapInvItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_SWAP_INV_ITEM,
            { 0x21, 0x22, 0x80, 0x32, 0x31, 0x42, 0x41, 0x99 });
        MopItemPackets::SwapInvItemRequest request;
        CHECK(!MopItemPackets::ParseSwapInvItem(packet, request));
    }
}

static void TestSwapItemDense()
{
    WorldPacket packet = Packet(CMSG_SWAP_ITEM,
        { 0x21, 0x31, 0x41, 0x22, 0x80, 0x31, 0x22, 0x41, 0x21 });
    MopItemPackets::SwapItemRequest request;

    CHECK(MopItemPackets::ParseSwapItem(packet, request));
    CHECK(request.sourceBag == 0x41);
    CHECK(request.sourceSlot == 0x21);
    CHECK(request.destinationBag == 0x31);
    CHECK(request.destinationSlot == 0x22);
    CHECK(request.updates[0].bag == 0x31);
    CHECK(request.updates[0].slot == 0x22);
    CHECK(request.updates[1].bag == 0x41);
    CHECK(request.updates[1].slot == 0x21);
    CHECK(packet.rpos() == packet.size());
}

static void TestSwapItemRejectsMalformedBodies()
{
    {
        WorldPacket packet = Packet(CMSG_SWAP_ITEM,
            { 0x21, 0x31, 0x41, 0x22, 0x40 });
        MopItemPackets::SwapItemRequest request;
        CHECK(!MopItemPackets::ParseSwapItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_SWAP_ITEM,
            { 0x21, 0x31, 0x41, 0x22, 0x81, 0x31, 0x22, 0x41, 0x21 });
        MopItemPackets::SwapItemRequest request;
        CHECK(!MopItemPackets::ParseSwapItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_SWAP_ITEM,
            { 0x21, 0x31, 0x41, 0x22, 0x80, 0x31 });
        MopItemPackets::SwapItemRequest request;
        CHECK(!MopItemPackets::ParseSwapItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_SWAP_ITEM,
            { 0x21, 0x31, 0x41, 0x22, 0x80, 0x31, 0x22, 0x41, 0x21, 0x99 });
        MopItemPackets::SwapItemRequest request;
        CHECK(!MopItemPackets::ParseSwapItem(packet, request));
    }
}

static void TestAutoStoreBagItem()
{
    WorldPacket packet = Packet(CMSG_AUTOSTORE_BAG_ITEM,
        { 0x21, 0x31, 0x41, 0x00 });
    MopItemPackets::AutoStoreBagItemRequest request;

    CHECK(MopItemPackets::ParseAutoStoreBagItem(packet, request));
    CHECK(request.sourceSlot == 0x21);
    CHECK(request.sourceBag == 0x31);
    CHECK(request.destinationBag == 0x41);
    CHECK(packet.rpos() == packet.size());
}

static void TestAutoStoreBagItemRejectsMalformedBodies()
{
    {
        WorldPacket packet = Packet(CMSG_AUTOSTORE_BAG_ITEM,
            { 0x21, 0x31, 0x41 });
        MopItemPackets::AutoStoreBagItemRequest request;
        CHECK(!MopItemPackets::ParseAutoStoreBagItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_AUTOSTORE_BAG_ITEM,
            { 0x21, 0x31, 0x41, 0x40 });
        MopItemPackets::AutoStoreBagItemRequest request;
        CHECK(!MopItemPackets::ParseAutoStoreBagItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_AUTOSTORE_BAG_ITEM,
            { 0x21, 0x31, 0x41, 0x00, 0x99 });
        MopItemPackets::AutoStoreBagItemRequest request;
        CHECK(!MopItemPackets::ParseAutoStoreBagItem(packet, request));
    }
}

static void TestAutoEquipItemDense()
{
    WorldPacket packet = Packet(CMSG_AUTOEQUIP_ITEM,
        { 0x21, 0x31, 0x40, 0x22, 0x41 });
    MopItemPackets::AutoEquipItemRequest request;

    CHECK(MopItemPackets::ParseAutoEquipItem(packet, request));
    CHECK(request.cursorSlot == 0x21);
    CHECK(request.cursorBag == 0x31);
    CHECK(request.update.bag == 0x41);
    CHECK(request.update.slot == 0x22);
    CHECK(packet.rpos() == packet.size());
}

static void TestAutoEquipItemSparse()
{
    WorldPacket packet = Packet(CMSG_AUTOEQUIP_ITEM,
        { 0x21, 0x31, 0x70 });
    MopItemPackets::AutoEquipItemRequest request;

    CHECK(MopItemPackets::ParseAutoEquipItem(packet, request));
    CHECK(request.update.bag == 0x00);
    CHECK(request.update.slot == 0x00);
    CHECK(packet.rpos() == packet.size());
}

static void TestAutoEquipItemRejectsMalformedBodies()
{
    {
        WorldPacket packet = Packet(CMSG_AUTOEQUIP_ITEM,
            { 0x21, 0x31, 0x00 });
        MopItemPackets::AutoEquipItemRequest request;
        CHECK(!MopItemPackets::ParseAutoEquipItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_AUTOEQUIP_ITEM,
            { 0x21, 0x31, 0x41, 0x22, 0x41 });
        MopItemPackets::AutoEquipItemRequest request;
        CHECK(!MopItemPackets::ParseAutoEquipItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_AUTOEQUIP_ITEM,
            { 0x21, 0x31, 0x40, 0x22 });
        MopItemPackets::AutoEquipItemRequest request;
        CHECK(!MopItemPackets::ParseAutoEquipItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_AUTOEQUIP_ITEM,
            { 0x21, 0x31, 0x70, 0x99 });
        MopItemPackets::AutoEquipItemRequest request;
        CHECK(!MopItemPackets::ParseAutoEquipItem(packet, request));
    }
}

static void TestSplitItem()
{
    WorldPacket packet = Packet(CMSG_SPLIT_ITEM,
        { 0x31, 0x05, 0x00, 0x00, 0x00, 0x41, 0x21, 0x22, 0x00 });
    MopItemPackets::SplitItemRequest request;

    CHECK(MopItemPackets::ParseSplitItem(packet, request));
    CHECK(request.sourceBag == 0x31);
    CHECK(request.count == 5);
    CHECK(request.destinationBag == 0x41);
    CHECK(request.sourceSlot == 0x21);
    CHECK(request.destinationSlot == 0x22);
    CHECK(packet.rpos() == packet.size());
}

static void TestSplitItemRejectsMalformedBodies()
{
    {
        WorldPacket packet = Packet(CMSG_SPLIT_ITEM,
            { 0x31, 0x00, 0x00, 0x00, 0x00, 0x41, 0x21, 0x22, 0x00 });
        MopItemPackets::SplitItemRequest request;
        CHECK(!MopItemPackets::ParseSplitItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_SPLIT_ITEM,
            { 0x31, 0x05, 0x00, 0x00, 0x00, 0x41, 0x21, 0x22, 0x40 });
        MopItemPackets::SplitItemRequest request;
        CHECK(!MopItemPackets::ParseSplitItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_SPLIT_ITEM,
            { 0x31, 0x05, 0x00, 0x00, 0x00, 0x41, 0x21, 0x22 });
        MopItemPackets::SplitItemRequest request;
        CHECK(!MopItemPackets::ParseSplitItem(packet, request));
    }
    {
        WorldPacket packet = Packet(CMSG_SPLIT_ITEM,
            { 0x31, 0x05, 0x00, 0x00, 0x00, 0x41, 0x21, 0x22, 0x00, 0x99 });
        MopItemPackets::SplitItemRequest request;
        CHECK(!MopItemPackets::ParseSplitItem(packet, request));
    }
}

static void TestInventoryChangeFailureMinimumBody()
{
    MopItemPackets::InventoryChangeFailure failure;
    WorldPacket packet;

    MopItemPackets::BuildInventoryChangeFailure(packet, failure);
    CheckPacket(packet, SMSG_INVENTORY_CHANGE_FAILURE,
        { 0x00, 0x00, 0x00, 0x00 });
}

static void TestInventoryChangeFailureLimitCategory()
{
    MopItemPackets::InventoryChangeFailure failure;
    failure.result = 84;
    failure.itemGuid = 0x0807060504030201ULL;
    failure.itemGuid2 = 0x1817161514131211ULL;
    failure.bagSubclass = 0x2A;
    failure.resultParameter = 0x44332211;
    WorldPacket packet;

    MopItemPackets::BuildInventoryChangeFailure(packet, failure);
    CheckPacket(packet, SMSG_INVENTORY_CHANGE_FAILURE,
        {
            0xFF, 0xFF,
            0x10, 0x2A, 0x16, 0x04, 0x00, 0x09, 0x05, 0x13,
            0x17, 0x07, 0x19, 0x12, 0x03, 0x06, 0x02, 0x15,
            0x14, 0x54,
            0x11, 0x22, 0x33, 0x44
        });
}

static void TestInventoryChangeFailureRequiredLevel()
{
    MopItemPackets::InventoryChangeFailure failure;
    failure.result = 1;
    failure.resultParameter = 0x44332211;
    WorldPacket packet;

    MopItemPackets::BuildInventoryChangeFailure(packet, failure);
    CheckPacket(packet, SMSG_INVENTORY_CHANGE_FAILURE,
        { 0x00, 0x00, 0x00, 0x01, 0x11, 0x22, 0x33, 0x44 });
}

static void TestInventoryChangeFailureBindConfirm()
{
    MopItemPackets::InventoryChangeFailure failure;
    failure.result = 81;
    failure.itemGuid = 0x0807060504030201ULL;
    failure.itemGuid2 = 0x1817161514131211ULL;
    failure.bagSubclass = 0x2A;
    failure.resultParameter = 0x78563412;
    failure.bindConfirmPendingGuid = 0x2827262524232221ULL;
    failure.bindConfirmHelperGuid = 0x3837363534333231ULL;
    WorldPacket packet;

    MopItemPackets::BuildInventoryChangeFailure(packet, failure);
    CheckPacket(packet, SMSG_INVENTORY_CHANGE_FAILURE,
        {
            0xFF, 0xFF,
            0x10, 0x2A, 0x16, 0x04, 0x00, 0x09, 0x05, 0x13,
            0x17, 0x07, 0x19, 0x12, 0x03, 0x06, 0x02, 0x15,
            0x14, 0x51,
            0x12, 0x34, 0x56, 0x78,
            0xFF, 0xFF,
            0x39, 0x25, 0x23, 0x24, 0x33, 0x29, 0x36, 0x32,
            0x26, 0x30, 0x22, 0x34, 0x20, 0x27, 0x37, 0x35
        });
}

int main(int /*argc*/, char** /*argv*/)
{
    TestSwapInvItemDense();
    TestSwapInvItemSparse();
    TestSwapInvItemRejectsMalformedBodies();
    TestSwapItemDense();
    TestSwapItemRejectsMalformedBodies();
    TestAutoStoreBagItem();
    TestAutoStoreBagItemRejectsMalformedBodies();
    TestAutoEquipItemDense();
    TestAutoEquipItemSparse();
    TestAutoEquipItemRejectsMalformedBodies();
    TestSplitItem();
    TestSplitItemRejectsMalformedBodies();
    TestInventoryChangeFailureMinimumBody();
    TestInventoryChangeFailureLimitCategory();
    TestInventoryChangeFailureRequiredLevel();
    TestInventoryChangeFailureBindConfirm();
    return g_fail ? 1 : 0;
}
