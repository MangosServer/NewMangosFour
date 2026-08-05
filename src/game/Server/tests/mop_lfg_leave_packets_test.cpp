/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Byte-exact fixtures for the 5.4.8.18414 dungeon-finder queue cancel,
 * CMSG_LFG_LEAVE 0x01E0. All four success cases are REAL CAPTURED BODIES.
 */

#include "Group.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void Feed(WorldPacket& packet, std::initializer_list<uint8> bytes)
{
    if (bytes.size())
    {
        packet.append(bytes.begin(), bytes.size());
    }
}

static void CheckSuccess(char const* what, std::initializer_list<uint8> bytes,
    uint32 type, uint32 flags, uint32 time, uint32 queueId, uint64 guid)
{
    WorldPacket packet(CMSG_LFG_LEAVE, bytes.size());
    Feed(packet, bytes);

    MopLfgLeavePackets::Request request;
    CHECK(MopLfgLeavePackets::ParseRequest(packet, request));
    CHECK(packet.rpos() == packet.size());
    CHECK(request.ticketType == type);
    CHECK(request.ticketFlags == flags);
    CHECK(request.ticketTime == time);
    CHECK(request.clientQueueId == queueId);
    if (request.ticketGuid.GetRawValue() != guid)
    {
        std::fprintf(stderr, "  %s: guid 0x%016llX expected 0x%016llX\n", what,
                     (unsigned long long)request.ticketGuid.GetRawValue(),
                     (unsigned long long)guid);
        ++g_fail;
    }
}

static void CheckFailure(char const* what, std::initializer_list<uint8> bytes)
{
    WorldPacket packet(CMSG_LFG_LEAVE, bytes.size());
    Feed(packet, bytes);

    MopLfgLeavePackets::Request request;
    if (MopLfgLeavePackets::ParseRequest(packet, request))
    {
        std::fprintf(stderr, "  %s: accepted a body it should have refused\n", what);
        ++g_fail;
    }
}

int main()
{
    // capture-000044 seq 47667, 22 bytes, a player ticket.
    CheckSuccess("22B player ticket",
        { 0x03,0x00,0x00,0x00, 0x08,0x00,0x00,0x00, 0x1E,0x64,0x14,0x54,
          0x85,0x9D,0x00,0x00, 0x79,0x28,0x90,0x07,0x63,0x05 },
        3, 8, 1410622494, 40325, 0x0400000006296291ULL);

    // capture-000476 seq 632673, 23 bytes, a group-shaped ticket.
    CheckSuccess("23B group ticket",
        { 0x03,0x00,0x00,0x00, 0x08,0x00,0x00,0x00, 0xAC,0xAC,0xEB,0x53,
          0xF6,0x12,0x00,0x00, 0xF9,0x7F,0xD2,0x55,0x13,0x45,0x1E },
        3, 8, 1407954092, 4854, 0x1F540000127E44D3ULL);

    // capture-000699 seq 307290, a different group ticket, same mask.
    CheckSuccess("23B other group",
        { 0x03,0x00,0x00,0x00, 0x08,0x00,0x00,0x00, 0x6A,0x3E,0xEA,0x53,
          0xB3,0x9D,0x00,0x00, 0xF9,0x7A,0xC9,0x55,0x13,0x2E,0x1E },
        3, 8, 1407860330, 40371, 0x1F540000127B2FC8ULL);

    // capture-000476 seq 610201, the 17-byte ZERO-mask form. A client with no
    // ticket legitimately sends this, so an empty mask must parse rather than
    // be treated as malformed.
    CheckSuccess("17B zero mask",
        { 0x00,0x00,0x00,0x00, 0x08,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
          0x00,0x00,0x00,0x00, 0x00 },
        0, 8, 0, 0, 0);

    // Below the fixed 17-byte head.
    CheckFailure("short body",
        { 0x03,0x00,0x00,0x00, 0x08,0x00,0x00,0x00 });

    // Mask promises a byte the body does not carry.
    CheckFailure("truncated guid",
        { 0x03,0x00,0x00,0x00, 0x08,0x00,0x00,0x00, 0x1E,0x64,0x14,0x54,
          0x85,0x9D,0x00,0x00, 0x79 });

    // Trailing byte past the exact tail.
    CheckFailure("trailing junk",
        { 0x00,0x00,0x00,0x00, 0x08,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
          0x00,0x00,0x00,0x00, 0x00, 0x99 });

    // A raw 0x01 would decode to zero against its own presence bit.
    CheckFailure("non-canonical byte",
        { 0x03,0x00,0x00,0x00, 0x08,0x00,0x00,0x00, 0x1E,0x64,0x14,0x54,
          0x85,0x9D,0x00,0x00, 0x79, 0x01,0x90,0x07,0x63,0x05 });

    if (g_fail)
    {
        std::fprintf(stderr, "mop_lfg_leave_packets: %d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_lfg_leave_packets: all checks passed\n");
    return 0;
}
