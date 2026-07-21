/**
 * Independent reader-inverse fixtures for the 5.4.8.18414 combat-log packets.
 */

#include "MopCombatLogPackets.h"
#include "MopWireCodec.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

class RefWriter
{
public:
    void Bit(bool value)
    {
        if (m_bit == 8)
        {
            m_bytes.push_back(0);
            m_bit = 0;
        }
        if (value)
            m_bytes.back() |= uint8(0x80u >> m_bit);
        ++m_bit;
    }

    void Bits(uint32 value, uint8 count)
    {
        for (uint8 i = 0; i < count; ++i)
            Bit((value & (uint32(1) << (count - i - 1))) != 0);
    }

    void Align() { m_bit = 8; }

    void U32(uint32 value)
    {
        Align();
        for (uint8 i = 0; i < 4; ++i)
            m_bytes.push_back(uint8(value >> (8 * i)));
    }

    void F32(float value)
    {
        uint32 raw = 0;
        std::memcpy(&raw, &value, sizeof(raw));
        U32(raw);
    }

    void GuidBit(uint64 guid, uint8 index) { Bit(GuidByteValue(guid, index) != 0); }

    void GuidByte(uint64 guid, uint8 index)
    {
        uint8 const value = GuidByteValue(guid, index);
        if (value)
            U8(value ^ 1);
    }

    std::vector<uint8> const& Bytes() const { return m_bytes; }

private:
    static uint8 GuidByteValue(uint64 guid, uint8 index) { return uint8(guid >> (8 * index)); }
    void U8(uint8 value) { Align(); m_bytes.push_back(value); }

    std::vector<uint8> m_bytes;
    uint8 m_bit = 8;
};

static void GuidBits(RefWriter& writer, uint64 guid, uint8 const* order, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        writer.GuidBit(guid, order[i]);
}

static void GuidBytes(RefWriter& writer, uint64 guid, uint8 const* order, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        writer.GuidByte(guid, order[i]);
}

static std::vector<uint8> ExpectedExecute(MopCombatLogPackets::SpellExecuteLog const& log)
{
    static uint8 const casterMaskPre[] = { 0, 6, 5, 7, 2 };
    static uint8 const extraMask[] = { 5, 4, 2, 3, 1, 0, 6, 7 };
    static uint8 const powerMask[] = { 0, 3, 1, 5, 6, 4, 7, 2 };
    static uint8 const targetMask[] = { 6, 5, 1, 0, 3, 4, 7, 2 };
    static uint8 const targetBytes[] = { 7, 5, 1, 2, 6, 4, 0, 3 };
    static uint8 const powerBytesA[] = { 3, 7, 5, 2, 0 };
    static uint8 const powerBytesB[] = { 4, 1, 6 };
    static uint8 const extraBytesA[] = { 0, 6, 4, 7, 2, 5, 3 };
    static uint8 const casterBytes[] = { 5, 7, 1, 6, 2, 0, 4, 3 };

    RefWriter writer;
    GuidBits(writer, log.casterGuid, casterMaskPre, 5);
    writer.Bits(1, 19);
    writer.GuidBit(log.casterGuid, 4);

    writer.Bits(log.kind == MopCombatLogPackets::ExecuteKind::ExtraAttacks ? 1 : 0, 21);
    if (log.kind == MopCombatLogPackets::ExecuteKind::ExtraAttacks)
        GuidBits(writer, log.targetGuid, extraMask, 8);

    writer.Bits(log.kind == MopCombatLogPackets::ExecuteKind::PowerChange ? 1 : 0, 20);
    if (log.kind == MopCombatLogPackets::ExecuteKind::PowerChange)
        GuidBits(writer, log.targetGuid, powerMask, 8);

    writer.Bits(0, 21); // Unnamed GUID-plus-two-dword vector is intentionally unresolved.
    writer.Bits(log.kind == MopCombatLogPackets::ExecuteKind::PetFeedItem ? 1 : 0, 22);
    writer.Bits(log.kind == MopCombatLogPackets::ExecuteKind::CreatedItem ? 1 : 0, 22);
    writer.Bits(log.kind == MopCombatLogPackets::ExecuteKind::Target ? 1 : 0, 24);
    if (log.kind == MopCombatLogPackets::ExecuteKind::Target)
        GuidBits(writer, log.targetGuid, targetMask, 8);

    writer.GuidBit(log.casterGuid, 1);
    writer.GuidBit(log.casterGuid, 3);
    writer.Bit(false); // no optional spell-cast-log data

    writer.Align();
    if (log.kind == MopCombatLogPackets::ExecuteKind::Target)
        GuidBytes(writer, log.targetGuid, targetBytes, 8);
    if (log.kind == MopCombatLogPackets::ExecuteKind::PowerChange)
    {
        GuidBytes(writer, log.targetGuid, powerBytesA, 5);
        writer.U32(log.powerType);
        writer.U32(log.amount);
        GuidBytes(writer, log.targetGuid, powerBytesB, 2);
        writer.F32(log.multiplier);
        GuidBytes(writer, log.targetGuid, powerBytesB + 2, 1);
    }
    if (log.kind == MopCombatLogPackets::ExecuteKind::ExtraAttacks)
    {
        GuidBytes(writer, log.targetGuid, extraBytesA, 7);
        writer.U32(log.amount);
        writer.GuidByte(log.targetGuid, 1);
    }
    if (log.kind == MopCombatLogPackets::ExecuteKind::PetFeedItem)
        writer.U32(log.itemEntry);
    writer.U32(log.effectId);
    if (log.kind == MopCombatLogPackets::ExecuteKind::CreatedItem)
        writer.U32(log.itemEntry);
    writer.U32(log.spellId);
    GuidBytes(writer, log.casterGuid, casterBytes, 8);
    return writer.Bytes();
}

static std::vector<uint8> ExpectedPeriodic(MopCombatLogPackets::PeriodicAuraLog const& log)
{
    static uint8 const bytesCasterA[] = { 5, 3 };
    static uint8 const bytesTargetA[] = { 4 };
    static uint8 const bytesTargetB[] = { 6 };
    static uint8 const bytesCasterB[] = { 7, 1 };
    static uint8 const bytesTargetC[] = { 5 };
    static uint8 const bytesCasterC[] = { 0 };
    static uint8 const bytesTargetD[] = { 1, 7 };
    static uint8 const bytesCasterD[] = { 4 };
    static uint8 const bytesTargetE[] = { 3 };
    static uint8 const bytesCasterE[] = { 2 };
    static uint8 const bytesTargetF[] = { 0, 2 };
    static uint8 const bytesCasterF[] = { 6 };
    bool const has8 = log.kind == MopCombatLogPackets::PeriodicKind::Damage || log.kind == MopCombatLogPackets::PeriodicKind::Heal;
    bool const has12 = log.kind == MopCombatLogPackets::PeriodicKind::Damage || log.kind == MopCombatLogPackets::PeriodicKind::Energize || log.kind == MopCombatLogPackets::PeriodicKind::ManaLeech;
    bool const has16 = log.kind == MopCombatLogPackets::PeriodicKind::Damage || log.kind == MopCombatLogPackets::PeriodicKind::Heal || log.kind == MopCombatLogPackets::PeriodicKind::ManaLeech;
    bool const has20 = log.kind == MopCombatLogPackets::PeriodicKind::Damage;

    RefWriter writer;
    writer.GuidBit(log.targetGuid, 7);
    writer.GuidBit(log.casterGuid, 0);
    writer.GuidBit(log.casterGuid, 7);
    writer.GuidBit(log.targetGuid, 1);
    writer.Bits(1, 21);
    writer.GuidBit(log.targetGuid, 0);
    writer.Bit(!has8);
    writer.Bit(!has16);
    writer.Bit(log.critical);
    writer.Bit(!has20);
    writer.Bit(!has12);
    writer.GuidBit(log.targetGuid, 5);
    writer.GuidBit(log.targetGuid, 3);
    writer.GuidBit(log.casterGuid, 1);
    writer.GuidBit(log.targetGuid, 2);
    writer.GuidBit(log.casterGuid, 6);
    writer.GuidBit(log.casterGuid, 3);
    writer.GuidBit(log.casterGuid, 4);
    writer.Bit(false); // no optional spell-cast-log data
    writer.GuidBit(log.casterGuid, 2);
    writer.GuidBit(log.targetGuid, 6);
    writer.GuidBit(log.casterGuid, 5);
    writer.GuidBit(log.targetGuid, 4);

    writer.Align();
    if (has8) writer.U32(log.overAmount);
    writer.U32(log.amount);
    writer.U32(log.auraType);
    if (has20) writer.U32(log.resist);
    if (has16)
    {
        if (log.kind == MopCombatLogPackets::PeriodicKind::ManaLeech)
            writer.F32(log.multiplier);
        else
            writer.U32(log.absorb);
    }
    if (has12)
        writer.U32(log.schoolOrPowerType);
    GuidBytes(writer, log.casterGuid, bytesCasterA, 2);
    GuidBytes(writer, log.targetGuid, bytesTargetA, 1);
    writer.U32(log.spellId);
    GuidBytes(writer, log.targetGuid, bytesTargetB, 1);
    GuidBytes(writer, log.casterGuid, bytesCasterB, 2);
    GuidBytes(writer, log.targetGuid, bytesTargetC, 1);
    GuidBytes(writer, log.casterGuid, bytesCasterC, 1);
    GuidBytes(writer, log.targetGuid, bytesTargetD, 2);
    GuidBytes(writer, log.casterGuid, bytesCasterD, 1);
    GuidBytes(writer, log.targetGuid, bytesTargetE, 1);
    GuidBytes(writer, log.casterGuid, bytesCasterE, 1);
    GuidBytes(writer, log.targetGuid, bytesTargetF, 2);
    GuidBytes(writer, log.casterGuid, bytesCasterF, 1);
    return writer.Bytes();
}

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    if (packet.size() != expected.size())
        return false;
    return std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static void CheckExecute(MopCombatLogPackets::SpellExecuteLog const& log)
{
    WorldPacket packet(SMSG_SPELL_EXECUTE_LOG, 64);
    CHECK(MopCombatLogPackets::BuildSpellExecuteLog(packet, log));
    CHECK(packet.GetOpcode() == SMSG_SPELL_EXECUTE_LOG);
    CHECK(Equal(packet, ExpectedExecute(log)));
}

static void test_execute_variants()
{
    MopCombatLogPackets::SpellExecuteLog log;
    log.casterGuid = 0x0123456789ABCDEFull;
    log.targetGuid = 0xF1E2D3C4B5A69788ull;
    log.spellId = 0x11223344u;
    log.effectId = 0x55667788u;
    log.amount = 0xA1A2A3A4u;
    log.powerType = 0x01020304u;
    log.multiplier = 1.5f;
    log.itemEntry = 0x0A0B0C0Du;

    log.kind = MopCombatLogPackets::ExecuteKind::ExtraAttacks;
    CheckExecute(log);
    log.kind = MopCombatLogPackets::ExecuteKind::PowerChange;
    CheckExecute(log);
    log.kind = MopCombatLogPackets::ExecuteKind::Target;
    CheckExecute(log);
    log.kind = MopCombatLogPackets::ExecuteKind::PetFeedItem;
    CheckExecute(log);
    log.kind = MopCombatLogPackets::ExecuteKind::CreatedItem;
    CheckExecute(log);

    WorldPacket invalid(SMSG_SPELL_EXECUTE_LOG, 1);
    invalid << uint8(0xAA);
    log.kind = static_cast<MopCombatLogPackets::ExecuteKind>(99);
    CHECK(!MopCombatLogPackets::BuildSpellExecuteLog(invalid, log));
    CHECK(invalid.size() == 1 && invalid.contents()[0] == 0xAA);
}

static void CheckPeriodic(MopCombatLogPackets::PeriodicAuraLog const& log)
{
    WorldPacket packet(SMSG_SPELL_PERIODIC_AURA_LOG, 64);
    MopCombatLogPackets::BuildPeriodicAuraLog(packet, log);
    CHECK(packet.GetOpcode() == SMSG_SPELL_PERIODIC_AURA_LOG);
    CHECK(Equal(packet, ExpectedPeriodic(log)));
}

static void test_periodic_variants()
{
    MopCombatLogPackets::PeriodicAuraLog log;
    log.targetGuid = 0x0123456789ABCDEFull;
    log.casterGuid = 0x8877665544332211ull;
    log.spellId = 0x10203040u;
    log.auraType = 3;
    log.amount = 0x11223344u;
    log.overAmount = 0x55667788u;
    log.schoolOrPowerType = 0xA1A2A3A4u;
    log.absorb = 0x01020304u;
    log.resist = 0x05060708u;
    log.multiplier = -1.25f;
    log.critical = true;

    log.kind = MopCombatLogPackets::PeriodicKind::Damage;
    CheckPeriodic(log);
    log.kind = MopCombatLogPackets::PeriodicKind::Heal;
    CheckPeriodic(log);
    log.kind = MopCombatLogPackets::PeriodicKind::Energize;
    log.critical = false;
    CheckPeriodic(log);
    log.kind = MopCombatLogPackets::PeriodicKind::ManaLeech;
    CheckPeriodic(log);
}

static void test_successor_opcodes_are_framable()
{
    CHECK(uint32(SMSG_SPELL_EXECUTE_LOG) == 0x00D8u);
    CHECK(uint32(SMSG_SPELL_PERIODIC_AURA_LOG) == 0x0CF2u);

    MopCombatLogPackets::SpellExecuteLog execute = {};
    execute.kind = MopCombatLogPackets::ExecuteKind::Target;
    WorldPacket executePacket(SMSG_SPELL_EXECUTE_LOG, 29);
    CHECK(MopCombatLogPackets::BuildSpellExecuteLog(executePacket, execute));
    CHECK(executePacket.size() == 29);

    uint8 header[4] = {};
    CHECK(MopWire::BuildServerHeader(true, executePacket.size(), executePacket.GetOpcode(), header));
    CHECK(header[0] == 0xD8 && header[1] == 0xA0 && header[2] == 0x03 && header[3] == 0x00);

    MopCombatLogPackets::PeriodicAuraLog periodic = {};
    periodic.kind = MopCombatLogPackets::PeriodicKind::Energize;
    WorldPacket periodicPacket(SMSG_SPELL_PERIODIC_AURA_LOG, 22);
    MopCombatLogPackets::BuildPeriodicAuraLog(periodicPacket, periodic);
    CHECK(periodicPacket.size() == 22);
    CHECK(MopWire::BuildServerHeader(true, periodicPacket.size(), periodicPacket.GetOpcode(), header));
    CHECK(header[0] == 0xF2 && header[1] == 0xCC && header[2] == 0x02 && header[3] == 0x00);
}

int main(int, char**)
{
    test_execute_variants();
    test_periodic_variants();
    test_successor_opcodes_are_framable();
    if (g_fail) return 1;
    std::printf("mop_combat_log_packets: all checks passed\n");
    return 0;
}
