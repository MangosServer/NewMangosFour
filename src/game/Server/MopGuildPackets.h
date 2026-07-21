/**
 * Fixed 5.4.8.18414 guild packet bodies.
 */

#ifndef MANGOS_MOP_GUILD_PACKETS_H
#define MANGOS_MOP_GUILD_PACKETS_H

#include <string>

class WorldPacket;

namespace MopGuildPackets
{
    bool BuildGuildMotd(WorldPacket& out, std::string const& motd);
}

#endif
