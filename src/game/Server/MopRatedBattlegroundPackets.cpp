/**
 * Fixed 5.4.8.18414 rated-battleground self-stat packet body.
 */

#include "MopRatedBattlegroundPackets.h"
#include "WorldPacket.h"

void MopRatedBattlegroundPackets::BuildBattlefieldRatedInfo(WorldPacket& out,
    RatedStats const& records)
{
    for (RatedStatsRecord const& record : records)
        for (uint32 field : record)
            out << field;
}
