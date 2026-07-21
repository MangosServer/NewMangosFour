/**
 * Fixed 5.4.8.18414 dungeon-finder packet bodies.
 */

#ifndef MANGOS_MOP_LFG_PACKETS_H
#define MANGOS_MOP_LFG_PACKETS_H

#include "Common.h"

#include <string>

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

    bool BuildBootPlayer(WorldPacket& out, BootUpdate const& update);
}

#endif
