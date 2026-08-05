/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Byte-exact fixtures for the 5.4.8.18414 loot-rules request,
 * CMSG_LOOT_METHOD 0x0DE1.
 *
 * The two success cases are REAL CAPTURED BODIES. Note that no captured body
 * carries a master looter -- the client only sends one for MASTER_LOOT -- so the
 * GUID orders come from the client writer alone. The synthetic master-loot case
 * below documents what this reader expects; it is not evidence of the order.
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
    uint32 expectedMethod, uint32 expectedThreshold, uint64 expectedLooter)
{
    WorldPacket packet(CMSG_LOOT_METHOD, bytes.size());
    Feed(packet, bytes);

    MopGroupLootMethodPackets::Request request;
    CHECK(MopGroupLootMethodPackets::ParseRequest(packet, request));
    CHECK(packet.rpos() == packet.size());
    CHECK(request.method == expectedMethod);
    CHECK(request.threshold == expectedThreshold);
    if (request.looterGuid.GetRawValue() != expectedLooter)
    {
        std::fprintf(stderr, "  %s: looter 0x%016llX expected 0x%016llX\n", what,
                     (unsigned long long)request.looterGuid.GetRawValue(),
                     (unsigned long long)expectedLooter);
        ++g_fail;
    }
}

static void CheckFailure(char const* what, std::initializer_list<uint8> bytes)
{
    WorldPacket packet(CMSG_LOOT_METHOD, bytes.size());
    Feed(packet, bytes);

    MopGroupLootMethodPackets::Request request;
    if (MopGroupLootMethodPackets::ParseRequest(packet, request))
    {
        std::fprintf(stderr, "  %s: accepted a body it should have refused\n", what);
        ++g_fail;
    }
}

int main()
{
    // capture-000476 seq 366056: free-for-all, uncommon threshold, no looter.
    CheckSuccess("FFA uncommon",
        { 0x7F, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00 },
        FREE_FOR_ALL, ITEM_QUALITY_UNCOMMON, 0);

    // capture-000803 seq 94720: group loot, uncommon threshold, no looter.
    CheckSuccess("group loot uncommon",
        { 0x7F, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00 },
        GROUP_LOOT, ITEM_QUALITY_UNCOMMON, 0);

    // Synthetic: master loot with one present GUID byte. Mask bit order is
    // 7,1,2,0,4,5,6,3 read MSB-first, so 0x20 -- bit index 2 -- selects GUID
    // byte 2, and the byte itself is written ^1 (0x43 on the wire is 0x42).
    CheckSuccess("master loot with looter",
        { 0x7F, 0x02, 0x04, 0x00, 0x00, 0x00, 0x20, 0x43 },
        MASTER_LOOT, ITEM_QUALITY_EPIC, 0x0000000000420000ULL);

    // NOT_GROUP_TYPE_LOOT is internal and must never arrive from the wire.
    CheckFailure("internal loot method",
        { 0x7F, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00 });

    // Threshold beyond ITEM_QUALITY_HEIRLOOM would be cast onto the enum.
    CheckFailure("threshold out of range",
        { 0x7F, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00 });

    CheckFailure("bad marker",
        { 0x00, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00 });

    // Mask promises a byte the body does not carry.
    CheckFailure("truncated guid",
        { 0x7F, 0x02, 0x02, 0x00, 0x00, 0x00, 0x20 });

    CheckFailure("trailing junk",
        { 0x7F, 0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x99 });

    CheckFailure("short body",
        { 0x7F, 0x03, 0x02 });

    if (g_fail)
    {
        std::fprintf(stderr, "mop_group_loot_method_packets: %d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_group_loot_method_packets: all checks passed\n");
    return 0;
}
