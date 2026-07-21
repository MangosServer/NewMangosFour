/**
 * Fixed 5.4.8.18414 dungeon-finder packet bodies.
 */

#ifndef MANGOS_MOP_LFG_PACKETS_H
#define MANGOS_MOP_LFG_PACKETS_H

#include "Common.h"

#include <array>
#include <string>
#include <vector>

class WorldPacket;

namespace MopLfgPackets
{
    struct BootUpdate
    {
        uint64 victimGuid = 0;
        std::string reason;
        bool inProgress = false;
        bool didVote = false;
        bool votePassed = false;
        bool agree = false;
        uint32 votesNeeded = 0;
        uint32 timeLeft = 0;
        uint32 agreeCount = 0;
        uint32 voteCount = 0;
    };

    struct StatusUpdate
    {
        uint64 requesterGuid = 0;
        std::vector<uint64> suspendedPlayerGuids;
        std::vector<uint32> dungeonEntries;
        std::string comment;
        std::array<uint8, 3> needs{};
        bool isParty = false;
        bool joined = false;
        bool notifyUi = true;
        bool lfgJoined = false;
        bool queued = false;
        uint8 updateReason = 0;
        uint32 requestedRoles = 0;
        uint32 ticketId = 0;
        uint32 ticketTime = 0;
        uint8 dungeonCategory = 0;
        uint32 ticketType = 3;
    };

    bool BuildBootPlayer(WorldPacket& out, BootUpdate const& update);
    bool BuildUpdateStatus(WorldPacket& out, StatusUpdate const& update);
}

#endif
