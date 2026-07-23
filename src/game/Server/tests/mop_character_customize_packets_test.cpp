/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "Player.h"
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

static void TestCustomizeRequest()
{
    WorldPacket packet(CMSG_CHAR_CUSTOMIZE, 15);
    uint8 const body[] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
        0xD4, 0x24, 0x41, 0x62, 0x00, 0x02, 0x06, 0x07, 0x05
    };
    packet.append(body, sizeof(body));

    MopCharacterCustomizePackets::Request const request =
        MopCharacterCustomizePackets::ReadRequest(packet);
    CHECK(request.guid == ObjectGuid(UINT64_C(0x0007060004030001)));
    CHECK(request.name == "Ab");
    CHECK(request.appearance.hairStyle == 0x11);
    CHECK(request.appearance.gender == 0x22);
    CHECK(request.appearance.skin == 0x33);
    CHECK(request.appearance.facialHair == 0x44);
    CHECK(request.appearance.face == 0x55);
    CHECK(request.appearance.hairColor == 0x66);
}

static MopCharacterCustomizePackets::Appearance MakeAppearance()
{
    MopCharacterCustomizePackets::Appearance appearance;
    appearance.hairStyle = 0x11;
    appearance.gender = 0x22;
    appearance.skin = 0x33;
    appearance.facialHair = 0x44;
    appearance.face = 0x55;
    appearance.hairColor = 0x66;
    return appearance;
}

static void TestCustomizeSuccess()
{
    WorldPacket packet;
    MopCharacterCustomizePackets::BuildResponse(packet, 0,
        ObjectGuid(UINT64_C(0x0007060004030001)), MakeAppearance(), "Ab");

    CHECK(packet.GetOpcode() == SMSG_CHAR_CUSTOMIZE);
    CHECK(BytesEqual(packet, {
        0xBC, 0x00, 0x44, 0x33, 0x22, 0x11, 0x55, 0x66,
        0x05, 0x07, 0x02, 0x06, 0x00, 0x08, 0x41, 0x62
    }));
}

static void TestCustomizeError()
{
    WorldPacket packet;
    MopCharacterCustomizePackets::BuildResponse(packet, 0x32,
        ObjectGuid(UINT64_C(0x0007060004030001)), MakeAppearance(), "ignored");

    CHECK(packet.GetOpcode() == SMSG_CHAR_CUSTOMIZE);
    CHECK(BytesEqual(packet, { 0xBC, 0x32, 0x05, 0x07, 0x02, 0x06, 0x00 }));
}

int main(int /*argc*/, char** /*argv*/)
{
    TestCustomizeRequest();
    TestCustomizeSuccess();
    TestCustomizeError();
    return g_fail ? 1 : 0;
}
