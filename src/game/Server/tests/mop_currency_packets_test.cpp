/**
 * Byte-exact tests for the 5.4.8 currency readers at 0x6F396E and 0x741F90.
 */

#include "MopCurrencyPackets.h"
#include "MopWireCodec.h"
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
    {
        std::fprintf(stderr, "  size %u, wanted %u\n", unsigned(packet.size()), unsigned(expected.size()));
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (packet.contents()[i] != expected[i])
        {
            std::fprintf(stderr, "  byte %u = 0x%02X, wanted 0x%02X\n",
                unsigned(i), packet.contents()[i], expected[i]);
            return false;
        }
    }
    return true;
}

static void test_update_currency_optional_combinations()
{
    MopCurrencyPackets::CurrencyUpdate const base = {
        0x11223344u, 0x55667788u, 0x99AABBCCu, false, false, false, 0, 0
    };

    WorldPacket none(SMSG_UPDATE_CURRENCY, 13);
    CHECK(MopCurrencyPackets::BuildUpdateCurrency(none, base));
    CHECK(ExpectBytes(none, {
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99, 0x00
    }));

    MopCurrencyPackets::CurrencyUpdate weekly = base;
    weekly.hasWeeklyQuantity = true;
    weekly.weeklyQuantity = 0x01020304u;
    WorldPacket weeklyPacket(SMSG_UPDATE_CURRENCY, 17);
    CHECK(MopCurrencyPackets::BuildUpdateCurrency(weeklyPacket, weekly));
    CHECK(ExpectBytes(weeklyPacket, {
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99, 0x80, 0x04, 0x03, 0x02, 0x01
    }));

    MopCurrencyPackets::CurrencyUpdate tracked = base;
    tracked.hasTrackedQuantity = true;
    tracked.trackedQuantity = 0x0A0B0C0Du;
    WorldPacket trackedPacket(SMSG_UPDATE_CURRENCY, 17);
    CHECK(MopCurrencyPackets::BuildUpdateCurrency(trackedPacket, tracked));
    CHECK(ExpectBytes(trackedPacket, {
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99, 0x20, 0x0D, 0x0C, 0x0B, 0x0A
    }));

    MopCurrencyPackets::CurrencyUpdate all = weekly;
    all.suppressGainMessage = true;
    all.hasTrackedQuantity = true;
    all.trackedQuantity = tracked.trackedQuantity;
    WorldPacket allPacket(SMSG_UPDATE_CURRENCY, 21);
    CHECK(MopCurrencyPackets::BuildUpdateCurrency(allPacket, all));
    CHECK(ExpectBytes(allPacket, {
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99, 0xE0,
        0x04, 0x03, 0x02, 0x01, 0x0D, 0x0C, 0x0B, 0x0A
    }));
}

static void test_setup_currency_empty_and_entries()
{
    WorldPacket empty(SMSG_SETUP_CURRENCY, 3);
    CHECK(MopCurrencyPackets::BuildSetupCurrency(empty, {}));
    CHECK(ExpectBytes(empty, { 0x00, 0x00, 0x00 }));

    MopCurrencyPackets::CurrencySetupEntry const flagsZero = {
        0x11223344u, 0x55667788u, 0, 0, 0, 0, false, false, false
    };
    WorldPacket zeroFlags(SMSG_SETUP_CURRENCY, 12);
    CHECK(MopCurrencyPackets::BuildSetupCurrency(zeroFlags, { flagsZero }));
    CHECK(ExpectBytes(zeroFlags, {
        0x00, 0x00, 0x20, 0x00,
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55
    }));

    MopCurrencyPackets::CurrencySetupEntry const allOptional = {
        0x11121314u, 0x21222324u, 0x31323334u, 0x41424344u,
        0x51525354u, 7, true, true, true
    };
    WorldPacket all(SMSG_SETUP_CURRENCY, 24);
    CHECK(MopCurrencyPackets::BuildSetupCurrency(all, { allOptional }));
    CHECK(ExpectBytes(all, {
        0x00, 0x00, 0x3F, 0x80,
        0x34, 0x33, 0x32, 0x31,
        0x14, 0x13, 0x12, 0x11,
        0x44, 0x43, 0x42, 0x41,
        0x24, 0x23, 0x22, 0x21,
        0x54, 0x53, 0x52, 0x51
    }));

    WorldPacket mixed(SMSG_SETUP_CURRENCY, 36);
    CHECK(MopCurrencyPackets::BuildSetupCurrency(mixed, { flagsZero, allOptional }));
    CHECK(ExpectBytes(mixed, {
        0x00, 0x00, 0x40, 0x7E,
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
        0x34, 0x33, 0x32, 0x31,
        0x14, 0x13, 0x12, 0x11,
        0x44, 0x43, 0x42, 0x41,
        0x24, 0x23, 0x22, 0x21,
        0x54, 0x53, 0x52, 0x51
    }));
}

static void test_setup_currency_rejects_invalid_inputs_without_writing()
{
    MopCurrencyPackets::CurrencySetupEntry invalidFlags = {
        1, 2, 0, 0, 0, 8, false, false, false
    };
    WorldPacket invalid(SMSG_SETUP_CURRENCY, 0);
    CHECK(!MopCurrencyPackets::BuildSetupCurrency(invalid, { invalidFlags }));
    CHECK(invalid.empty());

    MopCurrencyPackets::CurrencySetupEntry const entry = {
        1, 2, 0, 0, 0, 0, false, false, false
    };
    std::vector<MopCurrencyPackets::CurrencySetupEntry> overflow(0x80000u, entry);
    WorldPacket tooMany(SMSG_SETUP_CURRENCY, 0);
    CHECK(!MopCurrencyPackets::BuildSetupCurrency(tooMany, overflow));
    CHECK(tooMany.empty());
}

static void test_successor_opcodes_are_framable()
{
    CHECK(uint32_t(SMSG_UPDATE_CURRENCY) == 0x129Eu);
    CHECK(uint32_t(SMSG_SETUP_CURRENCY) == 0x1A8Bu);
    CHECK(uint32_t(SMSG_UPDATE_CURRENCY) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_SETUP_CURRENCY) <= 0x1FFFu);

    uint8_t header[4] = {};
    CHECK(MopWire::BuildServerHeader(true, 13, SMSG_UPDATE_CURRENCY, header));
    CHECK(header[0] == 0x9E && header[1] == 0xB2 && header[2] == 0x01 && header[3] == 0x00);

    CHECK(MopWire::BuildServerHeader(true, 3, SMSG_SETUP_CURRENCY, header));
    CHECK(header[0] == 0x8B && header[1] == 0x7A && header[2] == 0x00 && header[3] == 0x00);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_update_currency_optional_combinations();
    test_setup_currency_empty_and_entries();
    test_setup_currency_rejects_invalid_inputs_without_writing();
    test_successor_opcodes_are_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_currency_packets: all checks passed\n");
    return 0;
}
