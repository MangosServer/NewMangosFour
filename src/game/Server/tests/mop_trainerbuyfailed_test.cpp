/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * Byte-level assertion of the SMSG_TRAINER_BUY_FAILED (0x042E) body against the
 * layout recovered from the client's reader sub_6D369B.
 *
 * There is no way to run a 5.4.8 client against this server in the build
 * environment, so this test IS the verification: it pins the recovered wire
 * format as executable expectations. If the format was recovered wrongly, this
 * test is wrong in the same way -- it proves the encoder matches the recorded
 * reading of the reader, not that the reading itself is correct. The reading is
 * documented in claude/successors/WIRE-FORMAT.md with the decompilation
 * preserved alongside it.
 */

#include "MopTrainerBuyFailed.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <cstdio>
#include <cstdint>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if(!(c)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#c); ++g_fail; } } while(0)

static bool BytesEqual(WorldPacket const& p, std::vector<uint8_t> const& want)
{
    if (p.size() != want.size())
    {
        std::fprintf(stderr, "  size %u, wanted %u\n", (unsigned)p.size(), (unsigned)want.size());
        return false;
    }
    for (size_t i = 0; i < want.size(); ++i)
    {
        if (p.contents()[i] != want[i])
        {
            std::fprintf(stderr, "  byte %u = 0x%02X, wanted 0x%02X\n",
                         (unsigned)i, p.contents()[i], want[i]);
            return false;
        }
    }
    return true;
}

/**
 * All eight guid bytes non-zero, so every mask bit is 1 and every byte is
 * emitted. This pins the mask bit order and both byte-group orders at once.
 *
 * guid 0x0123456789ABCDEF -> byte0=0xEF byte1=0xCD byte2=0xAB byte3=0x89
 *                            byte4=0x67 byte5=0x45 byte6=0x23 byte7=0x01
 */
static void test_all_bytes_present()
{
    WorldPacket p(SMSG_TRAINER_BUY_FAILED, 17);
    MopTrainerBuyFailed::Build(p, 0x0123456789ABCDEFull,
                               MopTrainerBuyFailed::REASON_NOT_ENOUGH_MONEY, 0x1234);

    const std::vector<uint8_t> want = {
        0xFF,                                  // mask: all eight bits set
        0xCC, 0xAA, 0xEE, 0x88, 0x66,          // bytes 1,2,0,3,4 each XOR 1
        0x01, 0x00, 0x00, 0x00,                // uint32 reason = 1 (not enough money)
        0x44, 0x22, 0x00,                      // bytes 5,6,7 each XOR 1
        0x34, 0x12, 0x00, 0x00                 // uint32 serviceId = 0x1234
    };
    CHECK(BytesEqual(p, want));
    CHECK(p.GetOpcode() == SMSG_TRAINER_BUY_FAILED);
}

/**
 * Only byte 0 non-zero. Exercises the skip path: seven mask bits clear and only
 * one guid byte on the wire. Mask order 3,0,4,7,6,1,5,2 puts byte 0 in the
 * SECOND bit position, and bits are written MSB-first, so the mask byte is 0x40.
 */
static void test_sparse_guid()
{
    WorldPacket p(SMSG_TRAINER_BUY_FAILED, 11);
    MopTrainerBuyFailed::Build(p, 0x00000000000000FFull,
                               MopTrainerBuyFailed::REASON_UNAVAILABLE, 0x1234);

    const std::vector<uint8_t> want = {
        0x40,                                  // only the byte-0 mask bit, second position
        0xFE,                                  // byte 0 (0xFF ^ 1); bytes 1..4 skipped
        0x00, 0x00, 0x00, 0x00,                // uint32 reason = 0 (unavailable)
                                               // bytes 5,6,7 all zero -> skipped
        0x34, 0x12, 0x00, 0x00                 // uint32 serviceId = 0x1234
    };
    CHECK(BytesEqual(p, want));
}

/** A zero guid emits a zero mask byte and no guid bytes at all. */
static void test_zero_guid()
{
    WorldPacket p(SMSG_TRAINER_BUY_FAILED, 9);
    MopTrainerBuyFailed::Build(p, 0ull, MopTrainerBuyFailed::REASON_UNAVAILABLE, 0);

    const std::vector<uint8_t> want = {
        0x00,                                  // no mask bits set
        0x00, 0x00, 0x00, 0x00,                // reason
        0x00, 0x00, 0x00, 0x00                 // serviceId
    };
    CHECK(BytesEqual(p, want));
}

/**
 * The reason encoding is the defect most likely to be silently reintroduced,
 * because the server's own historical convention was the reverse. Pin it.
 */
static void test_reason_encoding_is_client_side()
{
    CHECK(MopTrainerBuyFailed::REASON_UNAVAILABLE == 0);
    CHECK(MopTrainerBuyFailed::REASON_NOT_ENOUGH_MONEY == 1);

    WorldPacket p(SMSG_TRAINER_BUY_FAILED, 9);
    MopTrainerBuyFailed::Build(p, 0ull, MopTrainerBuyFailed::REASON_NOT_ENOUGH_MONEY, 7);
    // reason occupies bytes 1..4 when the guid is zero
    CHECK(p.contents()[1] == 0x01);
    // serviceId follows immediately, since no guid bytes are emitted
    CHECK(p.contents()[5] == 0x07);
}

/** The opcode must be framable: MoP packs (size << 13) | (cmd & 0x1FFF). */
static void test_opcode_is_framable()
{
    CHECK(uint32(SMSG_TRAINER_BUY_FAILED) <= 0x1FFF);
    CHECK(uint32(SMSG_TRAINER_BUY_FAILED) == 0x042E);
}

int main()
{
    test_all_bytes_present();
    test_sparse_guid();
    test_zero_guid();
    test_reason_encoding_is_client_side();
    test_opcode_is_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    std::printf("mop_trainerbuyfailed: all checks passed\n");
    return 0;
}
