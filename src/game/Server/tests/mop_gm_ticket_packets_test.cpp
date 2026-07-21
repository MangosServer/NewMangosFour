/**
 * Independent byte fixtures for the 5.4.8.18414 GM-ticket update response.
 */

#include "GMTicketMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_response_values()
{
    GMTicketResponse const responses[] = {
        GMTICKET_RESPONSE_ALREADY_EXIST,
        GMTICKET_RESPONSE_CREATE_SUCCESS,
        GMTICKET_RESPONSE_CREATE_ERROR,
        GMTICKET_RESPONSE_UPDATE_SUCCESS,
        GMTICKET_RESPONSE_UPDATE_ERROR,
        GMTICKET_RESPONSE_TICKET_DELETED
    };
    uint8 const expected[] = { 1, 2, 3, 4, 5, 9 };

    for (size_t i = 0; i < sizeof(responses) / sizeof(responses[0]); ++i)
    {
        WorldPacket packet;
        MopGMTicketPackets::BuildUpdate(packet, responses[i]);
        CHECK(packet.GetOpcode() == SMSG_GM_TICKET_UPDATE);
        CHECK(packet.size() == 1);
        CHECK(packet.contents()[0] == expected[i]);
    }
}

static void test_opcode_is_framable()
{
    CHECK(uint32(SMSG_GM_TICKET_UPDATE) == 0x02A6u);
    CHECK(uint32(SMSG_GM_TICKET_UPDATE) < uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_response_values();
    test_opcode_is_framable();
    return g_fail ? 1 : 0;
}
