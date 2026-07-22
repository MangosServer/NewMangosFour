/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

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

static void test_guild_member_joined()
{
    WorldPacket packet;
    CHECK(MopGuildPackets::BuildGuildMemberJoined(packet,
        UI64LIT(0x0807060504030201), "J", 0x11223344u));
    CHECK(packet.GetOpcode() == SMSG_GUILD_EVENT_PLAYER_JOINED);
    CHECK(Equal(packet, {
        0xE0, 0xFC,
        0x02, 0x04, 0x03, 0x06, 0x07,
        0x44, 0x33, 0x22, 0x11,
        0x05, 0x00, 0x4A, 0x09
    }));
}

static void test_guild_presence_change()
{
    WorldPacket packet;
    CHECK(MopGuildPackets::BuildGuildPresenceChange(packet,
        UI64LIT(0x00C30000B20000A1), "P", 0x11223344u, true, false));
    CHECK(packet.GetOpcode() == SMSG_GUILD_EVENT_PRESENCE_CHANGE);
    CHECK(Equal(packet, {
        0xC4, 0x11,
        0xB3, 0xA0,
        0x44, 0x33, 0x22, 0x11,
        0xC2, 0x50
    }));
}

static void test_guild_member_rank_update()
{
    WorldPacket packet;
    MopGuildPackets::BuildGuildMemberRankUpdate(packet,
        UI64LIT(0x1817161514131211), UI64LIT(0x0807060504030201),
        3, true);
    CHECK(packet.GetOpcode() == SMSG_GUILD_RANKS_UPDATE);
    CHECK(Equal(packet, {
        0xFF, 0xFF, 0x80,
        0x02, 0x13, 0x06, 0x03, 0x07, 0x10,
        0x03, 0x00, 0x00, 0x00,
        0x15, 0x19, 0x09, 0x12, 0x05, 0x04,
        0x16, 0x17, 0x00, 0x14
    }));
}

static void test_guild_new_leader()
{
    WorldPacket packet;
    CHECK(MopGuildPackets::BuildGuildNewLeader(packet,
        UI64LIT(0x0807060504030201), "O", 0x11223344u,
        UI64LIT(0x1817161514131211), "N", 0x55667788u, false));
    CHECK(packet.GetOpcode() == SMSG_GUILD_EVENT_NEW_LEADER);
    CHECK(Equal(packet, {
        0xF0, 0x7B, 0xF8, 0x38,
        0x17, 0x16, 0x4F, 0x4E, 0x15, 0x14,
        0x88, 0x77, 0x66, 0x55,
        0x06, 0x10, 0x07, 0x12, 0x19, 0x09, 0x04,
        0x44, 0x33, 0x22, 0x11,
        0x13, 0x02, 0x03, 0x05, 0x00
    }));
}

static void test_guild_disbanded()
{
    WorldPacket packet;
    MopGuildPackets::BuildGuildDisbanded(packet);
    CHECK(packet.GetOpcode() == SMSG_GUILD_EVENT_DISBANDED);
    CHECK(packet.empty());
}

static void test_guild_player_left()
{
    WorldPacket selfLeave;
    CHECK(MopGuildPackets::BuildGuildPlayerLeft(selfLeave,
        UI64LIT(0x0807060504030201), "L", 0x11223344u,
        false, 0, "", 0));
    CHECK(selfLeave.GetOpcode() == SMSG_GUILD_EVENT_PLAYER_LEFT);
    CHECK(Equal(selfLeave, {
        0x83, 0xBE, 0x4C, 0x03,
        0x44, 0x33, 0x22, 0x11,
        0x00, 0x04, 0x02, 0x05, 0x06, 0x07, 0x09
    }));

    WorldPacket removed;
    CHECK(MopGuildPackets::BuildGuildPlayerLeft(removed,
        UI64LIT(0x0807060504030201), "L", 0x11223344u,
        true, UI64LIT(0x1817161514131211), "R", 0x55667788u));
    CHECK(Equal(removed, {
        0x83, 0xC0, 0x7F, 0xDF,
        0x13, 0x15, 0x17, 0x12, 0x10, 0x14, 0x16, 0x19,
        0x52, 0x88, 0x77, 0x66, 0x55,
        0x4C, 0x03, 0x44, 0x33, 0x22, 0x11,
        0x00, 0x04, 0x02, 0x05, 0x06, 0x07, 0x09
    }));
}

static void test_guild_event_name_bounds()
{
    std::string const maximum(63, 'x');
    std::string const oversized(64, 'x');

    WorldPacket joined;
    CHECK(MopGuildPackets::BuildGuildMemberJoined(joined, 1, maximum, 1));
    WorldPacket joinedRejected;
    CHECK(!MopGuildPackets::BuildGuildMemberJoined(joinedRejected, 1, oversized, 1));
    CHECK(joinedRejected.empty());

    WorldPacket presence;
    CHECK(MopGuildPackets::BuildGuildPresenceChange(presence, 1, maximum, 1, true, false));
    WorldPacket presenceRejected;
    CHECK(!MopGuildPackets::BuildGuildPresenceChange(presenceRejected, 1, oversized, 1, true, false));
    CHECK(presenceRejected.empty());

    WorldPacket leader;
    CHECK(MopGuildPackets::BuildGuildNewLeader(leader, 1, maximum, 1,
        2, maximum, 1, false));
    WorldPacket leaderRejected;
    CHECK(!MopGuildPackets::BuildGuildNewLeader(leaderRejected, 1, oversized, 1,
        2, "n", 1, false));
    CHECK(leaderRejected.empty());

    WorldPacket left;
    CHECK(MopGuildPackets::BuildGuildPlayerLeft(left, 1, maximum, 1,
        true, 2, maximum, 1));
    WorldPacket leftRejected;
    CHECK(!MopGuildPackets::BuildGuildPlayerLeft(leftRejected, 1, "l", 1,
        true, 2, oversized, 1));
    CHECK(leftRejected.empty());
}

static void test_guild_event_opcodes()
{
    CHECK(uint32(SMSG_GUILD_RANKS_UPDATE) == 0x0A60u);
    CHECK(uint32(SMSG_GUILD_EVENT_PLAYER_JOINED) == 0x0B69u);
    CHECK(uint32(SMSG_GUILD_EVENT_PRESENCE_CHANGE) == 0x0B70u);
    CHECK(uint32(SMSG_GUILD_EVENT_PLAYER_LEFT) == 0x0BF8u);
    CHECK(uint32(SMSG_GUILD_EVENT_NEW_LEADER) == 0x0E69u);
    CHECK(uint32(SMSG_GUILD_EVENT_DISBANDED) == 0x1E68u);
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
    test_guild_member_joined();
    test_guild_presence_change();
    test_guild_member_rank_update();
    test_guild_new_leader();
    test_guild_disbanded();
    test_guild_player_left();
    test_guild_event_name_bounds();
    test_guild_event_opcodes();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_guild_packets: all checks passed\n");
    return 0;
}
