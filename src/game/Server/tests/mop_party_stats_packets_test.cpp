/**
 * Independent byte fixtures for the 5.4.8.18414 party-member statistics
 * request and shared response envelope.
 */

#include "MopPartyStatsPackets.h"
#include "MopWireCodec.h"
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

static ByteBuffer Payload(std::vector<uint8> const& bytes)
{
    ByteBuffer payload;
    if (!bytes.empty())
        payload.append(bytes.data(), bytes.size());
    return payload;
}

static void test_requests()
{
    {
        WorldPacket packet(CMSG_REQUEST_PARTY_MEMBER_STATS, 2);
        Append(packet, { 0x00, 0x00 });
        MopPartyStatsPackets::Request const request = MopPartyStatsPackets::ReadRequest(packet);
        CHECK(request.mode == 0x00);
        CHECK(request.memberGuid == 0);
    }
    {
        WorldPacket packet(CMSG_REQUEST_PARTY_MEMBER_STATS, 10);
        Append(packet, {
            0x01, 0xFF,
            0x88, 0x22, 0x44, 0xAA, 0xCC, 0x66, 0xEE, 0x00
        });
        MopPartyStatsPackets::Request const request = MopPartyStatsPackets::ReadRequest(packet);
        CHECK(request.mode == 0x01);
        CHECK(request.memberGuid == 0x0123456789ABCDEFull);
    }
    {
        WorldPacket packet(CMSG_REQUEST_PARTY_MEMBER_STATS, 2);
        Append(packet, { 0x7F, 0x00 });
        MopPartyStatsPackets::Request const request = MopPartyStatsPackets::ReadRequest(packet);
        CHECK(request.mode == 0x7F);
        CHECK(request.memberGuid == 0);
    }
}

static void test_response_zero_guid_controls()
{
    {
        WorldPacket packet;
        ByteBuffer payload = Payload({ 0x00, 0x00 });
        MopPartyStatsPackets::BuildResponse(packet, 0, 0x00000001u,
            false, false, payload);
        CHECK(packet.GetOpcode() == SMSG_PARTY_MEMBER_STATS);
        CHECK(Equal(packet, {
            0x00, 0x00,
            0x01, 0x00, 0x00, 0x00,
            0x02, 0x00, 0x00, 0x00,
            0x00, 0x00
        }));
    }
    {
        WorldPacket packet;
        ByteBuffer payload;
        MopPartyStatsPackets::BuildResponse(packet, 0, 0,
            true, true, payload);
        CHECK(Equal(packet, {
            0x24, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        }));
    }
}

static void test_response_nontrivial_guid_controls()
{
    {
        WorldPacket packet;
        ByteBuffer payload;
        MopPartyStatsPackets::BuildResponse(packet, 0x0123456789ABCDEFull, 0,
            false, false, payload);
        CHECK(Equal(packet, {
            0xDB, 0xC0,
            0x88, 0xAA, 0x22, 0x00, 0x44,
            0x00, 0x00, 0x00, 0x00,
            0xCC, 0x66, 0xEE,
            0x00, 0x00, 0x00, 0x00
        }));
    }
    {
        WorldPacket packet;
        ByteBuffer payload = Payload({ 0xDE, 0xAD, 0xBE, 0xEF });
        MopPartyStatsPackets::BuildResponse(packet, 0x0123456789ABCDEFull,
            0x01020304u, true, true, payload);
        CHECK(Equal(packet, {
            0xFF, 0xC0,
            0x88, 0xAA, 0x22, 0x00, 0x44,
            0x04, 0x03, 0x02, 0x01,
            0xCC, 0x66, 0xEE,
            0x04, 0x00, 0x00, 0x00,
            0xDE, 0xAD, 0xBE, 0xEF
        }));
    }
}

static void test_postcrypt_header()
{
    CHECK(uint32(CMSG_REQUEST_PARTY_MEMBER_STATS) == 0x0806u);
    CHECK(uint32(SMSG_PARTY_MEMBER_STATS) == 0x0A9Au);

    uint8 header[4] = {};
    CHECK(MopWire::BuildServerHeader(true, 12, SMSG_PARTY_MEMBER_STATS, header));
    CHECK(header[0] == 0x9A && header[1] == 0x8A && header[2] == 0x01 && header[3] == 0x00);
    CHECK(MopWire::BuildServerHeader(true, 22, SMSG_PARTY_MEMBER_STATS, header));
    CHECK(header[0] == 0x9A && header[1] == 0xCA && header[2] == 0x02 && header[3] == 0x00);
}

int main(int, char**)
{
    test_requests();
    test_response_zero_guid_controls();
    test_response_nontrivial_guid_controls();
    test_postcrypt_header();
    if (g_fail)
        return 1;
    std::printf("mop_party_stats_packets: all checks passed\n");
    return 0;
}
