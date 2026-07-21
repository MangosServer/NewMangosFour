/**
 * Independent byte fixtures for the 5.4.8.18414 ready-check exchange.
 */

#include "MopReadyCheckPackets.h"
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

static void test_start_request()
{
    WorldPacket home(CMSG_DO_READY_CHECK, 1);
    Append(home, { 0x00 });
    CHECK(MopReadyCheckPackets::ReadStartRequest(home) == 0);

    WorldPacket instance(CMSG_DO_READY_CHECK, 1);
    Append(instance, { 0x01 });
    CHECK(MopReadyCheckPackets::ReadStartRequest(instance) == 1);
}

static void test_response_request()
{
    WorldPacket ready(CMSG_RAID_READY_CHECK_CONFIRM, 11);
    Append(ready, {
        0x01, 0xFF, 0x80,
        0xCC, 0xEE, 0x88, 0xAA, 0x66, 0x44, 0x00, 0x22
    });
    MopReadyCheckPackets::ResponseRequest decoded =
        MopReadyCheckPackets::ReadResponseRequest(ready);
    CHECK(decoded.partyIndex == 1);
    CHECK(decoded.reservedGuid == 0x0123456789ABCDEFull);
    CHECK(decoded.ready);

    WorldPacket notReady(CMSG_RAID_READY_CHECK_CONFIRM, 3);
    Append(notReady, { 0x00, 0x00, 0x00 });
    decoded = MopReadyCheckPackets::ReadResponseRequest(notReady);
    CHECK(decoded.partyIndex == 0);
    CHECK(decoded.reservedGuid == 0);
    CHECK(!decoded.ready);

    WorldPacket zeroGuidReady(CMSG_RAID_READY_CHECK_CONFIRM, 3);
    Append(zeroGuidReady, { 0x00, 0x04, 0x00 });
    decoded = MopReadyCheckPackets::ReadResponseRequest(zeroGuidReady);
    CHECK(decoded.reservedGuid == 0);
    CHECK(decoded.ready);
}

static void test_started_response()
{
    WorldPacket packet;
    MopReadyCheckPackets::BuildStarted(packet,
        0x0123456789ABCDEFull, 0x1020304050607080ull, 35000, 0);
    CHECK(packet.GetOpcode() == SMSG_RAID_READY_CHECK);
    CHECK(Equal(packet, {
        0xFF, 0xFF,
        0xB8, 0x88, 0x00, 0x00,
        0xAA, 0x00, 0x88, 0x41, 0xCC, 0xEE,
        0x71, 0x61, 0x21, 0x31, 0x22, 0x81,
        0x00,
        0x11, 0x66, 0x51, 0x44
    }));

    WorldPacket empty;
    MopReadyCheckPackets::BuildStarted(empty, 0, 0, 35000, 1);
    CHECK(Equal(empty, { 0x00, 0x00, 0xB8, 0x88, 0x00, 0x00, 0x01 }));

    WorldPacket sparse;
    MopReadyCheckPackets::BuildStarted(sparse,
        0x0000004400000000ull, 0x7700000000000010ull, 0x01020304, 1);
    CHECK(Equal(sparse, {
        0x80, 0x12, 0x04, 0x03, 0x02, 0x01,
        0x11, 0x01, 0x76, 0x45
    }));
}

static void test_member_response()
{
    WorldPacket ready;
    MopReadyCheckPackets::BuildResponse(ready,
        0x0123456789ABCDEFull, 0x1020304050607080ull, true);
    CHECK(ready.GetOpcode() == SMSG_RAID_READY_CHECK_CONFIRM);
    CHECK(Equal(ready, {
        0xFF, 0xFF, 0x80,
        0x41, 0x61, 0x71, 0x66, 0xAA, 0x81, 0x44, 0x88,
        0x11, 0x22, 0xCC, 0x21, 0x51, 0x31, 0xEE, 0x00
    }));

    WorldPacket notReady;
    MopReadyCheckPackets::BuildResponse(notReady,
        0x0123456789ABCDEFull, 0x1020304050607080ull, false);
    CHECK(notReady.size() == ready.size());
    CHECK(notReady.contents()[0] == 0xEF);
    CHECK(std::memcmp(notReady.contents() + 1, ready.contents() + 1,
        ready.size() - 1) == 0);

    WorldPacket zero;
    MopReadyCheckPackets::BuildResponse(zero, 0, 0, true);
    CHECK(Equal(zero, { 0x10, 0x00, 0x00 }));

    WorldPacket sparse;
    MopReadyCheckPackets::BuildResponse(sparse,
        0x7700000000220000ull, 0x0000550000000010ull, true);
    CHECK(Equal(sparse, {
        0x59, 0x00, 0x80, 0x23, 0x11, 0x54, 0x76
    }));
}

static void test_completed_response()
{
    WorldPacket packet;
    MopReadyCheckPackets::BuildCompleted(packet,
        0x0123456789ABCDEFull, 0);
    CHECK(packet.GetOpcode() == SMSG_RAID_READY_CHECK_COMPLETED);
    CHECK(Equal(packet, {
        0xFF,
        0x22, 0xEE, 0x88, 0xCC, 0x44,
        0x00,
        0x00, 0xAA, 0x66
    }));

    WorldPacket zero;
    MopReadyCheckPackets::BuildCompleted(zero, 0, 1);
    CHECK(Equal(zero, { 0x00, 0x01 }));

    WorldPacket sparse;
    MopReadyCheckPackets::BuildCompleted(sparse,
        0x0066004400001100ull, 1);
    CHECK(Equal(sparse, { 0x89, 0x67, 0x10, 0x01, 0x45 }));
}

int main(int, char**)
{
    test_start_request();
    test_response_request();
    test_started_response();
    test_member_response();
    test_completed_response();
    if (g_fail)
        return 1;
    std::printf("mop_ready_check_packets: all checks passed\n");
    return 0;
}
