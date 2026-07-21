/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * client version 5.4.8 build 18414.
 */

#ifndef MANGOS_H_MOPPARTYUPDATEPACKETS
#define MANGOS_H_MOPPARTYUPDATEPACKETS

#include "Common.h"

#include <string>
#include <vector>

class WorldPacket;

namespace MopPartyUpdatePackets
{
    struct Member
    {
        uint64 guid = 0;
        std::string name;
        uint8 roles = 0;
        uint8 status = 0;
        uint8 subgroup = 0;
        uint8 flags = 0;
    };

    struct PartyUpdate
    {
        uint64 groupGuid = 0;
        uint64 leaderGuid = 0;
        uint64 looterGuid = 0;
        std::vector<Member> members;

        bool hasInstanceDifficulty = false;
        uint32 raidDifficulty = 0;
        uint32 dungeonDifficulty = 0;

        bool hasLootMode = false;
        uint8 lootMethod = 0;
        uint8 lootThreshold = 0;

        bool isLfg = false;
        bool lfgUnknownBit0 = false;
        bool lfgUnknownBit1 = false;
        float lfgFloat = 1.0f;
        uint8 lfgUnknownByte0 = 0;
        uint8 lfgUnknownByte1 = 0;
        uint32 lfgDungeonEntry = 0;
        uint8 lfgUnknownByte2 = 0;
        uint8 lfgUnknownByte3 = 0;
        uint8 lfgUnknownByte4 = 0;
        uint32 lfgTail = 0;

        uint8 groupType = 0;
        uint8 partyIndex = 0;
        int32 groupPosition = 0;
        uint32 sequence = 0;
        uint8 groupRole = 0;
    };

    uint8 ReadRequest(WorldPacket& in);
    bool BuildPartyUpdate(WorldPacket& out, PartyUpdate const& update);
    bool BuildRemovedPartyUpdate(WorldPacket& out, PartyUpdate const& update);
}

#endif
