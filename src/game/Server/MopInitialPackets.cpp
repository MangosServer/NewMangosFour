/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "MopInitialPackets.h"
#include "WorldPacket.h"

namespace
{
    uint8 ActionButtonByte(MopInitialPackets::ActionButton const& button, uint8 index)
    {
        uint64 const value = uint64(button.action) | (uint64(button.type) << 56);
        return uint8(value >> (8 * index));
    }
}

void MopInitialPackets::BuildInitialSpells(WorldPacket& out,
    std::vector<uint32> const& spells)
{
    MANGOS_ASSERT(spells.size() < (size_t(1) << 22));
    out.WriteBit(false);
    out.WriteBits(uint32(spells.size()), 22);
    out.FlushBits();
    for (uint32 spell : spells)
        out << spell;
}

void MopInitialPackets::BuildSendUnlearnSpells(WorldPacket& out,
    std::vector<UnlearnSpell> const& records)
{
    MANGOS_ASSERT(records.size() < (size_t(1) << 21));
    out.WriteBits(uint32(records.size()), 21);
    out.FlushBits();
    for (UnlearnSpell const& record : records)
        out << record.first << record.middle << record.last;
}

void MopInitialPackets::BuildActionButtons(WorldPacket& out,
    std::array<ActionButton, ACTION_BUTTON_COUNT> const& buttons, uint8 state)
{
    uint8 const maskOrder[] = { 4, 5, 3, 1, 6, 7, 0, 2 };
    uint8 const byteOrder[] = { 0, 1, 4, 6, 7, 2, 5, 3 };

    for (uint8 byteIndex : maskOrder)
        for (ActionButton const& button : buttons)
            out.WriteBit(ActionButtonByte(button, byteIndex) != 0);
    out.FlushBits();

    for (uint8 byteIndex : byteOrder)
        for (ActionButton const& button : buttons)
            out.WriteByteSeq(ActionButtonByte(button, byteIndex));
    out << state;
}

void MopInitialPackets::BuildAccountDataTimes(WorldPacket& out,
    std::array<uint32, ACCOUNT_DATA_COUNT> const& times, uint32 mask,
    uint32 serverTime, bool forceUpdate)
{
    out.WriteBit(forceUpdate);
    out.FlushBits();
    for (uint32 value : times)
        out << value;
    out << mask << serverTime;
}

void MopInitialPackets::BuildFeatureSystemStatus(WorldPacket& out,
    bool storeEnabled, bool feedbackEnabled, bool excessiveWarning)
{
    out << uint32(0) << uint32(0) << uint32(0) << uint8(2) << uint32(0);
    out.WriteBit(false);
    out.WriteBit(storeEnabled);
    out.WriteBit(false);
    out.WriteBit(false);
    out.WriteBit(false);
    out.WriteBit(true);
    out.WriteBit(false);
    out.WriteBit(excessiveWarning);
    out.WriteBit(false);
    out.WriteBit(feedbackEnabled);
    out.FlushBits();

    if (excessiveWarning)
        out << uint32(0) << uint32(0) << uint32(0);
    if (feedbackEnabled)
        out << uint32(0) << uint32(1) << uint32(10) << uint32(60000);
}

void MopInitialPackets::BuildInitializeFactions(WorldPacket& out,
    std::array<Reputation, REPUTATION_COUNT> const& reputations)
{
    for (Reputation const& reputation : reputations)
        out << reputation.flags << uint32(reputation.standing);
    for (Reputation const& reputation : reputations)
        out.WriteBit(reputation.bonus);
    out.FlushBits();
}

void MopInitialPackets::BuildTutorialFlags(WorldPacket& out,
    std::array<uint32, TUTORIAL_WORD_COUNT> const& words)
{
    for (uint32 word : words)
        out << word;
}

void MopInitialPackets::BuildBindPointUpdate(WorldPacket& out, float x, float y,
    float z, uint32 areaId, uint32 mapId)
{
    out << x << y << z << areaId << mapId;
}

void MopInitialPackets::BuildSetProficiency(WorldPacket& out, uint32 mask,
    uint8 itemClass)
{
    out << mask << itemClass;
}

void MopInitialPackets::BuildWeather(WorldPacket& out, uint32 state,
    float intensity, bool abrupt)
{
    out << state << intensity;
    out.WriteBit(abrupt);
    out.FlushBits();
}
