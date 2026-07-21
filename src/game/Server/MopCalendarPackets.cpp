/**
 * Fixed 5.4.8.18414 calendar packet bodies.
 */

#include "MopCalendarPackets.h"
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

bool MopCalendarPackets::BuildCalendarInitialInvite(WorldPacket& out,
    std::vector<InitialInvite> const& entries)
{
    if (entries.size() >= (size_t(1) << 23))
        return false;

    out.WriteBits(entries.size(), 23);
    for (InitialInvite const& entry : entries)
        WriteGuidMask(out, entry.guid, { 1, 7, 5, 0, 4, 3, 6, 2 });
    out.FlushBits();

    for (InitialInvite const& entry : entries)
    {
        out << entry.level;
        WriteGuidBytes(out, entry.guid, { 3, 5, 4, 6, 7, 0, 2, 1 });
    }
    return true;
}

void MopCalendarPackets::BuildCalendarInviteStatus(WorldPacket& out,
    InviteStatus const& record)
{
    WriteGuidMask(out, record.inviteeGuid, { 5, 0 });
    out.WriteBit(record.displayPendingAction);
    WriteGuidMask(out, record.inviteeGuid, { 2, 1, 4, 6, 7, 3 });
    out.FlushBits();

    WriteGuidBytes(out, record.inviteeGuid, { 1, 6, 7 });
    out << record.lastUpdateTime;
    WriteGuidBytes(out, record.inviteeGuid, { 2, 4 });
    out << record.eventId;
    WriteGuidBytes(out, record.inviteeGuid, { 0, 3, 5 });
    out << record.status;
    out << record.eventFlags;
    out << record.eventTime;
}

void MopCalendarPackets::BuildCalendarModeratorStatus(WorldPacket& out,
    ModeratorStatus const& record)
{
    WriteGuidMask(out, record.inviteeGuid, { 3, 7, 2, 4, 1, 6, 5 });
    out.WriteBit(record.displayPendingAction);
    WriteGuidMask(out, record.inviteeGuid, { 0 });
    out.FlushBits();

    WriteGuidBytes(out, record.inviteeGuid, { 5, 4, 6, 1, 7, 2, 3 });
    out << record.rank;
    out << record.eventId;
    WriteGuidBytes(out, record.inviteeGuid, { 0 });
}
