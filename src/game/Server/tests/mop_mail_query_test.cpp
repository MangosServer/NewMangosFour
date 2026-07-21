/**
 * Independent byte fixtures for the 5.4.8.18414 next-mail-time result.
 */

#include "MopQueryPackets.h"
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

static void test_empty_result()
{
    WorldPacket packet(SMSG_MAIL_QUERY_NEXT_TIME_RESULT, 7);
    CHECK(MopQueryPackets::BuildMailQueryNextTimeResult(packet, {}, -1.0f));
    CHECK(Equal(packet, {
        0x00, 0x00, 0x00,
        0x00, 0x00, 0x80, 0xBF
    }));
}

static void test_sparse_sender_and_optional_realms()
{
    MopQueryPackets::MailNextTimeEntry entry;
    entry.senderGuid = 0x0066004400220010ULL;
    entry.nonPlayerSender = 0x11223344u;
    entry.messageType = 3;
    entry.deliveryTime = 1.0f;
    entry.hasNativeRealmAddress = true;
    entry.nativeRealmAddress = 0x10203040u;
    entry.stationery = 0x55667788u;
    entry.hasVirtualRealmAddress = true;
    entry.virtualRealmAddress = 0xAABBCCDDu;

    WorldPacket packet(SMSG_MAIL_QUERY_NEXT_TIME_RESULT, 32);
    CHECK(MopQueryPackets::BuildMailQueryNextTimeResult(packet, { entry }, 2.0f));
    CHECK(Equal(packet, {
        0x00, 0x00, 0x17, 0xB0,
        0x44, 0x33, 0x22, 0x11,
        0x45, 0x67,
        0x03,
        0x11,
        0x00, 0x00, 0x80, 0x3F,
        0x40, 0x30, 0x20, 0x10,
        0x88, 0x77, 0x66, 0x55,
        0x23,
        0xDD, 0xCC, 0xBB, 0xAA,
        0x00, 0x00, 0x00, 0x40
    }));
}

static void test_three_record_bound()
{
    MopQueryPackets::MailNextTimeEntry entry;
    std::vector<MopQueryPackets::MailNextTimeEntry> three(3, entry);
    WorldPacket accepted(SMSG_MAIL_QUERY_NEXT_TIME_RESULT, 0);
    CHECK(MopQueryPackets::BuildMailQueryNextTimeResult(accepted, three, 0.0f));

    std::vector<MopQueryPackets::MailNextTimeEntry> four(4, entry);
    WorldPacket rejected(SMSG_MAIL_QUERY_NEXT_TIME_RESULT, 1);
    rejected << uint8(0xA5);
    CHECK(!MopQueryPackets::BuildMailQueryNextTimeResult(rejected, four, 0.0f));
    CHECK(Equal(rejected, { 0xA5 }));
}

static void test_opcode_values()
{
    CHECK(uint32(CMSG_MAIL_QUERY_NEXT_TIME) == 0x077Bu);
    CHECK(uint32(SMSG_MAIL_QUERY_NEXT_TIME_RESULT) == 0x089Bu);
    CHECK(uint32(CMSG_MAIL_QUERY_NEXT_TIME) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_MAIL_QUERY_NEXT_TIME_RESULT) < uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_empty_result();
    test_sparse_sender_and_optional_realms();
    test_three_record_bound();
    test_opcode_values();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_mail_query: all checks passed\n");
    return 0;
}
