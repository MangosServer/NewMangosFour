/**
 * Byte-exact coverage for the SMSG_LFG_PLAYER_INFO (0x1861) lock array.
 *
 * The expected bytes are REAL captured server bytes at build 18414, lifted from a live
 * reply, not inverses of our own writer.
 *
 * Corpus catalogueGenerationId
 *   2BE10C899585BAECD237705AC13BBF9262D81B6BDC085B462808C6869CE88752
 * Reference packet: capture-000006 sequence 1953, 6068 bytes, sent to a max-level
 * character. Decoded header: lockCount 206, hasPlayerGuid 0, randomDungeonCount 35.
 *
 * Layout:
 *   bits   WriteBits(lockCount, 20)
 *          WriteBit(hasPlayerGuid)
 *          WriteBits(randomDungeonCount, 17)
 *          FlushBits                          -> 38 bits, 5 bytes
 *   ...random dungeon reward records, variable length...
 *   tail   lockCount x 16 bytes, flat and unpacked:
 *              uint32 dungeonEntry   (TypeID << 24) | id
 *              uint32 lockStatus     -- the client's LFG_INSTANCE_INVALID_CODES
 *              uint32 subReason1
 *              uint32 subReason2
 *
 * We send a locks-only reply (randomDungeonCount 0), which the client accepts: it
 * installs the lock list and raises LFG_LOCK_INFO_RECEIVED whether or not random records
 * follow. So the header differs from the reference by design, but the LOCK ARRAY must be
 * byte-identical -- that is what these cases assert.
 */

#include "LFGMgr.h"
#include "WorldPacket.h"

#include <cassert>
#include <cstdio>
#include <vector>

namespace
{
    void AssertBytes(uint8 const* actual, std::vector<uint8> const& expected,
                     size_t offset, char const* label)
    {
        for (size_t i = 0; i < expected.size(); ++i)
        {
            if (actual[offset + i] != expected[i])
            {
                std::printf("%s: byte %u is 0x%02X, expected 0x%02X\n", label,
                            unsigned(offset + i), actual[offset + i], expected[i]);
                assert(false);
            }
        }
    }

    MopLfgPackets::PlayerLockInfo Lock(uint32 entry, uint32 status)
    {
        MopLfgPackets::PlayerLockInfo l;
        l.dungeonEntry = entry;
        l.lockStatus = status;
        return l;
    }

    /// The first five lock records of capture-000006 seq 1953, byte for byte.
    ///
    /// All five are lockStatus 3 -- LEVEL_TOO_HIGH -- which is what a max-level character
    /// sees for low-level content, and 167 of that packet's 206 records carry it. Note
    /// the third is a TypeID 2 (raid) entry, so the array is not dungeons-only.
    void test_lock_records_match_capture()
    {
        std::vector<uint8> const expected = {
            0xBC, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xAA, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xA0, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x93, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xA3, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        std::vector<MopLfgPackets::PlayerLockInfo> locks;
        locks.push_back(Lock(0x010000BCu, 3));
        locks.push_back(Lock(0x010000AAu, 3));
        locks.push_back(Lock(0x020000A0u, 3));   // raid entry, TypeID 2
        locks.push_back(Lock(0x01000093u, 3));
        locks.push_back(Lock(0x010000A3u, 3));

        WorldPacket packet(SMSG_LFG_PLAYER_INFO, 5 + locks.size() * 16);
        MopLfgPackets::BuildPlayerInfo(packet, locks);

        assert(packet.size() == 5 + expected.size());       // 5-byte header, then the array
        AssertBytes(packet.contents(), expected, 5, "lock_records");
    }

    /// The 20/1/17 bit header, checked against the reference packet's own first five bytes.
    ///
    /// Feeding the reference counts back in must reproduce them exactly; this is what pins
    /// the field widths and their order.
    void test_header_matches_capture()
    {
        // 206 locks, hasPlayerGuid 0, 35 random records -> 00 0C E0 00 8D
        std::vector<uint8> const expectedHeader = { 0x00, 0x0C, 0xE0, 0x00, 0x8D };

        WorldPacket packet(SMSG_LFG_PLAYER_INFO, 5);
        packet.WriteBits(206, 20);
        packet.WriteBit(false);
        packet.WriteBits(35, 17);
        packet.FlushBits();

        assert(packet.size() == expectedHeader.size());
        AssertBytes(packet.contents(), expectedHeader, 0, "header");
    }

    /// Our own locks-only header: same widths, random count zero.
    void test_locks_only_header()
    {
        std::vector<MopLfgPackets::PlayerLockInfo> locks;
        locks.push_back(Lock(0x010000BCu, 3));

        WorldPacket packet(SMSG_LFG_PLAYER_INFO, 5 + 16);
        MopLfgPackets::BuildPlayerInfo(packet, locks);

        assert(packet.size() == 5 + 16);

        // Decode the header back out and confirm the counts survive the round trip.
        uint8 const* b = packet.contents();
        uint32 const bits = (uint32(b[0]) << 24) | (uint32(b[1]) << 16) |
                            (uint32(b[2]) << 8) | uint32(b[3]);
        assert((bits >> 12) == 1);                          // 20-bit lock count
        assert(((bits >> 11) & 1) == 0);                    // hasPlayerGuid
    }

    /// An empty lock list must still emit a well-formed 5-byte header, because that is
    /// what a character with nothing locked legitimately produces.
    void test_empty_lock_list()
    {
        std::vector<MopLfgPackets::PlayerLockInfo> locks;

        WorldPacket packet(SMSG_LFG_PLAYER_INFO, 5);
        MopLfgPackets::BuildPlayerInfo(packet, locks);

        assert(packet.size() == 5);
        for (size_t i = 0; i < packet.size(); ++i)
        {
            assert(packet.contents()[i] == 0x00);
        }
    }
}

int main()
{
    test_lock_records_match_capture();
    test_header_matches_capture();
    test_locks_only_header();
    test_empty_lock_list();

    std::printf("mop_lfg_player_info_packets_test: OK\n");
    return 0;
}
