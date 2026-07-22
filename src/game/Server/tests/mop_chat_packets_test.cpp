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
 * Independent byte fixtures for the 5.4.8.18414 generic chat packet.
 */

#include "Chat.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    return packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static void test_gm_system_message()
{
    MopChatPackets::Message message;
    message.chatType = CHAT_MSG_SYSTEM;
    message.language = LANG_UNIVERSAL;
    message.chatTag = CHAT_TAG_GM;
    message.senderGuid = UI64LIT(0x0102030405060708);
    message.text = "GM";
    message.senderName = "Admin";

    WorldPacket packet;
    CHECK(MopChatPackets::BuildMessage(packet, message));

    CHECK(packet.GetOpcode() == SMSG_MESSAGECHAT);
    CHECK(Equal(packet, {
        0x00, 0x2A, 0xA0, 0x00, 0x40, 0x03, 0xFF, 0x80, 0x0B, 0x00, 0x00,
        0x05, 0x00, 0x06, 0x02, 0x09, 0x03, 0x07, 0x04,
        0x00,
        0x47, 0x4D,
        0x41, 0x64, 0x6D, 0x69, 0x6E
    }));
}

static void test_whisper_with_target_language_and_realms()
{
    MopChatPackets::Message message;
    message.chatType = CHAT_MSG_WHISPER;
    message.language = Language(1);
    message.receiverGuid = UI64LIT(0x1122334455667788);
    message.realmId2 = 0xA1B2C3D4u;
    message.realmId1 = 0x10203040u;
    message.text = "Hi";

    WorldPacket packet;
    CHECK(MopChatPackets::BuildMessage(packet, message));
    CHECK(Equal(packet, {
        0x96, 0x00, 0x7F, 0x90, 0x08, 0x00, 0xA0, 0x00,
        0x07,
        0x67, 0x32, 0x54, 0x23, 0x10, 0x45, 0x76, 0x89,
        0x01,
        0xD4, 0xC3, 0xB2, 0xA1,
        0x48, 0x69,
        0x40, 0x30, 0x20, 0x10
    }));
}

static void test_achievement_message()
{
    MopChatPackets::Message message;
    message.chatType = CHAT_MSG_ACHIEVEMENT;
    message.achievementId = 0x12345678u;
    message.text = "A";

    WorldPacket packet;
    CHECK(MopChatPackets::BuildMessage(packet, message));
    CHECK(Equal(packet, {
        0x97, 0x00, 0x00, 0x30, 0x00, 0x00, 0x70, 0x00,
        0x30, 0x78, 0x56, 0x34, 0x12, 0x41
    }));
}

static void test_group_message()
{
    MopChatPackets::Message message;
    message.chatType = CHAT_MSG_PARTY;
    message.groupGuid = UI64LIT(0x0102030405060708);
    message.text = "P";

    WorldPacket packet;
    CHECK(MopChatPackets::BuildMessage(packet, message));
    CHECK(Equal(packet, {
        0x97, 0xFF, 0x00, 0x30, 0x08, 0x00, 0x70, 0x00,
        0x02, 0x06, 0x04, 0x05, 0x03, 0x09, 0x07, 0x02, 0x00, 0x50
    }));
}

static void test_guild_message()
{
    MopChatPackets::Message message;
    message.chatType = CHAT_MSG_GUILD;
    message.guildGuid = UI64LIT(0x0102030405060708);
    message.text = "G";

    WorldPacket packet;
    CHECK(MopChatPackets::BuildMessage(packet, message));
    CHECK(Equal(packet, {
        0x97, 0x00, 0x00, 0x30, 0x08, 0x00, 0x77, 0xF8,
        0x05, 0x02, 0x00, 0x04, 0x07, 0x03, 0x09, 0x06, 0x04, 0x47
    }));
}

static void test_length_boundaries()
{
    MopChatPackets::Message maximum;
    maximum.senderName.assign((size_t(1) << 11) - 1, 's');
    maximum.receiverName.assign((size_t(1) << 11) - 1, 'r');
    maximum.channelName.assign((size_t(1) << 7) - 1, 'c');
    maximum.addonPrefix.assign((size_t(1) << 5) - 1, 'p');
    maximum.text.assign((size_t(1) << 12) - 1, 'm');
    maximum.chatTag = (uint32(1) << 9) - 1;
    WorldPacket valid;
    CHECK(MopChatPackets::BuildMessage(valid, maximum));

    MopChatPackets::Message oversized;
    oversized.senderName.assign(size_t(1) << 11, 's');
    WorldPacket senderRejected;
    CHECK(!MopChatPackets::BuildMessage(senderRejected, oversized));
    CHECK(senderRejected.empty());

    oversized = MopChatPackets::Message{};
    oversized.receiverName.assign(size_t(1) << 11, 'r');
    WorldPacket receiverRejected;
    CHECK(!MopChatPackets::BuildMessage(receiverRejected, oversized));

    oversized = MopChatPackets::Message{};
    oversized.channelName.assign(size_t(1) << 7, 'c');
    WorldPacket channelRejected;
    CHECK(!MopChatPackets::BuildMessage(channelRejected, oversized));

    oversized = MopChatPackets::Message{};
    oversized.addonPrefix.assign(size_t(1) << 5, 'p');
    WorldPacket prefixRejected;
    CHECK(!MopChatPackets::BuildMessage(prefixRejected, oversized));

    oversized = MopChatPackets::Message{};
    oversized.text.assign(size_t(1) << 12, 'm');
    WorldPacket textRejected;
    CHECK(!MopChatPackets::BuildMessage(textRejected, oversized));

    oversized = MopChatPackets::Message{};
    oversized.chatTag = uint32(1) << 9;
    WorldPacket tagRejected;
    CHECK(!MopChatPackets::BuildMessage(tagRejected, oversized));
}

static void test_opcode()
{
    CHECK(uint32(SMSG_MESSAGECHAT) == 0x1A9Au);
    CHECK(uint32(SMSG_MESSAGECHAT) < uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_gm_system_message();
    test_whisper_with_target_language_and_realms();
    test_achievement_message();
    test_group_message();
    test_guild_message();
    test_length_boundaries();
    test_opcode();

    if (g_fail)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_fail);
        return 1;
    }

    std::puts("mop_chat_packets_test: PASS");
    return 0;
}
