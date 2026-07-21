/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 5.4.8 client.
 */

#ifndef MANGOS_H_MOPRATEDBATTLEGROUNDPACKETS
#define MANGOS_H_MOPRATEDBATTLEGROUNDPACKETS

#include "Common.h"

#include <array>

class WorldPacket;

namespace MopRatedBattlegroundPackets
{
    using RatedStatsRecord = std::array<uint32, 8>;
    using RatedStats = std::array<RatedStatsRecord, 4>;

    void BuildBattlefieldRatedInfo(WorldPacket& out, RatedStats const& records);
}

#endif
