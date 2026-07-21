/**
 * Fixed 5.4.8.18414 dungeon-finder packet bodies.
 */

#include "MopLfgPackets.h"
#include "WorldPacket.h"

namespace
{
    uint8 GuidByte(uint64 guid, size_t index)
    {
        return uint8(guid >> (index * 8));
    }

    void WriteGuidMask(WorldPacket& out, uint64 guid,
        std::initializer_list<size_t> order)
    {
        for (size_t index : order)
            out.WriteBit(GuidByte(guid, index) != 0);
    }

    void WriteGuidBytes(WorldPacket& out, uint64 guid,
        std::initializer_list<size_t> order)
    {
        for (size_t index : order)
            out.WriteByteSeq(GuidByte(guid, index));
    }
}

bool MopLfgPackets::BuildBootPlayer(WorldPacket& out,
    BootUpdate const& update)
{
    if (update.reason.size() >= (size_t(1) << 8))
        return false;

    out.WriteBit(update.reason.empty());
    WriteGuidMask(out, update.victimGuid, { 3 });
    out.WriteBit(update.didVote);
    out.WriteBit(update.votePassed);
    out.WriteBit(update.agree);
    WriteGuidMask(out, update.victimGuid, { 6 });
    if (!update.reason.empty())
        out.WriteBits(update.reason.size(), 8);
    out.WriteBit(update.inProgress);
    WriteGuidMask(out, update.victimGuid, { 1, 7, 5, 2, 0, 4 });
    out.FlushBits();

    WriteGuidBytes(out, update.victimGuid, { 2, 4, 3, 6 });
    out << update.votesNeeded;
    out << update.timeLeft;
    if (!update.reason.empty())
        out.append(update.reason.data(), update.reason.size());
    WriteGuidBytes(out, update.victimGuid, { 5, 0 });
    out << update.agreeCount;
    WriteGuidBytes(out, update.victimGuid, { 7 });
    out << update.voteCount;
    WriteGuidBytes(out, update.victimGuid, { 1 });
    return true;
}
