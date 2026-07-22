/**
 * Byte-exact inbound fixtures for the consolidated live-log opcode worklist.
 */

#include "Channel.h"
#include "WorldSession.h"

#include <cstdint>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_hotfix_request()
{
    ObjectGuid first(0x0800000500000001ull);
    ObjectGuid second(0x0007060004000200ull);
    WorldPacket packet(CMSG_REQUEST_HOTFIX, 40);
    packet << uint32(0x11223344u);
    packet.WriteBits(2, 21);
    packet.WriteGuidMask<6, 3, 0, 1, 4, 5, 7, 2>(first);
    packet.WriteGuidMask<6, 3, 0, 1, 4, 5, 7, 2>(second);
    packet.FlushBits();
    packet.WriteGuidBytes<1>(first);
    packet << uint32(0xA1B2C3D4u);
    packet.WriteGuidBytes<0, 5, 6, 4, 7, 2, 3>(first);
    packet.WriteGuidBytes<1>(second);
    packet << uint32(0x55667788u);
    packet.WriteGuidBytes<0, 5, 6, 4, 7, 2, 3>(second);

    MopHotfixPackets::HotfixRequest request;
    CHECK(MopHotfixPackets::ReadHotfixRequest(packet, request));
    CHECK(request.type == 0x11223344u);
    CHECK(request.records.size() == 2);
    CHECK(request.records[0].guid == first.GetRawValue());
    CHECK(request.records[0].entry == 0xA1B2C3D4u);
    CHECK(request.records[1].guid == second.GetRawValue());
    CHECK(request.records[1].entry == 0x55667788u);
    CHECK(packet.rpos() == packet.size());

    WorldPacket malformed(CMSG_REQUEST_HOTFIX, 7);
    malformed << uint32(0x11223344u);
    malformed.WriteBits(100, 21);
    malformed.FlushBits();
    MopHotfixPackets::HotfixRequest rejected;
    rejected.records.push_back({ 1, 2 });
    CHECK(!MopHotfixPackets::ReadHotfixRequest(malformed, rejected));
    CHECK(rejected.records.empty());
}

static void test_hotfix_reply()
{
    ByteBuffer record;
    record << uint8(0xAA) << uint8(0xBB) << uint8(0xCC);

    WorldPacket packet(SMSG_DB_REPLY, 19);
    MopHotfixPackets::BuildDbReply(packet, 0x11223344u, 0x55667788u, 0x99AABBCCu, record);

    uint32 entry = 0;
    uint32 hotfixDate = 0;
    uint32 type = 0;
    uint32 size = 0;
    packet >> entry >> hotfixDate >> type >> size;
    CHECK(entry == 0x11223344u);
    CHECK(hotfixDate == 0x55667788u);
    CHECK(type == 0x99AABBCCu);
    CHECK(size == 3u);
    CHECK(packet.read<uint8>() == 0xAAu);
    CHECK(packet.read<uint8>() == 0xBBu);
    CHECK(packet.read<uint8>() == 0xCCu);
    CHECK(packet.rpos() == packet.size());
}

static void test_join_channel_request()
{
    std::string const channel = "General";
    std::string const password = "pw";
    WorldPacket packet(CMSG_JOIN_CHANNEL, 20);
    packet << uint32(0x11223344u);
    packet.WriteBit(true);
    packet.WriteBits(uint32(channel.size()), 7);
    packet.WriteBits(uint32(password.size()), 7);
    packet.WriteBit(false);
    packet.FlushBits();
    packet.WriteStringData(password);
    packet.WriteStringData(channel);

    MopChannelPackets::JoinChannelRequest request;
    CHECK(MopChannelPackets::ReadJoinChannelRequest(packet, request));
    CHECK(request.channelId == 0x11223344u);
    CHECK(request.hasVoice);
    CHECK(!request.zoneUpdate);
    CHECK(request.channelName == channel);
    CHECK(request.password == password);
    CHECK(packet.rpos() == packet.size());

    WorldPacket truncated(CMSG_JOIN_CHANNEL, 7);
    truncated << uint32(0x11223344u);
    truncated.WriteBit(false);
    truncated.WriteBits(5, 7);
    truncated.WriteBits(5, 7);
    truncated.WriteBit(false);
    truncated.FlushBits();
    MopChannelPackets::JoinChannelRequest rejected;
    CHECK(!MopChannelPackets::ReadJoinChannelRequest(truncated, rejected));
}

static void test_load_screen_request()
{
    WorldPacket packet(CMSG_LOAD_SCREEN, 5);
    packet << uint32(0x11223344u);
    packet.WriteBit(true);
    packet.FlushBits();

    MopClientRequestPackets::LoadScreenRequest request =
        MopClientRequestPackets::ReadLoadScreenRequest(packet);
    CHECK(request.mapId == 0x11223344u);
    CHECK(request.loading);
    CHECK(packet.rpos() == packet.size());
}

static void test_opcode_values()
{
    CHECK(uint32(CMSG_REQUEST_HOTFIX) == 0x158Du);
    CHECK(uint32(CMSG_JOIN_CHANNEL) == 0x148Eu);
    CHECK(uint32(CMSG_CANCEL_TRADE) == 0x1941u);
    CHECK(uint32(CMSG_UI_TIME_REQUEST) == 0x15ABu);
    CHECK(uint32(CMSG_LOAD_SCREEN) == 0x1DBDu);
    CHECK(uint32(SMSG_UI_TIME) == 0x0027u);
    CHECK(uint32(SMSG_DB_REPLY) == 0x103Bu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_hotfix_request();
    test_hotfix_reply();
    test_join_channel_request();
    test_load_screen_request();
    test_opcode_values();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    std::printf("mop_client_opcode_worklist: all checks passed\n");
    return 0;
}
