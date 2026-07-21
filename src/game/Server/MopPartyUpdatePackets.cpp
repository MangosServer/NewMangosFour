/**
 * 5.4.8.18414 party-list packet codec.
 */

#include "MopPartyUpdatePackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

namespace
{
    uint8 GuidByte(uint64 guid, uint8 index)
    {
        return uint8(guid >> (index * 8));
    }

    bool Validate(MopPartyUpdatePackets::PartyUpdate const& update)
    {
        if (update.members.size() >= (size_t(1) << 21))
            return false;

        for (MopPartyUpdatePackets::Member const& member : update.members)
            if (member.name.size() >= (size_t(1) << 6))
                return false;

        return true;
    }
}

uint8 MopPartyUpdatePackets::ReadRequest(WorldPacket& in)
{
    uint8 partyIndex = 0;
    in >> partyIndex;
    return partyIndex;
}

bool MopPartyUpdatePackets::BuildPartyUpdate(WorldPacket& out,
    PartyUpdate const& update)
{
    if (!Validate(update))
        return false;

    uint64 const groupGuid = update.groupGuid;
    uint64 const leaderGuid = update.leaderGuid;
    uint64 const looterGuid = update.looterGuid;
    ByteBuffer memberData;

    out.Initialize(SMSG_GROUP_LIST, 64 + update.members.size() * 24);
    out.WriteBit(GuidByte(groupGuid, 0) != 0);
    out.WriteBit(GuidByte(leaderGuid, 7) != 0);
    out.WriteBit(GuidByte(leaderGuid, 1) != 0);
    out.WriteBit(update.hasInstanceDifficulty);
    out.WriteBit(GuidByte(groupGuid, 7) != 0);
    out.WriteBit(GuidByte(leaderGuid, 6) != 0);
    out.WriteBit(GuidByte(leaderGuid, 5) != 0);
    out.WriteBits(uint32(update.members.size()), 21);

    for (Member const& member : update.members)
    {
        uint64 const memberGuid = member.guid;
        uint8 const maskOrder[] = { 1, 2, 5, 6 };
        for (uint8 index : maskOrder)
            out.WriteBit(GuidByte(memberGuid, index) != 0);
        out.WriteBits(uint32(member.name.size()), 6);
        uint8 const finalMaskOrder[] = { 7, 3, 0, 4 };
        for (uint8 index : finalMaskOrder)
            out.WriteBit(GuidByte(memberGuid, index) != 0);

        memberData.WriteByteSeq(GuidByte(memberGuid, 6));
        memberData.WriteByteSeq(GuidByte(memberGuid, 3));
        memberData << member.roles << member.status;
        memberData.WriteByteSeq(GuidByte(memberGuid, 7));
        memberData.WriteByteSeq(GuidByte(memberGuid, 4));
        memberData.WriteByteSeq(GuidByte(memberGuid, 1));
        memberData.append(member.name.data(), member.name.size());
        memberData.WriteByteSeq(GuidByte(memberGuid, 5));
        memberData.WriteByteSeq(GuidByte(memberGuid, 2));
        memberData << member.subgroup;
        memberData.WriteByteSeq(GuidByte(memberGuid, 0));
        memberData << member.flags;
    }

    out.WriteBit(GuidByte(leaderGuid, 3) != 0);
    out.WriteBit(GuidByte(leaderGuid, 0) != 0);
    out.WriteBit(update.hasLootMode);
    out.WriteBit(GuidByte(groupGuid, 5) != 0);
    if (update.hasLootMode)
    {
        uint8 const looterMaskOrder[] = { 6, 4, 5, 2, 1, 0, 7, 3 };
        for (uint8 index : looterMaskOrder)
            out.WriteBit(GuidByte(looterGuid, index) != 0);
    }
    uint8 const groupMaskOrder[] = { 2, 4, 1 };
    for (uint8 index : groupMaskOrder)
        out.WriteBit(GuidByte(groupGuid, index) != 0);
    out.WriteBit(update.isLfg);
    out.WriteBit(GuidByte(leaderGuid, 2) != 0);
    out.WriteBit(GuidByte(groupGuid, 6) != 0);
    if (update.isLfg)
    {
        out.WriteBit(update.lfgUnknownBit0);
        out.WriteBit(update.lfgUnknownBit1);
    }
    out.WriteBit(GuidByte(leaderGuid, 4) != 0);
    out.WriteBit(GuidByte(groupGuid, 3) != 0);
    out.FlushBits();

    out.WriteByteSeq(GuidByte(leaderGuid, 0));
    if (update.hasInstanceDifficulty)
    {
        out << update.raidDifficulty;
        out << update.dungeonDifficulty;
    }
    out.append(memberData);
    out.WriteByteSeq(GuidByte(groupGuid, 1));

    if (update.isLfg)
    {
        out << update.lfgFloat;
        out << update.lfgUnknownByte0 << update.lfgUnknownByte1;
        out << update.lfgDungeonEntry;
        out << update.lfgUnknownByte2 << update.lfgUnknownByte3 << update.lfgUnknownByte4;
        out << update.lfgTail;
    }

    out.WriteByteSeq(GuidByte(leaderGuid, 4));
    out.WriteByteSeq(GuidByte(leaderGuid, 2));
    if (update.hasLootMode)
    {
        out << update.lootMethod;
        uint8 const looterBytesFirst[] = { 0, 5, 4, 3, 2 };
        for (uint8 index : looterBytesFirst)
            out.WriteByteSeq(GuidByte(looterGuid, index));
        out << update.lootThreshold;
        uint8 const looterBytesLast[] = { 7, 1, 6 };
        for (uint8 index : looterBytesLast)
            out.WriteByteSeq(GuidByte(looterGuid, index));
    }

    out.WriteByteSeq(GuidByte(groupGuid, 6));
    out.WriteByteSeq(GuidByte(groupGuid, 4));
    out << update.groupType;
    out << update.partyIndex;
    out << update.groupPosition;
    out.WriteByteSeq(GuidByte(groupGuid, 7));
    out.WriteByteSeq(GuidByte(leaderGuid, 3));
    out.WriteByteSeq(GuidByte(leaderGuid, 1));
    out << update.sequence;
    out.WriteByteSeq(GuidByte(groupGuid, 0));
    out.WriteByteSeq(GuidByte(groupGuid, 2));
    out.WriteByteSeq(GuidByte(groupGuid, 5));
    out.WriteByteSeq(GuidByte(groupGuid, 3));
    out.WriteByteSeq(GuidByte(leaderGuid, 7));
    out << update.groupRole;
    out.WriteByteSeq(GuidByte(leaderGuid, 5));
    out.WriteByteSeq(GuidByte(leaderGuid, 6));
    return true;
}

bool MopPartyUpdatePackets::BuildRemovedPartyUpdate(WorldPacket& out,
    PartyUpdate const& update)
{
    if (!update.members.empty() || update.hasInstanceDifficulty ||
        update.hasLootMode || update.isLfg)
        return false;
    return BuildPartyUpdate(out, update);
}
