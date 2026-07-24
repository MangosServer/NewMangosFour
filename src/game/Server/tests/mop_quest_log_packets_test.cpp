/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft,
 * supporting 5.4.8.18414.
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for directly verified 5.4.8 quest-log requests.
 */

#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <initializer_list>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket MakePacket(std::initializer_list<uint8_t> bytes)
{
    WorldPacket packet(CMSG_QUESTLOG_REMOVE_QUEST, bytes.size());
    for (uint8_t byte : bytes)
        packet << byte;
    return packet;
}

static void TestRemoveQuestRequest()
{
    uint8 slot = 0xFF;
    WorldPacket firstSlot = MakePacket({ 0x00 });
    CHECK(MopQuestPackets::ParseQuestLogRemoveRequest(firstSlot, slot));
    CHECK(slot == 0);
    CHECK(firstSlot.rpos() == firstSlot.size());

    WorldPacket lastSlot = MakePacket({ 0x18 });
    CHECK(MopQuestPackets::ParseQuestLogRemoveRequest(lastSlot, slot));
    CHECK(slot == 24);
    CHECK(lastSlot.rpos() == lastSlot.size());

    WorldPacket wireValidButOutOfRange = MakePacket({ 0xFF });
    CHECK(MopQuestPackets::ParseQuestLogRemoveRequest(
        wireValidButOutOfRange, slot));
    CHECK(slot == 0xFF);
    CHECK(wireValidButOutOfRange.rpos() ==
        wireValidButOutOfRange.size());
}

static void TestMalformedRemoveQuestRequest()
{
    uint8 slot = 0x7A;
    WorldPacket truncated = MakePacket({});
    CHECK(!MopQuestPackets::ParseQuestLogRemoveRequest(truncated, slot));
    CHECK(slot == 0);
    CHECK(truncated.rpos() == truncated.size());

    slot = 0x7A;
    WorldPacket trailing = MakePacket({ 0x03, 0xAA });
    CHECK(!MopQuestPackets::ParseQuestLogRemoveRequest(trailing, slot));
    CHECK(slot == 0);
    CHECK(trailing.rpos() == trailing.size());
}

static void TestOpcodeValue()
{
    CHECK(uint32(CMSG_QUESTLOG_REMOVE_QUEST) == 0x0779u);
    CHECK(uint32(CMSG_QUESTLOG_REMOVE_QUEST) <
        uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    TestRemoveQuestRequest();
    TestMalformedRemoveQuestRequest();
    TestOpcodeValue();
    return g_fail ? 1 : 0;
}
