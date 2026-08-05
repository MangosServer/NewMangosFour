/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Byte-exact fixtures for the 5.4.8.18414 group-invite popup, SMSG_GROUP_INVITE.
 *
 * These are REAL CAPTURED BODIES, not the inverse of our own writer. A fixture
 * generated from the builder cannot catch a transposed field, because it agrees
 * with whatever the builder does; that is how the party-kill roles went
 * unnoticed. Each case below feeds the builder the values decoded out of a
 * captured packet and asserts the output equals that packet byte for byte.
 */

#include "Group.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void CheckBytes(char const* what, WorldPacket const& built,
    std::vector<uint8> const& expected)
{
    CHECK(built.size() == expected.size());
    if (built.size() != expected.size())
    {
        std::fprintf(stderr, "  %s: built %u bytes, expected %u\n",
                     what, uint32(built.size()), uint32(expected.size()));
        return;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (built.contents()[i] != expected[i])
        {
            std::fprintf(stderr, "  %s: first difference at byte %u: built 0x%02X expected 0x%02X\n",
                         what, uint32(i), built.contents()[i], expected[i]);
            ++g_fail;
            return;
        }
    }
}

// capture-000033 sequence 196326, 58 bytes. A cross-realm invite from
// "Kenny" (UTF-8 K\xC3\xA9nny) on Dun Modr. Five GUID bytes present, so the
// presence bits and the XOR-by-one byte writes are both exercised.
static void Fixture_DunModr()
{
    static uint8 const expected[] = {
        0x07, 0x08, 0x86, 0x9E, 0x00, 0x00, 0x06, 0x00,
        0x44, 0x75, 0x6E, 0x4D, 0x6F, 0x64, 0x72, 0x07,
        0x81, 0x58, 0xCB, 0xF1, 0xFA, 0x05, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x06, 0x03, 0x62, 0x05,
        0x00, 0x00, 0x39, 0x44, 0x75, 0x6E, 0x20, 0x4D,
        0x6F, 0x64, 0x72, 0x00, 0x00, 0x00, 0x00, 0x4B,
        0xC3, 0xA9, 0x6E, 0x6E, 0x79, 0x03, 0x00, 0x00,
        0x00, 0x00
    };

    MopGroupInvitePackets::Invite invite;
    invite.inviterGuid = ObjectGuid(uint64(0x0600000002803859ULL));
    invite.inviterName = std::string("\x4B\xC3\xA9\x6E\x6E\x79", 6);
    invite.compactRealmName = "DunModr";
    invite.displayRealmName = "Dun Modr";
    invite.accountId = 100331979;
    invite.virtualRealmAddress = 50724865;
    invite.realmId = 1378;
    invite.notAlreadyInGroup = false;
    invite.flagA = false;
    invite.flagB = true;
    invite.crossRealmName = true;
    invite.realmTransferWarning = true;

    WorldPacket built;
    CHECK(MopGroupInvitePackets::BuildInvite(built, invite));
    CheckBytes("DunModr", built,
               std::vector<uint8>(expected, expected + sizeof(expected)));
}

// capture-000033 sequence 63033, 76 bytes. Multi-byte UTF-8 in both realm
// strings, and a different GUID presence set, so a fixed byte order that
// happened to fit the first fixture will not fit this one.
static void Fixture_ChantsEternels()
{
    static uint8 const expected[] = {
        0x0F, 0x10, 0x88, 0x9E, 0x00, 0x00, 0x02, 0x80,
        0x81, 0x43, 0x68, 0x61, 0x6E, 0x74, 0x73, 0xC3,
        0xA9, 0x74, 0x65, 0x72, 0x6E, 0x65, 0x6C, 0x73,
        0x06, 0x45, 0x70, 0x0E, 0x6E, 0x0E, 0x06, 0x00,
        0x00, 0x00, 0x00, 0x26, 0x00, 0x07, 0x03, 0x54,
        0x06, 0x00, 0x00, 0x0F, 0x43, 0x68, 0x61, 0x6E,
        0x74, 0x73, 0x20, 0xC3, 0xA9, 0x74, 0x65, 0x72,
        0x6E, 0x65, 0x6C, 0x73, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x6E, 0x74, 0x6F, 0x78, 0x69, 0x6F, 0x6E,
        0x00, 0x00, 0x00, 0x00
    };

    MopGroupInvitePackets::Invite invite;
    invite.inviterGuid = ObjectGuid(uint64(0x0780000000440E71ULL));
    invite.inviterName = "Intoxion";
    invite.compactRealmName = std::string("Chants\xC3\xA9ternels");
    invite.displayRealmName = std::string("Chants \xC3\xA9ternels");
    invite.accountId = 101608974;
    invite.virtualRealmAddress = 50790438;
    invite.realmId = 1620;
    invite.notAlreadyInGroup = false;
    invite.flagA = false;
    invite.flagB = true;
    invite.crossRealmName = true;
    invite.realmTransferWarning = true;

    WorldPacket built;
    CHECK(MopGroupInvitePackets::BuildInvite(built, invite));
    CheckBytes("ChantsEternels", built,
               std::vector<uint8>(expected, expected + sizeof(expected)));
}

// The size rule the derivation established:
//   32 + compactRealmLen + displayRealmLen + inviterNameLen + popcount(GUID)
// It reproduces every observed length independently of what the scalars mean,
// so it is a check on field boundaries rather than on semantics.
static void SizeRule()
{
    MopGroupInvitePackets::Invite invite;
    invite.inviterGuid = ObjectGuid(uint64(0x0600000002803859ULL));  // 5 non-zero bytes
    invite.inviterName = "Abc";
    invite.compactRealmName = "Realm";
    invite.displayRealmName = "Realm Name";

    WorldPacket built;
    CHECK(MopGroupInvitePackets::BuildInvite(built, invite));
    CHECK(built.size() == 32 + 5 + 10 + 3 + 5);
}

// The one configuration this server actually emits, asserted byte-wise across
// the whole header rather than by length alone.
//
// This is where the FrameXML requirement is pinned. UIParent.lua:800 chooses the
// dialog from these bits: any role bit shows the LFG invite popup, and the
// cross-realm flag shows PARTY_INVITE_XREALM. If a future change sets one, an
// ordinary friend invite silently renders as the wrong dialog while remaining a
// well-formed packet -- which no size or round-trip check would catch.
//
// The GUID is a realistic one: HIGHGUID_PLAYER is 0 and a player's raw GUID is
// its counter, so bytes 4..7 are always zero on this server. An earlier version
// of this fixture used a captured retail GUID with byte 7 set, a shape this
// server cannot produce.
static void OrdinarySameRealmInvite()
{
    static uint8 const expectedHeader[] = {
        0x00,   // compactRealmByteLength = 0
        0x00,   // displayRealmByteLength = 0
        0x0C,   // guid[2]=0, flagA=0, inviterNameByteLength=12
        0x28,   // guid[7]=0 guid[5]=0 notAlreadyInGroup=1 flagB=0 guid[1]=1
                // crossRealmName=0 realmTransferWarning=0, extraCount begins
        0x00, 0x00,
        0x02,   // extraCount ends; guid[3]=0 guid[0]=1 guid[4]=0
        0x00    // guid[6]=0, then padding to the eighth byte
    };

    MopGroupInvitePackets::Invite invite;
    invite.inviterGuid = ObjectGuid(uint64(0x0000000000001234ULL));
    invite.inviterName = "Humanwarrior";

    WorldPacket built;
    CHECK(MopGroupInvitePackets::BuildInvite(built, invite));
    CHECK(built.GetOpcode() == SMSG_GROUP_INVITE);

    // 32 + compactRealm(0) + displayRealm(0) + name(12) + popcount(GUID)(2)
    CHECK(built.size() == 46);

    for (size_t i = 0; i < sizeof(expectedHeader); ++i)
    {
        if (built.size() > i && built.contents()[i] != expectedHeader[i])
        {
            std::fprintf(stderr, "  same-realm header byte %u: built 0x%02X expected 0x%02X\n",
                         uint32(i), built.contents()[i], expectedHeader[i]);
            ++g_fail;
        }
    }
}

// A name the six-bit length field cannot represent must be refused rather than
// silently truncated into a body the client would mis-frame.
static void RejectsUnrepresentableLengths()
{
    MopGroupInvitePackets::Invite invite;
    invite.inviterGuid = ObjectGuid(uint64(0x0600000002803859ULL));
    invite.inviterName = std::string(64, 'x');              // 6 bits max 63

    WorldPacket built;
    CHECK(!MopGroupInvitePackets::BuildInvite(built, invite));

    MopGroupInvitePackets::Invite wideRealm;
    wideRealm.inviterGuid = ObjectGuid(uint64(0x0600000002803859ULL));
    wideRealm.inviterName = "Abc";
    wideRealm.compactRealmName = std::string(256, 'r');     // 8 bits max 255

    WorldPacket second;
    CHECK(!MopGroupInvitePackets::BuildInvite(second, wideRealm));
}

int main()
{
    Fixture_DunModr();
    Fixture_ChantsEternels();
    SizeRule();
    OrdinarySameRealmInvite();
    RejectsUnrepresentableLengths();

    if (g_fail)
    {
        std::fprintf(stderr, "mop_group_invite_popup_packets: %d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_group_invite_popup_packets: all checks passed\n");
    return 0;
}
