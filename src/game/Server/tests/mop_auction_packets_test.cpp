/**
 * Independent byte fixtures for the 5.4.8.18414 auction hello and
 * expired/removed notification exchange.
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

static void test_expired_or_removed_notification()
{
    MopAuctionPackets::ExpiredOrRemoved notification = {};
    notification.randomPropertyId = -123456;
    notification.auctionId = 0x11223344;
    notification.itemEntry = 0x55667788;

    WorldPacket removed;
    MopAuctionPackets::BuildExpiredOrRemovedNotification(removed, notification);
    CHECK(removed.GetOpcode() == SMSG_AUCTION_OWNER_NOTIFICATION);
    CHECK(Equal(removed, {
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
    notification.expired = true;

    WorldPacket expired;
    MopAuctionPackets::BuildExpiredOrRemovedNotification(expired, notification);
    CHECK(Equal(expired, {
        0xC0, 0x1D, 0xFE, 0xFF,
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0xDD, 0xCC, 0xBB, 0xAA,
        0x00, 0x00, 0x61, 0x45,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
        0x80
    }));
}

// Including the auction owner pulls ACE through World.h; ACE rewrites main to
// ace_main_i and requires the conventional argc/argv signature.
int main(int /*argc*/, char** /*argv*/)
{
    test_hello_request();
    test_hello_response();
    test_expired_or_removed_notification();
    return g_fail == 0 ? 0 : 1;
}
