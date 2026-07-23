/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 */

#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_empty_unlocked_journal()
{
    WorldPacket packet;
    MopBattlePetPackets::BuildEmptyJournal(packet);

    uint8 const expected[] = {
        0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0xFF, 0xFF
    };

    CHECK(packet.GetOpcode() == SMSG_BATTLE_PET_JOURNAL);
    CHECK(packet.size() == sizeof(expected));
    CHECK(std::memcmp(packet.contents(), expected, sizeof(expected)) == 0);
}

static void test_opcode_values_are_framable()
{
    CHECK(uint32(CMSG_BATTLE_PET_REQUEST_JOURNAL) == 0x0A23u);
    CHECK(uint32(SMSG_BATTLE_PET_JOURNAL) == 0x1542u);
    CHECK(uint32(CMSG_BATTLE_PET_REQUEST_JOURNAL) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_BATTLE_PET_JOURNAL) < uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_empty_unlocked_journal();
    test_opcode_values_are_framable();

    if (g_fail)
        std::fprintf(stderr, "%d battle-pet packet test(s) failed\n", g_fail);
    return g_fail ? 1 : 0;
}
