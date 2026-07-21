/**
 * Byte-exact tests for the 5.4.8 SMSG_AURA_UPDATE grammar.
 *
 * Expected bytes are independent transcriptions of the direct 18414 reader.
 */

#include "SpellAuras.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

char const* LookupOpcodeName(PacketDirection, uint16)
{
    return "TEST_AURA_OPCODE";
}

static std::vector<uint8> PacketBytes(WorldPacket& packet)
{
    packet.FlushBits();
    return std::vector<uint8>(packet.contents(), packet.contents() + packet.size());
}

static void CheckBytes(WorldPacket& packet, std::initializer_list<uint8> expected)
{
    std::vector<uint8> actual = PacketBytes(packet);
    CHECK(actual == std::vector<uint8>(expected));
}

static void TestEmptyAndRemovalForms()
{
    std::vector<MopAuraPackets::AuraUpdate> updates;

    WorldPacket fullEmpty(SMSG_AURA_UPDATE, 8);
    CHECK(MopAuraPackets::BuildAuraUpdate(fullEmpty, 0, true, updates));
    CheckBytes(fullEmpty, {0x40, 0x00, 0x00, 0x00, 0x00});

    WorldPacket incrementalEmpty(SMSG_AURA_UPDATE, 8);
    CHECK(MopAuraPackets::BuildAuraUpdate(incrementalEmpty, 0, false, updates));
    CheckBytes(incrementalEmpty, {0x00, 0x00, 0x00, 0x00, 0x00});

    MopAuraPackets::AuraUpdate removal;
    removal.hasPayload = false;
    removal.slot = 5;
    updates.push_back(removal);

    WorldPacket incrementalRemoval(SMSG_AURA_UPDATE, 8);
    CHECK(MopAuraPackets::BuildAuraUpdate(incrementalRemoval, 0, false, updates));
    CheckBytes(incrementalRemoval, {0x00, 0x00, 0x00, 0x40, 0x00, 0x05});

    WorldPacket fullRemoval(SMSG_AURA_UPDATE, 8);
    CHECK(MopAuraPackets::BuildAuraUpdate(fullRemoval, 0, true, updates));
    CheckBytes(fullRemoval, {0x40, 0x00, 0x00, 0x40, 0x00, 0x05});
}

static void TestFullApplyAndRemovalAllGuidBytes()
{
    MopAuraPackets::AuraUpdate apply;
    apply.slot = 5;
    apply.flags = 0x1E;
    apply.casterLevel = 0x2233;
    apply.spellId = 0x44556677;
    apply.hasMaxDuration = true;
    apply.maxDuration = 0x11223344;
    apply.hasDuration = true;
    apply.duration = 0x55667788;
    apply.hasCasterGuid = true;
    apply.casterGuid = 0x1817161514131211ull;
    apply.stacksOrCharges = 3;
    apply.effectMask = 5;
    apply.effectAmounts.push_back(1.5f);
    apply.effectAmounts.push_back(-2.25f);

    MopAuraPackets::AuraUpdate removal;
    removal.hasPayload = false;
    removal.slot = 9;

    std::vector<MopAuraPackets::AuraUpdate> updates;
    updates.push_back(apply);
    updates.push_back(removal);

    WorldPacket packet(SMSG_AURA_UPDATE, 64);
    CHECK(MopAuraPackets::BuildAuraUpdate(packet, 0x0807060504030201ull, true, updates));
    CheckBytes(packet, {
        0xC0, 0x00, 0x00, 0xBF, 0xC0, 0x00, 0x02, 0xFF,
        0x80, 0x00, 0x01, 0x80, 0x15, 0x12, 0x13, 0x16,
        0x14, 0x10, 0x17, 0x19, 0x1E, 0x33, 0x22, 0x77,
        0x66, 0x55, 0x44, 0x44, 0x33, 0x22, 0x11, 0x88,
        0x77, 0x66, 0x55, 0x03, 0x05, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x10, 0xC0,
        0x05, 0x09, 0x02, 0x06, 0x09, 0x03, 0x05, 0x04,
        0x00, 0x07
    });
}

static void TestSparseGuidAndOmittedCaster()
{
    MopAuraPackets::AuraUpdate apply;
    apply.slot = 2;
    apply.flags = MopAuraPackets::AURA_WIRE_CASTER |
        MopAuraPackets::AURA_WIRE_POSITIVE | MopAuraPackets::AURA_WIRE_DURATION;
    apply.casterLevel = 70;
    apply.spellId = 123;
    apply.hasMaxDuration = true;
    apply.maxDuration = 1000;
    apply.hasDuration = true;
    apply.duration = 900;
    apply.stacksOrCharges = 1;
    apply.effectMask = 1;

    std::vector<MopAuraPackets::AuraUpdate> updates(1, apply);
    WorldPacket packet(SMSG_AURA_UPDATE, 48);
    CHECK(MopAuraPackets::BuildAuraUpdate(packet, 0x0007000500030001ull, false, updates));
    CheckBytes(packet, {
        0x00, 0x00, 0x00, 0x67, 0x40, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x80, 0x07, 0x46, 0x00, 0x7B, 0x00,
        0x00, 0x00, 0xE8, 0x03, 0x00, 0x00, 0x84, 0x03,
        0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x02,
        0x02, 0x06, 0x04, 0x00
    });
}

static void TestOpcode()
{
    CHECK(uint32(SMSG_AURA_UPDATE) == 0x0072);
    CHECK(uint32(SMSG_AURA_UPDATE) <= 0x1FFF);
}

// Linking game pulls ACE, whose headers rewrite main to ace_main_i(argc, argv).
int main(int /*argc*/, char** /*argv*/)
{
    TestEmptyAndRemovalForms();
    TestFullApplyAndRemovalAllGuidBytes();
    TestSparseGuidAndOmittedCaster();
    TestOpcode();
    return g_fail ? 1 : 0;
}
