/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
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

/*
 * MaNGOS Four — MoP 5.4.8.18414 object-update protocol primitives.
 * See MopUpdateObject.h. Layout transcribed (MaNGOS idiom, no code lift) from the
 * confirmed 18414 movement/values structure.
 */

#include "MopUpdateObject.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include <cstring>
#include <vector>

namespace
{
    constexpr uint32 MopUnitFlagServerControlled = 0x00000001u;

    inline uint8 GuidByte(uint64 g, int i) { return uint8(g >> (i * 8)); }

    inline uint32 FloatBits(float f) { uint32 u; std::memcpy(&u, &f, 4); return u; }


    // Classic pack-guid (mask byte + present bytes), as used in the CREATE preamble.
    void AppendPackedGuid(ByteBuffer& out, uint64 guid)
    {
        uint8 mask = 0;
        uint8 bytes[8];
        int n = 0;
        for (int i = 0; i < 8; ++i)
        {
            uint8 b = GuidByte(guid, i);
            if (b)
            {
                mask |= uint8(1 << i);
                bytes[n++] = b;
            }
        }
        out << mask;
        if (n)
        {
            out.append(bytes, n);
        }
    }
}

uint16 MopUpdateObject::TranslateSelfInventoryIndex(uint16 legacyIndex)
{
    MANGOS_ASSERT(legacyIndex >= SelfInventorySourceStart &&
        legacyIndex < SelfInventorySourceStart + SelfInventoryFieldCount);
    return uint16(legacyIndex + 5);
}

uint16 MopUpdateObject::TranslateSelfQuestLogIndex(uint16 legacyIndex)
{
    MANGOS_ASSERT(legacyIndex >= SelfQuestLogSourceStart &&
        legacyIndex < SelfQuestLogSourceStart + SelfQuestLogFieldCount);
    const uint16 offset = uint16(legacyIndex - SelfQuestLogSourceStart);
    const uint16 slot = uint16(offset / SelfQuestLogSourceStride);
    const uint16 fieldInSlot = uint16(offset % SelfQuestLogSourceStride);
    return uint16(SelfQuestLogTargetStart + slot * SelfQuestLogTargetStride + fieldInSlot);
}

bool MopUpdateObject::TranslateObserverPlayerIndex(uint16 legacyIndex, uint16& targetIndex)
{
    if (legacyIndex >= ObserverVisibleItemSourceStart &&
        legacyIndex < ObserverVisibleItemSourceStart + ObserverVisibleItemFieldCount)
    {
        targetIndex = uint16(ObserverVisibleItemTargetStart +
            (legacyIndex - ObserverVisibleItemSourceStart));
        return true;
    }

    // Powers and max powers, which the creature create and the player's own
    // self create have always carried but the observer create did not. Both
    // shift by five, exactly as TranslateSelfPlayerFields does.
    if (legacyIndex >= 29 && legacyIndex <= 33)
    {
        targetIndex = uint16(legacyIndex + 5);          // POWER1..5 -> 34..38
        return true;
    }
    if (legacyIndex >= 35 && legacyIndex <= 39)
    {
        targetIndex = uint16(legacyIndex + 5);          // MAXPOWER1..5 -> 40..44
        return true;
    }

    switch (legacyIndex)
    {
        case 7:  targetIndex = 7;  return true; // scale
        // The current target, as a two-word GUID. Client indices 22/23 are
        // CGUnitData::target in the 18414 descriptor table. Nothing projected
        // this before, for players OR creatures, so no client ever learned
        // what any other unit was targeting - which is why target-of-target
        // was blank and a player could not see who was targeting them.
        // SetUInt64Value marks both words changed, so the incremental path
        // emits the pair together.
        case 20: targetIndex = 22; return true; // target guid, low
        case 21: targetIndex = 23; return true; // target guid, high
        case 26: targetIndex = 30; return true; // packed race/class/gender/power
        case 56: targetIndex = 62; return true; // flags2
        case 61: targetIndex = 67; return true; // bounding radius
        case 62: targetIndex = 68; return true; // combat reach
        case 28: targetIndex = 33; return true; // health
        case 34: targetIndex = 39; return true; // max health
        case 50: targetIndex = 55; return true; // level
        case 51: targetIndex = 57; return true; // faction template
        case 52: targetIndex = 58; return true; // virtual item 1
        case 53: targetIndex = 59; return true; // virtual item 2
        case 54: targetIndex = 60; return true; // virtual item 3
        // Unit flags. 61 is CGUnitData::flags in the client's own descriptor
        // table, and a player object is CGUnitData-derived, so the client
        // reads it at the same index for a player as for a creature - which
        // the unit projection at ObjectUpdate.cpp already relies on. Without
        // this an observer never learns another player entered or left
        // combat, was stunned, feared or silenced: nothing else carries
        // those bits, and the observer create block omitted them too.
        // Callers MUST pass the value through ProjectPlayerUnitFlags so a
        // GM's internal bit 0 does not leak to other players.
        case 55: targetIndex = 61; return true; // unit flags
        case 63: targetIndex = 69; return true; // display ID
        case 64: targetIndex = 70; return true; // native display ID
        case 65: targetIndex = 71; return true; // mount display ID
        // The packed UNIT_FIELD_BYTES_1. Client index 76 is
        // CGUnitData::animTier in the 18414 descriptor table, and the client
        // names each packed word after its byte 3 - byte 0 of the same word is
        // the STAND STATE. The unit projection has always sent this for
        // creatures; no player path ever did, so an observer never learned
        // another player's stand state or animation tier. Sitting, kneeling
        // and looping state emotes all failed to render on a watcher as a
        // result, even once the emote state itself was being delivered.
        case 70: targetIndex = 76; return true; // bytes1: stand state, anim tier
        // Emote state, so an observer sees a state emote end. Without this a
        // watcher keeps rendering the dance after the dancer has walked away.
        case 83: targetIndex = 89; return true; // emote state
        // Packed appearance words, all PUBLIC in the legacy layout and all
        // previously dropped here. Facial hair and gender are visible to
        // other players, not just to the owner. See the self projection for
        // where these indices come from.
        case 161: targetIndex = 166; return true; // skin/face/hair/hair colour
        case 162: targetIndex = 167; return true; // facial hair, rest state
        case 163: targetIndex = 168; return true; // gender, drunk, arena faction
        default: return false;
    }
}

uint32 MopUpdateObject::RepackUnitBytes0(uint32 legacyBytes0)
{
    return (legacyBytes0 & 0x0000FFFFu) |
        ((legacyBytes0 & 0xFF000000u) >> 8) |
        ((legacyBytes0 & 0x00FF0000u) << 8);
}

uint32 MopUpdateObject::ProjectPlayerUnitFlags(uint32 legacyFlags)
{
    // Four uses bit 0 internally for GM mode. The 18414 client treats it
    // as server-controlled and refuses to attach that player to an
    // MO_TRANSPORT, so keep the server state but omit the client flag.
    return legacyFlags & ~MopUnitFlagServerControlled;
}

uint32 MopUpdateObject::TranslateUnitDynamicFlags(uint32 legacyFlags)
{
    return (legacyFlags & 0x000000FFu) << 1;
}

uint32 MopUpdateObject::TranslateUnitDynamicFlagsForViewer(uint32 legacyFlags,
    UnitDynamicFlagView const& view)
{
    constexpr uint32 LegacyLootable = 0x00000001u;
    constexpr uint32 LegacyTapped = 0x00000004u;
    constexpr uint32 LegacyTappedByPlayer = 0x00000008u;

    // Tap ownership is server state, but TAPPED_BY_PLAYER is observer-relative
    // on 18414. Rebuild both bits for this viewer instead of leaking the
    // recipient's private cue to every client.
    uint32 projectedFlags = legacyFlags & ~(LegacyTapped | LegacyTappedByPlayer);
    if (view.hasLootRecipient)
    {
        projectedFlags |= LegacyTapped;
        if (view.tappedByViewer)
        {
            projectedFlags |= LegacyTappedByPlayer;
        }
    }

    if (!view.allowedToLoot)
    {
        projectedFlags &= ~LegacyLootable;
    }

    return TranslateUnitDynamicFlags(projectedFlags);
}

uint32 MopUpdateObject::TranslateGameObjectDynamic(uint32 legacyDynamic)
{
    return (legacyDynamic & 0xFFFF0000u) | ((legacyDynamic & 0x0000000Fu) << 1);
}

bool MopUpdateObject::CanUseSimpleUnitMovement(SimpleUnitEligibility const& eligibility)
{
    // Only reject states the encoder cannot represent. AppendSimpleLivingMovement
    // emits a state-invariant layout: it declares every optional block absent
    // (attacking target 0, spline 0, fall data 0, movement flags omitted, extra
    // movement flags omitted, forces 0) and then writes position plus all nine
    // speeds unconditionally. So a moving or fighting unit encodes to exactly the
    // same structure as an idle one - the client is simply told "stationary
    // snapshot here", and the SMSG_MONSTER_MOVE stream animates it from there.
    //
    // Rejecting those states instead made Object::BuildCreateUpdateBlockForPlayer
    // return without emitting anything, so any creature that was moving or in
    // combat when it entered view was never created client-side and stayed
    // invisible while still dealing damage.
    //
    // Vehicle boarding is still rejected because it requires TransportInfo's
    // vehicle create state. A legacy MO_TRANSPORT parent is representable by
    // this encoder's optional unit-transport block and is therefore accepted.
    //
    // Being a vehicle is not such a state. IsVehicle() is m_vehicleInfo != NULL,
    // so it means the unit CAN carry passengers, not that it is riding anything;
    // its own position is an ordinary world position. The transport-relative
    // case is the passenger, already covered by isBoarded. Rejecting vehicles
    // here hid every vehicle-flagged creature in the world -- 1,622 templates
    // over 6,529 spawns across 48 maps -- including quest givers such as Master
    // Shang Xi (53566), whose client never learned he existed and so never even
    // queried his entry.
    //
    // AppendSimpleLivingMovement declares the vehicle block absent, so the
    // create stays well formed and the unit renders as an ordinary creature. The
    // cost is that it is not yet rideable, which needs a real vehicle create
    // block; being visible but not rideable beats being invisible.
    return !eligibility.isBoarded;
}

bool MopUpdateObject::CanUseStationaryGameObjectMovement(StationaryGameObjectEligibility const& eligibility)
{
    // Reject only what the encoder cannot represent, exactly as the unit gate
    // does. AppendStationaryGameObjectMovement writes a state-invariant layout:
    // stationary position plus rotation, every optional block declared absent.
    // Nothing in it varies with the object's TYPE.
    //
    // Being a destructible building was rejected here on type alone, and a
    // destructible building's movement is identical to any other stationary
    // gameobject's - it carries UPDATEFLAG_HAS_POSITION | UPDATEFLAG_ROTATION
    // like the rest. Its damage state lives in the values block, not the
    // movement block. Rejecting it meant no create block was emitted at all,
    // so the client was never told the object existed and never even sent
    // CMSG_GAMEOBJECT_QUERY for it: 147 spawns invisible, 88 of them
    // open-world scenery - harbour ships on map 0, Gooblin Boats on map 1,
    // Jade Forest ship cosmetics on 870, Forlorn Spires and a Moonwell on 861.
    //
    // Confirmed in game at -7259.48 4101.91 -1.73 on map 0, with a control
    // that rules out grid loading, phasing, range and the map together: an
    // ordinary Rope Ladder (203735) thirteen yards away rendered and was
    // queried, while Alliance Ship 000 (203400) was never queried and never
    // appeared. Players see it as NPCs standing in mid-air, because creatures
    // resting on these objects render correctly while their platform does not.
    //
    // Admitting these objects then exposed a second, older omission: the
    // projection stopped at target index 17 and never emitted
    // GAMEOBJECT_BYTES_1, so every gameobject read back as type 0. The
    // client's model resolver excludes the WMO-backed types 11, 14, 15 and 33
    // and sends everything else to the M2 cache, which rejects a WMO filename,
    // returns null, and is dereferenced unchecked. That is now fixed by
    // emitting index 18 in the same projection, which the incremental VALUES
    // path reuses, so the type arrives on both paths.
    // Type-15 transports use the same stationary-position base plus the
    // transport-time branch recovered from the 18414 client. Type-11 animated
    // transports remain gated upstream until their frame layout is supported.
    // Rendering with a default sub-state beats not rendering at all.
    return eligibility.hasTemplate && !eligibility.isBoarded &&
        eligibility.hasStationaryPosition && eligibility.hasRotation &&
        !eligibility.hasUnsupportedMovement;
}

bool MopUpdateObject::CanUsePositionOnlyMovement(PositionOnlyEligibility const& eligibility)
{
    return !eligibility.isBoarded && eligibility.hasPosition && !eligibility.hasUnsupportedMovement;
}

bool MopUpdateObject::CanUseInventoryObject(InventoryObjectEligibility const& eligibility)
{
    return eligibility.hasTarget && eligibility.hasOwner && eligibility.ownerMatchesTarget;
}

void MopUpdateObject::AppendStaticValuesNoDynamic(ByteBuffer& out, StaticField const* fields, uint32 fieldCount)
{
    MANGOS_ASSERT(fields || fieldCount == 0);

    uint32 masks[63] = { 0 };
    uint8 blockCount = 0;
    uint16 previousIndex = 0;

    for (uint32 i = 0; i < fieldCount; ++i)
    {
        MANGOS_ASSERT(fields[i].index < 63 * 32);
        MANGOS_ASSERT(i == 0 || fields[i].index > previousIndex);

        previousIndex = fields[i].index;
        masks[fields[i].index / 32] |= uint32(1) << (fields[i].index % 32);
        blockCount = std::max<uint8>(blockCount, uint8(fields[i].index / 32 + 1));
    }

    out << blockCount;
    for (uint8 i = 0; i < blockCount; ++i)
    {
        out << masks[i];
    }

    for (uint32 i = 0; i < fieldCount; ++i)
    {
        out << fields[i].value;
    }

    out << uint8(0);
}

void MopUpdateObject::AppendEmptyMovement(ByteBuffer& out)
{
    out.WriteBits(0, 42);
    out.FlushBits();
}

void MopUpdateObject::AppendEmptyMovementCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
    StaticField const* fields, uint32 fieldCount)
{
    out << updateType;
    AppendPackedGuid(out, guid);
    out << typeId;
    AppendEmptyMovement(out);
    AppendStaticValuesNoDynamic(out, fields, fieldCount);
}

void MopUpdateObject::AppendInventoryCreateBlock(ByteBuffer& out, uint64 guid, uint8 typeId,
    uint32 const* values, uint32 valueCount)
{
    MANGOS_ASSERT(values);
    MANGOS_ASSERT((typeId == 1 && valueCount == ItemFieldCount) ||
        (typeId == 2 && valueCount == ContainerFieldCount));

    std::vector<StaticField> fields;
    fields.reserve(valueCount);
    for (uint16 i = 0; i < valueCount; ++i)
    {
        fields.push_back({ i, values[i] });
    }

    AppendEmptyMovementCreateBlock(out, 1, guid, typeId, fields.data(), uint32(fields.size()));
}

void MopUpdateObject::AppendInventoryValuesBlock(ByteBuffer& out, uint64 guid, uint8 typeId,
    StaticField const* fields, uint32 fieldCount)
{
    MANGOS_ASSERT(fields || fieldCount == 0);
    MANGOS_ASSERT(typeId == 1 || typeId == 2);
    const uint16 valueCount = typeId == 2 ? ContainerFieldCount : ItemFieldCount;
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        MANGOS_ASSERT(fields[i].index < valueCount);
    }
    AppendValuesBlock(out, guid, fields, fieldCount);
}

void MopUpdateObject::AppendSelfInventoryValuesBlock(ByteBuffer& out, uint64 guid,
    StaticField const* sourceFields, uint32 fieldCount)
{
    MANGOS_ASSERT(sourceFields || fieldCount == 0);

    std::vector<StaticField> fields;
    fields.reserve(fieldCount);
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        MANGOS_ASSERT(i == 0 || sourceFields[i - 1].index < sourceFields[i].index);
        fields.push_back({ TranslateSelfInventoryIndex(sourceFields[i].index), sourceFields[i].value });
    }

    AppendValuesBlock(out, guid, fields.data(), uint32(fields.size()));
}

void MopUpdateObject::TranslateSelfPlayerFields(StaticField const* sourceFields,
    uint32 fieldCount, std::vector<StaticField>& out)
{
    MANGOS_ASSERT(sourceFields || fieldCount == 0);

    // Projected into a local and swapped in at the end, so a caller may pass
    // the storage it is reading from: TranslateSelfPlayerFields(v.data(),
    // v.size(), v) is well defined. Clearing `out` up front instead would end
    // the lifetime of those source elements before they were read.
    std::vector<StaticField> fields;
    fields.reserve(fieldCount + 1);
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        MANGOS_ASSERT(i == 0 || sourceFields[i - 1].index < sourceFields[i].index);
        const uint16 sourceIndex = sourceFields[i].index;
        const uint32 value = sourceFields[i].value;

        // QuestLogFrame reads each slot at a fifteen-word stride, so Four's
        // five-word slots have to be re-strided rather than shifted.
        //
        // 18414 writes EVERY word of a touched slot. Retail captures show the
        // mask bit set for all fifteen words, with words 5..14 carried as
        // explicit zeroes; leaving them unmasked never touches that client-side
        // storage. Emit the whole slot so the client's view of it is fully
        // defined. Zero values are significant throughout: a cleared quest id
        // is how the client is told a slot was abandoned.
        //
        // Callers supply whole slots (see the quest-log feeds in
        // ObjectUpdate.cpp and Map.cpp). Any word the caller omits is emitted
        // as zero, so a partial slot would clear the rest of it.
        if (sourceIndex >= SelfQuestLogSourceStart &&
            sourceIndex < SelfQuestLogSourceStart + SelfQuestLogFieldCount)
        {
            const uint16 slot = uint16((sourceIndex - SelfQuestLogSourceStart) /
                SelfQuestLogSourceStride);
            uint32 slotWords[SelfQuestLogSourceStride] = { 0 };
            uint32 next = i;
            for (; next < fieldCount; ++next)
            {
                const uint16 candidate = sourceFields[next].index;
                if (candidate < SelfQuestLogSourceStart ||
                    candidate >= SelfQuestLogSourceStart + SelfQuestLogFieldCount)
                {
                    break;
                }
                if (uint16((candidate - SelfQuestLogSourceStart) /
                    SelfQuestLogSourceStride) != slot)
                {
                    break;
                }
                slotWords[(candidate - SelfQuestLogSourceStart) %
                    SelfQuestLogSourceStride] = sourceFields[next].value;
            }

            const uint16 targetBase =
                uint16(SelfQuestLogTargetStart + slot * SelfQuestLogTargetStride);
            for (uint16 word = 0; word < SelfQuestLogTargetStride; ++word)
            {
                fields.push_back({ uint16(targetBase + word),
                    word < SelfQuestLogSourceStride ? slotWords[word] : 0u });
            }

            i = next - 1;   // the loop's ++i steps past the consumed slot
            continue;
        }

        if (sourceIndex >= ObserverVisibleItemSourceStart &&
            sourceIndex < ObserverVisibleItemSourceStart + ObserverVisibleItemFieldCount)
        {
            uint16 targetIndex = 0;
            const bool translated = TranslateObserverPlayerIndex(sourceIndex, targetIndex);
            MANGOS_ASSERT(translated);
            fields.push_back({ targetIndex, value });
            continue;
        }

        if (sourceIndex >= SelfInventorySourceStart &&
            sourceIndex < SelfInventorySourceStart + SelfInventoryFieldCount)
        {
            fields.push_back({ TranslateSelfInventoryIndex(sourceIndex), value });
            continue;
        }
        // IDA 9.4 18414 CGPlayerData::local: coinage 1149-1150,
        // XP 1151, nextLevelXP 1152. Source indices are pinned by the caller.
        if (sourceIndex >= 1142 && sourceIndex <= 1145)
        {
            fields.push_back({ uint16(sourceIndex + 7), value });
            continue;
        }
        // 18414 CGPlayerData::local.exploredZones is 1627..1826 and
        // local.restStateBonusPool 1827; Four stores the same two ranges
        // contiguously at 1619..1818 and 1819. Both shift by eight. Without
        // this the rested-XP pool never reaches the client at all, and
        // MainMenuBar.lua compares a nil exhaustion threshold.
        if (sourceIndex >= SelfExploredSourceStart &&
            sourceIndex <= SelfExploredSourceEnd)
        {
            fields.push_back({ uint16(sourceIndex + SelfExploredTargetShift), value });
            continue;
        }
        // IDA 9.4 18414 CGPlayerData metadata places local.skill at
        // 1153..1600 (448 fields). Four stores the same seven parallel
        // 64-word arrays at 1146..1593.
        if (sourceIndex >= SelfSkillSourceStart &&
            sourceIndex < SelfSkillSourceStart + SelfSkillFieldCount)
        {
            fields.push_back({
                uint16(SelfSkillTargetStart + sourceIndex - SelfSkillSourceStart),
                value
            });
            continue;
        }
        // MerchantFrame.lua's GetNumBuybackItems() path ignores a logical
        // slot when its native buyback-price field is zero. IDA 9.4 places
        // the 18414 price/timestamp arrays at 1865..1888, eight fields after
        // Four's legacy storage. Preserve zero values so cleared slots also
        // reach the client.
        if (sourceIndex >= SelfBuybackSourceStart &&
            sourceIndex < SelfBuybackSourceStart + SelfBuybackFieldCount)
        {
            fields.push_back({
                uint16(SelfBuybackTargetStart + sourceIndex - SelfBuybackSourceStart),
                value
            });
            continue;
        }

        if (sourceIndex >= 29 && sourceIndex <= 33)
        {
            fields.push_back({ uint16(sourceIndex + 5), value });
            continue;
        }
        if (sourceIndex >= 35 && sourceIndex <= 39)
        {
            fields.push_back({ uint16(sourceIndex + 5), value });
            continue;
        }

        switch (sourceIndex)
        {
            case 7:  fields.push_back({ 7, value }); break;
            case 26:
                fields.push_back({ 30, RepackUnitBytes0(value) });
                fields.push_back({ 31, (value >> 24) & 0xFFu });
                break;
            case 28: fields.push_back({ 33, value }); break;
            case 34: fields.push_back({ 39, value }); break;
            case 50: fields.push_back({ 55, value }); break;
            case 51: fields.push_back({ 57, value }); break;
            case 55: fields.push_back({ 61, ProjectPlayerUnitFlags(value) }); break;
            case 61: fields.push_back({ 67, value }); break;
            case 62: fields.push_back({ 68, value }); break;
            case 63: fields.push_back({ 69, value }); break;
            case 64: fields.push_back({ 70, value }); break;
            case 65: fields.push_back({ 71, value }); break;
            // Emote state. BuildMopUnitStaticFields maps this to 89 for units,
            // but no PLAYER path carried it at all - neither the create block
            // nor either incremental path - so a player's state emote never
            // reached any client. /dance begins only because the text-emote
            // path also fires SMSG_EMOTE, and then never ends: every cancel
            // route, moving and talking alike, writes just this field, and
            // that write never left the server.
            case 83: fields.push_back({ 89, value }); break;
            // PLAYER_FLAGS, which carries PLAYER_FLAGS_GHOST. Without it the
            // client is never told the character died: no release dialog, so
            // no CMSG_REPOP_REQUEST, so release and .revive have nothing to
            // act on and the corpse outlives a state the client never entered.
            // The giveaway is a character sitting at 1 HP that will not
            // regenerate, because the server is correctly withholding regen
            // from a corpse while the client renders someone alive.
            //
            // 18414 index from the client's own field descriptors:
            // CGPlayerData's table is a 12-byte stride from dword_10F52B8, and
            // CGPlayerData::playerFlags writes at dword_10F52D0, so it sits at
            // relative index 2 -- the same relative position Four gives it.
            // CGPlayerData::questLog writes at dword_10F533C, relative index
            // 11, and its absolute 18414 index is already pinned at 171 by the
            // quest-log projection, which puts the block base at 160.
            case 157: fields.push_back({ 162, value }); break;
            // The three packed PLAYER_BYTES words. None was projected at all,
            // in either path, despite Player.cpp:2933-2935 marking all three
            // as visual bits - so rest state, facial hair and gender never
            // reached any client.
            //
            // Indices read from the client's own CGPlayerData descriptor
            // table (12-byte stride from dword_10F52B8, names written by
            // sub_7A0B33), not interpolated: entry 6 "hairColorID", entry 7
            // "restState", entry 8 "arenaFaction", against a block base of
            // 160 pinned independently by entry 2 "playerFlags" at 162 and
            // entry 11 "questLog" at 171. The client names each packed word
            // after its byte 3, and all three names match the legacy byte-3
            // meaning - hair colour, rest state, arena faction - which
            // corroborates the mapping rather than merely fitting it.
            //
            // Byte 3 of 167 is what GetRestState() reads; without it the Lua
            // returns nil and MainMenuBar.lua fails on the comparison at :119
            // and the arithmetic at :202. Byte 0 of 168 is gender, which for
            // a PLAYER the client reads from here rather than from
            // UNIT_FIELD_SEX - the reason a female character used male voice
            // and emote sounds while rendering correctly.
            case 161: fields.push_back({ 166, value }); break;
            case 162: fields.push_back({ 167, value }); break;
            case 163: fields.push_back({ 168, value }); break;
            default:
                MANGOS_ASSERT(false && "unsupported legacy self-player field");
                break;
        }
    }

    out.swap(fields);
}

void MopUpdateObject::AppendSelfPlayerValuesBlock(ByteBuffer& out, uint64 guid,
    StaticField const* sourceFields, uint32 fieldCount)
{
    std::vector<StaticField> fields;
    TranslateSelfPlayerFields(sourceFields, fieldCount, fields);
    AppendValuesBlock(out, guid, fields.data(), uint32(fields.size()));
}

void MopUpdateObject::AppendPositionOnlyMovement(ByteBuffer& out, PositionOnlyMovement const& movement)
{
    out.WriteBit(0);                 // game-object data
    out.WriteBit(0);                 // animation kits
    out.WriteBit(0);                 // living
    out.WriteBit(0);                 // scene-local script
    out.WriteBit(0);
    out.WriteBits(0, 22);            // transport frame count
    out.WriteBit(0);                 // vehicle
    out.WriteBit(0);
    out.WriteBit(0);
    out.WriteBit(0);                 // transport time
    out.WriteBit(0);                 // rotation
    out.WriteBit(0);
    out.WriteBit(0);                 // self
    out.WriteBit(0);                 // attacking target
    out.WriteBit(0);                 // scene object
    out.WriteBit(0);                 // scene pending instances
    out.WriteBit(0);
    out.WriteBit(0);                 // area trigger
    out.WriteBit(0);                 // game-object transport position
    out.WriteBit(0);                 // replace you
    out.WriteBit(1);                 // stationary position follows
    out.FlushBits();

    out << movement.y << movement.z << movement.o << movement.x;
}

void MopUpdateObject::AppendPositionOnlyCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
    PositionOnlyMovement const& movement, uint32 const* values, uint32 valueCount)
{
    MANGOS_ASSERT(values);
    MANGOS_ASSERT((typeId == 6 && valueCount == DynamicObjectFieldCount) ||
        (typeId == 7 && valueCount == CorpseFieldCount));

    std::vector<StaticField> fields;
    fields.reserve(valueCount);
    for (uint16 i = 0; i < valueCount; ++i)
    {
        fields.push_back({ i, values[i] });
    }

    out << updateType;
    AppendPackedGuid(out, guid);
    out << typeId;
    AppendPositionOnlyMovement(out, movement);
    AppendStaticValuesNoDynamic(out, fields.data(), uint32(fields.size()));
}

void MopUpdateObject::AppendPositionOnlyValuesBlock(ByteBuffer& out, uint64 guid, uint8 typeId,
    StaticField const* fields, uint32 fieldCount)
{
    MANGOS_ASSERT(fields || fieldCount == 0);
    MANGOS_ASSERT(typeId == 6 || typeId == 7);
    const uint16 valueCount = typeId == 7 ? CorpseFieldCount : DynamicObjectFieldCount;
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        MANGOS_ASSERT(fields[i].index < valueCount);
    }
    AppendValuesBlock(out, guid, fields, fieldCount);
}

void MopUpdateObject::AppendStationaryGameObjectMovement(ByteBuffer& out, StationaryGameObjectMovement const& movement)
{
    out.WriteBit(0);                 // game-object data
    out.WriteBit(0);                 // animation kits
    out.WriteBit(0);                 // living
    out.WriteBit(0);                 // scene-local script
    out.WriteBit(0);
    out.WriteBits(0, 22);            // transport frame count
    out.WriteBit(0);                 // vehicle
    out.WriteBit(0);
    out.WriteBit(0);
    out.WriteBit(movement.isTransport); // transport time
    out.WriteBit(1);                 // packed world rotation follows
    out.WriteBit(0);
    out.WriteBit(0);                 // self
    out.WriteBit(0);                 // attacking target
    out.WriteBit(0);                 // scene object
    out.WriteBit(0);                 // scene pending instances
    out.WriteBit(0);
    out.WriteBit(0);                 // area trigger
    out.WriteBit(0);                 // game-object transport position
    out.WriteBit(0);                 // replace you
    out.WriteBit(1);                 // stationary position follows
    out.FlushBits();

    out << movement.y << movement.z << movement.o << movement.x;
    if (movement.isTransport)
    {
        out << movement.transportTime;
    }
    out << movement.rotation;
}

void MopUpdateObject::AppendStationaryGameObjectCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
    StationaryGameObjectMovement const& movement, StaticField const* fields, uint32 fieldCount)
{
    out << updateType;
    AppendPackedGuid(out, guid);
    out << typeId;
    AppendStationaryGameObjectMovement(out, movement);
    AppendStaticValuesNoDynamic(out, fields, fieldCount);
}

namespace
{
    /// Smallest speed magnitude the 18414 create validator accepts.
    ///
    /// sub_768D2F tests all nine speeds with an approximately-equal-to-zero
    /// comparison (sub_45B733 -> sub_409DD6 at epsilon 0.00000023841858 = 2^-22)
    /// and rejects the object when any of them is nearer to zero than that. A
    /// rejected create returns 0 from sub_79DC30, which makes the block loop in
    /// sub_79E087 BREAK: the object is lost and so is every later block in the
    /// same packet, silently and with no reply.
    ///
    /// The floor sits an order of magnitude clear of the bound so no rounding can
    /// walk back across it. Speeds are conceptually non-negative, so this floors
    /// rather than preserving sign -- creature_template ships negative denormals
    /// which are meaningless as speeds and would be rejected on magnitude anyway.
    float SanitizeSpeed(float speed)
    {
        float const minWireSpeed = 0.000001f;
        return speed < minWireSpeed ? minWireSpeed : speed;
    }
}

void MopUpdateObject::AppendSimpleLivingMovement(ByteBuffer& out, SimpleLivingMovement const& movement)
{
    const uint64 g = movement.guid;
    const uint64 transportGuid = movement.transportGuid;
    const bool hasTransport = transportGuid != 0;

    // Sanitised HERE rather than at the callers, because there is more than one
    // caller and a bypass is silent. ObjectUpdate.cpp builds the observer create
    // and Map::SendInitSelf builds the player's own create; both funnel through
    // this writer, and the self path was missed when the clamp lived at the
    // observer call site. A rejected SELF create is worse than a rejected
    // observer create -- the player does not exist on their own client at all.
    float const speedWalk = SanitizeSpeed(movement.speedWalk);
    float const speedRun = SanitizeSpeed(movement.speedRun);
    float const speedRunBack = SanitizeSpeed(movement.speedRunBack);
    float const speedSwim = SanitizeSpeed(movement.speedSwim);
    float const speedSwimBack = SanitizeSpeed(movement.speedSwimBack);
    float const speedFlight = SanitizeSpeed(movement.speedFlight);
    float const speedFlightBack = SanitizeSpeed(movement.speedFlightBack);
    float const speedTurn = SanitizeSpeed(movement.speedTurn);
    float const speedPitch = SanitizeSpeed(movement.speedPitch);

    out.WriteBit(0);                     // game-object data
    out.WriteBit(0);                     // animation kits
    out.WriteBit(1);                     // living
    out.WriteBit(0);                     // scene-local script
    out.WriteBit(0);
    out.WriteBits(0, 22);                // transport frame count
    out.WriteBit(0);                     // vehicle
    out.WriteBit(0);
    out.WriteBit(0);
    out.WriteBit(0);                     // transport time
    out.WriteBit(0);                     // rotation
    out.WriteBit(0);
    out.WriteBit(movement.self);         // self
    out.WriteBit(0);                     // attacking target
    out.WriteBit(0);                     // scene object
    out.WriteBit(0);                     // scene pending instances
    out.WriteBit(0);
    out.WriteBit(0);                     // area trigger
    out.WriteBit(0);                     // game-object transport position
    out.WriteBit(0);                     // replace you
    out.WriteBit(0);                     // living carries its own position

    out.WriteBit(GuidByte(g, 2) != 0);
    out.WriteBit(0);
    out.WriteBit(1);                     // pitch omitted
    out.WriteBit(hasTransport);          // unit transport
    out.WriteBit(0);
    if (hasTransport)
    {
        out.WriteBit(GuidByte(transportGuid, 4) != 0);
        out.WriteBit(GuidByte(transportGuid, 2) != 0);
        out.WriteBit(movement.hasTransportTime3);
        out.WriteBit(GuidByte(transportGuid, 0) != 0);
        out.WriteBit(GuidByte(transportGuid, 1) != 0);
        out.WriteBit(GuidByte(transportGuid, 3) != 0);
        out.WriteBit(GuidByte(transportGuid, 6) != 0);
        out.WriteBit(GuidByte(transportGuid, 7) != 0);
        out.WriteBit(movement.hasTransportTime2);
        out.WriteBit(GuidByte(transportGuid, 5) != 0);
    }
    out.WriteBit(0);                     // movement time present
    out.WriteBit(GuidByte(g, 6) != 0);
    out.WriteBit(GuidByte(g, 4) != 0);
    out.WriteBit(GuidByte(g, 3) != 0);
    out.WriteBit(0);                     // orientation present
    out.WriteBit(1);                     // movement counter omitted
    out.WriteBit(GuidByte(g, 5) != 0);
    out.WriteBits(0, 22);                // forces
    out.WriteBit(1);                     // movement flags omitted
    out.WriteBits(0, 19);
    out.WriteBit(0);                     // fall data
    out.WriteBit(1);                     // spline elevation omitted
    out.WriteBit(0);                     // spline
    out.WriteBit(0);
    out.WriteBit(GuidByte(g, 0) != 0);
    out.WriteBit(GuidByte(g, 7) != 0);
    out.WriteBit(GuidByte(g, 1) != 0);
    out.WriteBit(1);                     // extra movement flags omitted
    out.FlushBits();

    if (hasTransport)
    {
        out.WriteByteSeq(GuidByte(transportGuid, 7));
        out << movement.transportX;
        if (movement.hasTransportTime3)
        {
            out << movement.transportTime3;
        }
        out << movement.transportO;
        out << movement.transportY;
        out.WriteByteSeq(GuidByte(transportGuid, 4));
        out.WriteByteSeq(GuidByte(transportGuid, 1));
        out.WriteByteSeq(GuidByte(transportGuid, 3));
        out << movement.transportZ;
        out.WriteByteSeq(GuidByte(transportGuid, 5));
        if (movement.hasTransportTime2)
        {
            out << movement.transportTime2;
        }
        out.WriteByteSeq(GuidByte(transportGuid, 0));
        out << movement.transportSeat;
        out.WriteByteSeq(GuidByte(transportGuid, 6));
        out.WriteByteSeq(GuidByte(transportGuid, 2));
        out << movement.transportTime;
    }

    out.WriteByteSeq(GuidByte(g, 4));
    out << speedFlight;
    out.WriteByteSeq(GuidByte(g, 2));
    out.WriteByteSeq(GuidByte(g, 1));
    out << speedTurn;
    out << movement.moveTime;
    out << speedRunBack;
    out.WriteByteSeq(GuidByte(g, 7));
    out << speedPitch;
    out << movement.x;
    out << movement.o;
    out << speedWalk;
    out << movement.y;
    out << speedFlightBack;
    out.WriteByteSeq(GuidByte(g, 3));
    out.WriteByteSeq(GuidByte(g, 5));
    out.WriteByteSeq(GuidByte(g, 6));
    out.WriteByteSeq(GuidByte(g, 0));
    out << speedSwimBack;
    out << speedRun;
    out << speedSwim;
    out << movement.z;
}

void MopUpdateObject::AppendSimpleLivingCreateBlock(ByteBuffer& out, uint8 updateType, uint64 guid, uint8 typeId,
    SimpleLivingMovement const& movement, StaticField const* fields, uint32 fieldCount)
{
    MANGOS_ASSERT(movement.guid == guid);
    out << updateType;
    AppendPackedGuid(out, guid);
    out << typeId;
    AppendSimpleLivingMovement(out, movement);
    AppendStaticValuesNoDynamic(out, fields, fieldCount);
}

void MopUpdateObject::AppendValuesBlock(ByteBuffer& out, uint64 guid, StaticField const* fields, uint32 fieldCount)
{
    out << uint8(0);
    AppendPackedGuid(out, guid);
    AppendStaticValuesNoDynamic(out, fields, fieldCount);
}

void MopUpdateObject::BuildDestroyObject(WorldPacket& out, uint64 guid, bool animation)
{
    const uint8 bytes[8] =
    {
        GuidByte(guid, 0), GuidByte(guid, 1), GuidByte(guid, 2), GuidByte(guid, 3),
        GuidByte(guid, 4), GuidByte(guid, 5), GuidByte(guid, 6), GuidByte(guid, 7),
    };

    out.Initialize(SMSG_DESTROY_OBJECT, 10);
    out.WriteBit(bytes[3] != 0);
    out.WriteBit(bytes[2] != 0);
    out.WriteBit(bytes[4] != 0);
    out.WriteBit(bytes[1] != 0);
    out.WriteBit(animation);
    out.WriteBit(bytes[7] != 0);
    out.WriteBit(bytes[0] != 0);
    out.WriteBit(bytes[6] != 0);
    out.WriteBit(bytes[5] != 0);
    out.FlushBits();
    out.WriteByteSeq(bytes[0]);
    out.WriteByteSeq(bytes[4]);
    out.WriteByteSeq(bytes[7]);
    out.WriteByteSeq(bytes[2]);
    out.WriteByteSeq(bytes[6]);
    out.WriteByteSeq(bytes[3]);
    out.WriteByteSeq(bytes[1]);
    out.WriteByteSeq(bytes[5]);
}

void MopUpdateObject::AppendSelfCreateBlock(ByteBuffer& out, const SelfPlayer& e,
    StaticField const* extraFields, uint32 extraFieldCount)
{
    MANGOS_ASSERT(extraFields || extraFieldCount == 0);

    SimpleLivingMovement movement{};
    movement.guid = e.guid;
    movement.x = e.x;
    movement.y = e.y;
    movement.z = e.z;
    movement.o = e.o;
    movement.moveTime = e.moveTime;
    movement.speedWalk = e.speedWalk;
    movement.speedRun = e.speedRun;
    movement.speedRunBack = e.speedRunBack;
    movement.speedSwim = e.speedSwim;
    movement.speedSwimBack = e.speedSwimBack;
    movement.speedFlight = e.speedFlight;
    movement.speedFlightBack = e.speedFlightBack;
    movement.speedTurn = e.speedTurn;
    movement.speedPitch = e.speedPitch;
    movement.transportGuid = e.transportGuid;
    movement.transportX = e.transportX;
    movement.transportY = e.transportY;
    movement.transportZ = e.transportZ;
    movement.transportO = e.transportO;
    movement.transportTime = e.transportTime;
    movement.transportTime2 = e.transportTime2;
    movement.transportTime3 = e.transportTime3;
    movement.transportSeat = e.transportSeat;
    movement.hasTransportTime2 = e.hasTransportTime2;
    movement.hasTransportTime3 = e.hasTransportTime3;
    movement.self = true;
    // ---- values block (essential 18414 fields, ascending index order) ----
    // UNIT_FIELD_SEX (renamed BYTES_0): race, class, power type, gender.
    const uint32 sex = uint32(e.race) | (uint32(e.class_) << 8) |
        (uint32(e.powerType) << 16) | (uint32(e.gender) << 24);

    const StaticField fields[] =
    {
        {  0, uint32(e.guid & 0xFFFFFFFFu) },   // OBJECT_FIELD_GUID low
        {  1, uint32(e.guid >> 32) },           // OBJECT_FIELD_GUID high
        {  4, 25u },                            // OBJECT_FIELD_TYPE (OBJECT|UNIT|PLAYER)
        {  7, FloatBits(e.scale) },             // OBJECT_FIELD_SCALE
        { 30, sex },                            // UNIT_FIELD_SEX  (OBJECT_END+0x16)
        { 31, uint32(e.powerType) },            // UNIT_FIELD_DISPLAY_POWER (+0x17)
        { 33, e.health },                       // UNIT_FIELD_HEALTH (+0x19)
        { 34, e.power[0] },                     // UNIT_FIELD_POWER1 (+0x1A)
        { 35, e.power[1] },                     // UNIT_FIELD_POWER2 (+0x1B)
        { 36, e.power[2] },                     // UNIT_FIELD_POWER3 (+0x1C)
        { 37, e.power[3] },                     // UNIT_FIELD_POWER4 (+0x1D)
        { 38, e.power[4] },                     // UNIT_FIELD_POWER5 (+0x1E)
        { 39, e.maxHealth },                    // UNIT_FIELD_MAX_HEALTH (+0x1F)
        { 40, e.maxPower[0] },                  // UNIT_FIELD_MAXPOWER1 (+0x20)
        { 41, e.maxPower[1] },                  // UNIT_FIELD_MAXPOWER2 (+0x21)
        { 42, e.maxPower[2] },                  // UNIT_FIELD_MAXPOWER3 (+0x22)
        { 43, e.maxPower[3] },                  // UNIT_FIELD_MAXPOWER4 (+0x23)
        { 44, e.maxPower[4] },                  // UNIT_FIELD_MAXPOWER5 (+0x24)
        { 55, uint32(e.level) },                // UNIT_FIELD_LEVEL (+0x2F)
        { 57, e.faction },                      // UNIT_FIELD_FACTION_TEMPLATE (+0x31)
        { 61, ProjectPlayerUnitFlags(e.unitFlags) }, // UNIT_FIELD_FLAGS (+0x35)
        { 67, FloatBits(e.boundingRadius) },    // UNIT_FIELD_BOUNDING_RADIUS (+0x3B)
        { 68, FloatBits(e.combatReach) },       // UNIT_FIELD_COMBAT_REACH (+0x3C)
        { 69, e.displayId },                    // UNIT_FIELD_DISPLAY_ID (+0x3D)
        { 70, e.nativeDisplayId },              // UNIT_FIELD_NATIVE_DISPLAY_ID (+0x3E)
    };
    const uint32 coreCount = uint32(sizeof(fields) / sizeof(fields[0]));

    if (extraFieldCount == 0)
    {
        AppendSimpleLivingCreateBlock(out, 2, e.guid, 4, movement, fields, coreCount);
        return;
    }

    // The core block ends at UNIT_FIELD_NATIVE_DISPLAY_ID (70) and every
    // projected self-player field sits above it - the lowest is PLAYER_FLAGS at
    // 162 - so appending preserves the ascending order the values serializer
    // requires. Assert it rather than trust it: a caller that passed unsorted
    // or overlapping fields would otherwise trip the serializer's own assert
    // with no indication of which side was wrong.
    MANGOS_ASSERT(extraFields[0].index > fields[coreCount - 1].index);

    std::vector<StaticField> combined;
    combined.reserve(coreCount + extraFieldCount);
    combined.insert(combined.end(), fields, fields + coreCount);
    combined.insert(combined.end(), extraFields, extraFields + extraFieldCount);
    AppendSimpleLivingCreateBlock(out, 2, e.guid, 4, movement, combined.data(),
        uint32(combined.size()));
}

void MopUpdateObject::BuildSelfCreate(WorldPacket& out, const SelfPlayer& e)
{
    out.Initialize(SMSG_UPDATE_OBJECT);

    // ---- packet header ----
    out << uint16(e.mapId);
    out << uint32(1);                    // one update block, no out-of-range section

    AppendSelfCreateBlock(out, e, nullptr, 0);
}
