/**
 * Independent byte fixtures for the 5.4.8.18414 respec confirmation exchange.
 */

#include "MopRespecPackets.h"
#include "MopWireCodec.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    if (packet.size() != expected.size())
        return false;
    return std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static void Append(WorldPacket& packet, std::vector<uint8> const& bytes)
{
    if (!bytes.empty())
        packet.append(bytes.data(), bytes.size());
}

static void test_outbound_zero_guid()
{
    WorldPacket packet(SMSG_RESPEC_WIPE_CONFIRM, 6);
    MopRespecPackets::BuildRespecWipeConfirm(packet, 0, 0, 0);
    CHECK(Equal(packet, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }));
}

static void test_outbound_nontrivial_guid()
{
    WorldPacket packet(SMSG_RESPEC_WIPE_CONFIRM, 14);
    MopRespecPackets::BuildRespecWipeConfirm(packet, 0x0123456789ABCDEFull, 0, 0x11223344u);
    CHECK(Equal(packet, {
        0xFF,
        0xCC, 0xEE,
        0x00,
        0x00, 0x88, 0xAA, 0x44, 0x22, 0x66,
        0x44, 0x33, 0x22, 0x11
    }));
}

static void test_inbound_zero_guid()
{
    WorldPacket packet(CMSG_CONFIRM_RESPEC_WIPE, 2);
    Append(packet, { 0x00, 0x00 });
    MopRespecPackets::ConfirmRespecWipe const confirm = MopRespecPackets::ReadConfirmRespecWipe(packet);
    CHECK(confirm.discriminator == 0);
    CHECK(confirm.trainerGuid == 0);
}

static void test_inbound_nontrivial_guid_and_discriminator()
{
    WorldPacket packet(CMSG_CONFIRM_RESPEC_WIPE, 10);
    Append(packet, {
        0x01,
        0xFF,
        0xAA, 0x66, 0xCC, 0x88, 0xEE, 0x44, 0x00, 0x22
    });
    MopRespecPackets::ConfirmRespecWipe const confirm = MopRespecPackets::ReadConfirmRespecWipe(packet);
    CHECK(confirm.discriminator == 1);
    CHECK(confirm.trainerGuid == 0x0123456789ABCDEFull);
}

static void test_postcrypt_header()
{
    CHECK(uint32(CMSG_CONFIRM_RESPEC_WIPE) == 0x0275u);
    CHECK(uint32(SMSG_RESPEC_WIPE_CONFIRM) == 0x02ABu);
    uint8 header[4] = {};
    CHECK(MopWire::BuildServerHeader(true, 6, SMSG_RESPEC_WIPE_CONFIRM, header));
    CHECK(header[0] == 0xAB && header[1] == 0xC2 && header[2] == 0x00 && header[3] == 0x00);
    CHECK(MopWire::BuildServerHeader(true, 14, SMSG_RESPEC_WIPE_CONFIRM, header));
    CHECK(header[0] == 0xAB && header[1] == 0xC2 && header[2] == 0x01 && header[3] == 0x00);
}

int main(int, char**)
{
    test_outbound_zero_guid();
    test_outbound_nontrivial_guid();
    test_inbound_zero_guid();
    test_inbound_nontrivial_guid_and_discriminator();
    test_postcrypt_header();
    if (g_fail)
        return 1;
    std::printf("mop_respec_packets: all checks passed\n");
    return 0;
}
