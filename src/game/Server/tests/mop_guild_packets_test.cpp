/**
 * Independent byte fixtures for the 5.4.8.18414 guild MOTD packet.
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

int main(int /*argc*/, char** /*argv*/)
{
    test_empty_motd();
    test_short_motd();
    test_length_boundary();
    test_opcode();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_guild_packets: all checks passed\n");
    return 0;
}
