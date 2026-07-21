/**
 * Byte-exact test for the 5.4.8.18414 realm-name-query serializer
 * (MopQueryPackets::BuildRealmNameQueryResponse / ReadRealmNameQueryRequest).
 *
 * The wire layout is live-confirmed against the retail 18414 client: the client
 * fires CMSG_REALM_NAME_QUERY (body = one uint32 realm id) from its name-cache
 * path, and its handler reads the STATUS byte FIRST, then the realm id (Variant A).
 * With the realm id leading, the client read the id's low byte as the status and
 * skipped the RealmCache store, hanging the queried player name at "Unknown". The
 * expected bytes below are the exact response the client accepted.
 */

#include "MopQueryPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(WorldPacket const& packet, std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        return false;
    }
    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (packet.contents()[i] != expected[i])
        {
            return false;
        }
    }
    return true;
}

// ACE (dragged in via 'game') rewrites main() and requires the (int, char**) signature.
int main(int /*argc*/, char** /*argv*/)
{
    // Found + home realm: status(0), realmId(5), name/normalizedName "four".
    // Byte-for-byte identical to the live response the 18414 client accepted.
    {
        MopQueryPackets::RealmNameQueryResponse record;
        record.realmId = 5;
        record.status = 0;
        record.isHomeRealm = true;
        record.name = "four";
        record.normalizedName = "four";

        WorldPacket out(SMSG_REALM_NAME_QUERY_RESPONSE, 32);
        MopQueryPackets::BuildRealmNameQueryResponse(out, record);

        // [status=00][realmId LE=05 00 00 00]
        // bit block: WriteBits(nameLen=4, 8)=0x04, WriteBit(isHome=1), WriteBits(normLen=4, 8)
        //   MSB-first -> 04 82 00
        // "four" then "four", no NUL.
        std::vector<uint8_t> const expected = {
            0x00,
            0x05, 0x00, 0x00, 0x00,
            0x04, 0x82, 0x00,
            'f', 'o', 'u', 'r',
            'f', 'o', 'u', 'r'
        };
        CHECK(out.GetOpcode() == SMSG_REALM_NAME_QUERY_RESPONSE);
        CHECK(ExpectBytes(out, expected));
    }

    // Not found: a nonzero status ends the body after the two scalars (no name tail).
    {
        MopQueryPackets::RealmNameQueryResponse record;
        record.realmId = 9;
        record.status = 1;
        record.name = "ignored";           // must not be emitted when status != 0
        record.normalizedName = "ignored";

        WorldPacket out(SMSG_REALM_NAME_QUERY_RESPONSE, 8);
        MopQueryPackets::BuildRealmNameQueryResponse(out, record);

        std::vector<uint8_t> const expected = { 0x01, 0x09, 0x00, 0x00, 0x00 };
        CHECK(ExpectBytes(out, expected));
    }

    // Request body is a single little-endian uint32 realm id.
    {
        WorldPacket in(CMSG_REALM_NAME_QUERY, 4);
        in << uint32(5);
        CHECK(MopQueryPackets::ReadRealmNameQueryRequest(in) == 5u);
    }

    if (g_fail == 0)
    {
        std::printf("ALL PASS\n");
        return 0;
    }
    std::printf("%d FAILURES\n", g_fail);
    return 1;
}
