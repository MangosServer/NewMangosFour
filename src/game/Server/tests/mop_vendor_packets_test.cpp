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

static void TestListInventoryRequest()
{
    WorldPacket packet(CMSG_LIST_INVENTORY, 6);
    uint8 const body[] = { 0xAD, 0x00, 0x06, 0x05, 0x07, 0x02 };
    packet.append(body, sizeof(body));

    CHECK(MopItemPackets::ReadListInventory(packet) == ObjectGuid(UINT64_C(0x0007060004030001)));
}

static void TestVendorList()
{
    MopItemPackets::VendorList list;
    list.vendorGuid = ObjectGuid(UINT64_C(0x0007060004030001));
    list.reason = 2;

    MopItemPackets::VendorItemRecord item;
    item.leftInStock = -1;
    item.price = 0x01020304;
    item.type = 1;
    item.maxDurability = 0x11121314;
    item.displayId = 0x21222324;
    item.buyCount = 0x31323334;
    item.itemId = 0x41424344;
    item.hasExtendedCost = true;
    item.extendedCost = 0x51525354;
    item.upgradeId = 0x61626364;
    item.hasCondition = true;
    item.condition = 0x71727374;
    item.slot = 0x81828384;
    item.doNotFilter = true;
    list.items.push_back(item);

    WorldPacket packet;
    MopItemPackets::BuildVendorList(packet, list);

    CHECK(packet.GetOpcode() == SMSG_LIST_INVENTORY);
    CHECK(BytesEqual(packet, {
        0x98, 0x00, 0x03, 0x18, 0x02,
        0xFF, 0xFF, 0xFF, 0xFF,
        0x04, 0x03, 0x02, 0x01,
        0x01, 0x00, 0x00, 0x00,
        0x14, 0x13, 0x12, 0x11,
        0x24, 0x23, 0x22, 0x21,
        0x34, 0x33, 0x32, 0x31,
        0x44, 0x43, 0x42, 0x41,
        0x54, 0x53, 0x52, 0x51,
        0x64, 0x63, 0x62, 0x61,
        0x74, 0x73, 0x72, 0x71,
        0x84, 0x83, 0x82, 0x81,
        0x05, 0x00, 0x06, 0x02, 0x07
    }));
}

int main(int /*argc*/, char** /*argv*/)
{
    TestListInventoryRequest();
    TestVendorList();
    return g_fail ? 1 : 0;
}
