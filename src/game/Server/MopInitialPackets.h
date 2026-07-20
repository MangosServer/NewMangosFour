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

#ifndef MANGOS_H_MOPINITIALPACKETS
#define MANGOS_H_MOPINITIALPACKETS

#include "Common.h"

#include <array>
#include <vector>

class WorldPacket;

namespace MopInitialPackets
{
    static size_t const ACTION_BUTTON_COUNT = 132;
    static size_t const ACCOUNT_DATA_COUNT = 8;
    static size_t const REPUTATION_COUNT = 256;
    static size_t const TUTORIAL_WORD_COUNT = 8;

    struct UnlearnSpell
    {
        uint32 first = 0;
        uint8 middle = 0;
        uint32 last = 0;
    };

    struct ActionButton
    {
        uint32 action = 0;
        uint8 type = 0;
    };

    struct Reputation
    {
        uint8 flags = 0;
        int32 standing = 0;
        bool bonus = false;
    };

    void BuildInitialSpells(WorldPacket& out, std::vector<uint32> const& spells);
    void BuildSendUnlearnSpells(WorldPacket& out, std::vector<UnlearnSpell> const& records);
    void BuildActionButtons(WorldPacket& out,
        std::array<ActionButton, ACTION_BUTTON_COUNT> const& buttons, uint8 state);
    void BuildAccountDataTimes(WorldPacket& out,
        std::array<uint32, ACCOUNT_DATA_COUNT> const& times, uint32 mask,
        uint32 serverTime, bool forceUpdate);
    void BuildFeatureSystemStatus(WorldPacket& out, bool storeEnabled,
        bool feedbackEnabled, bool excessiveWarning);
    void BuildInitializeFactions(WorldPacket& out,
        std::array<Reputation, REPUTATION_COUNT> const& reputations);
    void BuildTutorialFlags(WorldPacket& out,
        std::array<uint32, TUTORIAL_WORD_COUNT> const& words);
    void BuildBindPointUpdate(WorldPacket& out, float x, float y, float z,
        uint32 areaId, uint32 mapId);
    void BuildSetProficiency(WorldPacket& out, uint32 mask, uint8 itemClass);
    void BuildWeather(WorldPacket& out, uint32 state, float intensity, bool abrupt);
}

#endif
