/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOSSERVER_GOSSIP_H
#define MANGOSSERVER_GOSSIP_H

#include "Common.h"
#include "QuestDef.h"
#include "NPCHandler.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <array>

class WorldSession;

namespace MopQuestGiverPackets
{
    struct QuestEmote
    {
        uint32 delay = 0;
        uint32 emote = 0;
    };

    struct CompleteQuestRequest
    {
        uint64 questGiverGuid = 0;
        uint32 questId = 0;
        bool completionContext = false;
    };

    struct RequestRewardRequest
    {
        uint64 questGiverGuid = 0;
        uint32 questId = 0;
    };

    struct ChooseRewardRequest
    {
        uint64 questGiverGuid = 0;
        uint32 rewardItemId = 0;
        uint32 questId = 0;
    };

    struct QuestRequestItems
    {
        ObjectGuid questGiverGuid;
        std::array<uint32, QUEST_ITEM_OBJECTIVES_COUNT>
            requiredItemDisplayIds = {};
        std::array<uint32, QUEST_ITEM_OBJECTIVES_COUNT>
            requiredItemIds = {};
        std::array<uint32, QUEST_ITEM_OBJECTIVES_COUNT>
            requiredItemCounts = {};
        std::array<uint32, QUEST_REQUIRED_CURRENCY_COUNT>
            requiredCurrencyIds = {};
        std::array<uint32, QUEST_REQUIRED_CURRENCY_COUNT>
            requiredCurrencyCounts = {};
        uint32 suggestedPlayers = 0;
        uint32 questFlags = 0;
        uint32 emoteDelay = 0;
        uint32 statusFlags = 0;
        uint32 requiredMoney = 0;
        uint32 questFlags2 = 0;
        uint32 emote = 0;
        uint32 questId = 0;
        bool autoLaunch = false;
        std::string title;
        std::string requestText;
    };

    struct QuestRewardOffer
    {
        ObjectGuid questGiverGuid;
        std::array<uint32, QUEST_REWARDS_COUNT> rewardItemIds = {};
        std::array<uint32, QUEST_REWARDS_COUNT> rewardItemCounts = {};
        std::array<uint32, QUEST_REWARDS_COUNT>
            rewardItemDisplayIds = {};
        std::array<uint32, QUEST_REWARD_CHOICES_COUNT>
            rewardChoiceItemIds = {};
        std::array<uint32, QUEST_REWARD_CHOICES_COUNT>
            rewardChoiceItemCounts = {};
        std::array<uint32, QUEST_REWARD_CHOICES_COUNT>
            rewardChoiceItemDisplayIds = {};
        std::array<uint32, QUEST_REWARD_CURRENCY_COUNT>
            rewardCurrencyIds = {};
        std::array<uint32, QUEST_REWARD_CURRENCY_COUNT>
            rewardCurrencyCounts = {};
        std::array<uint32, QUEST_REPUTATIONS_COUNT>
            rewardFactionIds = {};
        std::array<uint32, QUEST_REPUTATIONS_COUNT>
            rewardFactionValueIds = {};
        std::array<uint32, QUEST_REPUTATIONS_COUNT>
            rewardFactionValueOverrides = {};
        uint32 rewardChoiceItemCount = 0;
        uint32 questId = 0;
        uint32 rewardSpellCast = 0;
        uint32 rewardPackageItemId = 0;
        uint32 questTurnInPortrait = 0;
        uint32 rewardReputationMask = 0;
        uint32 bonusTalents = 0;
        uint32 rewardSkillId = 0;
        uint32 questFlags = 0;
        uint32 questFlags2 = 0;
        uint32 rewardXp = 0;
        uint32 characterTitleId = 0;
        uint32 rewardItemCount = 0;
        uint32 suggestedPlayers = 0;
        uint32 questTakerEntry = 0;
        uint32 questGiverPortrait = 0;
        uint32 rewardMoney = 0;
        uint32 rewardSpell = 0;
        uint32 rewardSkillPoints = 0;
        bool autoLaunch = false;
        std::string questTurnTargetName;
        std::string questTitle;
        std::string offerRewardText;
        std::string questTurnTextWindow;
        std::string questGiverTargetName;
        std::string questGiverTextWindow;
        std::vector<QuestEmote> emotes;
    };

    struct QuestRewardSummary
    {
        uint32 bonusTalents = 0;
        uint32 money = 0;
        uint32 questId = 0;
        uint32 experience = 0;
    };

    struct QueryQuestRequest
    {
        uint64 questGiverGuid = 0;
        uint32 questId = 0;
    };

    struct AcceptQuestRequest
    {
        uint64 questGiverGuid = 0;
        uint32 questId = 0;
        bool questDetailsAcceptFlag = false;
    };

    struct QuestListEntry
    {
        uint32 questId = 0;
        uint32 icon = 0;
        int32 level = 0;
        uint32 flags = 0;
        uint32 flags2 = 0;
        bool repeatable = false;
        std::string title;
    };

    struct QuestList
    {
        ObjectGuid questGiverGuid;
        uint32 emote = 0;
        uint32 emoteDelay = 0;
        std::string greeting;
        std::vector<QuestListEntry> quests;
    };

    struct QuestObjective
    {
        uint8 type = 0;
        uint32 id = 0;
        int32 amount = 0;
        uint32 objectId = 0;
    };

    struct QuestDetails
    {
        ObjectGuid dividerGuid;
        ObjectGuid questGiverGuid;
        std::array<uint32, QUEST_REWARDS_COUNT> rewardItemIds = {};
        std::array<uint32, QUEST_REWARDS_COUNT> rewardItemCounts = {};
        std::array<uint32, QUEST_REWARDS_COUNT> rewardItemDisplayIds = {};
        std::array<uint32, QUEST_REWARD_CHOICES_COUNT>
            rewardChoiceItemIds = {};
        std::array<uint32, QUEST_REWARD_CHOICES_COUNT>
            rewardChoiceItemCounts = {};
        std::array<uint32, QUEST_REWARD_CHOICES_COUNT>
            rewardChoiceItemDisplayIds = {};
        std::array<uint32, QUEST_REWARD_CURRENCY_COUNT>
            rewardCurrencyIds = {};
        std::array<uint32, QUEST_REWARD_CURRENCY_COUNT>
            rewardCurrencyCounts = {};
        std::array<uint32, QUEST_REPUTATIONS_COUNT> rewardFactionIds = {};
        std::array<uint32, QUEST_REPUTATIONS_COUNT>
            rewardFactionValueIds = {};
        std::array<uint32, QUEST_REPUTATIONS_COUNT>
            rewardFactionValueOverrides = {};
        uint32 rewardChoiceItemCount = 0;
        uint32 questGiverPortrait = 0;
        uint32 questId = 0;
        uint32 suggestedPlayers = 0;
        uint32 bonusTalents = 0;
        uint32 rewardSkillId = 0;
        uint32 rewardXp = 0;
        uint32 rewardReputationMask = 0;
        uint32 rewardSpell = 0;
        uint32 questFlags = 0;
        uint32 characterTitleId = 0;
        uint32 rewardOrRequiredMoney = 0;
        uint32 questFlags2 = 0;
        uint32 rewardSpellCastOrUnknown = 0;
        uint32 rewardItemCount = 0;
        uint32 rewardSkillPoints = 0;
        uint32 rewardPackageItemId = 0;
        uint32 questTurnInPortrait = 0;
        bool startedByAreaTrigger = false;
        bool startQuestCheat = false;
        bool questDetailsAcceptFlag = false;
        std::string questTurnTargetName;
        std::string questGiverTextWindow;
        std::string questTitle;
        std::string questGiverTargetName;
        std::string questTurnTextWindow;
        std::string questDetails;
        std::string questObjectives;
        std::vector<QuestObjective> objectives;
        std::vector<QuestEmote> emotes;
        std::vector<uint32> learnSpells;
    };

    inline bool RejectRequest(WorldPacket& in)
    {
        in.rfinish();
        return false;
    }

    inline size_t PresentByteCount(uint8 mask)
    {
        size_t count = 0;
        for (; mask != 0; mask >>= 1)
        {
            count += mask & 1;
        }
        return count;
    }

    inline bool HasCanonicalGuidBytes(WorldPacket const& in,
        size_t offset, size_t count)
    {
        for (size_t index = 0; index < count; ++index)
        {
            // A present packed-GUID byte is XORed with 1 on the wire. Wire
            // value 1 therefore decodes to zero and has a non-canonical mask.
            if (in[offset + index] == 1)
            {
                return false;
            }
        }
        return true;
    }

    inline bool ParseCompleteQuest(WorldPacket& in,
        CompleteQuestRequest& request)
    {
        if (in.size() - in.rpos() < 6)
        {
            return RejectRequest(in);
        }

        CompleteQuestRequest parsed;
        in >> parsed.questId;

        uint8 const firstBits = in[in.rpos()];
        uint8 const secondBits = in[in.rpos() + 1];
        size_t const byteCount = PresentByteCount(firstBits & 0xFE) +
            ((secondBits & 0x80) != 0);
        if ((secondBits & 0x7F) != 0 ||
            in.size() - in.rpos() != 2 + byteCount ||
            !HasCanonicalGuidBytes(in, in.rpos() + 2, byteCount))
        {
            return RejectRequest(in);
        }

        ObjectGuid questGiverGuid;
        in.ReadGuidMask<4, 2, 1, 5, 6, 7, 3>(questGiverGuid);
        parsed.completionContext = in.ReadBit();
        in.ReadGuidMask<0>(questGiverGuid);
        uint32 const padding = in.ReadBits(7);
        in.ReadGuidBytes<0, 2, 1, 4, 3, 6, 7, 5>(questGiverGuid);
        if (padding != 0 || in.rpos() != in.size())
        {
            return RejectRequest(in);
        }

        parsed.questGiverGuid = questGiverGuid.GetRawValue();
        request = parsed;
        return true;
    }

    inline bool ParseRequestReward(WorldPacket& in,
        RequestRewardRequest& request)
    {
        if (in.size() - in.rpos() < 5)
        {
            return RejectRequest(in);
        }

        RequestRewardRequest parsed;
        in >> parsed.questId;

        uint8 const mask = in[in.rpos()];
        size_t const byteCount = PresentByteCount(mask);
        if (in.size() - in.rpos() != 1 + byteCount ||
            !HasCanonicalGuidBytes(in, in.rpos() + 1, byteCount))
        {
            return RejectRequest(in);
        }

        ObjectGuid questGiverGuid;
        in.ReadGuidMask<6, 3, 1, 2, 4, 0, 5, 7>(questGiverGuid);
        in.ReadGuidBytes<3, 0, 7, 6, 2, 1, 5, 4>(questGiverGuid);
        if (in.rpos() != in.size())
        {
            return RejectRequest(in);
        }

        parsed.questGiverGuid = questGiverGuid.GetRawValue();
        request = parsed;
        return true;
    }

    inline bool ParseChooseReward(WorldPacket& in,
        ChooseRewardRequest& request)
    {
        if (in.size() - in.rpos() < 9)
        {
            return RejectRequest(in);
        }

        ChooseRewardRequest parsed;
        // The 18414 client writes the selected reward item ID, not the
        // legacy zero-based reward-choice index.
        in >> parsed.rewardItemId >> parsed.questId;

        uint8 const mask = in[in.rpos()];
        size_t const byteCount = PresentByteCount(mask);
        if (in.size() - in.rpos() != 1 + byteCount ||
            !HasCanonicalGuidBytes(in, in.rpos() + 1, byteCount))
        {
            return RejectRequest(in);
        }

        ObjectGuid questGiverGuid;
        in.ReadGuidMask<2, 6, 0, 5, 1, 3, 7, 4>(questGiverGuid);
        in.ReadGuidBytes<1, 2, 5, 7, 0, 3, 6, 4>(questGiverGuid);
        if (in.rpos() != in.size())
        {
            return RejectRequest(in);
        }

        parsed.questGiverGuid = questGiverGuid.GetRawValue();
        request = parsed;
        return true;
    }

    inline bool ResolveRewardChoice(
        std::array<uint32, QUEST_REWARD_CHOICES_COUNT> const& itemIds,
        uint32 choiceCount, uint32 rewardItemId, uint32& choice)
    {
        if (choiceCount > itemIds.size())
        {
            return false;
        }
        if (choiceCount == 0)
        {
            if (rewardItemId != 0)
            {
                return false;
            }
            choice = 0;
            return true;
        }

        // The cached count is the number of populated columns, not the last
        // populated column. Scan all six slots so a sparse database row still
        // resolves the item ID to its real gameplay choice index.
        for (uint32 index = 0; index < itemIds.size(); ++index)
        {
            if (itemIds[index] != 0 && itemIds[index] == rewardItemId)
            {
                choice = index;
                return true;
            }
        }
        return false;
    }

    inline uint64 RawGuid(ObjectGuid const& guid)
    {
        return guid.GetRawValue();
    }

    inline bool ParseHello(WorldPacket& in, uint64& guid)
    {
        if (in.size() - in.rpos() < 1)
        {
            return RejectRequest(in);
        }

        uint8 const mask = in[in.rpos()];
        size_t const byteCount = PresentByteCount(mask);
        if (in.size() - in.rpos() != 1 + byteCount ||
            !HasCanonicalGuidBytes(in, in.rpos() + 1, byteCount))
        {
            return RejectRequest(in);
        }

        ObjectGuid parsed;
        in.ReadGuidMask<5, 6, 7, 3, 4, 2, 1, 0>(parsed);
        in.ReadGuidBytes<4, 1, 7, 3, 6, 0, 5, 2>(parsed);
        guid = RawGuid(parsed);
        return in.rpos() == in.size();
    }

    inline bool ParseQueryQuest(WorldPacket& in, QueryQuestRequest& request)
    {
        if (in.size() - in.rpos() < 6)
        {
            return RejectRequest(in);
        }

        QueryQuestRequest parsed;
        in >> parsed.questId;

        uint8 const mask = in[in.rpos()];
        size_t const byteCount = PresentByteCount(mask);
        if (in.size() - in.rpos() != 2 + byteCount ||
            in[in.rpos() + 1] != 0x80 ||
            !HasCanonicalGuidBytes(in, in.rpos() + 2, byteCount))
        {
            return RejectRequest(in);
        }

        ObjectGuid questGiverGuid;
        in.ReadGuidMask<2, 6, 5, 0, 4, 3, 1, 7>(questGiverGuid);
        bool const questDetailsRequest = in.ReadBit();
        uint32 const padding = in.ReadBits(7);
        in.ReadGuidBytes<2, 0, 4, 7, 5, 1, 3, 6>(questGiverGuid);
        if (!questDetailsRequest || padding != 0 ||
            in.rpos() != in.size())
        {
            return RejectRequest(in);
        }

        parsed.questGiverGuid = RawGuid(questGiverGuid);
        request = parsed;
        return true;
    }

    inline bool ParseAcceptQuest(WorldPacket& in, AcceptQuestRequest& request)
    {
        if (in.size() - in.rpos() < 6)
        {
            return RejectRequest(in);
        }

        AcceptQuestRequest parsed;
        in >> parsed.questId;

        uint8 const mask = in[in.rpos()];
        uint8 const trailingBits = in[in.rpos() + 1];
        size_t const byteCount = PresentByteCount(mask & 0xDF) +
            ((trailingBits & 0x80) != 0);
        if (in.size() - in.rpos() != 2 + byteCount ||
            (trailingBits & 0x7F) != 0 ||
            !HasCanonicalGuidBytes(in, in.rpos() + 2, byteCount))
        {
            return RejectRequest(in);
        }

        ObjectGuid questGiverGuid;
        in.ReadGuidMask<6, 0>(questGiverGuid);
        parsed.questDetailsAcceptFlag = in.ReadBit();
        in.ReadGuidMask<2, 7, 5, 4, 3, 1>(questGiverGuid);
        uint32 const padding = in.ReadBits(7);
        in.ReadGuidBytes<5, 4, 0, 1, 6, 2, 3, 7>(questGiverGuid);
        if (padding != 0 || in.rpos() != in.size())
        {
            return RejectRequest(in);
        }

        parsed.questGiverGuid = RawGuid(questGiverGuid);
        request = parsed;
        return true;
    }

    inline size_t BoundedLength(std::string const& value, size_t maximum)
    {
        return value.size() < maximum ? value.size() : maximum;
    }

    inline size_t GuidByteCount(ObjectGuid const& guid)
    {
        size_t count = 0;
        uint64 raw = guid.GetRawValue();
        for (size_t index = 0; index < 8; ++index)
        {
            count += uint8(raw >> (index * 8)) != 0;
        }
        return count;
    }

    inline void AppendString(WorldPacket& out, std::string const& value,
        size_t length)
    {
        out.append(value.data(), length);
    }

    inline bool BuildQuestRequestItems(WorldPacket& out,
        QuestRequestItems const& request)
    {
        size_t itemCount = 0;
        for (uint32 itemId : request.requiredItemIds)
        {
            itemCount += itemId != 0;
        }
        size_t currencyCount = 0;
        for (uint32 currencyId : request.requiredCurrencyIds)
        {
            currencyCount += currencyId != 0;
        }

        size_t const titleLength = BoundedLength(request.title, 0x1FF);
        size_t const requestLength =
            BoundedLength(request.requestText, 0xFFF);
        size_t const payloadSize = 45 +
            GuidByteCount(request.questGiverGuid) + titleLength +
            requestLength + 12 * itemCount + 8 * currencyCount;
        if (payloadSize > 0x7FFFF)
        {
            return false;
        }

        out.Initialize(SMSG_QUESTGIVER_REQUEST_ITEMS, payloadSize);
        // sub_6B24E4 consumes these nine fixed words before its bit phase.
        out << request.suggestedPlayers << request.questFlags <<
            request.emoteDelay << request.statusFlags <<
            request.requiredMoney;
        out << uint32(0); // Parsed but unused by the 18414 handler.
        out << request.questFlags2 << request.emote << request.questId;

        out.WriteBits(uint32(currencyCount), 21);
        out.WriteBit(request.autoLaunch);
        out.WriteGuidMask<2, 5, 1>(request.questGiverGuid);
        out.WriteBits(uint32(titleLength), 9);
        out.WriteBits(uint32(requestLength), 12);
        out.WriteGuidMask<6, 0>(request.questGiverGuid);
        out.WriteBits(uint32(itemCount), 20);
        out.WriteGuidMask<4, 7, 3>(request.questGiverGuid);
        out.FlushBits();

        out.WriteGuidBytes<0, 2>(request.questGiverGuid);
        AppendString(out, request.title, titleLength);
        for (size_t index = 0;
            index < request.requiredCurrencyIds.size(); ++index)
        {
            if (request.requiredCurrencyIds[index] == 0)
            {
                continue;
            }
            out << request.requiredCurrencyIds[index] <<
                request.requiredCurrencyCounts[index];
        }
        for (size_t index = 0;
            index < request.requiredItemIds.size(); ++index)
        {
            if (request.requiredItemIds[index] == 0)
            {
                continue;
            }
            // The client rearranges this wire triplet into item ID, count,
            // display ID for the request-items UI.
            out << request.requiredItemDisplayIds[index] <<
                request.requiredItemIds[index] <<
                request.requiredItemCounts[index];
        }
        out.WriteGuidBytes<3, 1>(request.questGiverGuid);
        AppendString(out, request.requestText, requestLength);
        out.WriteGuidBytes<4, 5, 7, 6>(request.questGiverGuid);
        return true;
    }

    inline bool BuildQuestOfferReward(WorldPacket& out,
        QuestRewardOffer const& offer)
    {
        if (offer.emotes.size() >= (size_t(1) << 21))
        {
            return false;
        }

        size_t const turnTargetLength =
            BoundedLength(offer.questTurnTargetName, 0xFF);
        size_t const titleLength =
            BoundedLength(offer.questTitle, 0x1FF);
        size_t const offerTextLength =
            BoundedLength(offer.offerRewardText, 0xFFF);
        size_t const turnWindowLength =
            BoundedLength(offer.questTurnTextWindow, 0x3FF);
        size_t const giverTargetLength =
            BoundedLength(offer.questGiverTargetName, 0xFF);
        size_t const giverWindowLength =
            BoundedLength(offer.questGiverTextWindow, 0x3FF);
        size_t const payloadSize = 299 +
            GuidByteCount(offer.questGiverGuid) + 8 * offer.emotes.size() +
            turnTargetLength + titleLength + offerTextLength +
            turnWindowLength + giverTargetLength + giverWindowLength;
        if (payloadSize > 0x7FFFF)
        {
            return false;
        }

        out.Initialize(SMSG_QUESTGIVER_OFFER_REWARD, payloadSize);
        // sub_6B1C96 consumes this exact 72-word permutation before its
        // variable bit/string section.
        out << offer.rewardItemCounts[2] << offer.questId <<
            offer.rewardItemIds[3] << offer.rewardChoiceItemDisplayIds[2];
        for (size_t index = 0; index < QUEST_REPUTATIONS_COUNT; ++index)
        {
            out << offer.rewardFactionIds[index] <<
                offer.rewardFactionValueIds[index] <<
                offer.rewardFactionValueOverrides[index];
        }
        out << offer.rewardItemCounts[0] << offer.rewardItemCounts[3] <<
            offer.rewardItemDisplayIds[3] << offer.rewardItemIds[1] <<
            offer.rewardChoiceItemIds[3] <<
            offer.rewardChoiceItemDisplayIds[3] <<
            offer.rewardChoiceItemCount << offer.rewardSpellCast <<
            offer.rewardItemDisplayIds[1] <<
            offer.rewardChoiceItemCounts[5] <<
            offer.rewardChoiceItemDisplayIds[4] <<
            offer.rewardChoiceItemCounts[1] <<
            offer.rewardChoiceItemDisplayIds[0] <<
            offer.rewardItemDisplayIds[0] << offer.rewardPackageItemId <<
            offer.questTurnInPortrait << offer.rewardItemCounts[1] <<
            offer.rewardReputationMask << offer.rewardChoiceItemIds[0] <<
            offer.rewardChoiceItemCounts[3] <<
            offer.rewardChoiceItemCounts[4] <<
            offer.rewardChoiceItemIds[1] << offer.bonusTalents <<
            offer.rewardSkillId;
        for (size_t index = 0; index < QUEST_REWARD_CURRENCY_COUNT; ++index)
        {
            out << offer.rewardCurrencyIds[index] <<
                offer.rewardCurrencyCounts[index];
        }
        out << offer.questFlags << offer.questFlags2 << offer.rewardXp <<
            offer.characterTitleId << offer.rewardChoiceItemIds[2] <<
            offer.rewardItemCount << offer.suggestedPlayers <<
            offer.rewardChoiceItemIds[4] << offer.questTakerEntry <<
            offer.rewardItemIds[2] << offer.rewardChoiceItemCounts[0] <<
            offer.rewardChoiceItemDisplayIds[5] <<
            offer.questGiverPortrait << offer.rewardMoney <<
            offer.rewardChoiceItemIds[5] <<
            offer.rewardChoiceItemDisplayIds[1] <<
            offer.rewardChoiceItemCounts[2] <<
            offer.rewardItemDisplayIds[2] << offer.rewardSpell <<
            offer.rewardItemIds[0] << offer.rewardSkillPoints;

        out.WriteBits(uint32(turnWindowLength), 10);
        out.WriteBits(uint32(turnTargetLength), 8);
        out.WriteGuidMask<6>(offer.questGiverGuid);
        out.WriteBits(uint32(offer.emotes.size()), 21);
        out.WriteGuidMask<3, 7>(offer.questGiverGuid);
        out.WriteBits(uint32(titleLength), 9);
        out.WriteGuidMask<4>(offer.questGiverGuid);
        out.WriteBits(uint32(giverTargetLength), 8);
        out.WriteBits(uint32(giverWindowLength), 10);
        out.WriteBits(uint32(offerTextLength), 12);
        out.WriteGuidMask<1, 2, 0, 5>(offer.questGiverGuid);
        out.WriteBit(offer.autoLaunch);
        out.FlushBits();

        AppendString(out, offer.questTurnTargetName, turnTargetLength);
        AppendString(out, offer.questTitle, titleLength);
        for (QuestEmote const& emote : offer.emotes)
        {
            out << emote.delay << emote.emote;
        }
        out.WriteGuidBytes<2>(offer.questGiverGuid);
        AppendString(out, offer.offerRewardText, offerTextLength);
        AppendString(out, offer.questTurnTextWindow, turnWindowLength);
        AppendString(out, offer.questGiverTargetName, giverTargetLength);
        out.WriteGuidBytes<5, 1>(offer.questGiverGuid);
        AppendString(out, offer.questGiverTextWindow, giverWindowLength);
        out.WriteGuidBytes<0, 7, 6, 4, 3>(offer.questGiverGuid);
        return true;
    }

    inline void BuildQuestRewardSummary(WorldPacket& out,
        QuestRewardSummary const& summary)
    {
        out.Initialize(SMSG_QUESTGIVER_QUEST_COMPLETE, 25);
        // The two 18414 flags alter terminal quest-frame cleanup. The
        // reference client/server exchange uses the common 1,0 state.
        out.WriteBit(true);
        out.WriteBit(false);
        out.FlushBits();
        out << summary.bonusTalents << summary.money << summary.questId;
        out << uint32(0); // Unconsumed 18414 scalar; no authoritative backend.
        out << summary.experience;
        out << uint32(0); // Unconsumed 18414 scalar; no authoritative backend.
    }

    inline void BuildQuestUpdateComplete(WorldPacket& out, uint32 questId)
    {
        out.Initialize(SMSG_QUESTUPDATE_COMPLETE, 4);
        out << questId;
    }

    inline bool BuildQuestList(WorldPacket& out, QuestList const& list)
    {
        if (list.quests.size() >= (size_t(1) << 19))
        {
            return false;
        }

        size_t const greetingLength = BoundedLength(list.greeting, 0x7FF);
        size_t payloadSize = 8 + (38 + 10 * list.quests.size() + 7) / 8 +
            GuidByteCount(list.questGiverGuid) + greetingLength +
            20 * list.quests.size();
        for (QuestListEntry const& quest : list.quests)
        {
            payloadSize += BoundedLength(quest.title, 0x1FF);
        }
        if (payloadSize > 0x7FFFF)
        {
            return false;
        }

        out.Initialize(SMSG_QUESTGIVER_QUEST_LIST, payloadSize);
        out << list.emote << list.emoteDelay;
        out.WriteGuidMask<2>(list.questGiverGuid);
        out.WriteBits(uint32(greetingLength), 11);
        out.WriteGuidMask<6, 0>(list.questGiverGuid);
        out.WriteBits(uint32(list.quests.size()), 19);
        for (QuestListEntry const& quest : list.quests)
        {
            out.WriteBit(quest.repeatable);
            out.WriteBits(uint32(BoundedLength(quest.title, 0x1FF)), 9);
        }
        out.WriteGuidMask<1, 3, 4, 5, 7>(list.questGiverGuid);
        out.FlushBits();

        out.WriteGuidBytes<1, 0, 6, 7>(list.questGiverGuid);
        for (QuestListEntry const& quest : list.quests)
        {
            out << quest.flags << quest.questId;
            AppendString(out, quest.title,
                BoundedLength(quest.title, 0x1FF));
            out << quest.flags2 << quest.icon << uint32(quest.level);
        }
        out.WriteGuidBytes<5, 3, 2>(list.questGiverGuid);
        AppendString(out, list.greeting, greetingLength);
        out.WriteGuidBytes<4>(list.questGiverGuid);
        return true;
    }

    inline bool BuildQuestDetails(WorldPacket& out,
        QuestDetails const& details)
    {
        if (details.objectives.size() >= (size_t(1) << 20) ||
            details.emotes.size() >= (size_t(1) << 21) ||
            details.learnSpells.size() >= (size_t(1) << 22))
        {
            return false;
        }

        size_t const turnTargetLength =
            BoundedLength(details.questTurnTargetName, 0xFF);
        size_t const giverWindowLength =
            BoundedLength(details.questGiverTextWindow, 0x3FF);
        size_t const titleLength =
            BoundedLength(details.questTitle, 0x1FF);
        size_t const giverTargetLength =
            BoundedLength(details.questGiverTargetName, 0xFF);
        size_t const turnWindowLength =
            BoundedLength(details.questTurnTextWindow, 0x3FF);
        size_t const detailsLength =
            BoundedLength(details.questDetails, 0xFFF);
        size_t const objectivesLength =
            BoundedLength(details.questObjectives, 0xFFF);

        size_t const payloadSize = 303 +
            GuidByteCount(details.dividerGuid) +
            GuidByteCount(details.questGiverGuid) + turnTargetLength +
            giverWindowLength + titleLength + giverTargetLength +
            turnWindowLength + detailsLength + objectivesLength +
            13 * details.objectives.size() + 8 * details.emotes.size() +
            4 * details.learnSpells.size();
        if (payloadSize > 0x7FFFF)
        {
            return false;
        }

        out.Initialize(SMSG_QUESTGIVER_QUEST_DETAILS, payloadSize);

        // The 18414 reader consumes these 71 u32 values in this exact
        // non-semantic order before its bit section.
        out << details.rewardItemCounts[3];
        out << details.rewardChoiceItemDisplayIds[4];
        out << details.rewardChoiceItemIds[2];
        for (size_t index = 0; index < QUEST_REWARD_CURRENCY_COUNT; ++index)
        {
            out << details.rewardCurrencyCounts[index];
            out << details.rewardCurrencyIds[index];
        }
        out << details.rewardChoiceItemCount;
        out << details.rewardChoiceItemCounts[2];
        out << details.rewardItemCounts[1];
        out << details.rewardChoiceItemDisplayIds[5];
        out << details.rewardItemCounts[0];
        out << details.rewardItemDisplayIds[3];
        out << details.rewardChoiceItemIds[0];
        out << details.rewardChoiceItemCounts[3];
        out << details.questGiverPortrait;
        out << details.rewardChoiceItemDisplayIds[3];
        out << details.rewardItemIds[0];
        out << details.questId;
        out << details.suggestedPlayers;
        out << details.rewardChoiceItemDisplayIds[0];
        out << details.rewardChoiceItemCounts[4];
        out << details.rewardChoiceItemCounts[5];
        out << details.bonusTalents;
        out << details.rewardChoiceItemCounts[1];
        out << details.rewardChoiceItemDisplayIds[2];
        for (size_t index = 0; index < QUEST_REPUTATIONS_COUNT; ++index)
        {
            out << details.rewardFactionValueIds[index];
            out << details.rewardFactionValueOverrides[index];
            out << details.rewardFactionIds[index];
        }
        out << details.rewardItemIds[3];
        out << details.rewardSkillId;
        out << details.rewardXp;
        out << details.rewardReputationMask;
        out << details.rewardItemDisplayIds[2];
        out << details.rewardItemIds[1];
        out << details.rewardChoiceItemIds[1];
        out << details.rewardChoiceItemIds[5];
        out << details.rewardSpell;
        out << details.questFlags;
        out << details.characterTitleId;
        out << details.rewardItemIds[2];
        out << details.rewardOrRequiredMoney;
        out << details.rewardItemCounts[2];
        out << details.questFlags2;
        out << details.rewardSpellCastOrUnknown;
        out << details.rewardChoiceItemIds[3];
        out << details.rewardItemCount;
        out << details.rewardSkillPoints;
        out << details.rewardItemDisplayIds[0];
        out << details.rewardChoiceItemIds[4];
        out << details.rewardPackageItemId;
        out << details.rewardChoiceItemCounts[0];
        out << details.rewardItemDisplayIds[1];
        out << details.rewardChoiceItemDisplayIds[1];
        out << details.questTurnInPortrait;

        out.WriteGuidMask<7>(details.dividerGuid);
        out.WriteGuidMask<1>(details.questGiverGuid);
        out.WriteBits(uint32(turnTargetLength), 8);
        out.WriteGuidMask<2>(details.questGiverGuid);
        out.WriteBits(uint32(giverWindowLength), 10);
        out.WriteBit(details.startedByAreaTrigger);
        out.WriteGuidMask<2>(details.dividerGuid);
        out.WriteBits(uint32(titleLength), 9);
        out.WriteBits(uint32(details.emotes.size()), 21);
        out.WriteGuidMask<0>(details.dividerGuid);
        out.WriteGuidMask<6, 5>(details.questGiverGuid);
        out.WriteBits(uint32(giverTargetLength), 8);
        out.WriteGuidMask<3>(details.questGiverGuid);
        out.WriteGuidMask<1>(details.dividerGuid);
        out.WriteGuidMask<0>(details.questGiverGuid);
        out.WriteBit(details.startQuestCheat);
        out.WriteGuidMask<4>(details.questGiverGuid);
        out.WriteGuidMask<3, 5, 4>(details.dividerGuid);
        out.WriteBits(uint32(turnWindowLength), 10);
        out.WriteBit(details.questDetailsAcceptFlag);
        out.WriteGuidMask<6>(details.dividerGuid);
        out.WriteGuidMask<7>(details.questGiverGuid);
        out.WriteBits(uint32(detailsLength), 12);
        out.WriteBits(uint32(details.learnSpells.size()), 22);
        out.WriteBits(uint32(details.objectives.size()), 20);
        out.WriteBits(uint32(objectivesLength), 12);
        out.FlushBits();

        out.WriteGuidBytes<0>(details.dividerGuid);
        AppendString(out, details.questGiverTargetName, giverTargetLength);
        AppendString(out, details.questTurnTextWindow, turnWindowLength);
        AppendString(out, details.questTitle, titleLength);
        out.WriteGuidBytes<6>(details.questGiverGuid);
        AppendString(out, details.questObjectives, objectivesLength);
        out.WriteGuidBytes<2>(details.dividerGuid);
        AppendString(out, details.questGiverTextWindow, giverWindowLength);
        for (QuestObjective const& objective : details.objectives)
        {
            out << objective.type << objective.id << objective.amount <<
                objective.objectId;
        }
        AppendString(out, details.questTurnTargetName, turnTargetLength);
        AppendString(out, details.questDetails, detailsLength);
        out.WriteGuidBytes<5, 7>(details.dividerGuid);
        out.WriteGuidBytes<7, 3, 0>(details.questGiverGuid);
        for (QuestEmote const& emote : details.emotes)
        {
            out << emote.delay << emote.emote;
        }
        out.WriteGuidBytes<4, 3>(details.dividerGuid);
        out.WriteGuidBytes<5, 1, 2>(details.questGiverGuid);
        out.WriteGuidBytes<1, 6>(details.dividerGuid);
        out.WriteGuidBytes<4>(details.questGiverGuid);
        for (uint32 spellId : details.learnSpells)
        {
            out << spellId;
        }
        return true;
    }
}

namespace MopQuestQueryPackets
{
    struct Request
    {
        uint32 questId = 0;
        uint64 sourceGuid = 0;
    };

    struct Objective
    {
        int32 amount = 0;
        uint32 id = 0;
        std::string text;
        uint32 flags = 0;
        uint8 index = 0;
        uint8 type = 0;
        uint32 objectId = 0;
        std::vector<uint32> visualEffects;
    };

    struct Response
    {
        uint32 questId = 0;
        std::array<uint32, QUEST_SOURCE_ITEM_IDS_COUNT>
            requiredSourceItemIds = {};
        std::array<uint32, QUEST_SOURCE_ITEM_IDS_COUNT>
            requiredSourceItemCounts = {};
        std::array<uint32, QUEST_REWARDS_COUNT> rewardItemIds = {};
        std::array<uint32, QUEST_REWARDS_COUNT> rewardItemCounts = {};
        std::array<uint32, QUEST_REWARD_CHOICES_COUNT>
            rewardChoiceItemIds = {};
        std::array<uint32, QUEST_REWARD_CHOICES_COUNT>
            rewardChoiceItemCounts = {};
        std::array<uint32, QUEST_REWARD_CURRENCY_COUNT>
            rewardCurrencyIds = {};
        std::array<uint32, QUEST_REWARD_CURRENCY_COUNT>
            rewardCurrencyCounts = {};
        std::array<uint32, QUEST_REPUTATIONS_COUNT> rewardFactionIds = {};
        std::array<uint32, QUEST_REPUTATIONS_COUNT>
            rewardFactionValueIds = {};
        std::array<uint32, QUEST_REPUTATIONS_COUNT>
            rewardFactionValueOverrides = {};
        uint32 characterTitleId = 0;
        float pointY = 0.0f;
        uint32 soundTurnIn = 0;
        int32 rewardMoney = 0;
        uint32 minimapTargetMark = 0;
        uint32 rewardMoneyMaxLevel = 0;
        uint32 rewardHonorAddition = 0;
        uint32 obsoleteArenaPoints = 0;
        uint32 suggestedPlayers = 0;
        uint32 repObjectiveFaction = 0;
        int32 minLevel = 0;
        uint32 rewardReputationMask = 0;
        uint32 pointOpt = 0;
        int32 questLevel = 0;
        uint32 requiredOppositeRepFaction = 0;
        uint32 rewardXpId = 0;
        uint32 rewardSpellCast = 0;
        uint32 rewardSkillPoints = 0;
        uint32 questType = 0;
        int32 requiredOppositeRepValue = 0;
        uint32 playersSlain = 0;
        uint32 pointMapId = 0;
        uint32 nextQuestInChain = 0;
        float pointX = 0.0f;
        uint32 soundAccept = 0;
        float rewardHonorMultiplier = 0.0f;
        uint32 requiredSpell = 0;
        int32 zoneOrSort = 0;
        uint32 bonusTalents = 0;
        uint32 rewardSpell = 0;
        uint32 rewardSkillId = 0;
        uint32 questFlags = 0;
        uint32 questMethod = 0;
        uint32 sourceItemId = 0;
        std::string title;
        std::string details;
        std::string objectivesText;
        std::string endText;
        std::string completedText;
        std::string portraitGiverText;
        std::string portraitGiverName;
        std::string portraitTurnInText;
        std::string portraitTurnInName;
        std::vector<Objective> objectives;
    };

    inline size_t PresentByteCount(uint8 mask)
    {
        size_t count = 0;
        for (; mask != 0; mask >>= 1)
        {
            count += mask & 1;
        }
        return count;
    }

    inline bool HasCanonicalGuidBytes(WorldPacket const& in,
        size_t offset, size_t count)
    {
        for (size_t index = 0; index < count; ++index)
        {
            if (in[offset + index] == 1)
            {
                return false;
            }
        }
        return true;
    }

    inline bool RejectRequest(WorldPacket& in)
    {
        in.rfinish();
        return false;
    }

    inline bool ParseRequest(WorldPacket& in, Request& request)
    {
        if (in.size() - in.rpos() < 5)
        {
            return RejectRequest(in);
        }

        Request parsed;
        in >> parsed.questId;
        uint8 const mask = in[in.rpos()];
        size_t const byteCount = PresentByteCount(mask);
        if (in.size() - in.rpos() != 1 + byteCount ||
            !HasCanonicalGuidBytes(in, in.rpos() + 1, byteCount))
        {
            return RejectRequest(in);
        }

        ObjectGuid sourceGuid;
        in.ReadGuidMask<0, 5, 2, 7, 6, 4, 1, 3>(sourceGuid);
        in.ReadGuidBytes<4, 1, 7, 5, 2, 3, 6, 0>(sourceGuid);
        if (in.rpos() != in.size())
        {
            return RejectRequest(in);
        }

        parsed.sourceGuid = sourceGuid.GetRawValue();
        request = parsed;
        return true;
    }

    inline size_t BoundedLength(std::string const& value, size_t maximum)
    {
        return value.size() < maximum ? value.size() : maximum;
    }

    inline void AppendString(WorldPacket& out, std::string const& value,
        size_t length)
    {
        out.append(value.data(), length);
    }

    inline void BuildAbsentResponse(WorldPacket& out, uint32 questId)
    {
        out.Initialize(SMSG_QUEST_QUERY_RESPONSE, 5);
        out << questId;
        out.WriteBit(false);
        out.FlushBits();
    }

    inline bool BuildResponse(WorldPacket& out, Response const& response)
    {
        if (response.objectives.size() >= (size_t(1) << 19))
        {
            return false;
        }
        for (Objective const& objective : response.objectives)
        {
            if (objective.visualEffects.size() >= (size_t(1) << 22))
            {
                return false;
            }
        }

        size_t const turnTextLength =
            BoundedLength(response.portraitTurnInText, 0x3FF);
        size_t const titleLength = BoundedLength(response.title, 0x1FF);
        size_t const completedLength =
            BoundedLength(response.completedText, 0x7FF);
        size_t const detailsLength =
            BoundedLength(response.details, 0xFFF);
        size_t const turnNameLength =
            BoundedLength(response.portraitTurnInName, 0xFF);
        size_t const giverNameLength =
            BoundedLength(response.portraitGiverName, 0xFF);
        size_t const giverTextLength =
            BoundedLength(response.portraitGiverText, 0x3FF);
        size_t const endTextLength =
            BoundedLength(response.endText, 0x1FF);
        size_t const objectivesTextLength =
            BoundedLength(response.objectivesText, 0xFFF);

        WorldPacket built(SMSG_QUEST_QUERY_RESPONSE, 512);
        built << response.questId;
        built.WriteBit(true);
        built.WriteBits(uint32(turnTextLength), 10);
        built.WriteBits(uint32(titleLength), 9);
        built.WriteBits(uint32(completedLength), 11);
        built.WriteBits(uint32(detailsLength), 12);
        built.WriteBits(uint32(turnNameLength), 8);
        built.WriteBits(uint32(giverNameLength), 8);
        built.WriteBits(uint32(giverTextLength), 10);
        built.WriteBits(uint32(endTextLength), 9);
        built.WriteBits(uint32(response.objectives.size()), 19);
        built.WriteBits(uint32(objectivesTextLength), 12);
        for (Objective const& objective : response.objectives)
        {
            built.WriteBits(uint32(BoundedLength(objective.text, 0xFF)), 8);
            built.WriteBits(uint32(objective.visualEffects.size()), 22);
        }
        built.FlushBits();

        for (Objective const& objective : response.objectives)
        {
            built << objective.amount << objective.id;
            AppendString(built, objective.text,
                BoundedLength(objective.text, 0xFF));
            built << objective.flags << objective.index << objective.type <<
                objective.objectId;
            for (uint32 visualEffect : objective.visualEffects)
            {
                built << visualEffect;
            }
        }

        // sub_6B73C9 consumes the record in this non-semantic order. Fields
        // without an authoritative MaNGOS backend remain explicit zeroes.
        built << response.requiredSourceItemIds[0];
        built << response.rewardChoiceItemIds[4];
        built << response.rewardItemIds[3];
        built << response.rewardItemCounts[1];
        built << response.rewardChoiceItemCounts[2];
        for (size_t index = 0; index < QUEST_REWARD_CURRENCY_COUNT; ++index)
        {
            built << response.rewardCurrencyIds[index];
            built << response.rewardCurrencyCounts[index];
        }
        built << response.characterTitleId;
        built << response.pointY;
        built << response.soundTurnIn;
        for (size_t index = 0; index < QUEST_REPUTATIONS_COUNT; ++index)
        {
            built << response.rewardFactionValueIds[index];
            built << response.rewardFactionValueOverrides[index];
            built << response.rewardFactionIds[index];
        }
        built << response.rewardMoney;
        built << response.rewardChoiceItemCounts[4];
        built << response.rewardChoiceItemCounts[1];
        built << response.minimapTargetMark;
        AppendString(built, response.endText, endTextLength);
        built << response.rewardChoiceItemIds[1];
        built << response.rewardMoneyMaxLevel;
        built << response.rewardItemIds[0];
        AppendString(built, response.completedText, completedLength);
        built << response.rewardChoiceItemIds[3];
        built << response.rewardHonorAddition;
        AppendString(built, response.portraitGiverText, giverTextLength);
        AppendString(built, response.objectivesText, objectivesTextLength);
        built << response.obsoleteArenaPoints;
        built << response.rewardChoiceItemIds[5];
        built << response.suggestedPlayers;
        built << response.repObjectiveFaction;
        built << response.requiredSourceItemIds[1];
        built << response.rewardItemIds[1];
        built << response.minLevel;
        built << response.rewardReputationMask;
        built << response.pointOpt;
        built << response.questLevel;
        built << response.requiredOppositeRepFaction;
        built << response.requiredSourceItemCounts[2];
        built << response.rewardXpId;
        AppendString(built, response.details, detailsLength);
        built << response.rewardItemCounts[0];
        built << response.rewardChoiceItemCounts[5];
        built << response.rewardItemCounts[2];
        built << response.rewardSpellCast;
        built << uint32(0);
        AppendString(built, response.portraitTurnInName, turnNameLength);
        built << uint32(0);
        built << response.requiredSourceItemCounts[1];
        built << response.requiredSourceItemIds[2];
        built << response.rewardSkillPoints;
        AppendString(built, response.title, titleLength);
        built << response.questType;
        built << response.requiredOppositeRepValue;
        built << uint32(0);
        built << response.playersSlain;
        built << response.pointMapId;
        built << response.nextQuestInChain;
        built << response.rewardChoiceItemIds[0];
        AppendString(built, response.portraitGiverName, giverNameLength);
        built << uint32(0);
        built << response.requiredSourceItemIds[3];
        built << response.pointX;
        built << response.rewardChoiceItemIds[2];
        built << uint32(0);
        built << response.rewardItemCounts[3];
        built << response.soundAccept;
        built << response.rewardItemIds[2];
        built << response.rewardHonorMultiplier;
        built << response.requiredSpell;
        AppendString(built, response.portraitTurnInText, turnTextLength);
        built << response.rewardChoiceItemCounts[3];
        built << response.requiredSourceItemCounts[0];
        built << response.zoneOrSort;
        built << response.bonusTalents;
        built << response.rewardChoiceItemCounts[0];
        built << response.rewardSpell;
        built << response.rewardSkillId;
        built << uint32(0);
        built << uint32(0);
        built << response.questFlags;
        built << response.questMethod;
        built << response.sourceItemId;

        if (built.size() > 0x7FFFF)
        {
            return false;
        }
        out = built;
        return true;
    }
}

namespace MopQuestStatusPackets
{
    struct StatusEntry
    {
        ObjectGuid guid;
        uint32 status = 0;
    };

    inline bool ParseMultipleStatusQuery(WorldPacket& in)
    {
        // The direct 18414 packet class has a null body; any payload is
        // malformed rather than an optional extension.
        if (in.rpos() != in.size())
        {
            in.rfinish();
            return false;
        }
        return true;
    }

    inline ObjectGuid ReadStatusQuery(WorldPacket& in)
    {
        ObjectGuid guid;
        in.ReadGuidMask<4, 3, 2, 1, 0, 5, 7, 6>(guid);
        in.ReadGuidBytes<5, 7, 4, 0, 2, 1, 6, 3>(guid);
        return guid;
    }

    inline void BuildStatus(WorldPacket& out, uint32 status,
        ObjectGuid questGiverGuid)
    {
        out.Initialize(SMSG_QUESTGIVER_STATUS, 13);
        out.WriteGuidMask<1, 7, 4, 2, 5, 3, 6, 0>(questGiverGuid);
        out.FlushBits();
        out.WriteGuidBytes<7>(questGiverGuid);
        out << status;
        out.WriteGuidBytes<4, 6, 1, 5, 2, 0, 3>(questGiverGuid);
    }

    inline bool BuildMultipleStatus(WorldPacket& out,
        std::vector<StatusEntry> const& entries)
    {
        if (entries.size() >= (size_t(1) << 21))
        {
            return false;
        }

        // The entry count below is 21 bits, but the separate post-crypt
        // transport header limits the complete payload to 19 bits.
        size_t payloadSize = 3 + entries.size() * 5;
        for (StatusEntry const& entry : entries)
        {
            uint64 guid = entry.guid.GetRawValue();
            for (size_t index = 0; index < 8; ++index)
            {
                payloadSize += uint8(guid >> (index * 8)) != 0;
            }
        }
        if (payloadSize > 0x7FFFF)
        {
            return false;
        }

        out.Initialize(SMSG_QUESTGIVER_STATUS_MULTIPLE, payloadSize);
        out.WriteBits(uint32(entries.size()), 21);
        for (StatusEntry const& entry : entries)
        {
            out.WriteGuidMask<4, 0, 3, 6, 5, 7, 1, 2>(entry.guid);
        }
        out.FlushBits();

        for (StatusEntry const& entry : entries)
        {
            out.WriteGuidBytes<6, 2, 7, 5, 4>(entry.guid);
            out << entry.status;
            out.WriteGuidBytes<1, 3, 0>(entry.guid);
        }
        return true;
    }
}

namespace MopGossipPackets
{
    struct GossipItem
    {
        uint32 optionId = 0;
        uint8 icon = 0;
        bool coded = false;
        uint32 boxMoney = 0;
        std::string message;
        std::string boxMessage;
    };

    struct QuestItem
    {
        uint32 questId = 0;
        uint32 icon = 0;
        int32 level = 0;
        uint32 flags = 0;
        uint32 flags2 = 0;
        bool repeatable = false;
        std::string title;
    };

    struct Message
    {
        ObjectGuid sourceGuid;
        uint32 menuId = 0;
        uint32 titleTextId = 0;
        std::vector<GossipItem> gossipItems;
        std::vector<QuestItem> quests;
    };

    inline ObjectGuid ReadHello(WorldPacket& in)
    {
        ObjectGuid guid;
        in.ReadGuidMask<2, 4, 0, 3, 6, 7, 5, 1>(guid);
        in.ReadGuidBytes<4, 7, 1, 0, 5, 3, 6, 2>(guid);
        return guid;
    }

    inline size_t BoundedLength(std::string const& value, size_t maximum)
    {
        return value.size() < maximum ? value.size() : maximum;
    }

    inline void AppendString(WorldPacket& out, std::string const& value,
        size_t length)
    {
        out.append(value.data(), length);
    }

    inline void BuildMessage(WorldPacket& out, Message const& message)
    {
        size_t const questCount = message.quests.size() < 0x7FFFF
            ? message.quests.size() : 0x7FFFF;
        size_t const gossipCount = message.gossipItems.size() < 0xFFFFF
            ? message.gossipItems.size() : 0xFFFFF;

        out.Initialize(SMSG_GOSSIP_MESSAGE, 150);
        out.WriteBits(uint32(questCount), 19);
        for (size_t i = 0; i < questCount; ++i)
        {
            QuestItem const& quest = message.quests[i];
            out.WriteBit(quest.repeatable);
            out.WriteBits(uint32(BoundedLength(quest.title, 0x1FF)), 9);
        }

        out.WriteGuidMask<5, 7, 4, 0>(message.sourceGuid);
        out.WriteBits(uint32(gossipCount), 20);
        out.WriteGuidMask<6, 2>(message.sourceGuid);
        for (size_t i = 0; i < gossipCount; ++i)
        {
            GossipItem const& item = message.gossipItems[i];
            out.WriteBits(uint32(BoundedLength(item.boxMessage, 0xFFF)), 12);
            out.WriteBits(uint32(BoundedLength(item.message, 0xFFF)), 12);
        }
        out.WriteGuidMask<3, 1>(message.sourceGuid);
        out.FlushBits();

        for (size_t i = 0; i < questCount; ++i)
        {
            QuestItem const& quest = message.quests[i];
            AppendString(out, quest.title, BoundedLength(quest.title, 0x1FF));
            out << quest.flags;
            out << quest.level;
            out << quest.icon;
            out << quest.questId;
            out << quest.flags2;
        }

        out.WriteGuidBytes<1, 0>(message.sourceGuid);
        for (size_t i = 0; i < gossipCount; ++i)
        {
            GossipItem const& item = message.gossipItems[i];
            out << item.boxMoney;
            AppendString(out, item.boxMessage, BoundedLength(item.boxMessage, 0xFFF));
            out << item.optionId;
            out << uint8(item.coded);
            AppendString(out, item.message, BoundedLength(item.message, 0xFFF));
            out << item.icon;
        }

        out.WriteGuidBytes<5, 3>(message.sourceGuid);
        out << message.menuId;
        out.WriteGuidBytes<2, 6, 4>(message.sourceGuid);
        out << uint32(0); // friendship faction ID
        out.WriteGuidBytes<7>(message.sourceGuid);
        out << message.titleTextId;
    }
}

#define GOSSIP_MAX_MENU_ITEMS       32                      // client supports showing max 32 items
#define DEFAULT_GOSSIP_MESSAGE      0xffffff

/**
 * Enum representing different gossip options available in the game.
 */
enum Gossip_Option
{
    GOSSIP_OPTION_NONE              = 0,                    // UNIT_NPC_FLAG_NONE                (0)
    GOSSIP_OPTION_GOSSIP            = 1,                    // UNIT_NPC_FLAG_GOSSIP              (1)
    GOSSIP_OPTION_QUESTGIVER        = 2,                    // UNIT_NPC_FLAG_QUESTGIVER          (2)
    GOSSIP_OPTION_VENDOR            = 3,                    // UNIT_NPC_FLAG_VENDOR              (128)
    GOSSIP_OPTION_TAXIVENDOR        = 4,                    // UNIT_NPC_FLAG_TAXIVENDOR          (8192)
    GOSSIP_OPTION_TRAINER           = 5,                    // UNIT_NPC_FLAG_TRAINER             (16)
    GOSSIP_OPTION_SPIRITHEALER      = 6,                    // UNIT_NPC_FLAG_SPIRITHEALER        (16384)
    GOSSIP_OPTION_SPIRITGUIDE       = 7,                    // UNIT_NPC_FLAG_SPIRITGUIDE         (32768)
    GOSSIP_OPTION_INNKEEPER         = 8,                    // UNIT_NPC_FLAG_INNKEEPER           (65536)
    GOSSIP_OPTION_BANKER            = 9,                    // UNIT_NPC_FLAG_BANKER              (131072)
    GOSSIP_OPTION_PETITIONER        = 10,                   // UNIT_NPC_FLAG_PETITIONER          (262144)
    GOSSIP_OPTION_TABARDDESIGNER    = 11,                   // UNIT_NPC_FLAG_TABARDDESIGNER      (524288)
    GOSSIP_OPTION_BATTLEFIELD       = 12,                   // UNIT_NPC_FLAG_BATTLEFIELDPERSON   (1048576)
    GOSSIP_OPTION_AUCTIONEER        = 13,                   // UNIT_NPC_FLAG_AUCTIONEER          (2097152)
    GOSSIP_OPTION_STABLEPET         = 14,                   // UNIT_NPC_FLAG_STABLE              (4194304)
    GOSSIP_OPTION_ARMORER           = 15,                   // UNIT_NPC_FLAG_ARMORER             (4096)
    GOSSIP_OPTION_UNLEARNTALENTS    = 16,                   // UNIT_NPC_FLAG_TRAINER             (16) (bonus option for GOSSIP_OPTION_TRAINER)
    GOSSIP_OPTION_UNLEARNPETSKILLS  = 17,                   // legacy DB value; hidden because 5.x removed the client exchange
    GOSSIP_OPTION_MAILBOX           = 18,                   // UNIT_NPC_FLAG_GOSSIP             (1)
    GOSSIP_OPTION_MAX
};

/**
 * Enum representing different icons used in gossip options.
 */
enum GossipOptionIcon
{
    GOSSIP_ICON_CHAT                = 0,                    // White chat bubble
    GOSSIP_ICON_VENDOR              = 1,                    // Brown bag
    GOSSIP_ICON_TAXI                = 2,                    // Flight
    GOSSIP_ICON_TRAINER             = 3,                    // Book
    GOSSIP_ICON_INTERACT_1          = 4,                    // Interaction wheel
    GOSSIP_ICON_INTERACT_2          = 5,                    // Interaction wheel
    GOSSIP_ICON_MONEY_BAG           = 6,                    // Brown bag with yellow dot
    GOSSIP_ICON_TALK                = 7,                    // White chat bubble with black dots
    GOSSIP_ICON_TABARD              = 8,                    // Tabard
    GOSSIP_ICON_BATTLE              = 9,                    // Two swords
    GOSSIP_ICON_DOT                 = 10,                   // Yellow dot
    GOSSIP_ICON_CHAT_11             = 11,                   // Similar to GOSSIP_ICON_CHAT
    GOSSIP_ICON_CHAT_12             = 12,                   // Similar to GOSSIP_ICON_CHAT
    GOSSIP_ICON_CHAT_13             = 13,                   // Yellow dot
    GOSSIP_ICON_CHAT_14             = 14,                   // Probably invalid
    GOSSIP_ICON_CHAT_15             = 15,                   // Probably invalid
    GOSSIP_ICON_CHAT_16             = 16,                   // Yellow dot
    GOSSIP_ICON_CHAT_17             = 17,                   // Yellow dot
    GOSSIP_ICON_CHAT_18             = 18,                   // Yellow dot
    GOSSIP_ICON_CHAT_19             = 19,                   // Yellow dot
    GOSSIP_ICON_CHAT_20             = 20,                   // Yellow dot
    GOSSIP_ICON_MAX
};

/**
 * Enum representing different Point of Interest (POI) icons.
 */
enum Poi_Icon
{
    ICON_POI_BLANK              =   0,                      // Blank (not visible), in 2.4.3 have value 15 with 1..15 values in 0..14 range
    ICON_POI_GREY_AV_MINE       =   1,                      // Grey mine lorry
    ICON_POI_RED_AV_MINE        =   2,                      // Red mine lorry
    ICON_POI_BLUE_AV_MINE       =   3,                      // Blue mine lorry
    ICON_POI_BWTOMB             =   4,                      // Blue and White Tomb Stone
    ICON_POI_SMALL_HOUSE        =   5,                      // Small house
    ICON_POI_GREYTOWER          =   6,                      // Grey Tower
    ICON_POI_REDFLAG            =   7,                      // Red Flag w/Yellow !
    ICON_POI_TOMBSTONE          =   8,                      // Normal tomb stone (brown)
    ICON_POI_BWTOWER            =   9,                      // Blue and White Tower
    ICON_POI_REDTOWER           =   10,                     // Red Tower
    ICON_POI_BLUETOWER          =   11,                     // Blue Tower
    ICON_POI_RWTOWER            =   12,                     // Red and White Tower
    ICON_POI_REDTOMB            =   13,                     // Red Tomb Stone
    ICON_POI_RWTOMB             =   14,                     // Red and White Tomb Stone
    ICON_POI_BLUETOMB           =   15,                     // Blue Tomb Stone
    ICON_POI_16                 =   16,                     // Grey ?
    ICON_POI_17                 =   17,                     // Blue/White ?
    ICON_POI_18                 =   18,                     // Blue ?
    ICON_POI_19                 =   19,                     // Red and White ?
    ICON_POI_20                 =   20,                     // Red ?
    ICON_POI_GREYLOGS           =   21,                     // Grey Wood Logs
    ICON_POI_BWLOGS             =   22,                     // Blue and White Wood Logs
    ICON_POI_BLUELOGS           =   23,                     // Blue Wood Logs
    ICON_POI_RWLOGS             =   24,                     // Red and White Wood Logs
    ICON_POI_REDLOGS            =   25,                     // Red Wood Logs
    ICON_POI_26                 =   26,                     // Grey ?
    ICON_POI_27                 =   27,                     // Blue and White ?
    ICON_POI_28                 =   28,                     // Blue ?
    ICON_POI_29                 =   29,                     // Red and White ?
    ICON_POI_30                 =   30,                     // Red ?
    ICON_POI_GREYHOUSE          =   31,                     // Grey House
    ICON_POI_BWHOUSE            =   32,                     // Blue and White House
    ICON_POI_BLUEHOUSE          =   33,                     // Blue House
    ICON_POI_RWHOUSE            =   34,                     // Red and White House
    ICON_POI_REDHOUSE           =   35,                     // Red House
    ICON_POI_GREYHORSE          =   36,                     // Grey Horse
    ICON_POI_BWHORSE            =   37,                     // Blue and White Horse
    ICON_POI_BLUEHORSE          =   38,                     // Blue Horse
    ICON_POI_RWHORSE            =   39,                     // Red and White Horse
    ICON_POI_REDHORSE           =   40                      // Red Horse
};

/**
 * Structure representing a gossip menu item.
 */
struct GossipMenuItem
{
    uint8       m_gIcon;            // Icon for the gossip menu item
    bool        m_gCoded;           // Whether the gossip menu item is coded
    std::string m_gMessage;         // Message for the gossip menu item
    uint32      m_gSender;          // Sender ID for the gossip menu item
    uint32      m_gOptionId;        // Option ID for the gossip menu item
    std::string m_gBoxMessage;      // Box message for the gossip menu item
    uint32      m_gBoxMoney;        // Box money for the gossip menu item
};

typedef std::vector<GossipMenuItem> GossipMenuItemList;

/**
 * Structure representing data for a gossip menu item.
 */
struct GossipMenuItemData
{
    int32  m_gAction_menu;          // Action menu ID (negative for close gossip)
    uint32 m_gAction_poi;           // Action POI ID
    uint32 m_gAction_script;        // Action script ID
};

typedef std::vector<GossipMenuItemData> GossipMenuItemDataList;

/**
 * Structure representing a quest menu item.
 */
struct QuestMenuItem
{
    uint32      m_qId;              // Quest ID
    uint8       m_qIcon;            // Icon for the quest menu item
};

typedef std::vector<QuestMenuItem> QuestMenuItemList;

/**
 * Class representing a gossip menu.
 */
class GossipMenu
{
    public:
        explicit GossipMenu(WorldSession* session);
        ~GossipMenu();

        void AddMenuItem(uint8 Icon, const std::string& Message, bool Coded = false);
        void AddMenuItem(uint8 Icon, const std::string& Message, uint32 dtSender, uint32 dtAction, const std::string& BoxMessage, uint32 BoxMoney, bool Coded = false);

        // for using from scripts, don't must be inlined
        void AddMenuItem(uint8 Icon, char const* Message, bool Coded = false);
        void AddMenuItem(uint8 Icon, char const* Message, uint32 dtSender, uint32 dtAction, char const* BoxMessage, uint32 BoxMoney, bool Coded = false);

        void AddMenuItem(uint8 Icon, int32 itemText, uint32 dtSender, uint32 dtAction, int32 boxText, uint32 BoxMoney, bool Coded = false);

        void SetMenuId(uint32 menu_id) { m_gMenuId = menu_id; }
        uint32 GetMenuId() const { return m_gMenuId; }

        void AddMenuItemData(int32 action_menu, uint32 action_poi, uint32 action_script);

        unsigned int MenuItemCount() const
        {
            return static_cast<unsigned int>(m_gItems.size());
        }

        unsigned int MenuItemDataCount() const
        {
            return static_cast<unsigned int>(m_gItemsData.size());
        }

        bool Empty() const
        {
            return m_gItems.empty();
        }

        GossipMenuItem const& GetItem(unsigned int Id) const
        {
            return m_gItems.at(Id);
        }

        GossipMenuItemData const* GetItemData(unsigned int indexId) const
        {
            if (indexId >= m_gItemsData.size())
            {
                sLog.outError("GossipMenu::GetItemData> indexId is out of bounds!");
                return nullptr;
            }
            return &m_gItemsData.at(indexId);
        }

        uint32 MenuItemSender(unsigned int ItemId) const;
        uint32 MenuItemAction(unsigned int ItemId) const;
        bool MenuItemCoded(unsigned int ItemId) const;

        void ClearMenu();

        WorldSession* GetMenuSession() const { return m_session; }

    protected:
        GossipMenuItemList      m_gItems;           // List of gossip menu items
        GossipMenuItemDataList  m_gItemsData;       // List of gossip menu item data

        uint32 m_gMenuId;                           // Gossip menu ID

    private:
        WorldSession* m_session;                    // Session associated with the gossip menu
};

/**
 * Class representing a quest menu.
 */
class QuestMenu
{
    public:
        QuestMenu();
        ~QuestMenu();

        void AddMenuItem(uint32 QuestId, uint8 Icon);
        void ClearMenu();

        uint8 MenuItemCount() const
        {
            return static_cast<uint8>(m_qItems.size());
        }

        bool Empty() const
        {
            return m_qItems.empty();
        }

        bool HasItem(uint32 questid) const;

        QuestMenuItem const& GetItem(uint16 Id) const
        {
            return m_qItems.at(Id);
        }

    protected:
        QuestMenuItemList m_qItems;                 // List of quest menu items
};

/**
 * Class representing a player menu, which includes both gossip and quest menus.
 */
class PlayerMenu
{
    private:
        GossipMenu mGossipMenu;                     // Gossip menu
        QuestMenu  mQuestMenu;                      // Quest menu

    public:
        explicit PlayerMenu(WorldSession* Session);
        ~PlayerMenu();

        GossipMenu& GetGossipMenu() { return mGossipMenu; }
        QuestMenu& GetQuestMenu() { return mQuestMenu; }

        WorldSession* GetMenuSession() const { return mGossipMenu.GetMenuSession(); }

        bool Empty() const { return mGossipMenu.Empty() && mQuestMenu.Empty(); }

        void ClearMenus();
        uint32 GossipOptionSender(unsigned int Selection) const;
        uint32 GossipOptionAction(unsigned int Selection) const;
        bool GossipOptionCoded(unsigned int Selection) const;

        void SendGossipMenu(uint32 titleTextId, ObjectGuid objectGuid);
        void CloseGossip() const;
        void SendPointOfInterest(float X, float Y, uint32 Icon, uint32 Flags, uint32 Data, const char* locName) const;
        void SendPointOfInterest(uint32 poi_id) const;
        void SendTalking(uint32 textID) const;

        /*********************************************************/
        /***                    QUEST SYSTEM                   ***/
        /*********************************************************/
        void SendQuestGiverStatus(uint32 questStatus, ObjectGuid npcGUID);

        void SendQuestGiverQuestList(QEmote eEmote, const std::string& Title, ObjectGuid npcGUID);

        void SendQuestQueryResponse(uint32 questId,
            Quest const* pQuest) const;
        void SendQuestGiverQuestDetails(Quest const* pQuest, ObjectGuid npcGUID, bool ActivateAccept) const;

        void SendQuestGiverOfferReward(Quest const* pQuest, ObjectGuid npcGUID, bool EnbleNext) const;
        void SendQuestGiverRequestItems(Quest const* pQuest,
            ObjectGuid npcGUID, bool Completable, bool AutoLaunch) const;
};
#endif
