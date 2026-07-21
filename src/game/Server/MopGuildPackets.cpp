/**
 * Fixed 5.4.8.18414 guild packet bodies.
 */

#include "MopGuildPackets.h"
#include "WorldPacket.h"

bool MopGuildPackets::BuildGuildMotd(WorldPacket& out,
    std::string const& motd)
{
    if (motd.size() >= (size_t(1) << 10))
        return false;

    out.WriteBits(motd.size(), 10);
    out.FlushBits();
    out.append(motd.data(), motd.size());
    return true;
}
