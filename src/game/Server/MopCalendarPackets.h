/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 5.4.8 client.
 */

#ifndef MANGOS_H_MOPCALENDARPACKETS
#define MANGOS_H_MOPCALENDARPACKETS

#include "Common.h"

#include <vector>

class WorldPacket;

namespace MopCalendarPackets
{
    struct InitialInvite
    {
        uint64 guid = 0;
        uint8 level = 0;
    };

    struct InviteStatus
    {
        uint64 inviteeGuid = 0;
        uint64 eventId = 0;
        uint32 eventFlags = 0;
        uint32 lastUpdateTime = 0;
        uint32 eventTime = 0;
        uint8 status = 0;
        bool displayPendingAction = false;
    };

    struct ModeratorStatus
    {
        uint64 inviteeGuid = 0;
        uint64 eventId = 0;
        uint8 rank = 0;
        bool displayPendingAction = false;
    };

    bool BuildCalendarInitialInvite(WorldPacket& out,
        std::vector<InitialInvite> const& entries);
    void BuildCalendarInviteStatus(WorldPacket& out,
        InviteStatus const& record);
    void BuildCalendarModeratorStatus(WorldPacket& out,
        ModeratorStatus const& record);
}

#endif
