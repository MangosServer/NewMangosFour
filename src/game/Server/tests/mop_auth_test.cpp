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

#include "MopAuthSession.h"
#include "Utilities/ByteBuffer.h"
#include <cstdio>
#include <type_traits>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

// The legacy 11-bit name length is read MSB-first across two bytes with no leading ReadBit(),
// so the encoded value is (bits0 << 3) | (bits1 >> 5). Vectors below are hand-encoded to that.
static ByteBuffer make_legacy_body(uint32_t addonSize, uint32_t inflatedSize,
                                   uint8_t nameBits0, uint8_t nameBits1,
                                   char const* name, size_t nameBytes)
{
    ByteBuffer in;
    for (size_t i = 0; i < 52; ++i)
    {
        in << uint8_t(0);
    }
    in << addonSize;
    if (addonSize >= 4)
    {
        in << inflatedSize;
    }
    for (uint32_t i = 4; i < addonSize; ++i)
    {
        in << uint8_t(0);
    }
    in << nameBits0 << nameBits1;
    if (nameBytes)
    {
        in.append(reinterpret_cast<uint8_t const*>(name), nameBytes);
    }
    return in;
}

static MopAuth::DecodeResult decode(ByteBuffer& in)
{
    MopAuth::AuthSessionFields out{};
    return MopAuth::DecodeAuthSession(in, out);
}

static void test_short_body_rejected()
{
    ByteBuffer in;
    in << uint32_t(0);
    MopAuth::AuthSessionFields out{};
    CHECK(MopAuth::DecodeAuthSession(in, out) == MopAuth::DecodeResult::ShortBody);
}

// Structure only: the legacy decode is bug-compatible, so field values are deliberately not asserted.
static void test_exact_valid_structure_accepted()
{
    ByteBuffer in = make_legacy_body(4, 1, 0x00, 0x20, "A", 1);   // 11-bit name length 1
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}

static void test_prefix_one_byte_short_rejected()
{
    ByteBuffer in;
    for (size_t i = 0; i < 55; ++i)                               // 55 < LegacyFixedPrefixBytes (56)
    {
        in << uint8_t(0);
    }
    CHECK(decode(in) == MopAuth::DecodeResult::ShortBody);
}

static void test_addon_size_below_minimum_rejected()
{
    ByteBuffer in = make_legacy_body(0, 0, 0x00, 0x20, "A", 1);   // addon size 0 cannot hold its own header
    CHECK(decode(in) == MopAuth::DecodeResult::BadAddonSize);
}

// Built by hand so the helper cannot accidentally supply the bytes this bound is meant to reject.
static void test_addon_size_beyond_remaining_rejected()
{
    ByteBuffer in;
    for (size_t i = 0; i < 52; ++i)
    {
        in << uint8_t(0);
    }
    in << uint32_t(1000);                                         // claims 1000 bytes ...
    in << uint32_t(1);                                            // ... but only 4 follow
    CHECK(decode(in) == MopAuth::DecodeResult::BadAddonSize);
}

static void test_inflated_size_zero_rejected()
{
    ByteBuffer in = make_legacy_body(4, 0, 0x00, 0x20, "A", 1);
    CHECK(decode(in) == MopAuth::DecodeResult::BadInflatedAddonSize);
}

static void test_inflated_size_over_maximum_rejected()
{
    ByteBuffer in = make_legacy_body(4, 0x100000, 0x00, 0x20, "A", 1);  // one over MaxInflatedAddonBytes
    CHECK(decode(in) == MopAuth::DecodeResult::BadInflatedAddonSize);
}

static void test_inflated_size_at_maximum_accepted()
{
    ByteBuffer in = make_legacy_body(4, MopAuth::MaxInflatedAddonBytes, 0x00, 0x20, "A", 1);
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}

// Built by hand: the helper always emits the two name-length bytes.
static void test_missing_name_bitfield_rejected()
{
    ByteBuffer in;
    for (size_t i = 0; i < 52; ++i)
    {
        in << uint8_t(0);
    }
    in << uint32_t(4);
    in << uint32_t(1);                                            // addon blob, then nothing at all
    CHECK(decode(in) == MopAuth::DecodeResult::ShortBody);

    ByteBuffer partial;
    for (size_t i = 0; i < 52; ++i)
    {
        partial << uint8_t(0);
    }
    partial << uint32_t(4);
    partial << uint32_t(1);
    partial << uint8_t(0);                                        // one of the two bytes present
    CHECK(decode(partial) == MopAuth::DecodeResult::ShortBody);
}

static void test_name_length_zero_rejected()
{
    ByteBuffer in = make_legacy_body(4, 1, 0x00, 0x00, "A", 1);   // 11-bit name length 0
    CHECK(decode(in) == MopAuth::DecodeResult::BadNameLength);
}

static void test_name_length_over_maximum_rejected()
{
    ByteBuffer in = make_legacy_body(4, 1, 0x02, 0x20, "AAAAAAAAAAAAAAAAA", 17);  // length 17 > 16
    CHECK(decode(in) == MopAuth::DecodeResult::BadNameLength);
}

static void test_name_length_at_maximum_accepted()
{
    ByteBuffer in = make_legacy_body(4, 1, 0x02, 0x00, "AAAAAAAAAAAAAAAA", 16);   // length 16 == max
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}

// ReadString() truncates silently at end-of-buffer; the decoder must reject rather than shorten.
static void test_name_truncated_rejected()
{
    ByteBuffer in = make_legacy_body(4, 1, 0x00, 0x40, "A", 1);   // length 2, one byte remaining
    CHECK(decode(in) == MopAuth::DecodeResult::TruncatedName);
}

static_assert(std::is_same<decltype(MopAuth::AuthSessionFields{}.builtNumberClient), uint16_t>::value,
              "auth build field must stay 16-bit");

static void test_wire_field_widths()
{
    CHECK(sizeof(MopAuth::AuthSessionFields{}.builtNumberClient) == 2);
}

// NOTE: linking 'shared' drags in ACE, whose OS_main.h rewrites main() to ace_main_i() and
// requires the (int, char**) signature. A no-argument main() therefore fails to link (LNK2019).
int main(int /*argc*/, char** /*argv*/)
{
    test_short_body_rejected();
    test_exact_valid_structure_accepted();
    test_prefix_one_byte_short_rejected();
    test_addon_size_below_minimum_rejected();
    test_addon_size_beyond_remaining_rejected();
    test_inflated_size_zero_rejected();
    test_inflated_size_over_maximum_rejected();
    test_inflated_size_at_maximum_accepted();
    test_missing_name_bitfield_rejected();
    test_name_length_zero_rejected();
    test_name_length_over_maximum_rejected();
    test_name_length_at_maximum_accepted();
    test_name_truncated_rejected();
    test_wire_field_widths();
    std::printf(g_fail ? "FAILED (%d)\n" : "OK\n", g_fail);
    return g_fail ? 1 : 0;
}
