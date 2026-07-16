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
#include "Auth/MopAuthKey.h"
#include "Auth/AuthCrypt.h"
#include "Auth/BigNumber.h"
#include <openssl/crypto.h>
#include <cstdio>
#include <cstring>
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

// The 11-bit name length is read MSB-first across two bytes with no leading ReadBit(), so the
// encoded value is (bits0 << 3) | (bits1 >> 5). Derived rather than hand-written: 2047 in
// particular is easy to mis-encode by hand.
static uint8_t name_bits0(uint32_t len)
{
    return uint8_t(len >> 3);
}

static uint8_t name_bits1(uint32_t len)
{
    return uint8_t((len & 7) << 5);
}

// The decoder applies NO cap to the name length, because the legacy path applied none:
//     account = recvPacket.ReadString(recvPacket.ReadBits(11));
// It took whatever the 11-bit field said and carried on to the build check and the account
// lookup. Every structurally well-formed length must therefore decode to Ok:
//   0     -> ReadString(0) returns "" and cannot throw (ByteBuffer.h:1005-1013); legacy then
//            reached IsAcceptableClientBuild and the account query, yielding a build mismatch or
//            AUTH_UNKNOWN_ACCOUNT -- NOT the AUTH_FAILED a decode rejection would short-circuit to.
//   17,32 -> ordinary ASCII usernames AccountMgr creates.
//   33+   -> a username of multi-byte code points. AccountMgr's own username limit is a CHARACTER
//            count, applied as utf8length(username) (AccountMgr.cpp:101), so a perfectly ordinary
//            account can encode to far more bytes than characters.
//   2047  -> the 11-bit maximum, the true worst case; ReadString self-bounds on size() regardless.
// Nothing downstream needs a cap: ReadString cannot read past the buffer whatever the count says,
// and TruncatedName below already rejects a length exceeding the bytes actually present.
static void test_name_lengths_uncapped()
{
    static uint32_t const lengths[] = { 0, 1, 17, 32, 33, 128, 129, 2047 };

    for (uint32_t len : lengths)
    {
        std::string const name(len, 'A');
        ByteBuffer in = make_legacy_body(4, 1, name_bits0(len), name_bits1(len),
                                         name.c_str(), name.size());
        MopAuth::AuthSessionFields out{};
        CHECK(MopAuth::DecodeAuthSession(in, out) == MopAuth::DecodeResult::Ok);
        CHECK(out.account.size() == len);
    }
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

// ---------------------------------------------------------------------------------------------
// MopAuth::SessionKeyFromHex -- the canonical account.sessionkey hex -> raw-40 K adapter
// (Auth/MopAuthKey.h / .cpp).
// ---------------------------------------------------------------------------------------------

// The pure primitive: copy little-endian bytes, pad the TAIL. A named function rather than an
// inline memcpy so the tail-vs-head rule has one place to live and one place to be tested.
static void test_raw40_from_littleendian()
{
    // (a) full 40 bytes: a straight copy.
    {
        uint8 le[40];
        for (int i = 0; i < 40; ++i)
        {
            le[i] = uint8(i);
        }
        uint8 out[40] = { 0xEE };
        CHECK(MopAuth::Raw40FromLittleEndian(le, 40, out));
        for (int i = 0; i < 40; ++i)
        {
            CHECK(out[i] == uint8(i));
        }
    }
    // (b) THE TRAP: a short value pads at the TAIL. AsByteArray(40) pads at the HEAD and shifts
    //     every real byte by one.
    {
        uint8 le[39];
        for (int i = 0; i < 39; ++i)
        {
            le[i] = uint8(i);
        }
        uint8 out[40] = { 0xEE };
        CHECK(MopAuth::Raw40FromLittleEndian(le, 39, out));
        for (int i = 0; i < 39; ++i)
        {
            CHECK(out[i] == uint8(i));
        }
        CHECK(out[39] == 0x00);
    }
    // (c) TWO leading zeros -> 38. "LENGTH IN (78,80)" was an incomplete enumeration: there is no
    //     floor. BN_bn2hex emits the BIGNUM's numeric byte count.
    {
        uint8 le[38] = { 0 };
        uint8 out[40] = { 0xEE };
        CHECK(MopAuth::Raw40FromLittleEndian(le, 38, out));
        CHECK(out[38] == 0x00);
        CHECK(out[39] == 0x00);
    }
    // (d) over-long must be REJECTED, never truncated.
    {
        uint8 le[41] = { 0 };
        uint8 out[40] = { 0xEE };
        CHECK(!MopAuth::Raw40FromLittleEndian(le, 41, out));
    }
    // (e) null input rejected.
    {
        uint8 out[40] = { 0xEE };
        CHECK(!MopAuth::Raw40FromLittleEndian(NULL, 0, out));
    }
    // (f) a ZERO-LENGTH decode is REJECTED, not silently zero-filled (spec 6.2a, Codex round 3 H3).
    //     "" is vacuously hex-only, even-length and <= 80; it converts to zero bytes, which would
    //     pad into forty 0x00s -- a K of all zeros that looks well-formed to every consumer
    //     downstream. That is EXACTLY the stale/NULL-K confounder the ladder exists to eliminate.
    {
        uint8 le[1] = { 0 };
        uint8 out[40] = { 0xEE };
        CHECK(!MopAuth::Raw40FromLittleEndian(le, 0, out));
    }
}

// *** THE TEST THAT CATCHES THE DOUBLE REVERSAL. ***
//
// It drives the REAL adapter over the REAL chain, simulating realmd's own write path:
//     realmd:  vK[40] --SetBinary--> BigNumber --AsHexStr--> account.sessionkey
//     world:   hex --SessionKeyFromHex--> canonical raw-40 K
// and asserts the round trip returns vK BYTE FOR BYTE.
//
// This is the ONLY test that can see an endianness mismatch. Plan v2 fed AsByteArray() -- which is
// ALREADY little-endian (BigNumber.cpp:194 std::reverse) -- into a helper that reverses, producing
// a big-endian K, a deterministic proof failure, and a total auth outage. Every hand-crafted
// byte-array test still passed, because none of them ever called BigNumber. The defect lived in the
// SEAM between the helper's contract and the call site, so the test has to span the seam.
static void test_sessionkey_from_hex_roundtrip()
{
    // (a) all 40 bytes significant -> BN_num_bytes() == 40 -> 80 hex chars, the no-pad path.
    {
        uint8 vK[40];
        for (int i = 0; i < 40; ++i)
        {
            vK[i] = uint8(i * 7 + 3);
        }
        CHECK(vK[39] != 0);

        BigNumber bn;                              // exactly what realmd does (AuthSocket.cpp:668)
        bn.SetBinary(vK, 40);
        const char* hex = bn.AsHexStr();           // AuthSocket.cpp:707 -> the DB column
        CHECK(hex != NULL);
        CHECK(std::strlen(hex) == 80);

        uint8 out[40] = { 0xEE };
        CHECK(MopAuth::SessionKeyFromHex(hex, out));
        for (int i = 0; i < 40; ++i)
        {
            CHECK(out[i] == vK[i]);                // a reversed K fails on byte 0
        }
        OPENSSL_free((void*)hex);                  // AsHexStr = BN_bn2hex; the caller frees
    }
    // (b) THE ~1/256 CASE: most-significant raw byte zero -> BN_bn2hex DROPS it -> 78 hex chars.
    //     78 is NORMAL, not corruption. The tail pad must restore vK[39] == 0.
    {
        uint8 vK[40];
        for (int i = 0; i < 40; ++i)
        {
            vK[i] = uint8(i * 7 + 3);
        }
        vK[39] = 0x00;

        BigNumber bn;
        bn.SetBinary(vK, 40);
        const char* hex = bn.AsHexStr();
        CHECK(hex != NULL);
        CHECK(std::strlen(hex) == 78);             // the row IS short, and that is FINE

        uint8 out[40] = { 0xEE };
        CHECK(MopAuth::SessionKeyFromHex(hex, out));
        for (int i = 0; i < 40; ++i)
        {
            CHECK(out[i] == vK[i]);                // incl. out[39] == 0x00, padded at the TAIL
        }
        OPENSSL_free((void*)hex);
    }
    // (c) the adapter REJECTS what it must, at its own boundary.
    {
        uint8 out[40] = { 0xEE };
        CHECK(!MopAuth::SessionKeyFromHex(NULL, out));
        CHECK(!MopAuth::SessionKeyFromHex("", out));                 // empty -> would be K of zeros
        CHECK(!MopAuth::SessionKeyFromHex("ABC", out));              // odd length
        CHECK(!MopAuth::SessionKeyFromHex("ZZZZ", out));             // non-hex
        CHECK(!MopAuth::SessionKeyFromHex(std::string(82, 'A').c_str(), out));   // 41 bytes
    }
}

// ---------------------------------------------------------------------------------------------
// AuthCrypt -- the two-phase Prepare()/Activate() crypt and its MoP world seeds
// (Auth/AuthCrypt.h / .cpp, Auth/ARC4.*, Auth/HMACSHA1.*).
// ---------------------------------------------------------------------------------------------

// AuthCrypt known-answer test. Fixtures from the spec-derived oracle (tools/mop_stage2_fixtures.py),
// which asserts these vectors genuinely discriminate: direction (A != B), the 1024-byte drop
// (drop != no-drop), and short-K (40 bytes != 39). A test that cannot fail on those is decorative.
//
// Seeds are BINARY-CONFIRMED in both client binaries (spec 3.1):
//   A server-encrypt/client-decrypt 08F1959F47E5D2DBA13D778F3F3EE700  Wow-64 0x140F4CCA0 / Wow 0xDC6FD0
//   B server-decrypt/client-encrypt 40AAD392267143473A3108A6E7DC982A  Wow-64 0x140F4CCB0 / Wow 0xDC6FE0
static void test_authcrypt_kat()
{
    static const uint8 kK_FULL[40] = {
        0x03,0x0A,0x11,0x18,0x1F,0x26,0x2D,0x34,0x3B,0x42,0x49,0x50,0x57,0x5E,0x65,0x6C,
        0x73,0x7A,0x81,0x88,0x8F,0x96,0x9D,0xA4,0xAB,0xB2,0xB9,0xC0,0xC7,0xCE,0xD5,0xDC,
        0xE3,0xEA,0xF1,0xF8,0xFF,0x06,0x0D,0x14 };
    static const uint8 kKs_SeedA_serverEncrypt[4] = { 0xF6, 0x05, 0x69, 0xC6 };
    static const uint8 kKs_SeedB_serverDecrypt[4] = { 0x18, 0xEF, 0x76, 0x35 };

    AuthCrypt crypt;
    CHECK(!crypt.IsInitialized());
    CHECK(crypt.IsUsable());                       // RC4 selected + sized by the ctor
    CHECK(crypt.Prepare(kK_FULL));
    crypt.Activate();
    CHECK(crypt.IsInitialized());

    // Encrypting zeros yields the keystream verbatim (ARC4 is XOR), so this pins seed A's
    // direction AND the drop-1024 in one assertion. The return value is CHECKED: a silent false
    // would leave `send` as zeros, and an unchecked call would then "pass" nothing at all.
    uint8 send[4] = { 0, 0, 0, 0 };
    CHECK(crypt.EncryptSend(send, 4));
    for (int i = 0; i < 4; ++i)
    {
        CHECK(send[i] == kKs_SeedA_serverEncrypt[i]);
    }

    uint8 recv[4] = { 0, 0, 0, 0 };
    CHECK(crypt.DecryptRecv(recv, 4));
    for (int i = 0; i < 4; ++i)
    {
        CHECK(recv[i] == kKs_SeedB_serverDecrypt[i]);
    }
}

// THE STAGE 1 BLOCKER, closed. IsInitialized() used to be a RAN-TO-COMPLETION flag, not a SUCCEEDED
// flag: AuthCrypt set _initialized = true unconditionally while ARC4 discarded every EVP return
// value. Since IsInitialized() is the codec discriminator (WorldSocket.cpp:218 send / :862 recv), a
// failed init would have framed PLAINTEXT headers as HDR_POSTCRYPT.
static void test_authcrypt_prepare_reports_success()
{
    static const uint8 kK_FULL[40] = { 0x03,0x0A,0x11,0x18,0x1F,0x26,0x2D,0x34,0x3B,0x42,
                                       0x49,0x50,0x57,0x5E,0x65,0x6C,0x73,0x7A,0x81,0x88,
                                       0x8F,0x96,0x9D,0xA4,0xAB,0xB2,0xB9,0xC0,0xC7,0xCE,
                                       0xD5,0xDC,0xE3,0xEA,0xF1,0xF8,0xFF,0x06,0x0D,0x14 };
    AuthCrypt crypt;
    const bool prepared = crypt.Prepare(kK_FULL);
    CHECK(prepared);
    crypt.Activate();
    CHECK(crypt.IsInitialized());                  // success, not merely two false values agreeing
}

// *** THE COMMIT-REGION INVARIANT, AS A TEST. ***
// Between Prepare() and Activate() the ARC4 contexts are fully keyed -- but the crypt must still be
// INERT: IsInitialized() false, so Decrypt/Encrypt no-op AND the wire codec discriminator
// (WorldSocket.cpp:218/862) still frames PRE-crypt. That is what lets HandleAuthSession do every
// fallible thing -- allocate the session, load account data, inflate addons -- AFTER the crypt is
// prepared, with a throw still leaving the socket exactly as unauthenticated as it started.
//
// Stage 1's equivalent ordering rule was load-bearing and its own ledger said "NO TEST COVERS THIS".
// This is that test.
static void test_authcrypt_prepare_does_not_activate()
{
    static const uint8 kK_FULL[40] = { 0x03,0x0A,0x11,0x18,0x1F,0x26,0x2D,0x34,0x3B,0x42,
                                       0x49,0x50,0x57,0x5E,0x65,0x6C,0x73,0x7A,0x81,0x88,
                                       0x8F,0x96,0x9D,0xA4,0xAB,0xB2,0xB9,0xC0,0xC7,0xCE,
                                       0xD5,0xDC,0xE3,0xEA,0xF1,0xF8,0xFF,0x06,0x0D,0x14 };
    AuthCrypt crypt;
    CHECK(crypt.Prepare(kK_FULL));
    CHECK(!crypt.IsInitialized());                 // keyed, but NOT published

    uint8 header[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    CHECK(!crypt.EncryptSend(header, 4));          // still refuses...
    CHECK(header[0] == 0xDE && header[3] == 0xEF); // ...and has not touched the buffer

    crypt.Activate();
    CHECK(crypt.IsInitialized());
    CHECK(crypt.EncryptSend(header, 4));           // only now
}

// FAIL-CLOSED: Activate() without a successful Prepare() must leave the crypt inert, never
// half-active. If this ever passes as "initialized", a failed Prepare would publish an unkeyed
// crypt and the codec would frame plaintext as post-crypt -- the blocker, by another route.
static void test_authcrypt_activate_without_prepare_is_inert()
{
    AuthCrypt crypt;
    crypt.Activate();                              // no Prepare
    CHECK(!crypt.IsInitialized());

    uint8 header[4] = { 0x11, 0x22, 0x33, 0x44 };
    CHECK(!crypt.EncryptSend(header, 4));
    CHECK(header[0] == 0x11 && header[3] == 0x44);
}

// A repeated Prepare while PREPARED is an invalid transition. It must poison the unpublished
// preparation so a caller that ignores the false return cannot Activate stale key material.
static void test_authcrypt_second_prepare_cannot_activate_stale_key()
{
    static const uint8 kK_FULL[40] = { 0x03,0x0A,0x11,0x18,0x1F,0x26,0x2D,0x34,0x3B,0x42,
                                       0x49,0x50,0x57,0x5E,0x65,0x6C,0x73,0x7A,0x81,0x88,
                                       0x8F,0x96,0x9D,0xA4,0xAB,0xB2,0xB9,0xC0,0xC7,0xCE,
                                       0xD5,0xDC,0xE3,0xEA,0xF1,0xF8,0xFF,0x06,0x0D,0x14 };
    AuthCrypt crypt;
    CHECK(crypt.Prepare(kK_FULL));
    CHECK(!crypt.Prepare(kK_FULL));                // rejected before either context is touched
    CHECK(!crypt.Prepare(kK_FULL));                // FAILED is terminal; no retry/re-key path
    crypt.Activate();                              // must NOT publish the earlier preparation
    CHECK(!crypt.IsInitialized());
}

// Prepare after activation must be rejected before touching the live stream. The next four bytes
// must therefore be bytes 4..7 of the SAME keystream, not a restarted/re-keyed stream.
static void test_authcrypt_prepare_after_activate_does_not_mutate_stream()
{
    static const uint8 kK_FULL[40] = { 0x03,0x0A,0x11,0x18,0x1F,0x26,0x2D,0x34,0x3B,0x42,
                                       0x49,0x50,0x57,0x5E,0x65,0x6C,0x73,0x7A,0x81,0x88,
                                       0x8F,0x96,0x9D,0xA4,0xAB,0xB2,0xB9,0xC0,0xC7,0xCE,
                                       0xD5,0xDC,0xE3,0xEA,0xF1,0xF8,0xFF,0x06,0x0D,0x14 };
    // Both arrays are emitted by tools/mop_stage2_fixtures.py's independent RC4 oracle.
    static const uint8 first[4] = { 0xF6, 0x05, 0x69, 0xC6 };
    static const uint8 next[4]  = { 0x3A, 0xED, 0x33, 0xFB };

    AuthCrypt crypt;
    CHECK(crypt.Prepare(kK_FULL));
    crypt.Activate();

    uint8 bytes[4] = { 0, 0, 0, 0 };
    CHECK(crypt.EncryptSend(bytes, 4));
    CHECK(std::memcmp(bytes, first, 4) == 0);

    CHECK(!crypt.Prepare(kK_FULL));                 // ACTIVE -> Prepare rejected, stream untouched
    CHECK(crypt.IsInitialized());
    std::memset(bytes, 0, sizeof(bytes));
    CHECK(crypt.EncryptSend(bytes, 4));
    CHECK(std::memcmp(bytes, next, 4) == 0);
}

// An UNINITIALIZED crypt must REFUSE, never silently pass the data through. This is the property
// that makes the blocker unreachable: if the crypt cannot encrypt, the caller must find out rather
// than ship the buffer untouched.
static void test_authcrypt_refuses_before_init()
{
    AuthCrypt crypt;
    CHECK(!crypt.IsInitialized());

    uint8 header[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    CHECK(!crypt.EncryptSend(header, 4));          // must report failure...
    CHECK(header[0] == 0xDE && header[1] == 0xAD && header[2] == 0xBE && header[3] == 0xEF);
    CHECK(!crypt.DecryptRecv(header, 4));          // ...and leave the buffer untouched
    CHECK(header[0] == 0xDE && header[1] == 0xAD && header[2] == 0xBE && header[3] == 0xEF);
}

// MopAuth::IsPlausibleSessionKeyHex edge cases -- the text half of the sessionkey rule
// (Auth/MopAuthKey.h). test_sessionkey_from_hex_roundtrip covers it THROUGH the adapter; this pins
// the predicate's own edges, where the "even and <=80 admits the empty string" bug lived.
static void test_sessionkey_hex_validation()
{
    CHECK(!MopAuth::IsPlausibleSessionKeyHex(NULL));
    CHECK(!MopAuth::IsPlausibleSessionKeyHex(""));            // empty -> would zero-fill; REJECT
    CHECK(!MopAuth::IsPlausibleSessionKeyHex("ABC"));         // odd length
    CHECK(!MopAuth::IsPlausibleSessionKeyHex("ABCG"));        // non-hex
    CHECK(!MopAuth::IsPlausibleSessionKeyHex(std::string(82, 'A').c_str()));  // 41 bytes
    CHECK(MopAuth::IsPlausibleSessionKeyHex(std::string(80, 'A').c_str()));   // 40 bytes, normal
    CHECK(MopAuth::IsPlausibleSessionKeyHex(std::string(78, 'A').c_str()));   // one leading zero
    CHECK(MopAuth::IsPlausibleSessionKeyHex(std::string(76, 'A').c_str()));   // two -- also normal
    CHECK(MopAuth::IsPlausibleSessionKeyHex(std::string(2, 'A').c_str()));    // no floor exists
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
    test_name_lengths_uncapped();
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
    test_raw40_from_littleendian();
    test_sessionkey_from_hex_roundtrip();
    test_authcrypt_kat();
    test_authcrypt_prepare_reports_success();
    test_authcrypt_prepare_does_not_activate();
    test_authcrypt_activate_without_prepare_is_inert();
    test_authcrypt_second_prepare_cannot_activate_stale_key();
    test_authcrypt_prepare_after_activate_does_not_mutate_stream();
    test_authcrypt_refuses_before_init();
    test_sessionkey_hex_validation();
    std::printf(g_fail ? "FAILED (%d)\n" : "OK\n", g_fail);
    return g_fail ? 1 : 0;
}
