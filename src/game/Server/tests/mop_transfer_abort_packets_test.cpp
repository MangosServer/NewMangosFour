/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(WorldPacket const& packet, std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
        return false;
    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;
    return true;
}

static void test_error_without_argument()
{
    WorldPacket packet(SMSG_TRANSFER_ABORTED, 5);
    MopTransferPackets::BuildTransferAborted(
        packet, 0x11223344u, TRANSFER_ABORT_ERROR, 0);
    CHECK(ExpectBytes(packet, { 0x94, 0x44, 0x33, 0x22, 0x11 }));
}

static void test_difficulty_with_argument()
{
    WorldPacket packet(SMSG_TRANSFER_ABORTED, 6);
    MopTransferPackets::BuildTransferAborted(
        packet, 0x11223344u, TRANSFER_ABORT_DIFFICULTY, 3);
    CHECK(ExpectBytes(packet, { 0x50, 0x03, 0x44, 0x33, 0x22, 0x11 }));
}

static void test_max_players_without_argument()
{
    WorldPacket packet(SMSG_TRANSFER_ABORTED, 5);
    MopTransferPackets::BuildTransferAborted(
        packet, 0x11223344u, TRANSFER_ABORT_MAX_PLAYERS, 0);
    CHECK(ExpectBytes(packet, { 0xDC, 0x44, 0x33, 0x22, 0x11 }));
}

int main(int /*argc*/, char** /*argv*/)
{
    CHECK(SMSG_TRANSFER_ABORTED == 0x0C8F);
    test_error_without_argument();
    test_difficulty_with_argument();
    test_max_players_without_argument();
    return g_fail == 0 ? 0 : 1;
}
