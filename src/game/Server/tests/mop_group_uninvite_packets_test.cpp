/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Byte-exact fixtures for the 5.4.8.18414 group-uninvite request,
 * CMSG_GROUP_UNINVITE_GUID 0x0CE1.
 *
 * These are REAL CAPTURED BODIES. Two of them are the same target uninvited
 * twice from one capture, once with a reason, so they check the GUID byte order
 * semantically rather than only self-consistently -- a decoder with a
 * transposed order would yield two different players.
 */

#include "Group.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>
#include <vector>

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
    uint64 expectedGuid, char const* expectedReason)
{
    WorldPacket packet(CMSG_GROUP_UNINVITE_GUID, bytes.size());
    Feed(packet, bytes);

    MopGroupUninvitePackets::Request request;
    CHECK(MopGroupUninvitePackets::ParseRequest(packet, request));
    CHECK(packet.rpos() == packet.size());
    if (request.targetGuid.GetRawValue() != expectedGuid)
    {
        std::fprintf(stderr, "  %s: guid 0x%016llX expected 0x%016llX\n", what,
                     (unsigned long long)request.targetGuid.GetRawValue(),
                     (unsigned long long)expectedGuid);
        ++g_fail;
    }
    CHECK(request.reason == expectedReason);
}

static void CheckFailure(char const* what, std::initializer_list<uint8> bytes)
{
    WorldPacket packet(CMSG_GROUP_UNINVITE_GUID, bytes.size());
    Feed(packet, bytes);

    MopGroupUninvitePackets::Request request;
    if (MopGroupUninvitePackets::ParseRequest(packet, request))
    {
        std::fprintf(stderr, "  %s: accepted a body it should have refused\n", what);
        ++g_fail;
    }
}

int main()
{
    // capture-000075 seq 1647847, 9 bytes, no reason.
    CheckSuccess("9B no reason",
        { 0x7F, 0xBE, 0x00, 0x81, 0x87, 0x04, 0x03, 0x00, 0x73 },
        0x0180000005028672ULL, "");

    // capture-000075 seq 1647890, 12 bytes, SAME target, reason "afk".
    CheckSuccess("12B reason afk",
        { 0x7F, 0xBE, 0x03, 0x61, 0x66, 0x6B, 0x81, 0x87, 0x04, 0x03, 0x00, 0x73 },
        0x0180000005028672ULL, "afk");

    // A different target and a different presence mask, so a byte order that
    // happened to fit the first two does not fit this one.
    CheckSuccess("8B other target",
        { 0x7F, 0x3E, 0x00, 0x05, 0x07, 0x15, 0x05, 0xAA },
        0x04000000061404ABULL, "");

    // Wrong marker: the family sentinel is the first thing checked.
    CheckFailure("bad marker",
        { 0x00, 0xBE, 0x00, 0x81, 0x87, 0x04, 0x03, 0x00, 0x73 });

    // Declared reason length longer than the body, which must not over-read.
    CheckFailure("reason overruns",
        { 0x7F, 0xBE, 0x40, 0x81, 0x87, 0x04, 0x03, 0x00, 0x73 });

    // One GUID byte short of what the mask promises.
    CheckFailure("truncated guid",
        { 0x7F, 0xBE, 0x00, 0x81, 0x87, 0x04, 0x03, 0x00 });

    // Trailing byte beyond the exact tail.
    CheckFailure("trailing junk",
        { 0x7F, 0xBE, 0x00, 0x81, 0x87, 0x04, 0x03, 0x00, 0x73, 0x99 });

    // A raw 0x01 would decode to zero and contradict its own presence bit.
    CheckFailure("non-canonical byte",
        { 0x7F, 0xBE, 0x00, 0x01, 0x87, 0x04, 0x03, 0x00, 0x73 });

    if (g_fail)
    {
        std::fprintf(stderr, "mop_group_uninvite_packets: %d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_group_uninvite_packets: all checks passed\n");
    return 0;
}
