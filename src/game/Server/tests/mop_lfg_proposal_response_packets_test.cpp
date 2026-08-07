/**
 * Byte-exact coverage for the CMSG_LFG_PROPOSAL_RESPONSE (0x1D9D) reader.
 *
 * The body below is REAL captured client bytes at build 18414, not an inverse of our
 * own writer.
 *
 * Corpus catalogueGenerationId
 *   2BE10C899585BAECD237705AC13BBF9262D81B6BDC085B462808C6869CE88752
 *
 * Layout derived from the client's body writer sub_66A29E, reached as vtable slot 1
 * behind the opcode thunk sub_6622E8 (which writes 7581). GUID A is at this+24..31 and
 * GUID B at this+48..55:
 *
 *   uint32 proposalId, clientQueueId, flags, joinTime
 *   bits   accept, then the mask A6 A0 A2 A4 B6 B7 A3 B4 A7 B1 A5 B0 A1 B2 B3 B5
 *   Flush
 *   bytes  A3 A6 A4 A1 B7 B0 A7 B6 A5 B3 B1 B5 B4 A0 A2 B2, each XOR 1 when present
 *
 * What makes this case worth having is that it closes a loop. capture-000059 seq
 * 2063770 is the client's answer to seq 2063424 in the SAME capture -- the 156-byte
 * SMSG_LFG_PROPOSAL_UPDATE covered by mop_lfg_proposal_packets_test. Every echoed field
 * matches what that packet carried: proposal 11132, queue 37743, flags 3, join time
 * 1409232359, and both GUIDs. The inbound and outbound derivations were done separately
 * and agree, which neither could establish alone.
 */

#include "Group.h"
#include "WorldPacket.h"

#include "Database/DatabaseEnv.h"
#include <cstdio>
#include <vector>

// Linker stubs. The server defines these in mangosd; a test binary that reaches any
// game.lib translation unit needs them. This test did not need them before only
// because its checks lived inside assert(), which is not compiled under NDEBUG --
// so it never actually referenced the code under test.
DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace
{
    WorldPacket MakeBody(std::vector<uint8> const& bytes)
    {
        WorldPacket packet(CMSG_LFG_PROPOSAL_RESPONSE, bytes.size());
        packet.append(bytes.data(), bytes.size());
        return packet;
    }

    /// capture-000059 seq 2063770, 29 bytes: an ACCEPT of the 25-man raid proposal.
    void test_accept_body()
    {
        std::vector<uint8> const body = {
            0x7C, 0x2B, 0x00, 0x00, 0x6F, 0x93, 0x00, 0x00, 0x03, 0x00,
            0x00, 0x00, 0xE7, 0x2D, 0xFF, 0x53, 0xB7, 0x67, 0x00, 0x04,
            0x4D, 0x1E, 0x05, 0x45, 0x10, 0xF3, 0xD5, 0xFF, 0x4D
        };

        WorldPacket packet = MakeBody(body);
        MopLfgProposalResponsePackets::Request request;
        CHECK(MopLfgProposalResponsePackets::ParseRequest(packet, request));

        CHECK(request.accepted);
        CHECK(request.proposalId == 11132);
        CHECK(request.clientQueueId == 37743);
        CHECK(request.flags == 3);
        CHECK(request.joinTime == 1409232359u);

        // Both GUIDs match the SMSG_LFG_PROPOSAL_UPDATE this is answering.
        CHECK(request.guidA.GetRawValue() == 0x0400000005FE4CD4ULL);
        CHECK(request.guidB.GetRawValue() == 0x1F440000114CF200ULL);

        CHECK(packet.rpos() == packet.size());     // no tail left unread
    }

    /// A body truncated inside its GUID run must be refused, not read past its end.
    void test_truncated_body_is_refused()
    {
        std::vector<uint8> const body = {
            0x7C, 0x2B, 0x00, 0x00, 0x6F, 0x93, 0x00, 0x00, 0x03, 0x00,
            0x00, 0x00, 0xE7, 0x2D, 0xFF, 0x53, 0xB7, 0x67, 0x00, 0x04
        };

        WorldPacket packet = MakeBody(body);
        MopLfgProposalResponsePackets::Request request;
        CHECK(!MopLfgProposalResponsePackets::ParseRequest(packet, request));
    }

    /// Shorter than the fixed header plus its mask bytes.
    void test_short_body_is_refused()
    {
        std::vector<uint8> const body = { 0x7C, 0x2B, 0x00, 0x00 };

        WorldPacket packet = MakeBody(body);
        MopLfgProposalResponsePackets::Request request;
        CHECK(!MopLfgProposalResponsePackets::ParseRequest(packet, request));
    }
}

int main()
{
    test_accept_body();
    test_truncated_body_is_refused();
    test_short_body_is_refused();
    std::printf(g_fail ? "mop_lfg_proposal_response_packets_test: FAILED (%d)\n" : "mop_lfg_proposal_response_packets_test: OK\n", g_fail);
    return g_fail ? 1 : 0;
}
