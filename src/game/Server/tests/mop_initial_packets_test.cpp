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
 * Byte-exact tests for the ten regular 5.4.8 initial UI packet bodies.
 */

#include "Player.h"
#include "Item.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <array>
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
        std::fprintf(stderr, "  size %u, wanted %u\n", unsigned(packet.size()), unsigned(expected.size()));
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (packet.contents()[i] != expected[i])
        {
            std::fprintf(stderr, "  byte %u = 0x%02X, wanted 0x%02X\n",
                         unsigned(i), packet.contents()[i], expected[i]);
            return false;
        }
    }
    return true;
}

static uint64_t Fnv1a64(WorldPacket const& packet)
{
    uint64_t hash = UINT64_C(0xcbf29ce484222325);
    for (size_t i = 0; i < packet.size(); ++i)
    {
        hash ^= packet.contents()[i];
        hash *= UINT64_C(0x100000001b3);
    }
    return hash;
}

static void test_initial_spells()
{
    {
        WorldPacket packet(SMSG_INITIAL_SPELLS, 3);
        MopInitialPackets::BuildInitialSpells(packet, {});
        CHECK(ExpectBytes(packet, { 0x00, 0x00, 0x00 }));
    }
    {
        WorldPacket packet(SMSG_INITIAL_SPELLS, 11);
        MopInitialPackets::BuildInitialSpells(packet, { 0x11223344u, 0xAABBCCDDu });
        CHECK(ExpectBytes(packet, {
            0x00, 0x00, 0x04,
            0x44, 0x33, 0x22, 0x11, 0xDD, 0xCC, 0xBB, 0xAA
        }));
    }
}

static void test_send_unlearn_spells()
{
    {
        WorldPacket packet(SMSG_SEND_UNLEARN_SPELLS, 3);
        MopInitialPackets::BuildSendUnlearnSpells(packet, {});
        CHECK(ExpectBytes(packet, { 0x00, 0x00, 0x00 }));
    }
    {
        WorldPacket packet(SMSG_SEND_UNLEARN_SPELLS, 12);
        std::vector<MopInitialPackets::UnlearnSpell> records = {
            { 0x11223344u, 0x55u, 0xAABBCCDDu }
        };
        MopInitialPackets::BuildSendUnlearnSpells(packet, records);
        CHECK(ExpectBytes(packet, {
            0x00, 0x00, 0x08,
            0x44, 0x33, 0x22, 0x11, 0x55, 0xDD, 0xCC, 0xBB, 0xAA
        }));
    }
}

static void test_action_buttons()
{
    {
        std::array<MopInitialPackets::ActionButton, MopInitialPackets::ACTION_BUTTON_COUNT> buttons{};
        WorldPacket packet(SMSG_ACTION_BUTTONS, 133);
        MopInitialPackets::BuildActionButtons(packet, buttons, 1);
        std::vector<uint8_t> expected(133, 0);
        expected.back() = 1;
        CHECK(ExpectBytes(packet, expected));
    }
    {
        std::array<MopInitialPackets::ActionButton, MopInitialPackets::ACTION_BUTTON_COUNT> buttons{};
        buttons[0] = { 0x00112233u, 0x44u };
        buttons[131] = { 0x00A1B2C3u, 0xD4u };

        WorldPacket packet(SMSG_ACTION_BUTTONS, 141);
        MopInitialPackets::BuildActionButtons(packet, buttons, 1);

        CHECK(packet.size() == 141);
        CHECK(Fnv1a64(packet) == UINT64_C(0xc9ed25616a200ea3));
        CHECK(packet.contents()[0] == 0x00);
        CHECK(packet.contents()[49] == 0x08);
        CHECK(packet.contents()[65] == 0x01);
        CHECK(packet.contents()[82] == 0x08);
        CHECK(packet.contents()[98] == 0x01);
        CHECK(packet.contents()[99] == 0x80);
        CHECK(packet.contents()[115] == 0x18);
        CHECK(packet.contents()[131] == 0x01);
        CHECK(packet.contents()[132] == 0x32);
        CHECK(packet.contents()[133] == 0xC2);
        CHECK(packet.contents()[134] == 0x23);
        CHECK(packet.contents()[135] == 0xB3);
        CHECK(packet.contents()[136] == 0x45);
        CHECK(packet.contents()[137] == 0xD5);
        CHECK(packet.contents()[138] == 0x10);
        CHECK(packet.contents()[139] == 0xA0);
        CHECK(packet.contents()[140] == 0x01);
    }
}

static void test_account_data_times()
{
    std::array<uint32, MopInitialPackets::ACCOUNT_DATA_COUNT> times = {
        0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u,
        0x55555555u, 0x66666666u, 0x77777777u, 0x88888888u
    };
    std::vector<uint8_t> expected = {
        0x00,
        0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x22,
        0x33, 0x33, 0x33, 0x33, 0x44, 0x44, 0x44, 0x44,
        0x55, 0x55, 0x55, 0x55, 0x66, 0x66, 0x66, 0x66,
        0x77, 0x77, 0x77, 0x77, 0x88, 0x88, 0x88, 0x88,
        0xD4, 0xC3, 0xB2, 0xA1, 0x40, 0x30, 0x20, 0x10
    };

    WorldPacket packetFalse(SMSG_ACCOUNT_DATA_TIMES, 41);
    MopInitialPackets::BuildAccountDataTimes(packetFalse, times, 0xA1B2C3D4u, 0x10203040u, false);
    CHECK(ExpectBytes(packetFalse, expected));

    expected[0] = 0x80;
    WorldPacket packetTrue(SMSG_ACCOUNT_DATA_TIMES, 41);
    MopInitialPackets::BuildAccountDataTimes(packetTrue, times, 0xA1B2C3D4u, 0x10203040u, true);
    CHECK(ExpectBytes(packetTrue, expected));
}

static void test_feature_system_status()
{
    {
        WorldPacket packet(SMSG_FEATURE_SYSTEM_STATUS, 19);
        MopInitialPackets::BuildFeatureSystemStatus(packet, false, false, false);
        CHECK(ExpectBytes(packet, {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
            0x00, 0x04, 0x00
        }));
    }
    {
        WorldPacket packet(SMSG_FEATURE_SYSTEM_STATUS, 47);
        MopInitialPackets::BuildFeatureSystemStatus(packet, true, true, true);
        CHECK(ExpectBytes(packet, {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
            0x00, 0x45, 0x40,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x0A, 0x00, 0x00, 0x00, 0x60, 0xEA, 0x00, 0x00
        }));
    }
}

static void test_initialize_factions()
{
    {
        std::array<MopInitialPackets::Reputation, MopInitialPackets::REPUTATION_COUNT> reputations{};
        WorldPacket packet(SMSG_INITIALIZE_FACTIONS, 1312);
        MopInitialPackets::BuildInitializeFactions(packet, reputations);
        CHECK(packet.size() == 1312);
        CHECK(Fnv1a64(packet) == UINT64_C(0xf70edd465a0cd9a5));
        CHECK(packet.contents()[0] == 0x00);
        CHECK(packet.contents()[1311] == 0x00);
    }
    {
        std::array<MopInitialPackets::Reputation, MopInitialPackets::REPUTATION_COUNT> reputations{};
        reputations[0] = { 0x12u, 0x11223344, true };
        reputations[255] = { 0xABu, -2, true };

        WorldPacket packet(SMSG_INITIALIZE_FACTIONS, 1312);
        MopInitialPackets::BuildInitializeFactions(packet, reputations);
        CHECK(packet.size() == 1312);
        CHECK(Fnv1a64(packet) == UINT64_C(0x2f9bebe47b1a271c));
        CHECK(packet.contents()[0] == 0x12);
        CHECK(packet.contents()[1] == 0x44);
        CHECK(packet.contents()[4] == 0x11);
        CHECK(packet.contents()[1275] == 0xAB);
        CHECK(packet.contents()[1276] == 0xFE);
        CHECK(packet.contents()[1279] == 0xFF);
        CHECK(packet.contents()[1280] == 0x80);
        CHECK(packet.contents()[1311] == 0x01);
    }
}

static void test_tutorial_flags()
{
    std::array<uint32, MopInitialPackets::TUTORIAL_WORD_COUNT> words = {
        0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u,
        0x55555555u, 0x66666666u, 0x77777777u, 0x88888888u
    };
    WorldPacket packet(SMSG_TUTORIAL_FLAGS, 32);
    MopInitialPackets::BuildTutorialFlags(packet, words);
    CHECK(ExpectBytes(packet, {
        0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x22,
        0x33, 0x33, 0x33, 0x33, 0x44, 0x44, 0x44, 0x44,
        0x55, 0x55, 0x55, 0x55, 0x66, 0x66, 0x66, 0x66,
        0x77, 0x77, 0x77, 0x77, 0x88, 0x88, 0x88, 0x88
    }));
}

static void test_bind_point_update()
{
    WorldPacket packet(SMSG_BINDPOINTUPDATE, 20);
    MopInitialPackets::BuildBindPointUpdate(packet, 1.0f, 2.0f, 3.0f, 0x11223344u, 0x55667788u);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40,
        0x00, 0x00, 0x40, 0x40, 0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55
    }));
}

static void test_binder_activate()
{
    {
        WorldPacket packet(CMSG_BINDER_ACTIVATE, 9);
        uint8_t const bytes[] = { 0xFF, 0x00, 0x04, 0x02, 0x05, 0x09, 0x03, 0x07, 0x06 };
        packet.append(bytes, sizeof(bytes));
        CHECK(MopBindPackets::ReadBinderActivate(packet) == UI64LIT(0x0807060504030201));
    }
    {
        WorldPacket packet(CMSG_BINDER_ACTIVATE, 4);
        uint8_t const bytes[] = { 0x89, 0xA0, 0xB3, 0xC2 };
        packet.append(bytes, sizeof(bytes));
        CHECK(MopBindPackets::ReadBinderActivate(packet) == UI64LIT(0x00C30000B20000A1));
    }
}

static void test_binder_confirm()
{
    {
        WorldPacket packet;
        MopBindPackets::BuildBinderConfirm(packet, UI64LIT(0x0807060504030201));
        CHECK(packet.GetOpcode() == SMSG_BINDER_CONFIRM);
        CHECK(ExpectBytes(packet, {
            0xFF, 0x06, 0x02, 0x07, 0x00, 0x04, 0x09, 0x03, 0x05
        }));
    }
    {
        WorldPacket packet;
        MopBindPackets::BuildBinderConfirm(packet, UI64LIT(0x00C30000B20000A1));
        CHECK(ExpectBytes(packet, { 0x46, 0xC2, 0xA0, 0xB3 }));
    }
}

static void test_player_bound()
{
    {
        WorldPacket packet;
        MopBindPackets::BuildPlayerBound(packet, UI64LIT(0x0807060504030201), 0x11223344u);
        CHECK(packet.GetOpcode() == SMSG_PLAYERBOUND);
        CHECK(ExpectBytes(packet, {
            0xFF, 0x06, 0x03, 0x02, 0x05, 0x04, 0x07, 0x09, 0x00,
            0x44, 0x33, 0x22, 0x11
        }));
    }
    {
        WorldPacket packet;
        MopBindPackets::BuildPlayerBound(packet, UI64LIT(0x00C30000B20000A1), 0x55667788u);
        CHECK(ExpectBytes(packet, {
            0x38, 0xC2, 0xB3, 0xA0, 0x88, 0x77, 0x66, 0x55
        }));
    }
}

static void test_set_proficiency()
{
    WorldPacket packet(SMSG_SET_PROFICIENCY, 5);
    MopInitialPackets::BuildSetProficiency(packet, 0xA1B2C3D4u, 0x55u);
    CHECK(ExpectBytes(packet, { 0xD4, 0xC3, 0xB2, 0xA1, 0x55 }));
}

static void test_weather()
{
    {
        WorldPacket packet(SMSG_WEATHER, 9);
        MopInitialPackets::BuildWeather(packet, 0x11223344u, 1.5f, false);
        CHECK(ExpectBytes(packet, {
            0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0xC0, 0x3F, 0x00
        }));
    }
    {
        WorldPacket packet(SMSG_WEATHER, 9);
        MopInitialPackets::BuildWeather(packet, 0x11223344u, 1.5f, true);
        CHECK(ExpectBytes(packet, {
            0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0xC0, 0x3F, 0x80
        }));
    }
}

static void test_motd()
{
    WorldPacket packet(SMSG_MOTD, 6);
    MopInitialPackets::BuildMotd(packet, { "Hi", "A" });
    CHECK(ExpectBytes(packet, { 0x20, 0x40, 0x40, 0x48, 0x69, 0x41 }));
}

static void test_corpse_reclaim_delay()
{
    {
        WorldPacket packet(SMSG_CORPSE_RECLAIM_DELAY, 5);
        MopDeathPackets::BuildCorpseReclaimDelay(packet, 0x11223344u);
        CHECK(ExpectBytes(packet, { 0x00, 0x44, 0x33, 0x22, 0x11 }));
    }
    {
        WorldPacket packet(SMSG_CORPSE_RECLAIM_DELAY, 1);
        MopDeathPackets::BuildCorpseReclaimDelay(packet, 0);
        CHECK(ExpectBytes(packet, { 0x80 }));
    }
}

static void test_forced_reactions()
{
    {
        WorldPacket packet(SMSG_SET_FORCED_REACTIONS, 17);
        std::vector<MopReputationPackets::ForcedReaction> reactions = {
            { 0x11223344u, 0x55667788u },
            { 0xA1B2C3D4u, 0x10203040u }
        };
        CHECK(MopReputationPackets::BuildForcedReactions(packet, reactions));
        CHECK(ExpectBytes(packet, {
            0x80,
            0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
            0xD4, 0xC3, 0xB2, 0xA1, 0x40, 0x30, 0x20, 0x10
        }));
    }
    {
        WorldPacket packet(SMSG_SET_FORCED_REACTIONS, 1);
        std::vector<MopReputationPackets::ForcedReaction> reactions(4);
        CHECK(!MopReputationPackets::BuildForcedReactions(packet, reactions));
        CHECK(packet.empty());
    }
}

static void test_item_time_update()
{
    {
        WorldPacket packet(SMSG_ITEM_TIME_UPDATE, 13);
        MopItemPackets::BuildItemTimeUpdate(packet, 0x0807060504030201ull,
            0x11223344u);
        CHECK(ExpectBytes(packet, {
            0xFF,
            0x02, 0x06, 0x09, 0x04, 0x00, 0x05, 0x07, 0x03,
            0x44, 0x33, 0x22, 0x11
        }));
    }
    {
        WorldPacket packet(SMSG_ITEM_TIME_UPDATE, 7);
        MopItemPackets::BuildItemTimeUpdate(packet, 0x0000060000000001ull,
            0x11223344u);
        CHECK(ExpectBytes(packet, {
            0x82, 0x00, 0x07, 0x44, 0x33, 0x22, 0x11
        }));
    }
}

static void test_item_enchant_time_update()
{
    {
        WorldPacket packet(SMSG_ITEM_ENCHANT_TIME_UPDATE, 26);
        MopItemPackets::BuildItemEnchantTimeUpdate(packet,
            0x1817161514131211ull, 0x0807060504030201ull,
            0xA1B2C3D4u, 0x11223344u);
        CHECK(ExpectBytes(packet, {
            0xFF, 0xFF,
            0xD4, 0xC3, 0xB2, 0xA1,
            0x14, 0x12, 0x07, 0x04, 0x16, 0x03, 0x10, 0x13,
            0x06, 0x02, 0x19, 0x00, 0x05, 0x09, 0x15, 0x17,
            0x44, 0x33, 0x22, 0x11
        }));
    }
    {
        WorldPacket packet(SMSG_ITEM_ENCHANT_TIME_UPDATE, 14);
        MopItemPackets::BuildItemEnchantTimeUpdate(packet,
            0x0000000014001200ull, 0x0800000500000000ull,
            0xA1B2C3D4u, 0x11223344u);
        CHECK(ExpectBytes(packet, {
            0xA0, 0x03,
            0xD4, 0xC3, 0xB2, 0xA1,
            0x04, 0x13, 0x09, 0x15,
            0x44, 0x33, 0x22, 0x11
        }));
    }
}

static void test_opcode_values_are_framable()
{
    struct ExpectedOpcode { uint32_t actual; uint32_t expected; };
    ExpectedOpcode const values[] = {
        { uint32_t(SMSG_INITIAL_SPELLS), 0x045Au },
        { uint32_t(SMSG_SEND_UNLEARN_SPELLS), 0x10F1u },
        { uint32_t(SMSG_ACTION_BUTTONS), 0x081Au },
        { uint32_t(SMSG_ACCOUNT_DATA_TIMES), 0x162Bu },
        { uint32_t(SMSG_FEATURE_SYSTEM_STATUS), 0x16BBu },
        { uint32_t(SMSG_INITIALIZE_FACTIONS), 0x0AAAu },
        { uint32_t(SMSG_TUTORIAL_FLAGS), 0x1B90u },
        { uint32_t(SMSG_BINDPOINTUPDATE), 0x0E3Bu },
        { uint32_t(CMSG_BINDER_ACTIVATE), 0x1248u },
        { uint32_t(SMSG_BINDER_CONFIRM), 0x1287u },
        { uint32_t(SMSG_PLAYERBOUND), 0x088Eu },
        { uint32_t(SMSG_SET_PROFICIENCY), 0x1440u },
        { uint32_t(SMSG_WEATHER), 0x06ABu },
        { uint32_t(SMSG_MOTD), 0x183Bu },
        { uint32_t(SMSG_CORPSE_RECLAIM_DELAY), 0x022Au },
        { uint32_t(SMSG_SET_FORCED_REACTIONS), 0x068Fu },
        { uint32_t(SMSG_ITEM_TIME_UPDATE), 0x18C1u },
        { uint32_t(SMSG_ITEM_ENCHANT_TIME_UPDATE), 0x10A2u }
    };

    for (ExpectedOpcode const& value : values)
    {
        CHECK(value.actual == value.expected);
        CHECK(value.actual <= 0x1FFFu);
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    test_initial_spells();
    test_send_unlearn_spells();
    test_action_buttons();
    test_account_data_times();
    test_feature_system_status();
    test_initialize_factions();
    test_tutorial_flags();
    test_bind_point_update();
    test_binder_activate();
    test_binder_confirm();
    test_player_bound();
    test_set_proficiency();
    test_weather();
    test_motd();
    test_corpse_reclaim_delay();
    test_forced_reactions();
    test_item_time_update();
    test_item_enchant_time_update();
    test_opcode_values_are_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_initial_packets: all checks passed\n");
    return 0;
}
