/**
 * Independent byte fixtures for 5.4.8.18414 guild packets.
 */

#include "Guild.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    return packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static void Append(WorldPacket& packet, std::vector<uint8> const& bytes)
{
    if (!bytes.empty())
        packet.append(bytes.data(), bytes.size());
}

static void test_empty_motd()
{
    WorldPacket packet(SMSG_GUILD_EVENT_MOTD, 2);
    CHECK(MopGuildPackets::BuildGuildMotd(packet, ""));
    CHECK(Equal(packet, { 0x00, 0x00 }));
}

static void test_short_motd()
{
    WorldPacket packet(SMSG_GUILD_EVENT_MOTD, 5);
    CHECK(MopGuildPackets::BuildGuildMotd(packet, "ABC"));
    CHECK(Equal(packet, { 0x00, 0xC0, 0x41, 0x42, 0x43 }));
}

static void test_length_boundary()
{
    std::string maximum(1023, 'x');
    WorldPacket valid(SMSG_GUILD_EVENT_MOTD, maximum.size() + 2);
    CHECK(MopGuildPackets::BuildGuildMotd(valid, maximum));
    CHECK(valid.size() == maximum.size() + 2);
    CHECK(valid.contents()[0] == 0xFF);
    CHECK(valid.contents()[1] == 0xC0);

    WorldPacket rejected(SMSG_GUILD_EVENT_MOTD, 0);
    CHECK(!MopGuildPackets::BuildGuildMotd(rejected, std::string(1024, 'x')));
    CHECK(rejected.empty());
}

static void test_opcode()
{
    CHECK(uint32(SMSG_GUILD_EVENT_MOTD) == 0x0B68u);
    CHECK(uint32(SMSG_GUILD_EVENT_MOTD) < uint32(OPCODE_TABLE_SIZE));
}

static void test_tabard_vendor_activate_request()
{
    {
        WorldPacket packet(CMSG_TABARD_VENDOR_ACTIVATE, 9);
        Append(packet, { 0xFF, 0x09, 0x06, 0x02, 0x07, 0x00, 0x03, 0x04, 0x05 });
        CHECK(MopGuildPackets::ReadTabardVendorActivate(packet) == UI64LIT(0x0807060504030201));
    }
    {
        WorldPacket packet(CMSG_TABARD_VENDOR_ACTIVATE, 4);
        Append(packet, { 0x1A, 0xC2, 0xA0, 0xB3 });
        CHECK(MopGuildPackets::ReadTabardVendorActivate(packet) == UI64LIT(0x00C30000B20000A1));
    }
}

static void test_tabard_vendor_activate_response()
{
    WorldPacket full;
    MopGuildPackets::BuildTabardVendorActivate(full, UI64LIT(0x0807060504030201));
    CHECK(full.GetOpcode() == SMSG_TABARD_VENDOR_ACTIVATE);
    CHECK(Equal(full, { 0xFF, 0x07, 0x04, 0x02, 0x05, 0x06, 0x00, 0x03, 0x09 }));

    WorldPacket sparse;
    MopGuildPackets::BuildTabardVendorActivate(sparse, UI64LIT(0x00C30000B20000A1));
    CHECK(Equal(sparse, { 0x26, 0xB3, 0xC2, 0xA0 }));
}

static void test_save_guild_emblem_request()
{
    WorldPacket packet(CMSG_SAVE_GUILD_EMBLEM, 29);
    Append(packet, {
        0x34, 0x33, 0x32, 0x31, // border style
        0x54, 0x53, 0x52, 0x51, // background color
        0x44, 0x43, 0x42, 0x41, // border color
        0x24, 0x23, 0x22, 0x21, // emblem color
        0x14, 0x13, 0x12, 0x11, // emblem style
        0x91, 0xC2, 0xA0, 0xB3
    });

    MopGuildPackets::EmblemDesign const design = MopGuildPackets::ReadSaveGuildEmblem(packet);
    CHECK(design.vendorGuid == UI64LIT(0x00C30000B20000A1));
    CHECK(design.emblemStyle == 0x11121314u);
    CHECK(design.emblemColor == 0x21222324u);
    CHECK(design.borderStyle == 0x31323334u);
    CHECK(design.borderColor == 0x41424344u);
    CHECK(design.backgroundColor == 0x51525354u);
}

static void test_save_guild_emblem_result()
{
    for (uint32 result = ERR_GUILDEMBLEM_SUCCESS;
            result <= ERR_GUILDEMBLEM_INVALIDVENDOR; ++result)
    {
        WorldPacket packet;
        MopGuildPackets::BuildSaveGuildEmblemResult(packet, result);
        CHECK(packet.GetOpcode() == SMSG_SAVE_GUILD_EMBLEM);
        CHECK(Equal(packet, {
            uint8(result), 0x00, 0x00, 0x00
        }));
    }
}

static void test_tabard_opcodes()
{
    CHECK(uint32(CMSG_TABARD_VENDOR_ACTIVATE) == 0x11C3u);
    CHECK(uint32(SMSG_TABARD_VENDOR_ACTIVATE) == 0x0A3Eu);
    CHECK(uint32(CMSG_SAVE_GUILD_EMBLEM) == 0x1D60u);
    CHECK(uint32(SMSG_SAVE_GUILD_EMBLEM) == 0x089Fu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_empty_motd();
    test_short_motd();
    test_length_boundary();
    test_opcode();
    test_tabard_vendor_activate_request();
    test_tabard_vendor_activate_response();
    test_save_guild_emblem_request();
    test_save_guild_emblem_result();
    test_tabard_opcodes();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_guild_packets: all checks passed\n");
    return 0;
}
