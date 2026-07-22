/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * Independent byte fixtures for the 5.4.8.18414 auction hello and
 * owner, bidder, and bid-state notification exchange.
 */

#include "AuctionHouseMgr.h"
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

static void Append(WorldPacket& packet, std::vector<uint8> const& bytes)
{
    if (!bytes.empty())
        packet.append(bytes.data(), bytes.size());
}

static void test_hello_request()
{
    WorldPacket full(CMSG_AUCTION_HELLO, 9);
    Append(full, { 0xFF, 0xAA, 0x00, 0xCC, 0x88, 0x44, 0xEE, 0x66, 0x22 });
    CHECK(MopAuctionPackets::ReadHelloRequest(full) == 0x0123456789ABCDEFull);

    WorldPacket zero(CMSG_AUCTION_HELLO, 1);
    Append(zero, { 0x00 });
    CHECK(MopAuctionPackets::ReadHelloRequest(zero) == 0);

    WorldPacket sparse(CMSG_AUCTION_HELLO, 3);
    Append(sparse, { 0x82, 0x11, 0x21 });
    CHECK(MopAuctionPackets::ReadHelloRequest(sparse) == 0x0000002000001000ull);
}

static void test_hello_response()
{
    WorldPacket full;
    MopAuctionPackets::BuildHello(full, 0x0123456789ABCDEFull, 0x12345678, true);
    CHECK(full.GetOpcode() == SMSG_AUCTION_HELLO);
    CHECK(Equal(full, {
        0xFF, 0x80,
        0x88,
        0x78, 0x56, 0x34, 0x12,
        0x66, 0xAA, 0x00, 0xCC, 0xEE, 0x22, 0x44
    }));

    WorldPacket zero;
    MopAuctionPackets::BuildHello(zero, 0, 0, false);
    CHECK(Equal(zero, {
        0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }));

    WorldPacket sparse;
    MopAuctionPackets::BuildHello(sparse, 0x0000200010000000ull, 7, true);
    CHECK(Equal(sparse, {
        0x32, 0x00,
        0x11,
        0x07, 0x00, 0x00, 0x00,
        0x21
    }));
}

static void test_command_result()
{
    MopAuctionPackets::CommandResult minimal = {};
    WorldPacket minimalPacket;
    MopAuctionPackets::BuildCommandResult(minimalPacket, minimal);
    CHECK(minimalPacket.GetOpcode() == SMSG_AUCTION_COMMAND_RESULT);
    CHECK(Equal(minimalPacket, {
        0xE0, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }));

    MopAuctionPackets::CommandResult full = {};
    full.bidderGuid = 0x0123456789ABCDEFull;
    full.minimumIncrement = 0x1112131415161718ull;
    full.errorCode = 0x11223344;
    full.action = 0x55667788;
    full.auctionId = 0x10203040;
    full.bid = 0x0102030405060708ull;
    full.inventoryError = 0xAABBCCDD;
    full.hasMinimumIncrement = true;
    full.hasBid = true;

    WorldPacket fullPacket;
    MopAuctionPackets::BuildCommandResult(fullPacket, full);
    CHECK(Equal(fullPacket, {
        0x3F, 0xE0,
        0x88, 0xEE, 0x00, 0xCC, 0x66, 0x22, 0x44, 0xAA,
        0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0x40, 0x30, 0x20, 0x10,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0xDD, 0xCC, 0xBB, 0xAA
    }));
}

static void test_sold_or_expired_owner_notification()
{
    MopAuctionPackets::SoldOrExpired notification = {};
    notification.randomPropertyId = -123456;
    notification.auctionId = 0x11223344;
    notification.itemEntry = 0x55667788;

    WorldPacket expired;
    MopAuctionPackets::BuildSoldOrExpiredNotification(expired, notification);
    CHECK(expired.GetOpcode() == SMSG_AUCTION_OWNER_NOTIFICATION);
    CHECK(Equal(expired, {
        0xC0, 0x1D, 0xFE, 0xFF,
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00
    }));

    notification.itemNameOverrideId = 0xAABBCCDD;
    notification.delay = 3600.0f;
    notification.amount = 0x0102030405060708ull;
    notification.sold = true;

    WorldPacket sold;
    MopAuctionPackets::BuildSoldOrExpiredNotification(sold, notification);
    CHECK(Equal(sold, {
        0xC0, 0x1D, 0xFE, 0xFF,
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0xDD, 0xCC, 0xBB, 0xAA,
        0x00, 0x00, 0x61, 0x45,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x80
    }));
}

static void test_won_notification()
{
    MopAuctionPackets::Won notification = {};
    notification.contextGuid = 0x0123456789ABCDEFull;
    notification.itemEntry = 0x11223344;
    notification.context = 0x55667788;
    notification.randomPropertyId = -123456;
    notification.itemNameOverrideId = 0xAABBCCDD;
    notification.auctionId = 0x10203040;

    WorldPacket packet;
    MopAuctionPackets::BuildWonNotification(packet, notification);
    CHECK(packet.GetOpcode() == SMSG_AUCTION_WON_NOTIFICATION);
    CHECK(Equal(packet, {
        0xFF,
        0x00, 0x88,
        0x44, 0x33, 0x22, 0x11,
        0xCC, 0xAA,
        0x88, 0x77, 0x66, 0x55,
        0xEE,
        0xC0, 0x1D, 0xFE, 0xFF,
        0x44, 0x66, 0x22,
        0xDD, 0xCC, 0xBB, 0xAA,
        0x40, 0x30, 0x20, 0x10
    }));

    MopAuctionPackets::Won sparse = {};
    sparse.contextGuid = 0x0000100000000020ull;
    WorldPacket sparsePacket;
    MopAuctionPackets::BuildWonNotification(sparsePacket, sparse);
    CHECK(Equal(sparsePacket, {
        0x88,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x21,
        0x00, 0x00, 0x00, 0x00,
        0x11,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }));
}

static void test_outbid_notification()
{
    MopAuctionPackets::Outbid notification = {};
    notification.auctionId = 0x10203040;
    notification.auctionHouseId = 0x11223344;
    notification.itemNameOverrideId = 0xAABBCCDD;
    notification.bid = 0x0102030405060708ull;
    notification.itemEntry = 0x55667788;
    notification.randomPropertyId = -123456;
    notification.minimumIncrement = 0x1112131415161718ull;
    notification.bidderGuid = 0x0123456789ABCDEFull;

    WorldPacket packet;
    MopAuctionPackets::BuildOutbidNotification(packet, notification);
    CHECK(packet.GetOpcode() == SMSG_AUCTION_OUTBID_NOTIFICATION);
    CHECK(Equal(packet, {
        0x40, 0x30, 0x20, 0x10,
        0x44, 0x33, 0x22, 0x11,
        0xDD, 0xCC, 0xBB, 0xAA,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x88, 0x77, 0x66, 0x55,
        0xC0, 0x1D, 0xFE, 0xFF,
        0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
        0xFF,
        0x66, 0x00, 0x88, 0xEE, 0xCC, 0x22, 0xAA, 0x44
    }));

    MopAuctionPackets::Outbid sparse = {};
    sparse.bidderGuid = 0x2000000000100000ull;
    WorldPacket sparsePacket;
    MopAuctionPackets::BuildOutbidNotification(sparsePacket, sparse);
    CHECK(Equal(sparsePacket, {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x81, 0x21, 0x11
    }));
}

static void test_bid_update_notification()
{
    MopAuctionPackets::BidUpdate notification = {};
    notification.bid = 0x0102030405060708ull;
    notification.context52 = 0x11223344;
    notification.context48 = 0x55667788;
    notification.context44 = 0x99AABBCC;
    notification.auctionId = 0x10203040;
    notification.minimumIncrement = 0x1112131415161718ull;
    notification.bidderGuid = 0x0123456789ABCDEFull;

    WorldPacket packet;
    MopAuctionPackets::BuildBidUpdateNotification(packet, notification);
    CHECK(packet.GetOpcode() == SMSG_AUCTION_BID_UPDATE_NOTIFICATION);
    CHECK(Equal(packet, {
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99,
        0x40, 0x30, 0x20, 0x10,
        0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
        0xFF,
        0x44, 0xCC, 0xEE, 0x00, 0xAA, 0x22, 0x88, 0x66
    }));

    MopAuctionPackets::BidUpdate sparse = {};
    sparse.bidderGuid = 0x0020000010000000ull;
    WorldPacket sparsePacket;
    MopAuctionPackets::BuildBidUpdateNotification(sparsePacket, sparse);
    CHECK(Equal(sparsePacket, {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x81, 0x21, 0x11
    }));
}

// Including the auction owner pulls ACE through World.h; ACE rewrites main to
// ace_main_i and requires the conventional argc/argv signature.
int main(int /*argc*/, char** /*argv*/)
{
    test_hello_request();
    test_hello_response();
    test_command_result();
    test_sold_or_expired_owner_notification();
    test_won_notification();
    test_outbid_notification();
    test_bid_update_notification();
    return g_fail == 0 ? 0 : 1;
}
