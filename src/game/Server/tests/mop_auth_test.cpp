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
#include "MopSocketDrain.h"
#include "Utilities/ByteBuffer.h"
#include <cstdio>
#include <string>
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
        in << inflatedSize;                                       // blob's self-described size
        for (uint32_t i = 4; i < addonSize; ++i)
        {
            in << uint8_t(0);
        }
    }
    else
    {
        // A blob too small to carry a 4-byte header still has to be emitted at its stated length,
        // or the decoder would read the name bits as addon bytes and the vector would prove nothing.
        for (uint32_t i = 0; i < addonSize; ++i)
        {
            in << uint8_t(0);
        }
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

// An addon blob too small to carry a 4-byte header is NOT malformed. The legacy inline parser did
//     addonsData.resize(m_addonSize);
//     recvPacket.read((uint8*)addonsData.contents(), m_addonSize);
// which for 0..3 is a no-op, never a rejection -- and the consumer agrees, since ReadAddonsInfo
// bails benignly via "if (data.rpos() + 4 > data.size()) { return; }". Each must decode to Ok with
// addonData at exactly the stated length, and must still go on to read the account name.
static void test_small_addon_sizes_accepted()
{
    for (uint32_t addonSize = 0; addonSize < 4; ++addonSize)
    {
        ByteBuffer in = make_legacy_body(addonSize, 0, 0x00, 0x20, "A", 1);
        MopAuth::AuthSessionFields out{};
        CHECK(MopAuth::DecodeAuthSession(in, out) == MopAuth::DecodeResult::Ok);
        CHECK(out.addonSize == addonSize);
        CHECK(out.addonData.size() == addonSize);
        CHECK(out.account == "A");                                // the name read is still reached
    }
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

// The embedded inflated size must NOT decide authentication. The real consumer,
// WorldSession::ReadAddonsInfo (WorldSession.cpp:1329-1341), treats size 0 as "no addon info"
// and simply returns -- it does not reject the login. Auth must be equally tolerant.
static void test_inflated_size_zero_accepted()
{
    ByteBuffer in = make_legacy_body(4, 0, 0x00, 0x20, "A", 1);
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}

// Same class: ReadAddonsInfo logs "addon info too big" and returns for size > 0xFFFFF. That is a
// skipped addon parse, not a failed authentication.
static void test_inflated_size_over_maximum_accepted()
{
    ByteBuffer in = make_legacy_body(4, 0x100000, 0x00, 0x20, "A", 1);
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}

static void test_inflated_size_at_maximum_accepted()
{
    ByteBuffer in = make_legacy_body(4, 0xFFFFF, 0x00, 0x20, "A", 1);
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}

// The bound that DOES matter and must stay: the outer addonSize is attacker-controlled and drives
// a resize(), so it is still checked against the bytes actually present.
static void test_outer_addon_size_still_bounds_allocation()
{
    ByteBuffer in;
    for (size_t i = 0; i < 52; ++i)
    {
        in << uint8_t(0);
    }
    in << uint32_t(0xFFFFFFFF);                                   // claims 4GB ...
    in << uint32_t(1);                                            // ... but only 4 bytes follow
    CHECK(decode(in) == MopAuth::DecodeResult::BadAddonSize);
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

// 129 > MaxAccountNameBytes (MAX_ACCOUNT_STR * 4): longer than any username AccountMgr can create
// even if every character were a 4-byte code point, so bounding the allocation is legitimate. The
// 11-bit field would otherwise permit 2047.
static void test_name_length_over_maximum_rejected()
{
    std::string const name(129, 'A');
    ByteBuffer in = make_legacy_body(4, 1, 0x10, 0x20, name.c_str(), name.size());
    CHECK(decode(in) == MopAuth::DecodeResult::BadNameLength);
}

// 128 == MAX_ACCOUNT_STR * 4: the byte cap itself. A 32-character username of 4-byte code points
// encodes to exactly this, so it must decode.
static void test_name_length_at_byte_cap_accepted()
{
    std::string const name(128, 'A');
    ByteBuffer in = make_legacy_body(4, 1, 0x10, 0x00, name.c_str(), name.size());
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}

// 32 == MAX_ACCOUNT_STR: the longest ASCII username AccountMgr will create.
static void test_name_length_max_account_chars_accepted()
{
    ByteBuffer in = make_legacy_body(4, 1, 0x04, 0x00,
                                     "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 32);
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}

// The multi-byte lockout: MAX_ACCOUNT_STR is a CHARACTER limit applied as
// utf8length(username) > MAX_ACCOUNT_STR (AccountMgr.cpp:101), and utf8length is utf8::distance
// (Util.cpp:544-547), which counts code points -- so e.g. a 20-character Cyrillic username is 40
// bytes and is perfectly creatable. A 32-BYTE cap would reject it, exactly as the original 16-byte
// cap locked out 17..32 character ASCII accounts. Legacy capped nothing at all.
static void test_name_length_thirty_three_accepted()
{
    std::string const name(33, 'A');
    ByteBuffer in = make_legacy_body(4, 1, 0x04, 0x20, name.c_str(), name.size());
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}

// The regression a 16-byte cap actually caused: AccountMgr accepts usernames up to
// MAX_ACCOUNT_STR (32) and the legacy inline parser capped nothing at all, so every existing
// account of 17..32 characters would have been locked out with BadNameLength once this path
// activates. Valid input must decode exactly as it did before the extraction.
static void test_name_length_seventeen_accepted()
{
    ByteBuffer in = make_legacy_body(4, 1, 0x02, 0x20, "AAAAAAAAAAAAAAAAA", 17);
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}

// ReadString() truncates silently at end-of-buffer; the decoder must reject rather than shorten.
static void test_name_truncated_rejected()
{
    ByteBuffer in = make_legacy_body(4, 1, 0x00, 0x40, "A", 1);   // length 2, one byte remaining
    CHECK(decode(in) == MopAuth::DecodeResult::TruncatedName);
}

// ---------------------------------------------------------------------------------------------
// Auth-error drain decisions (MopSocketDrain.h)
//
// These cover the two pure decisions only. They are NOT a test of the ACE peer/reactor lifecycle:
// that the production socket calls them in the right places is established by inspection, not here.
// ---------------------------------------------------------------------------------------------

// While Open, traffic is ordinary and must keep flowing.
static void test_open_state_processes_input()
{
    CHECK(MopSock::MayProcessInput(MopSock::DrainState::Open) == true);
}

// The core of the quiesce: MopFrameReader may have already coalesced a further frame into the
// buffer before we decided to reject. Once Flushing, that frame must never be acted upon.
static void test_flushing_state_rejects_coalesced_next_frame()
{
    CHECK(MopSock::MayProcessInput(MopSock::DrainState::Flushing) == false);
}

// The bug this whole task exists to fix: closing while bytes are still buffered discards the
// auth error response, leaving the peer with a bare TCP close and no reason for the rejection.
static void test_buffered_output_prevents_close()
{
    CHECK(MopSock::ShouldCloseNow(MopSock::DrainState::Flushing, 7, true, false) == false);
}

// Same, for output that overflowed the buffer and went to the message queue instead.
static void test_queued_output_prevents_close()
{
    CHECK(MopSock::ShouldCloseNow(MopSock::DrainState::Flushing, 0, false, false) == false);
}

// Neither buffer nor queue holds anything: the response is on the wire, so the socket may go.
static void test_fully_drained_flushing_closes()
{
    CHECK(MopSock::ShouldCloseNow(MopSock::DrainState::Flushing, 0, true, false) == true);
}

// The dequeue_head()/send() window. handle_output_queue() removes the block from the queue and
// only then calls send(), so mid-send the buffer is empty AND the queue is empty while the
// response is still nothing but a local pointer. That combination previously read as "fully
// drained" and closed the socket, discarding the auth response -- reintroducing the exact bug the
// drain exists to fix. An in-flight write must veto the close on its own.
static void test_in_flight_send_prevents_close()
{
    CHECK(MopSock::ShouldCloseNow(MopSock::DrainState::Flushing, 0, true, true) == false);
}

// In-flight vetoes regardless of what else is pending.
static void test_in_flight_send_vetoes_with_pending_output()
{
    CHECK(MopSock::ShouldCloseNow(MopSock::DrainState::Flushing, 7, true, true) == false);
    CHECK(MopSock::ShouldCloseNow(MopSock::DrainState::Flushing, 0, false, true) == false);
}

// A healthy idle socket is drained by definition; it must never be closed on that basis alone.
static void test_open_state_never_closes()
{
    CHECK(MopSock::ShouldCloseNow(MopSock::DrainState::Open, 0, true, false) == false);
    CHECK(MopSock::ShouldCloseNow(MopSock::DrainState::Open, 7, false, false) == false);
    CHECK(MopSock::ShouldCloseNow(MopSock::DrainState::Open, 0, true, true) == false);
}

// Pins the rule handle_input_missing_data() RETURNS -- WorldSocket.cpp calls MopSock::InputStatus
// directly and open-codes nothing, so this test covers production rather than a copy of it.
// A full read normally reports 1, which ACE_TP_Reactor::dispatch_socket_event turns into an
// immediate re-invocation of handle_input ("while (status > 0) status = (event_handler->*callback)
// (handle)") without consulting the wait set -- so while draining, a full read must NOT report 1
// or a rejected peer keeps being served.
static void test_drain_never_requests_reactor_reentry()
{
    CHECK(MopSock::InputStatus(MopSock::DrainState::Open, true) == 1);      // healthy full read: keep reading
    CHECK(MopSock::InputStatus(MopSock::DrainState::Open, false) == 2);     // healthy partial read: stop
    CHECK(MopSock::InputStatus(MopSock::DrainState::Flushing, true) != 1);  // draining: never ask to be re-called
    CHECK(MopSock::InputStatus(MopSock::DrainState::Flushing, false) != 1);
}

// A fresh socket must start Open, or the first Update() would tear down a healthy connection.
static_assert(MopSock::DrainState{} == MopSock::DrainState::Open,
              "value-initialized DrainState must be Open");

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
    test_small_addon_sizes_accepted();
    test_addon_size_beyond_remaining_rejected();
    test_inflated_size_zero_accepted();
    test_inflated_size_over_maximum_accepted();
    test_inflated_size_at_maximum_accepted();
    test_outer_addon_size_still_bounds_allocation();
    test_missing_name_bitfield_rejected();
    test_name_length_zero_rejected();
    test_name_length_over_maximum_rejected();
    test_name_length_at_byte_cap_accepted();
    test_name_length_max_account_chars_accepted();
    test_name_length_thirty_three_accepted();
    test_name_length_seventeen_accepted();
    test_name_truncated_rejected();
    test_wire_field_widths();
    test_open_state_processes_input();
    test_flushing_state_rejects_coalesced_next_frame();
    test_buffered_output_prevents_close();
    test_queued_output_prevents_close();
    test_fully_drained_flushing_closes();
    test_in_flight_send_prevents_close();
    test_in_flight_send_vetoes_with_pending_output();
    test_open_state_never_closes();
    test_drain_never_requests_reactor_reentry();
    std::printf(g_fail ? "FAILED (%d)\n" : "OK\n", g_fail);
    return g_fail ? 1 : 0;
}
