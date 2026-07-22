/**
 * Independent byte fixtures for the 5.4.8.18414 quest-sharing protocol.
 */

#include "Player.h"
#include "Opcodes.h"
#include "QuestDef.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(WorldPacket const& packet,
    std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
        return false;

    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;
    return true;
}

static void test_confirmation_prompt()
{
    WorldPacket packet;
    CHECK(MopQuestPackets::BuildQuestConfirmAccept(packet, 0x11223344u,
        "ABC", UINT64_C(0x0807060504030201)));
    CHECK(packet.GetOpcode() == SMSG_QUEST_CONFIRM_ACCEPT);
    CHECK(ExpectBytes(packet, {
        0xDF, 0x00, 0xE0,
        0x06,
        0x41, 0x42, 0x43,
        0x00, 0x07, 0x05, 0x03, 0x04, 0x02, 0x09,
        0x44, 0x33, 0x22, 0x11
    }));

    WorldPacket omitted;
    CHECK(MopQuestPackets::BuildQuestConfirmAccept(omitted, 0x11223344u,
        "", UINT64_C(0x0807060504030201)));
    CHECK(ExpectBytes(omitted, {
        0xFF, 0x80,
        0x06,
        0x00, 0x07, 0x05, 0x03, 0x04, 0x02, 0x09,
        0x44, 0x33, 0x22, 0x11
    }));

    WorldPacket oversized;
    CHECK(!MopQuestPackets::BuildQuestConfirmAccept(oversized, 1,
        std::string(1024, 'x'), 1));
    CHECK(oversized.empty());

    WorldPacket sparse;
    CHECK(MopQuestPackets::BuildQuestConfirmAccept(sparse, 0x11223344u,
        "Q", UINT64_C(0x00C30000B20000A1)));
    CHECK(ExpectBytes(sparse, {
        0xC4, 0x00, 0x40,
        0xC2,
        0x51,
        0xA0, 0xB3,
        0x44, 0x33, 0x22, 0x11
    }));
}

static void test_server_push_result()
{
    WorldPacket packet;
    MopQuestPackets::BuildQuestPushResult(packet,
        UINT64_C(0x0807060504030201), 13);
    CHECK(packet.GetOpcode() == SMSG_QUEST_PUSH_RESULT);
    CHECK(ExpectBytes(packet, {
        0xFF, 0x04, 0x0D,
        0x03, 0x07, 0x05, 0x09, 0x06, 0x02, 0x00
    }));

    WorldPacket sparse;
    MopQuestPackets::BuildQuestPushResult(sparse,
        UINT64_C(0x00C30000B20000A1), 3);
    CHECK(ExpectBytes(sparse, { 0xC2, 0x03, 0xB3, 0xC2, 0xA0 }));
}

static void test_client_push_result()
{
    uint8 const fixture[] = {
        0x44, 0x33, 0x22, 0x11,
        0x03,
        0xFF,
        0x03, 0x02, 0x00, 0x07, 0x06, 0x04, 0x09, 0x05
    };
    WorldPacket packet(CMSG_QUEST_PUSH_RESULT, sizeof(fixture));
    packet.append(fixture, sizeof(fixture));

    MopQuestPackets::QuestPushResult const result =
        MopQuestPackets::ReadQuestPushResult(packet);
    CHECK(result.questId == 0x11223344u);
    CHECK(result.result == 3);
    CHECK(result.sharerGuid == UINT64_C(0x0807060504030201));
    CHECK(packet.rpos() == packet.size());

    uint8 const sparseFixture[] = {
        0x44, 0x33, 0x22, 0x11,
        0x03,
        0x70,
        0xA0, 0xC2, 0xB3
    };
    WorldPacket sparse(CMSG_QUEST_PUSH_RESULT, sizeof(sparseFixture));
    sparse.append(sparseFixture, sizeof(sparseFixture));
    MopQuestPackets::QuestPushResult const sparseResult =
        MopQuestPackets::ReadQuestPushResult(sparse);
    CHECK(sparseResult.questId == 0x11223344u);
    CHECK(sparseResult.result == 3);
    CHECK(sparseResult.sharerGuid == UINT64_C(0x00C30000B20000A1));
    CHECK(sparse.rpos() == sparse.size());
}

static void test_opcode_values()
{
    CHECK(uint32(CMSG_PUSHQUESTTOPARTY) == 0x03D2u);
    CHECK(uint32(CMSG_QUEST_CONFIRM_ACCEPT) == 0x124Bu);
    CHECK(uint32(SMSG_QUEST_CONFIRM_ACCEPT) == 0x13C7u);
    CHECK(uint32(SMSG_QUEST_PUSH_RESULT) == 0x074Du);
    CHECK(uint32(CMSG_QUEST_PUSH_RESULT) == 0x1370u);
    CHECK(uint32(CMSG_QUEST_PUSH_RESULT) < uint32(OPCODE_TABLE_SIZE));
}

static void test_result_values()
{
    CHECK(QUEST_PARTY_MSG_SHARING_QUEST == 0);
    CHECK(QUEST_PARTY_MSG_CANT_TAKE_QUEST == 1);
    CHECK(QUEST_PARTY_MSG_ACCEPT_QUEST == 2);
    CHECK(QUEST_PARTY_MSG_DECLINE_QUEST == 3);
    CHECK(QUEST_PARTY_MSG_BUSY == 4);
    CHECK(QUEST_PARTY_MSG_DEAD == 5);
    CHECK(QUEST_PARTY_MSG_LOG_FULL == 6);
    CHECK(QUEST_PARTY_MSG_HAVE_QUEST == 7);
    CHECK(QUEST_PARTY_MSG_FINISH_QUEST == 8);
    CHECK(QUEST_PARTY_MSG_CANT_BE_SHARED_TODAY == 9);
    CHECK(QUEST_PARTY_MSG_SHARING_TIMER_EXPIRED == 10);
    CHECK(QUEST_PARTY_MSG_NOT_IN_PARTY == 11);
    CHECK(QUEST_PARTY_MSG_DIFFERENT_SERVER_DAILY == 12);
    CHECK(QUEST_PARTY_MSG_NOT_ALLOWED == 13);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_confirmation_prompt();
    test_server_push_result();
    test_client_push_result();
    test_opcode_values();
    test_result_values();
    return g_fail ? 1 : 0;
}
