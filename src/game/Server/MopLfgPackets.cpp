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

bool MopLfgPackets::BuildUpdateStatus(WorldPacket& out,
    StatusUpdate const& update)
{
    if (update.comment.size() >= (size_t(1) << 8) ||
        update.dungeonEntries.size() >= (size_t(1) << 22) ||
        update.suspendedPlayerGuids.size() >= (size_t(1) << 24))
    {
        return false;
    }

    out.WriteBits(update.comment.size(), 8);
    out.WriteBit(update.isParty);
    out.WriteBit(update.joined);
    out.WriteBits(update.dungeonEntries.size(), 22);
    WriteGuidMask(out, update.requesterGuid, { 2, 3, 1 });
    out.WriteBit(update.notifyUi);
    WriteGuidMask(out, update.requesterGuid, { 7, 6, 0 });
    out.WriteBit(update.lfgJoined);
    out.WriteBit(update.queued);
    out.WriteBits(update.suspendedPlayerGuids.size(), 24);
    WriteGuidMask(out, update.requesterGuid, { 5 });
    for (uint64 guid : update.suspendedPlayerGuids)
        WriteGuidMask(out, guid, { 7, 0, 4, 2, 5, 3, 1, 6 });
    WriteGuidMask(out, update.requesterGuid, { 4 });
    out.FlushBits();

    WriteGuidBytes(out, update.requesterGuid, { 3 });
    for (uint8 need : update.needs)
        out << need;
    WriteGuidBytes(out, update.requesterGuid, { 4 });
    for (uint64 guid : update.suspendedPlayerGuids)
        WriteGuidBytes(out, guid, { 7, 0, 1, 6, 4, 5, 2, 3 });
    WriteGuidBytes(out, update.requesterGuid, { 6 });
    out << update.updateReason;
    out << update.requestedRoles;
    out << update.ticketId;
    WriteGuidBytes(out, update.requesterGuid, { 5 });
    if (!update.comment.empty())
        out.append(update.comment.data(), update.comment.size());
    WriteGuidBytes(out, update.requesterGuid, { 2 });
    for (uint32 entry : update.dungeonEntries)
        out << entry;
    WriteGuidBytes(out, update.requesterGuid, { 0, 1 });
    out << update.ticketTime;
    out << update.dungeonCategory;
    out << update.ticketType;
    WriteGuidBytes(out, update.requesterGuid, { 7 });
    return true;
}
