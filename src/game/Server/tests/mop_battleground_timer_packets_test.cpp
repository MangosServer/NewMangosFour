/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "BattleGround.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool BytesEqual(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    if (packet.size() != expected.size())
        return false;

    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;

    return true;
}

static void TestStartTimer()
{
    WorldPacket packet;
    MopBattleGroundPackets::BuildStartTimer(packet,
        0x11223344, 0x55667788, 0x99AABBCC);

    CHECK(packet.GetOpcode() == SMSG_START_TIMER);
    CHECK(BytesEqual(packet, {
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99
    }));
}

int main(int /*argc*/, char** /*argv*/)
{
    CHECK(CMSG_QUERY_COUNTDOWN_TIMER == 0x044E);
    CHECK(SMSG_START_TIMER == 0x0E3F);
    TestStartTimer();
    return g_fail ? 1 : 0;
}
