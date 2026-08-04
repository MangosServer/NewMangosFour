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

#include "DBCfmt.h"
#include "DB2fmt.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "Policies/Singleton.h"
#include "Log.h"
#include "ProgressBar.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "ObjectGuid.h"
#include "Util.h"

#include <map>
#include <set>

typedef std::map<uint16, uint32> AreaFlagByAreaID;
typedef std::map<uint32, uint32> AreaFlagByMapID;

struct WMOAreaTableTripple
{
    WMOAreaTableTripple(int32 r, int32 a, int32 g) : groupId(g), rootId(r), adtId(a)
    {
    }

    bool operator <(const WMOAreaTableTripple& b) const
    {
        return memcmp(this, &b, sizeof(WMOAreaTableTripple)) < 0;
    }

    // ordered by entropy; that way memcmp will have a minimal medium runtime
    int32 groupId;
    int32 rootId;
    int32 adtId;
};

typedef std::map<WMOAreaTableTripple, WMOAreaTableEntry const*> WMOAreaInfoByTripple;

DBCStorage <AreaTableEntry> sAreaStore(AreaTableEntryfmt);
DBCStorage <AreaGroupEntry> sAreaGroupStore(AreaGroupEntryfmt);
static AreaFlagByAreaID sAreaFlagByAreaID;
static AreaFlagByMapID  sAreaFlagByMapID;                   // for instances without generated *.map files

static WMOAreaInfoByTripple sWMOAreaInfoByTripple;

DBCStorage <AchievementEntry> sAchievementStore(Achievementfmt);
DBCStorage <AchievementCriteriaEntry> sAchievementCriteriaStore(AchievementCriteriafmt);
DBCStorage <AreaTriggerEntry> sAreaTriggerStore(AreaTriggerEntryfmt);
DBCStorage <ArmorLocationEntry> sArmorLocationStore(ArmorLocationfmt);
DBCStorage <AuctionHouseEntry> sAuctionHouseStore(AuctionHouseEntryfmt);
DBCStorage <BankBagSlotPricesEntry> sBankBagSlotPricesStore(BankBagSlotPricesEntryfmt);
DBCStorage <BattlemasterListEntry> sBattlemasterListStore(BattlemasterListEntryfmt);
DBCStorage <BarberShopStyleEntry> sBarberShopStyleStore(BarberShopStyleEntryfmt);
DBCStorage <CharStartOutfitEntry> sCharStartOutfitStore(CharStartOutfitEntryfmt);
DBCStorage <CharTitlesEntry> sCharTitlesStore(CharTitlesEntryfmt);
DBCStorage <ChatChannelsEntry> sChatChannelsStore(ChatChannelsEntryfmt);
DBCStorage <ChrClassesEntry> sChrClassesStore(ChrClassesEntryfmt);
DBCStorage <ChrPowerTypesEntry> sChrPowerTypesStore(ChrClassesXPowerTypesfmt);
// pair<class,power> => powerIndex
uint32 sChrClassXPowerTypesStore[MAX_CLASSES][MAX_POWERS];
// pair<class,powerIndex> => power
uint32 sChrClassXPowerIndexStore[MAX_CLASSES][MAX_STORED_POWERS];
DBCStorage <ChrRacesEntry> sChrRacesStore(ChrRacesEntryfmt);
DBCStorage <CinematicSequencesEntry> sCinematicSequencesStore(CinematicSequencesEntryfmt);
DBCStorage <CreatureDisplayInfoEntry> sCreatureDisplayInfoStore(CreatureDisplayInfofmt);
DBCStorage <CreatureDisplayInfoExtraEntry> sCreatureDisplayInfoExtraStore(CreatureDisplayInfoExtrafmt);
DBCStorage <CreatureFamilyEntry> sCreatureFamilyStore(CreatureFamilyfmt);
DBCStorage <CreatureModelDataEntry> sCreatureModelDataStore(CreatureModelDatafmt);
DBCStorage <CreatureSpellDataEntry> sCreatureSpellDataStore(CreatureSpellDatafmt); // sCreatureModelDataStore
DBCStorage <CreatureTypeEntry> sCreatureTypeStore(CreatureTypefmt);
DBCStorage <CurrencyTypesEntry> sCurrencyTypesStore(CurrencyTypesfmt);
uint32 PowersByClass[MAX_CLASSES][MAX_POWERS];

DBCStorage <DestructibleModelDataEntry> sDestructibleModelDataStore(DestructibleModelDataFmt);
DBCStorage <DungeonEncounterEntry> sDungeonEncounterStore(DungeonEncounterfmt);
DBCStorage <DurabilityQualityEntry> sDurabilityQualityStore(DurabilityQualityfmt);
DBCStorage <DurabilityCostsEntry> sDurabilityCostsStore(DurabilityCostsfmt);

DBCStorage <EmotesEntry> sEmotesStore(EmotesEntryfmt);
DBCStorage <EmotesTextEntry> sEmotesTextStore(EmotesTextEntryfmt);

typedef std::map<uint32, SimpleFactionsList> FactionTeamMap;
static FactionTeamMap sFactionTeamMap;
DBCStorage <FactionEntry> sFactionStore(FactionEntryfmt);
DBCStorage <FactionTemplateEntry> sFactionTemplateStore(FactionTemplateEntryfmt);

DBCStorage <GameObjectDisplayInfoEntry> sGameObjectDisplayInfoStore(GameObjectDisplayInfofmt);
DBCStorage <GemPropertiesEntry> sGemPropertiesStore(GemPropertiesEntryfmt);
DBCStorage <GlyphPropertiesEntry> sGlyphPropertiesStore(GlyphPropertiesfmt);
DBCStorage <GlyphSlotEntry> sGlyphSlotStore(GlyphSlotfmt);

DBCStorage <GtBarberShopCostBaseEntry>    sGtBarberShopCostBaseStore(GtBarberShopCostBasefmt);
DBCStorage <GtCombatRatingsEntry>         sGtCombatRatingsStore(GtCombatRatingsfmt);
DBCStorage <GtChanceToMeleeCritBaseEntry> sGtChanceToMeleeCritBaseStore(GtChanceToMeleeCritBasefmt);
DBCStorage <GtChanceToMeleeCritEntry>     sGtChanceToMeleeCritStore(GtChanceToMeleeCritfmt);
DBCStorage <GtChanceToSpellCritBaseEntry> sGtChanceToSpellCritBaseStore(GtChanceToSpellCritBasefmt);
DBCStorage <GtChanceToSpellCritEntry>     sGtChanceToSpellCritStore(GtChanceToSpellCritfmt);
DBCStorage <GtOCTClassCombatRatingScalarEntry> sGtOCTClassCombatRatingScalarStore(GtOCTClassCombatRatingScalarfmt);
//DBCStorage <GtOCTRegenMPEntry>            sGtOCTRegenMPStore(GtOCTRegenMPfmt);  -- not used currently
DBCStorage <GtOCTHpPerStaminaEntry>       sGtOCTHpPerStaminaStore(GtOCTHpPerStaminafmt);
DBCStorage <GtRegenMPPerSptEntry>         sGtRegenMPPerSptStore(GtRegenMPPerSptfmt);
DBCStorage <GtSpellScalingEntry>          sGtSpellScalingStore(GtSpellScalingfmt);
DBCStorage <GtOCTBaseHPByClassEntry>      sGtOCTBaseHPByClassStore(GtOCTBaseHPByClassfmt);
DBCStorage <GtOCTBaseMPByClassEntry>      sGtOCTBaseMPByClassStore(GtOCTBaseMPByClassfmt);

DBCStorage <HolidaysEntry>                sHolidaysStore(Holidaysfmt);

DBCStorage <ItemArmorQualityEntry>        sItemArmorQualityStore(ItemArmorQualityfmt);
DBCStorage <ItemArmorShieldEntry>         sItemArmorShieldStore(ItemArmorShieldfmt);
DBCStorage <ItemArmorTotalEntry>          sItemArmorTotalStore(ItemArmorTotalfmt);
DBCStorage <ItemBagFamilyEntry>           sItemBagFamilyStore(ItemBagFamilyfmt);
DBCStorage <ItemClassEntry>               sItemClassStore(ItemClassfmt);
DBCStorage <ItemDamageEntry>              sItemDamageAmmoStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageOneHandStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageOneHandCasterStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageRangedStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageThrownStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageTwoHandStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageTwoHandCasterStore(ItemDamagefmt);
DBCStorage <ItemDamageEntry>              sItemDamageWandStore(ItemDamagefmt);
//DBCStorage <ItemDisplayInfoEntry> sItemDisplayInfoStore(ItemDisplayTemplateEntryfmt); -- not used currently
DBCStorage <ItemLimitCategoryEntry>       sItemLimitCategoryStore(ItemLimitCategoryEntryfmt);
DBCStorage <ItemRandomPropertiesEntry>    sItemRandomPropertiesStore(ItemRandomPropertiesfmt);
DBCStorage <ItemRandomSuffixEntry>        sItemRandomSuffixStore(ItemRandomSuffixfmt);
DBCStorage <ItemReforgeEntry>             sItemReforgeStore(ItemReforgefmt);
DBCStorage <ItemSetEntry> sItemSetStore(ItemSetEntryfmt);
DBCStorage <LfgDungeonsEntry> sLfgDungeonsStore(LfgDungeonsEntryfmt);
DBCStorage <LiquidTypeEntry> sLiquidTypeStore(LiquidTypefmt);
DBCStorage <LockEntry> sLockStore(LockEntryfmt);

DBCStorage <MailTemplateEntry> sMailTemplateStore(MailTemplateEntryfmt);
DBCStorage <MapEntry> sMapStore(MapEntryfmt);

DBCStorage <MapDifficultyEntry> sMapDifficultyStore(MapDifficultyEntryfmt); // only for loading
MapDifficultyMap sMapDifficultyMap;
// (mapId, internal 0-based Difficulty) -> row. Built by BuildMapDifficultyLegacyIndex();
// see GetMapDifficultyData for why the raw DBC keying above cannot serve both.
static MapDifficultyMap sMapDifficultyLegacyMap;
static void BuildMapDifficultyLegacyIndex();

// MAKE_PAIR32(mapId, internal Difficulty) for every map/tier that ships at least one
// NON-wildcard DungeonEncounter row of its own. Consulted by EncounterDifficultyMatches to
// decide whether falling back to a lower tier is allowed; see there for why it must be.
static std::set<uint32> sEncounterExactTiers;
static void BuildEncounterExactTierIndex();

DBCStorage <MovieEntry> sMovieStore(MovieEntryfmt);
DBCStorage <MountCapabilityEntry> sMountCapabilityStore(MountCapabilityfmt);
DBCStorage <MountTypeEntry> sMountTypeStore(MountTypefmt);

DBCStorage <NumTalentsAtLevelEntry> sNumTalentsAtLevelStore(NumTalentsAtLevelfmt);

DBCStorage <OverrideSpellDataEntry> sOverrideSpellDataStore(OverrideSpellDatafmt);
DBCStorage <QuestFactionRewardEntry> sQuestFactionRewardStore(QuestFactionRewardfmt);
DBCStorage <QuestV2Entry> sQuestV2Store(QuestV2fmt);
DBCStorage <QuestSortEntry> sQuestSortStore(QuestSortEntryfmt);
DBCStorage <QuestXPLevel> sQuestXPLevelStore(QuestXPLevelfmt);

DBCStorage <NameGenEntry> sNameGenStore(NameGenEntryfmt);
// (race, sex) -> the names the client's randomiser may return. Built once at load so the
// handler is a bounded lookup rather than a scan of all 12972 rows per click.
static std::map<uint32 /*MAKE_PAIR32(race, sex)*/, std::vector<std::string> > sNameGenIndex;

DBCStorage <PhaseEntry> sPhaseStore(Phasefmt);
DBCStorage <PowerDisplayEntry> sPowerDisplayStore(PowerDisplayfmt);
DBCStorage <PvPDifficultyEntry> sPvPDifficultyStore(PvPDifficultyfmt);

DBCStorage <RandomPropertiesPointsEntry> sRandomPropertiesPointsStore(RandomPropertiesPointsfmt);
DBCStorage <ScalingStatDistributionEntry> sScalingStatDistributionStore(ScalingStatDistributionfmt);
DBCStorage <ScalingStatValuesEntry> sScalingStatValuesStore(ScalingStatValuesfmt);

DBCStorage <SkillLineEntry> sSkillLineStore(SkillLinefmt);
DBCStorage <SkillLineAbilityEntry> sSkillLineAbilityStore(SkillLineAbilityfmt);
DBCStorage <SkillRaceClassInfoEntry> sSkillRaceClassInfoStore(SkillRaceClassInfofmt);

DBCStorage <SoundEntriesEntry> sSoundEntriesStore(SoundEntriesfmt);

DBCStorage <SpellItemEnchantmentEntry> sSpellItemEnchantmentStore(SpellItemEnchantmentfmt);
DBCStorage <SpellItemEnchantmentConditionEntry> sSpellItemEnchantmentConditionStore(SpellItemEnchantmentConditionfmt);
DBCStorage <SpellEntry> sSpellStore(SpellEntryfmt);
SpellCategoryStore sSpellCategoryStore;
PetFamilySpellsStore sPetFamilySpellsStore;

DBCStorage <SpellAuraOptionsEntry> sSpellAuraOptionsStore(SpellAuraOptionsEntryfmt);
DBCStorage <SpellAuraRestrictionsEntry> sSpellAuraRestrictionsStore(SpellAuraRestrictionsEntryfmt);
DBCStorage <SpellCastingRequirementsEntry> sSpellCastingRequirementsStore(SpellCastingRequirementsEntryfmt);
DBCStorage <SpellCategoriesEntry> sSpellCategoriesStore(SpellCategoriesEntryfmt);
DBCStorage <SpellClassOptionsEntry> sSpellClassOptionsStore(SpellClassOptionsEntryfmt);
DBCStorage <SpellCooldownsEntry> sSpellCooldownsStore(SpellCooldownsEntryfmt);
DBCStorage <SpellEffectEntry> sSpellEffectStore(SpellEffectEntryfmt);
DBCStorage <SpellEquippedItemsEntry> sSpellEquippedItemsStore(SpellEquippedItemsEntryfmt);
DBCStorage <SpellInterruptsEntry> sSpellInterruptsStore(SpellInterruptsEntryfmt);
DBCStorage <SpellLevelsEntry> sSpellLevelsStore(SpellLevelsEntryfmt);
DBCStorage <SpellPowerEntry> sSpellPowerStore(SpellPowerEntryfmt);
//DBCStorage <SpellReagentsEntry> sSpellReagentsStore(SpellReagentsEntryfmt);
DBCStorage <SpellScalingEntry> sSpellScalingStore(SpellScalingEntryfmt);
DBCStorage <SpellShapeshiftEntry> sSpellShapeshiftStore(SpellShapeshiftEntryfmt);
DBCStorage <SpellTargetRestrictionsEntry> sSpellTargetRestrictionsStore(SpellTargetRestrictionsEntryfmt);
DBCStorage <SpellTotemsEntry> sSpellTotemsStore(SpellTotemsEntryfmt);

SpellEffectMap sSpellEffectMap;

DBCStorage <SpellCastTimesEntry> sSpellCastTimesStore(SpellCastTimefmt);
DBCStorage <SpellDifficultyEntry> sSpellDifficultyStore(SpellDifficultyfmt);
DBCStorage <SpellDurationEntry> sSpellDurationStore(SpellDurationfmt);
DBCStorage <SpellFocusObjectEntry> sSpellFocusObjectStore(SpellFocusObjectfmt);
DBCStorage <SpellMiscEntry> sSpellMiscStore(SpellMiscfmt);
DBCStorage <SpellRadiusEntry> sSpellRadiusStore(SpellRadiusfmt);
DBCStorage <SpellRangeEntry> sSpellRangeStore(SpellRangefmt);
DBCStorage <SpellRuneCostEntry> sSpellRuneCostStore(SpellRuneCostfmt);
DBCStorage <SpellShapeshiftFormEntry> sSpellShapeshiftFormStore(SpellShapeshiftFormfmt);
//DBCStorage <StableSlotPricesEntry> sStableSlotPricesStore(StableSlotPricesfmt);
DBCStorage <SummonPropertiesEntry> sSummonPropertiesStore(SummonPropertiesfmt);
DBCStorage <TalentEntry> sTalentStore(TalentEntryfmt);
TalentSpellPosMap sTalentSpellPosMap;
DBCStorage <TalentTabEntry> sTalentTabStore(TalentTabEntryfmt);
DBCStorage <TalentTreePrimarySpellsEntry> sTalentTreePrimarySpellsStore(TalentTreePrimarySpellsfmt);
typedef std::map<uint32, std::vector<uint32> > TalentTreeSpellsMap;
TalentTreeSpellsMap sTalentTreeMasterySpellsMap;
TalentTreeSpellsMap sTalentTreePrimarySpellsMap;
typedef std::map<uint32, uint32> TalentTreeRolesMap;
TalentTreeRolesMap sTalentTreeRolesMap;

// store absolute bit position for first rank for talent inspect
static uint32 sTalentTabPages[MAX_CLASSES][3];

DBCStorage <TaxiNodesEntry> sTaxiNodesStore(TaxiNodesEntryfmt);
TaxiMask sTaxiNodesMask;
TaxiMask sOldContinentsNodesMask;
TaxiMask sHordeTaxiNodesMask;
TaxiMask sAllianceTaxiNodesMask;
TaxiMask sDeathKnightTaxiNodesMask;

// DBC used only for initialization sTaxiPathSetBySource at startup.
TaxiPathSetBySource sTaxiPathSetBySource;
DBCStorage <TaxiPathEntry> sTaxiPathStore(TaxiPathEntryfmt);

// DBC store data but sTaxiPathNodesByPath used for fast access to entries (it's not owner pointed data).
TaxiPathNodesByPath sTaxiPathNodesByPath;
static DBCStorage <TaxiPathNodeEntry> sTaxiPathNodeStore(TaxiPathNodeEntryfmt);

TransportAnimationsByEntry sTransportAnimationsByEntry;
DBCStorage <TransportAnimationEntry> sTransportAnimationStore(TransportAnimationEntryfmt);
TransportRotationsByEntry sTransportRotationsByEntry;
DBCStorage <TransportRotationEntry> sTransportRotationStore(TransportRotationEntryfmt);
DBCStorage <TotemCategoryEntry> sTotemCategoryStore(TotemCategoryEntryfmt);
DBCStorage <VehicleEntry> sVehicleStore(VehicleEntryfmt);
DBCStorage <VehicleSeatEntry> sVehicleSeatStore(VehicleSeatEntryfmt);
DBCStorage <WMOAreaTableEntry>  sWMOAreaTableStore(WMOAreaTableEntryfmt);
DBCStorage <WorldMapAreaEntry>  sWorldMapAreaStore(WorldMapAreaEntryfmt);
DBCStorage <WorldMapOverlayEntry> sWorldMapOverlayStore(WorldMapOverlayEntryfmt);
DBCStorage <WorldSafeLocsEntry> sWorldSafeLocsStore(WorldSafeLocsEntryfmt);
DBCStorage <WorldPvPAreaEntry>  sWorldPvPAreaStore(WorldPvPAreaEnrtyfmt);

typedef std::list<std::string> StoreProblemList;

/**
 * @brief Checks whether a client build is supported by the server.
 *
 * @param build The client build number.
 * @return true if the build is accepted; otherwise false.
 */
bool IsAcceptableClientBuild(uint32 build)
{
    int accepted_versions[] = EXPECTED_MANGOSD_CLIENT_BUILD;
    for (int i = 0; accepted_versions[i]; ++i)
        if (int(build) == accepted_versions[i])
        {
            return true;
        }

    return false;
}

/**
 * @brief Builds a space-separated list of supported client builds.
 *
 * @return std::string The formatted build list.
 */
std::string AcceptableClientBuildsListStr()
{
    std::ostringstream data;
    int accepted_versions[] = EXPECTED_MANGOSD_CLIENT_BUILD;
    for (int i = 0; accepted_versions[i]; ++i)
    {
        data << accepted_versions[i] << " ";
    }
    return data.str();
}

/**
 * @brief Checks whether a connecting client's wire build is supported.
 *
 * Separate from IsAcceptableClientBuild on purpose. That one guards data files and must accept
 * 18273, because that is how the 18414 client tags its own MPQ content. This one guards the
 * session and accepts 18414 alone, because the opcode table and every packet body in this core
 * are 18414-specific.
 *
 * @param build The build number the client reported in CMSG_AUTH_SESSION.
 * @return true if the build is accepted; otherwise false.
 */
bool IsAcceptableClientWireBuild(uint32 build)
{
    int accepted_versions[] = EXPECTED_MANGOSD_WIRE_BUILD;
    for (int i = 0; accepted_versions[i]; ++i)
        if (int(build) == accepted_versions[i])
        {
            return true;
        }

    return false;
}

/**
 * @brief Builds a space-separated list of supported client wire builds.
 *
 * @return std::string The formatted build list.
 */
std::string AcceptableClientWireBuildsListStr()
{
    std::ostringstream data;
    int accepted_versions[] = EXPECTED_MANGOSD_WIRE_BUILD;
    for (int i = 0; accepted_versions[i]; ++i)
    {
        data << accepted_versions[i] << " ";
    }
    return data.str();
}

static uint32 sDBCLoadedBuild = 0;                          ///< Client build of the DBC files loaded at startup

/**
 * @brief Returns the client build of the DBC files loaded at startup.
 *
 * @return uint32 The loaded DBC build, or 0 before LoadDBCStores has run.
 */
uint32 GetDBCLoadedBuild()
{
    return sDBCLoadedBuild;
}

static bool ReadDBCBuildFileText(const std::string& dbc_path, char const* localeName, std::string& text)
{
    std::string filename  = dbc_path + "component.wow-" + localeName + ".txt";

    if (FILE* file = fopen(filename.c_str(), "rb"))
    {
        char buf[100];
        fread(buf, 1, 100 - 1, file);
        fclose(file);

        text = &buf[0];
        return true;
    }
    else
    {
        return false;
    }
}

int ReadDBCLocale(const std::string sDataPath)
{
    std::string sDBCpath = sDataPath + "dbc/";
    std::string sFilename;

    sLog.outString ("%i Locales defined in core", MAX_LOCALE);
    for (int uLocaleIndex = 0; uLocaleIndex <= MAX_LOCALE; ++uLocaleIndex)
    {
        sFilename  = sDBCpath + "component.wow-" + fullLocaleNameList[uLocaleIndex].name + ".txt";
        if (FILE* file = fopen(sFilename.c_str(), "rb"))
        {
            if (uLocaleIndex==0)
            {
                uLocaleIndex=1;  // Map enus and engb to 0
            }

            return uLocaleIndex-1; // Successfully located the locale
        }
    }

    return -1; // Failed to locate or access the component.wow<locale>.txt file
}

static uint32 ReadDBCBuild(const std::string& dbc_path, LocaleNameStr const*&localeNameStr)
{
    std::string text;

    if (!localeNameStr)
    {
        for (LocaleNameStr const* itr = &fullLocaleNameList[0]; itr->name; ++itr)
        {
            if (ReadDBCBuildFileText(dbc_path, itr->name, text))
            {
                localeNameStr = itr;
                break;
            }
        }
    }
    else
    {
        ReadDBCBuildFileText(dbc_path, localeNameStr->name, text);
    }

    if (text.empty())
    {
        return 0;
    }

    size_t pos = text.find("version=\"");
    size_t pos1 = pos + strlen("version=\"");
    size_t pos2 = text.find("\"", pos1);
    if (pos == text.npos || pos2 == text.npos || pos1 >= pos2)
    {
        return 0;
    }

    std::string build_str = text.substr(pos1, pos2 - pos1);

    int build = atoi(build_str.c_str());
    if (build <= 0)
    {
        return 0;
    }

    return build;
}

/**
 * @brief Reports a DBC structure size mismatch before asserting.
 *
 * @param fsize The record size defined by the format string.
 * @param rsize The size of the C++ structure.
 * @param filename The DBC file being validated.
 * @return false Always returns false so the assert condition fails.
 */
static bool LoadDBC_assert_print(uint32 fsize, uint32 rsize, const std::string& filename)
{
    sLog.outError("Size of '%s' setted by format string (%u) not equal size of C++ structure (%u).", filename.c_str(), fsize, rsize);

    // ASSERT must fail after function call
    return false;
}

struct LocalData
{
    LocalData(uint32 build, LocaleConstant loc)
        : main_build(build), defaultLocale(loc), availableDbcLocales(0xFFFFFFFF),checkedDbcLocaleBuilds(0) {}

    uint32 main_build;
    LocaleConstant defaultLocale;

    // bitmasks for index of fullLocaleNameList
    uint32 availableDbcLocales;
    uint32 checkedDbcLocaleBuilds;
};

template<class T>
/**
 * @brief Loads a DBC file and its localized string tables.
 *
 * @tparam T The DBC record type.
 * @param localeData Locale-availability tracker shared across the DBC load.
 * @param bar The startup progress indicator.
 * @param errlist The list collecting missing or incompatible files.
 * @param storage The storage receiving loaded records.
 * @param dbc_path The base DBC directory.
 * @param filename The DBC filename to load.
 */
inline void LoadDBC(LocalData& localeData, BarGoLink& bar, StoreProblemList& errlist, DBCStorage<T>& storage, const std::string& dbc_path, const std::string& filename)
{
    // compatibility format and C++ structure sizes
    MANGOS_ASSERT(DBCFileLoader::GetFormatRecordSize(storage.GetFormat()) == sizeof(T) || LoadDBC_assert_print(DBCFileLoader::GetFormatRecordSize(storage.GetFormat()), sizeof(T), filename));

    std::string dbc_filename = dbc_path + filename;
    if (storage.Load(dbc_filename.c_str(),localeData.defaultLocale))
    {
        bar.step();
        for (uint8 i = 0; fullLocaleNameList[i].name; ++i)
        {
            if (!(localeData.availableDbcLocales & (1 << i)))
            {
                continue;
            }

            LocaleNameStr const* localStr = &fullLocaleNameList[i];

            std::string dbc_dir_loc = dbc_path + localStr->name + "/";

            if (!(localeData.checkedDbcLocaleBuilds & (1 << i)))
            {
                localeData.checkedDbcLocaleBuilds |= (1 << i); // mark as checked for speedup next checks

                uint32 build_loc = ReadDBCBuild(dbc_dir_loc, localStr);
                if (localeData.main_build != build_loc)
                {
                    localeData.availableDbcLocales &= ~(1 << i); // mark as not available for speedup next checks

                    // exist but wrong build
                    if (build_loc)
                    {
                        std::string dbc_filename_loc = dbc_path + localStr->name + "/" + filename;
                        char buf[200];
                        snprintf(buf, 200, " (exist, but DBC locale subdir %s have DBCs for build %u instead expected build %u, it and other DBC from subdir skipped)", localStr->name, build_loc, localeData.main_build);
                        errlist.push_back(dbc_filename_loc + buf);
                    }

                    continue;
                }
            }

            std::string dbc_filename_loc = dbc_path + localStr->name + "/" + filename;
            if (!storage.LoadStringsFrom(dbc_filename_loc.c_str(),localStr->locale))
                localeData.availableDbcLocales &= ~(1 << i);// mark as not available for speedup next checks
        }
    }
    else
    {
        // sort problematic dbc to (1) non compatible and (2) nonexistent
        FILE* f = fopen(dbc_filename.c_str(), "rb");
        if (f)
        {
            char buf[100];
            snprintf(buf, 100, " (exist, but have %u fields instead %zu) Wrong client version DBC file?", storage.GetFieldCount(), strlen(storage.GetFormat()));
            errlist.push_back(dbc_filename + buf);
            fclose(f);
        }
        else
        {
            errlist.push_back(dbc_filename);
        }
    }
}

/**
 * @brief Loads all required DBC stores and initializes lookup helpers.
 *
 * @param dataPath The base data directory containing DBC files.
 */
void LoadDBCStores(const std::string& dataPath)
{
    std::string dbcPath = dataPath + "dbc/";

    LocaleNameStr const* defaultLocaleNameStr = NULL;
    uint32 build = ReadDBCBuild(dbcPath,defaultLocaleNameStr);

    // Check the expected DBC version
    if (!IsAcceptableClientBuild(build))
    {
        if (build)
            sLog.outError("Found DBC files for build %u but mangosd expected DBC for one from builds: %s Please extract correct DBC files.", build, AcceptableClientBuildsListStr().c_str());
        else
        {
            sLog.outError("Incorrect DataDir value in mangosd.conf or not found build info (outdated DBC files). Required one from builds: %s Please extract correct DBC files.", AcceptableClientBuildsListStr().c_str());
        }
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    const uint32 DBCFilesCount = 131;

    BarGoLink bar(DBCFilesCount);

    StoreProblemList bad_dbc_files;

    LocalData availableDbcLocales(build,defaultLocaleNameStr->locale);

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAreaStore,                dbcPath, "AreaTable.dbc");

    // must be after sAreaStore loading
    for (uint32 i = 0; i < sAreaStore.GetNumRows(); ++i)    // areaflag numbered from 0
    {
        if (AreaTableEntry const* area = sAreaStore.LookupEntry(i))
        {
            // fill AreaId->DBC records
            sAreaFlagByAreaID.insert(AreaFlagByAreaID::value_type(uint16(area->ID), area->AreaBit));

            // fill MapId->DBC records ( skip sub zones and continents )
            if (area->ParentAreaID == 0 && area->ContinentID != 0 && area->ContinentID != 1 && area->ContinentID != 530 && area->ContinentID != 571 && area->ContinentID != 860 && area->ContinentID != 870)
            {
                sAreaFlagByMapID.insert(AreaFlagByMapID::value_type(area->ContinentID, area->AreaBit));
            }
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAchievementStore,         dbcPath, "Achievement.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAchievementCriteriaStore, dbcPath, "Achievement_Criteria.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAreaTriggerStore,         dbcPath, "AreaTrigger.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAreaGroupStore,           dbcPath, "AreaGroup.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sArmorLocationStore,       dbcPath,"ArmorLocation.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sAuctionHouseStore,        dbcPath, "AuctionHouse.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sBankBagSlotPricesStore,   dbcPath, "BankBagSlotPrices.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sBattlemasterListStore,    dbcPath, "BattlemasterList.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sBarberShopStyleStore,     dbcPath, "BarberShopStyle.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCharStartOutfitStore,     dbcPath, "CharStartOutfit.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sCharTitlesStore,          dbcPath, "CharTitles.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sChatChannelsStore,        dbcPath, "ChatChannels.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sChrClassesStore,          dbcPath, "ChrClasses.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sChrPowerTypesStore,       dbcPath,"ChrClassesXPowerTypes.dbc");
    for (uint32 i = 0; i < MAX_CLASSES; ++i)
    {
        for (uint32 j = 0; j < MAX_POWERS; ++j)
        {
            sChrClassXPowerTypesStore[i][j] = INVALID_POWER_INDEX;
            PowersByClass[i][j] = MAX_POWERS;
        }
        for (uint32 j = 0; j < MAX_STORED_POWERS; ++j)
        {
            sChrClassXPowerIndexStore[i][j] = INVALID_POWER;
        }
    }
    for (uint32 i = 0; i < sChrPowerTypesStore.GetNumRows(); ++i)
    {
        ChrPowerTypesEntry const* entry = sChrPowerTypesStore.LookupEntry(i);
        if (!entry)
        {
            continue;
        }

        MANGOS_ASSERT(entry->ClassID < MAX_CLASSES && "MAX_CLASSES not updated");
        MANGOS_ASSERT(entry->PowerType < MAX_POWERS && "MAX_POWERS not updated");

        uint32 index = 0;

        for (uint32 j = 0; j < MAX_POWERS; ++j)
        {
            if (sChrClassXPowerTypesStore[entry->ClassID][j] != INVALID_POWER_INDEX)
            {
                ++index;
            }
        }
        if (ChrPowerTypesEntry const* power = sChrPowerTypesStore.LookupEntry(i))
        {
            uint32 index = 0;
            for (uint32 j = 0; j < MAX_POWERS; ++j)
                if (PowersByClass[power->ClassID][j] != MAX_POWERS)
                {
                    ++index;
                }

            PowersByClass[power->ClassID][power->PowerType] = index;
        }
        MANGOS_ASSERT(index < MAX_STORED_POWERS && "MAX_STORED_POWERS not updated");

        sChrClassXPowerTypesStore[entry->ClassID][entry->PowerType] = index;
        sChrClassXPowerIndexStore[entry->ClassID][index] = entry->PowerType;
    }
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sChrRacesStore,            dbcPath,"ChrRaces.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sCinematicSequencesStore,  dbcPath,"CinematicSequences.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sCreatureDisplayInfoStore, dbcPath,"CreatureDisplayInfo.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sCreatureDisplayInfoExtraStore,dbcPath,"CreatureDisplayInfoExtra.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sCreatureFamilyStore,      dbcPath,"CreatureFamily.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sCreatureModelDataStore,   dbcPath,"CreatureModelData.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sCreatureSpellDataStore,   dbcPath,"CreatureSpellData.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sCreatureTypeStore,        dbcPath,"CreatureType.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sCurrencyTypesStore,       dbcPath,"CurrencyTypes.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sDestructibleModelDataStore,dbcPath,"DestructibleModelData.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sDungeonEncounterStore,    dbcPath,"DungeonEncounter.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sDurabilityCostsStore,     dbcPath,"DurabilityCosts.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sDurabilityQualityStore,   dbcPath,"DurabilityQuality.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sEmotesStore,              dbcPath,"Emotes.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sEmotesTextStore,          dbcPath,"EmotesText.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sFactionStore,             dbcPath,"Faction.dbc");
    for (uint32 i = 0; i < sFactionStore.GetNumRows(); ++i)
    {
        FactionEntry const* faction = sFactionStore.LookupEntry(i);
        if (faction && faction->ParentFactionID)
        {
            SimpleFactionsList& flist = sFactionTeamMap[faction->ParentFactionID];
            flist.push_back(i);
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sFactionTemplateStore,     dbcPath, "FactionTemplate.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGameObjectDisplayInfoStore, dbcPath, "GameObjectDisplayInfo.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGemPropertiesStore,       dbcPath, "GemProperties.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGlyphPropertiesStore,     dbcPath, "GlyphProperties.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGlyphSlotStore,           dbcPath, "GlyphSlot.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtBarberShopCostBaseStore, dbcPath, "gtBarberShopCostBase.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtCombatRatingsStore,     dbcPath, "gtCombatRatings.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtChanceToMeleeCritBaseStore, dbcPath, "gtChanceToMeleeCritBase.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtChanceToMeleeCritStore, dbcPath, "gtChanceToMeleeCrit.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtChanceToSpellCritBaseStore, dbcPath, "gtChanceToSpellCritBase.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtChanceToSpellCritStore, dbcPath, "gtChanceToSpellCrit.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtOCTClassCombatRatingScalarStore, dbcPath, "gtOCTClassCombatRatingScalar.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sGtOCTHpPerStaminaStore,   dbcPath,"gtOCTHpPerStamina.dbc");
    // LoadDBC(availableDbcLocales,bar,bad_dbc_files,sGtOCTRegenMPStore,        dbcPath,"gtOCTRegenMP.dbc");       -- not used currently
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sGtRegenMPPerSptStore,     dbcPath, "gtRegenMPPerSpt.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sGtSpellScalingStore,      dbcPath,"gtSpellScaling.dbc");     // 15595
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sGtOCTBaseHPByClassStore,  dbcPath,"gtOCTBaseHPByClass.dbc"); // 15595
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sGtOCTBaseMPByClassStore,  dbcPath,"gtOCTBaseMPByClass.dbc"); // 15595
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sHolidaysStore,            dbcPath, "Holidays.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemArmorQualityStore,    dbcPath,"ItemArmorQuality.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemArmorShieldStore,     dbcPath,"ItemArmorShield.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemArmorTotalStore,      dbcPath,"ItemArmorTotal.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemBagFamilyStore,       dbcPath, "ItemBagFamily.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemReforgeStore,         dbcPath, "ItemReforge.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemClassStore,           dbcPath, "ItemClass.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemDamageAmmoStore,      dbcPath,"ItemDamageAmmo.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemDamageOneHandStore,   dbcPath,"ItemDamageOneHand.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemDamageOneHandCasterStore,dbcPath,"ItemDamageOneHandCaster.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemDamageRangedStore,    dbcPath,"ItemDamageRanged.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemDamageThrownStore,    dbcPath,"ItemDamageThrown.dbc");
    // LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemDisplayInfoStore,     dbcPath,"ItemDisplayInfo.dbc");     -- not used currently

    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemDamageTwoHandStore,   dbcPath,"ItemDamageTwoHand.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemDamageTwoHandCasterStore,dbcPath,"ItemDamageTwoHandCaster.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sItemDamageWandStore,      dbcPath,"ItemDamageWand.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemLimitCategoryStore,   dbcPath, "ItemLimitCategory.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemRandomPropertiesStore, dbcPath, "ItemRandomProperties.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemRandomSuffixStore,    dbcPath, "ItemRandomSuffix.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sItemSetStore,             dbcPath, "ItemSet.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sLfgDungeonsStore,         dbcPath, "LFGDungeons.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sLiquidTypeStore,          dbcPath, "LiquidType.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sLockStore,                dbcPath, "Lock.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sMailTemplateStore,        dbcPath, "MailTemplate.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sMapStore,                 dbcPath, "Map.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sMapDifficultyStore,       dbcPath, "MapDifficulty.dbc");
    // fill data
    for (uint32 i = 1; i < sMapDifficultyStore.GetNumRows(); ++i)
        if (MapDifficultyEntry const* entry = sMapDifficultyStore.LookupEntry(i))
        {
            sMapDifficultyMap[MAKE_PAIR32(entry->MapID, entry->DifficultyID)] = entry;
        }
    // Map.dbc is already loaded above, which the 25-player-only raid widening needs.
    BuildMapDifficultyLegacyIndex();

    // DungeonEncounter.dbc is loaded above; this records which map/tier pairs have their own
    // rows, so EncounterDifficultyMatches knows when falling back is allowed.
    BuildEncounterExactTierIndex();

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sMovieStore,               dbcPath, "Movie.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files, sMountCapabilityStore,     dbcPath,"MountCapability.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files, sMountTypeStore,           dbcPath,"MountType.dbc");
    //LoadDBC(availableDbcLocales,bar,bad_dbc_files,sNumTalentsAtLevelStore,   dbcPath,"NumTalentsAtLevel.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sOverrideSpellDataStore,   dbcPath, "OverrideSpellData.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sQuestFactionRewardStore,  dbcPath, "QuestFactionReward.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sQuestV2Store,             dbcPath, "QuestV2.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sQuestSortStore,           dbcPath, "QuestSort.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sQuestXPLevelStore,        dbcPath, "QuestXP.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sNameGenStore,            dbcPath, "NameGen.dbc");
    // Index from the locale the base file was actually loaded into, NOT slot 0. LoadDBC stores
    // the base DBC at availableDbcLocales.defaultLocale, which ReadDBCBuild derives from whatever
    // locale the dbc/ directory was extracted from -- only on an enUS extraction is that slot 0.
    // The original `*entry->Name` read slot 0 unconditionally, so on a deDE or ruRU install every
    // row saw the initialised empty string, the index stayed empty, and the randomise button
    // answered failure forever. It worked here purely because this box extracts enUS.
    for (uint32 i = 0; i < sNameGenStore.GetNumRows(); ++i)
    {
        NameGenEntry const* entry = sNameGenStore.LookupEntry(i);
        if (!entry || !entry->Name)
        {
            continue;
        }

        char const* name = entry->Name[availableDbcLocales.defaultLocale];
        if (!name || !*name)
        {
            // Fall back to any populated slot. NameGen rows are proper nouns and repeat heavily
            // across locales, so a name from another locale beats refusing to generate one.
            for (uint8 loc = 0; loc < MAX_LOCALE; ++loc)
            {
                if (entry->Name[loc] && *entry->Name[loc])
                {
                    name = entry->Name[loc];
                    break;
                }
            }
        }

        if (name && *name)
        {
            sNameGenIndex[MAKE_PAIR32(entry->Race, entry->Sex)].push_back(name);
        }
    }

    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sPhaseStore,               dbcPath,"Phase.dbc");
    //LoadDBC(availableDbcLocales,bar,bad_dbc_files,sPowerDisplayStore,        dbcPath,"PowerDisplay.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sPvPDifficultyStore,       dbcPath, "PvpDifficulty.dbc");
    for (uint32 i = 0; i < sPvPDifficultyStore.GetNumRows(); ++i)
        if (PvPDifficultyEntry const* entry = sPvPDifficultyStore.LookupEntry(i))
            if (entry->RangeIndex > MAX_BATTLEGROUND_BRACKETS)
            {
                MANGOS_ASSERT(false && "Need update MAX_BATTLEGROUND_BRACKETS by DBC data");
            }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sRandomPropertiesPointsStore, dbcPath, "RandPropPoints.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sScalingStatDistributionStore, dbcPath, "ScalingStatDistribution.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sScalingStatValuesStore,   dbcPath, "ScalingStatValues.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSkillLineStore,           dbcPath, "SkillLine.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSkillLineAbilityStore,    dbcPath, "SkillLineAbility.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSkillRaceClassInfoStore,  dbcPath, "SkillRaceClassInfo.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSoundEntriesStore,        dbcPath, "SoundEntries.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellStore,               dbcPath, "Spell.dbc");

    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellAuraOptionsStore,    dbcPath,"SpellAuraOptions.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellAuraRestrictionsStore, dbcPath,"SpellAuraRestrictions.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellCastingRequirementsStore, dbcPath,"SpellCastingRequirements.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellCategoriesStore,     dbcPath,"SpellCategories.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellClassOptionsStore,   dbcPath,"SpellClassOptions.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellCooldownsStore,      dbcPath,"SpellCooldowns.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellEffectStore,         dbcPath,"SpellEffect.dbc");

    for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
    {
        if(SpellEntry const * spell = sSpellStore.LookupEntry(i))
        {
            if(SpellCategoriesEntry const* category = spell->GetSpellCategories())
                if(uint32 cat = category->Category)
                {
                    sSpellCategoryStore[cat].insert(i);
                }

            // DBC not support uint64 fields but SpellEntry have SpellFamilyFlags mapped at 2 uint32 fields
            // uint32 field already converted to bigendian if need, but must be swapped for correct uint64 bigendian view
            #if MANGOS_ENDIAN == MANGOS_BIGENDIAN
            std::swap(*((uint32*)(&spell->SpellFamilyFlags)),*(((uint32*)(&spell->SpellFamilyFlags))+1));
            #endif
        }
    }

    uint32 oobSpellEffectIndex = 0;
    uint32 spellEffectTierDropped = 0;
    uint32 spellEffectBasePreferred = 0;
    for(uint32 i = 1; i < sSpellEffectStore.GetNumRows(); ++i)
    {
        if (SpellEffectEntry const *spellEffect = sSpellEffectStore.LookupEntry(i))
        {
            switch (spellEffect->EffectAura)
            {
                case SPELL_AURA_MOD_INCREASE_ENERGY:
                case SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT:
                case SPELL_AURA_PERIODIC_MANA_LEECH:
                case SPELL_AURA_PERIODIC_ENERGIZE:
                case SPELL_AURA_POWER_BURN_MANA:
                    MANGOS_ASSERT(spellEffect->EffectMiscValue >= 0 && spellEffect->EffectMiscValue < MAX_POWERS);
                    break;
            }

            if (spellEffect->EffectIndex >= MAX_SPELL_EFFECTS_MOP)
            {
                ++oobSpellEffectIndex;
                continue;
            }

            // SpellEffect.dbc carries a DifficultyID and this map has no room for it: one row
            // survives per (SpellID, EffectIndex). 3043 keys ship rows at more than one tier --
            // 2684 distinct spells, 2940 of those keys with genuinely different payloads.
            //
            // Plain assignment let whichever row this loop reached LAST win. The loop walks
            // ascending Id via LookupEntry, so that was the highest-Id row, and across the shipped
            // DBC that was overwhelmingly an instance tier. Open-world and normal-dungeon casts of
            // those 2684 spells therefore used the heroic, raid or LFR variant of the effect.
            //
            // TWO things change below, and the FIRST matters far more than the second:
            //
            //   the else branch  keeps the row already installed instead of overwriting it, so the
            //                    LOWEST-Id row wins rather than the highest. This is what alters
            //                    the outcome on 2967 of the 2983 keys whose survivor changes.
            //   the preference   lets a base (DifficultyID 0) row displace an already-installed
            //                    instance row. It fires on 76 keys, but on 60 of those the base row
            //                    is also the HIGHEST Id, so the old fill already ended on that very
            //                    row and the preference is inert. It changes 16 outcomes.
            //
            // So the honest description is "lowest-Id wins, with the base row allowed to jump the
            // queue", not "the base row now wins". An earlier revision of this comment led with the
            // preference and offered spell 130078 "Instability" as the example -- which is inert:
            // its rows are 167016/d7, 167017/d6, 167018/d5, 167019/d0, so the base row IS the
            // highest Id and the old fill already landed on it. Real examples, all differing in
            // payload: spell 135146 "Shatter" effect 0 (old Id 182109 tier 7 -> new 176307 tier 0)
            // and 134691 "Impale" effect 0 (old 184928 tier 4 -> new 175542 tier 0).
            //
            // This does NOT make the lookup difficulty-aware: an instance-specific variant is still
            // never applied. That needs the tier threaded through GetSpellEffect's 176 call sites
            // across some thirty files, several of which have no difficulty in scope at all, and is
            // deliberately separate work.
            //
            // 41 keys ship no base row at all, 18 of them carrying just {5, 6}. These are
            // predominantly WotLK ICC and ToC spells -- 66320 Fire Bomb, 66495 Fel Inferno, 69075
            // Bone Storm, 70867/70879 Essence of the Blood Queen, 72151 Frenzied Bloodthirst. For
            // those the lowest-Id row is kept, whichever tier that is. There is no principled
            // answer without tier awareness; this at least makes the choice deterministic.
            //
            // Those 41 are also EXACTLY the keys whose survivor still depends on the direction of
            // this loop. Wherever a base row exists it wins from either direction -- ascending, it
            // is installed first or displaces what was; descending, it displaces what was. Measured:
            // reversing the walk changes 41 keys across 40 spells, and no key ships more than one
            // base row, so there is no ambiguity among base rows either. The loop direction is still
            // load-bearing, just narrowly, and the gate pins it.
            SpellEffectEntry const*& slot =
                sSpellEffectMap[spellEffect->SpellID].effects[spellEffect->EffectIndex];

            if (!slot)
            {
                slot = spellEffect;
            }
            else if (spellEffect->DifficultyID == 0 && slot->DifficultyID != 0)
            {
                slot = spellEffect;                         // base tier jumps the queue
                ++spellEffectBasePreferred;
            }
            else
            {
                ++spellEffectTierDropped;
            }
        }
    }

    if (oobSpellEffectIndex)
    {
        sLog.outErrorDb("SpellEffect.dbc: skipped %u records with EffectIndex >= %u", oobSpellEffectIndex, MAX_SPELL_EFFECTS_MOP);
    }

    if (spellEffectTierDropped || spellEffectBasePreferred)
    {
        // This counts ROWS the map had no slot for, not spells affected. The two are far apart:
        // 6915 surplus rows collapse onto 3043 multi-tier keys, and only 2983 of those keys end up
        // with a different survivor than a last-row-wins fill would have chosen. Do not read this
        // number as "N spells changed behaviour".
        sLog.outString("SpellEffect.dbc: %u per-difficulty row(s) not representable in the "
                       "single-slot effect map (%u lost to an already-installed row, %u displaced "
                       "by a preferred base-difficulty row)",
                       spellEffectTierDropped + spellEffectBasePreferred,
                       spellEffectTierDropped, spellEffectBasePreferred);
    }

    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellEquippedItemsStore,  dbcPath,"SpellEquippedItems.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellInterruptsStore,     dbcPath,"SpellInterrupts.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellLevelsStore,         dbcPath,"SpellLevels.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellPowerStore,          dbcPath,"SpellPower.dbc");
//    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellReagentsStore,       dbcPath,"SpellReagents.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellScalingStore,        dbcPath,"SpellScaling.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellShapeshiftStore,     dbcPath,"SpellShapeshift.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellTargetRestrictionsStore, dbcPath,"SpellTargetRestrictions.dbc");
    LoadDBC(availableDbcLocales,bar,bad_dbc_files,sSpellTotemsStore,         dbcPath,"SpellTotems.dbc");

    for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
    {
        SkillLineAbilityEntry const* skillLine = sSkillLineAbilityStore.LookupEntry(j);

        if (!skillLine)
        {
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(skillLine->Spell);
        //if (spellInfo && (spellInfo->Attributes & (SPELL_ATTR_ABILITY | SPELL_ATTR_PASSIVE | SPELL_ATTR_UNK7 | SPELL_ATTR_UNK8)) == (SPELL_ATTR_ABILITY | SPELL_ATTR_PASSIVE | SPELL_ATTR_UNK7 | SPELL_ATTR_UNK8))
        if (spellInfo && (spellInfo->GetAttributes() & (SPELL_ATTR_ABILITY | SPELL_ATTR_PASSIVE | SPELL_ATTR_UNK7 | SPELL_ATTR_UNK8)) == (SPELL_ATTR_ABILITY | SPELL_ATTR_PASSIVE | SPELL_ATTR_UNK7 | SPELL_ATTR_UNK8))
        {
            for (unsigned int i = 1; i < sCreatureFamilyStore.GetNumRows(); ++i)
            {
                CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(i);
                if (!cFamily)
                {
                    continue;
                }

                if (skillLine->SkillLine != cFamily->SkillLine[0] && skillLine->SkillLine != cFamily->SkillLine[1])
                {
                    continue;
                }

                sPetFamilySpellsStore[i].insert(spellInfo->ID);
            }
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellCastTimesStore,      dbcPath, "SpellCastTimes.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellDurationStore,       dbcPath, "SpellDuration.dbc");
    //LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellDifficultyStore,     dbcPath, "SpellDifficulty.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellFocusObjectStore,    dbcPath, "SpellFocusObject.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellItemEnchantmentStore, dbcPath, "SpellItemEnchantment.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellItemEnchantmentConditionStore, dbcPath, "SpellItemEnchantmentCondition.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellMiscStore,            dbcPath, "SpellMisc.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellRadiusStore,         dbcPath, "SpellRadius.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellRangeStore,          dbcPath, "SpellRange.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellRuneCostStore,       dbcPath, "SpellRuneCost.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSpellShapeshiftFormStore, dbcPath, "SpellShapeshiftForm.dbc");
    //LoadDBC(availableDbcLocales,bar,bad_dbc_files,sStableSlotPricesStore,    dbcPath,"StableSlotPrices.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sSummonPropertiesStore,    dbcPath, "SummonProperties.dbc");
    //LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTalentStore,              dbcPath, "Talent.dbc");

    // create talent spells set
    for (unsigned int i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo) continue;
        for (int j = 0; j < MAX_TALENT_RANK; j++)
            if (talentInfo->RankID[j])
            {
                sTalentSpellPosMap[talentInfo->RankID[j]] = TalentSpellPos(i, j);
            }
    }

    //LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTalentTabStore,           dbcPath, "TalentTab.dbc");

    // prepare fast data access to bit pos of talent ranks for use at inspecting
    {
        // now have all max ranks (and then bit amount used for store talent ranks in inspect)
        for (uint32 talentTabId = 1; talentTabId < sTalentTabStore.GetNumRows(); ++talentTabId)
        {
            TalentTabEntry const* talentTabInfo = sTalentTabStore.LookupEntry(talentTabId);
            if (!talentTabInfo)
            {
                continue;
            }

            for (uint32 i = 0; i < MAX_MASTERY_SPELLS; ++i)
                if (uint32 spellid = talentTabInfo->masterySpells[i])
                    if (sSpellStore.LookupEntry(spellid))
                    {
                        sTalentTreeMasterySpellsMap[talentTabId].push_back(spellid);
                    }

            // prevent memory corruption; otherwise cls will become 12 below
            if ((talentTabInfo->ClassMask & CLASSMASK_ALL_PLAYABLE) == 0)
            {
                continue;
            }

            // store class talent tab pages
            for (uint32 cls = 1; cls < MAX_CLASSES; ++cls)
                if (talentTabInfo->ClassMask & (1 << (cls - 1)))
                {
                    sTalentTabPages[cls][talentTabInfo->tabpage] = talentTabId;
                }

            sTalentTreeRolesMap[talentTabId] = talentTabInfo->rolesMask;
        }
    }

    //LoadDBC(availableDbcLocales,bar,bad_dbc_files, sTalentTreePrimarySpellsStore, dbcPath, "TalentTreePrimarySpells.dbc");
    for (uint32 i = 0; i < sTalentTreePrimarySpellsStore.GetNumRows(); ++i)
        if (TalentTreePrimarySpellsEntry const* talentSpell = sTalentTreePrimarySpellsStore.LookupEntry(i))
            if (sSpellStore.LookupEntry(talentSpell->SpellId))
            {
                sTalentTreePrimarySpellsMap[talentSpell->TalentTree].push_back(talentSpell->SpellId);
            }
    sTalentTreePrimarySpellsStore.Clear();

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiNodesStore,           dbcPath, "TaxiNodes.dbc");

    uint32 maxTaxiNodeId = 0;
    for (uint32 i = 1; i < sTaxiNodesStore.GetNumRows(); ++i)
    {
        if (TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(i))
        {
            maxTaxiNodeId = std::max(maxTaxiNodeId, node->ID);
        }
    }

    size_t const taxiMaskBytesRequired = TaxiMaskRequiredBytes(maxTaxiNodeId);
    if (taxiMaskBytesRequired > TaxiMaskSize)
    {
        sLog.outError("TaxiNodes.dbc requires %zu taxi-mask bytes for node %u, but this server supports only %u bytes.",
            taxiMaskBytesRequired, maxTaxiNodeId, uint32(TaxiMaskSize));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiPathStore,            dbcPath, "TaxiPath.dbc");
    for (uint32 i = 1; i < sTaxiPathStore.GetNumRows(); ++i)
        if (TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(i))
        {
            sTaxiPathSetBySource[entry->FromTaxiNode][entry->ToTaxiNode] = TaxiPathBySourceAndDestination(entry->ID, entry->Cost);
        }
    uint32 pathCount = sTaxiPathStore.GetNumRows();

    //## TaxiPathNode.dbc ## Loaded only for initialization different structures
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiPathNodeStore,        dbcPath, "TaxiPathNode.dbc");
    // Calculate path nodes count
    std::vector<uint32> pathLength;
    pathLength.resize(pathCount);                           // 0 and some other indexes not used
    for (uint32 i = 1; i < sTaxiPathNodeStore.GetNumRows(); ++i)
        if (TaxiPathNodeEntry const* entry = sTaxiPathNodeStore.LookupEntry(i))
        {
            if (pathLength[entry->PathID] < entry->NodeIndex + 1)
            {
                pathLength[entry->PathID] = entry->NodeIndex + 1;
            }
        }
    // Set path length
    sTaxiPathNodesByPath.resize(pathCount);                 // 0 and some other indexes not used
    for (uint32 i = 1; i < sTaxiPathNodesByPath.size(); ++i)
    {
        sTaxiPathNodesByPath[i].resize(pathLength[i]);
    }
    // fill data (pointers to sTaxiPathNodeStore elements
    for (uint32 i = 1; i < sTaxiPathNodeStore.GetNumRows(); ++i)
        if (TaxiPathNodeEntry const* entry = sTaxiPathNodeStore.LookupEntry(i))
        {
            sTaxiPathNodesByPath[entry->PathID].set(entry->NodeIndex, entry);
        }

    // Initialize global taxinodes mask
    // include existing nodes that have at least single not spell base (scripted) path
    {
        std::set<uint32> spellPaths;
        for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
            if (SpellEntry const* sInfo = sSpellStore.LookupEntry(i))
                for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
                    if (SpellEffectEntry const* effect = sInfo->GetSpellEffect(SpellEffectIndex(j)))
                        if (effect->Effect==123 /*SPELL_EFFECT_SEND_TAXI*/)
                        {
                            spellPaths.insert(effect->EffectMiscValue);
                        }

        memset(sTaxiNodesMask, 0, sizeof(sTaxiNodesMask));
        memset(sOldContinentsNodesMask, 0, sizeof(sTaxiNodesMask));
        memset(sHordeTaxiNodesMask, 0, sizeof(sHordeTaxiNodesMask));
        memset(sAllianceTaxiNodesMask, 0, sizeof(sAllianceTaxiNodesMask));
        memset(sDeathKnightTaxiNodesMask, 0, sizeof(sDeathKnightTaxiNodesMask));
        for (uint32 i = 1; i < sTaxiNodesStore.GetNumRows(); ++i)
        {
            TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(i);
            if (!node)
            {
                continue;
            }

            TaxiMaskPosition maskPosition = {};
            if (!GetTaxiMaskPosition(node->ID, maskPosition))
            {
                sLog.outError("Taxi node %u is outside the %u-byte taxi-mask domain.", node->ID, uint32(TaxiMaskSize));
                Log::WaitBeforeContinueIfNeed();
                exit(1);
            }

            TaxiPathSetBySource::const_iterator src_i = sTaxiPathSetBySource.find(i);
            if (src_i != sTaxiPathSetBySource.end() && !src_i->second.empty())
            {
                bool ok = false;
                for (TaxiPathSetForSource::const_iterator dest_i = src_i->second.begin(); dest_i != src_i->second.end(); ++dest_i)
                {
                    // not spell path
                    if (spellPaths.find(dest_i->second.ID) == spellPaths.end())
                    {
                        ok = true;
                        break;
                    }
                }

                if (!ok)
                {
                    continue;
                }
            }

            // valid taxi network node
            sTaxiNodesMask[maskPosition.byteIndex] |= maskPosition.bitMask;

            if (node->MountCreatureID[0] && node->MountCreatureID[0] != 32981)
            {
                sHordeTaxiNodesMask[maskPosition.byteIndex] |= maskPosition.bitMask;
            }
            if (node->MountCreatureID[1] && node->MountCreatureID[1] != 32981)
            {
                sAllianceTaxiNodesMask[maskPosition.byteIndex] |= maskPosition.bitMask;
            }
            if (node->MountCreatureID[0] == 32981 || node->MountCreatureID[1] == 32981)
            {
                sDeathKnightTaxiNodesMask[maskPosition.byteIndex] |= maskPosition.bitMask;
            }

            // old continent node (+ nodes virtually at old continents, check explicitly to avoid loading map files for zone info)
            if (node->ContinentID < 2 || node->ID == 82 || node->ID == 83 || node->ID == 93 || node->ID == 94)
            {
                sOldContinentsNodesMask[maskPosition.byteIndex] |= maskPosition.bitMask;
            }

            // fix DK node at Ebon Hold
            if (i == 315)
            {
                (const_cast<TaxiNodesEntry*>(node))->MountCreatureID[1] = node->MountCreatureID[0];
            }
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTotemCategoryStore,       dbcPath, "TotemCategory.dbc");

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTransportAnimationStore, dbcPath, "TransportAnimation.dbc");
    sTransportAnimationsByEntry.clear();
    for (uint32 i = 0; i < sTransportAnimationStore.GetNumRows(); ++i)
    {
        if (TransportAnimationEntry const* entry = sTransportAnimationStore.LookupEntry(i))
        {
            TransportAnimationEntryMap& route = sTransportAnimationsByEntry[entry->TransportEntry];
            if (!route.insert(TransportAnimationEntryMap::value_type(entry->TimeIndex, entry)).second)
            {
                sLog.outError("TransportAnimation.dbc has duplicate entry %u at time %u", entry->TransportEntry, entry->TimeIndex);
            }
        }
    }

    for (TransportAnimationsByEntry::const_iterator itr = sTransportAnimationsByEntry.begin(); itr != sTransportAnimationsByEntry.end(); ++itr)
    {
        if (itr->second.size() < 2 || itr->second.rbegin()->first == 0)
        {
            sLog.outError("TransportAnimation.dbc entry %u has no usable animation route", itr->first);
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTransportRotationStore, dbcPath, "TransportRotation.dbc");
    sTransportRotationsByEntry.clear();
    for (uint32 i = 0; i < sTransportRotationStore.GetNumRows(); ++i)
    {
        if (TransportRotationEntry const* entry = sTransportRotationStore.LookupEntry(i))
        {
            TransportRotationEntryMap& route = sTransportRotationsByEntry[entry->TransportEntry];
            if (!route.insert(TransportRotationEntryMap::value_type(entry->TimeIndex, entry)).second)
            {
                sLog.outError("TransportRotation.dbc has duplicate entry %u at time %u", entry->TransportEntry, entry->TimeIndex);
            }
        }
    }

    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sVehicleStore,             dbcPath, "Vehicle.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sVehicleSeatStore,         dbcPath, "VehicleSeat.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sWorldMapAreaStore,        dbcPath, "WorldMapArea.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sWMOAreaTableStore,        dbcPath, "WMOAreaTable.dbc");
    for (uint32 i = 0; i < sWMOAreaTableStore.GetNumRows(); ++i)
    {
        if (WMOAreaTableEntry const* entry = sWMOAreaTableStore.LookupEntry(i))
        {
            sWMOAreaInfoByTripple.insert(WMOAreaInfoByTripple::value_type(WMOAreaTableTripple(entry->WMOID, entry->NameSetID, entry->WMOGroupID), entry));
        }
    }
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sWorldMapOverlayStore,     dbcPath, "WorldMapOverlay.dbc");
    LoadDBC(availableDbcLocales, bar, bad_dbc_files, sWorldSafeLocsStore,       dbcPath, "WorldSafeLocs.dbc");

    // error checks
    if (bad_dbc_files.size() >= DBCFilesCount)
    {
        sLog.outError("\nIncorrect DataDir value in mangosd.conf or ALL required *.dbc files (%d) not found by path: %sdbc", DBCFilesCount, dataPath.c_str());
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }
    else if (!bad_dbc_files.empty())
    {
        std::string str;
        for (std::list<std::string>::iterator i = bad_dbc_files.begin(); i != bad_dbc_files.end(); ++i)
        {
            str += *i + "\n";
        }

        sLog.outError("\nSome required *.dbc files (%u from %d) not found or not compatible:\n%s", (uint32)bad_dbc_files.size(), DBCFilesCount, str.c_str());
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    // Check loaded DBC files proper version
    if (!GetAreaEntryByAreaID(6863)                ||       // last area added in 5.4.8 (18414)
        !sCharTitlesStore.LookupEntry(389)         ||       // max char title in 5.4.8 (18414) data (unchanged since 5.4.1)
        !sGemPropertiesStore.LookupEntry(2467)     ||       // max gem property in 5.4.8 (18414) data (unchanged since 5.4.1)
        !sMapStore.LookupEntry(1173)               ||       // max map in 5.4.8 (18414) data (unchanged since 5.4.1)
        !sSpellStore.LookupEntry(163227)           )        // last added spell in 5.4.8 (18414)
    {
        sLog.outError("\nYou have mixed version DBC files. Please re-extract DBC files for one from client build: %s", AcceptableClientBuildsListStr().c_str());
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    // Remember the validated build for consumers like the character
    // database cleaner (see GetDBCLoadedBuild).
    sDBCLoadedBuild = build;

    sLog.outString();
    sLog.outString(">> Initialized %d data stores", DBCFilesCount);
}

/**
 * @brief Gets the faction list associated with a faction team id.
 *
 * @param faction The faction team id.
 * @return SimpleFactionsList const* The faction list, or null if none exists.
 */
SimpleFactionsList const* GetFactionTeamList(uint32 faction)
{
    FactionTeamMap::const_iterator itr = sFactionTeamMap.find(faction);
    if (itr == sFactionTeamMap.end())
    {
        return NULL;
    }
    return &itr->second;
}

/**
 * @brief Gets the localized pet family name.
 *
 * @param petfamily The creature family id.
 * @param dbclang The locale index.
 * @return char const* The localized pet name, or null if unavailable.
 */
char const* GetPetName(uint32 petfamily, uint32 dbclang)
{
    if (!petfamily)
    {
        return NULL;
    }
    CreatureFamilyEntry const* pet_family = sCreatureFamilyStore.LookupEntry(petfamily);
    if (!pet_family)
    {
        return NULL;
    }
    return pet_family->Name[dbclang] ? pet_family->Name[dbclang] : NULL;
}

/**
 * @brief Finds the talent position metadata for a spell id.
 *
 * @param spellId The talent spell id.
 * @return TalentSpellPos const* The talent position, or null if not found.
 */
TalentSpellPos const* GetTalentSpellPos(uint32 spellId)
{
    TalentSpellPosMap::const_iterator itr = sTalentSpellPosMap.find(spellId);
    if (itr == sTalentSpellPosMap.end())
    {
        return NULL;
    }

    return &itr->second;
}

SpellEffectEntry const* GetSpellEffectEntry(uint32 spellId, SpellEffectIndex effect)
{
    if (effect >= MAX_SPELL_EFFECTS_MOP)
    {
        return NULL;
    }

    SpellEffectMap::const_iterator itr = sSpellEffectMap.find(spellId);
    if (itr == sSpellEffectMap.end())
    {
        return NULL;
    }

    return itr->second.effects[effect];
}

void RegisterCustomSpellEffect(uint32 spellId, SpellEffectIndex index, SpellEffectEntry const* effect)
{
    if (index < MAX_SPELL_EFFECTS_MOP)
    {
        sSpellEffectMap[spellId].effects[index] = effect;
    }
}

/**
 * @brief Computes the talent point cost from a talent position.
 *
 * @param pos The talent spell position.
 * @return uint32 The talent point cost.
 */
uint32 GetTalentSpellCost(TalentSpellPos const* pos)
{
    if (pos)
    {
        return pos->rank + 1;
    }

    return 0;
}

/**
 * @brief Computes the talent point cost for a spell id.
 *
 * @param spellId The talent spell id.
 * @return uint32 The talent point cost.
 */
uint32 GetTalentSpellCost(uint32 spellId)
{
    return GetTalentSpellCost(GetTalentSpellPos(spellId));
}

/**
 * @brief Gets the explore flag for an area id.
 *
 * @param area_id The area id.
 * @return int32 The explore flag, or -1 if the area is unknown.
 */
int32 GetAreaFlagByAreaID(uint32 area_id)
{
    AreaFlagByAreaID::iterator i = sAreaFlagByAreaID.find(area_id);
    if (i == sAreaFlagByAreaID.end())
    {
        return -1;
    }

    return i->second;
}

/**
 * @brief Finds WMO area data by root, ADT, and group identifiers.
 *
 * @param rootid The WMO root id.
 * @param adtid The ADT id.
 * @param groupid The group id.
 * @return WMOAreaTableEntry const* The matching area entry, or null if not found.
 */
WMOAreaTableEntry const* GetWMOAreaTableEntryByTripple(int32 rootid, int32 adtid, int32 groupid)
{
    WMOAreaInfoByTripple::iterator i = sWMOAreaInfoByTripple.find(WMOAreaTableTripple(rootid, adtid, groupid));
    if (i == sWMOAreaInfoByTripple.end())
    {
        return NULL;
    }
    return i->second;
}

/**
 * @brief Gets an area table entry by area id.
 *
 * @param area_id The area id.
 * @return AreaTableEntry const* The matching area entry, or null if not found.
 */
AreaTableEntry const* GetAreaEntryByAreaID(uint32 area_id)
{
    int32 areaflag = GetAreaFlagByAreaID(area_id);
    if (areaflag < 0)
    {
        return NULL;
    }

    return sAreaStore.LookupEntry(areaflag);
}

/**
 * @brief Finds an area entry by explore flag and map id.
 *
 * @param area_flag The explore flag.
 * @param map_id The map id.
 * @return AreaTableEntry const* The best matching area entry, or null if none exists.
 */
AreaTableEntry const* GetAreaEntryByAreaFlagAndMap(uint32 area_flag, uint32 map_id)
{
    if (area_flag)
    {
        return sAreaStore.LookupEntry(area_flag);
    }

    if (MapEntry const* mapEntry = sMapStore.LookupEntry(map_id))
    {
        return GetAreaEntryByAreaID(mapEntry->AreaTableID);
    }

    return NULL;
}

/**
 * @brief Gets the default area flag associated with a map id.
 *
 * @param mapid The map id.
 * @return uint32 The area flag, or 0 if none is mapped.
 */
uint32 GetAreaFlagByMapId(uint32 mapid)
{
    AreaFlagByMapID::iterator i = sAreaFlagByMapID.find(mapid);
    if (i == sAreaFlagByMapID.end())
    {
        return 0;
    }
    else
    {
        return i->second;
    }
}

uint32 GetVirtualMapForMapAndZone(uint32 mapid, uint32 zoneId)
{
    if (mapid != 530 && mapid != 571 && mapid != 732)           // speed for most cases
    {
        return mapid;
    }

    if (WorldMapAreaEntry const* wma = sWorldMapAreaStore.LookupEntry(zoneId))
    {
        return wma->DisplayMapID >= 0 ? wma->DisplayMapID : wma->MapID;
    }

    return mapid;
}

ContentLevels GetContentLevelsForMap(uint32 mapid)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry)
    {
        return CONTENT_1_60;
    }

    // exceptions for 648 - Goblin Starter area and 654 - Worgen Starter area
    if (mapid == 648 || mapid == 654)
    {
        return CONTENT_1_60;
    }

    switch (mapEntry->Expansion())
    {
        default: return CONTENT_1_60;
        case 1:  return CONTENT_61_70;
        case 2:  return CONTENT_71_80;
        case 3:  return CONTENT_81_85;
        case 4:  return CONTENT_86_90;
    }
}

/**
 * @brief Finds a chat channel entry by channel id.
 *
 * @param channel_id The channel id.
 * @return ChatChannelsEntry const* The matching channel entry, or null if not found.
 */
ChatChannelsEntry const* GetChannelEntryFor(uint32 channel_id)
{
    // not sorted, numbering index from 0
    for (uint32 i = 0; i < sChatChannelsStore.GetNumRows(); ++i)
    {
        ChatChannelsEntry const* ch = sChatChannelsStore.LookupEntry(i);
        if (ch && ch->ID == channel_id)
        {
            return ch;
        }
    }
    return NULL;
}

bool IsTotemCategoryCompatiableWith(uint32 itemTotemCategoryId, uint32 requiredTotemCategoryId)
{
    if (requiredTotemCategoryId == 0)
    {
        return true;
    }
    if (itemTotemCategoryId == 0)
    {
        return false;
    }

    TotemCategoryEntry const* itemEntry = sTotemCategoryStore.LookupEntry(itemTotemCategoryId);
    if (!itemEntry)
    {
        return false;
    }
    TotemCategoryEntry const* reqEntry = sTotemCategoryStore.LookupEntry(requiredTotemCategoryId);
    if (!reqEntry)
    {
        return false;
    }

    if (itemEntry->TotemCategoryType != reqEntry->TotemCategoryType)
    {
        return false;
    }

    return (itemEntry->TotemCategoryMask & reqEntry->TotemCategoryMask) == reqEntry->TotemCategoryMask;
}

/**
 * @brief Converts zone map percentages into world map coordinates.
 *
 * @param x The X coordinate to convert.
 * @param y The Y coordinate to convert.
 * @param zone The world map area id.
 * @return true if conversion succeeded; otherwise false.
 */
bool Zone2MapCoordinates(float& x, float& y, uint32 zone)
{
    WorldMapAreaEntry const* maEntry = sWorldMapAreaStore.LookupEntry(zone);

    // if not listed then map coordinates (instance)
    if (!maEntry || maEntry->LocBottom == maEntry->LocTop || maEntry->LocRight == maEntry->LocLeft)
    {
        return false;
    }

    std::swap(x, y);                                        // at client map coords swapped
    x = x * ((maEntry->LocBottom - maEntry->LocTop) / 100) + maEntry->LocTop;
    y = y * ((maEntry->LocRight - maEntry->LocLeft) / 100) + maEntry->LocLeft; // client y coord from top to down

    return true;
}

/**
 * @brief Converts world map coordinates into zone map percentages.
 *
 * @param x The X coordinate to convert.
 * @param y The Y coordinate to convert.
 * @param zone The world map area id.
 * @return true if conversion succeeded; otherwise false.
 */
bool Map2ZoneCoordinates(float& x, float& y, uint32 zone)
{
    WorldMapAreaEntry const* maEntry = sWorldMapAreaStore.LookupEntry(zone);

    // if not listed then map coordinates (instance)
    if (!maEntry || maEntry->LocBottom == maEntry->LocTop || maEntry->LocRight == maEntry->LocLeft)
    {
        return false;
    }

    x = (x - maEntry->LocTop) / ((maEntry->LocBottom - maEntry->LocTop) / 100);
    y = (y - maEntry->LocLeft) / ((maEntry->LocRight - maEntry->LocLeft) / 100); // client y coord from top to down
    std::swap(x, y);                                        // client have map coords swapped

    return true;
}

ContentLevels GetContentLevelsForMapAndZone(uint32 mapId, uint32 zoneId)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
    if (!mapEntry)
    {
        return CONTENT_1_60;
    }

    if (mapEntry->RootPhaseMap != -1)
    {
        mapId = mapEntry->RootPhaseMap;
    }

    switch (mapId)
    {
        case 648:   // Lost Islands
        case 654:   // Gilneas
            return CONTENT_1_60;
        default:
            break;
    }

    switch (zoneId)
    {
        case 616:   // Mount Hyjal
        case 4922:  // Twilight Highlands
        case 5034:  // Uldum
        case 5042:  // Deepholm
            return CONTENT_81_85;
        default:
            break;
    }

    switch (mapEntry->Expansion())
    {
        default: return CONTENT_1_60;
        case 1:  return CONTENT_61_70;
        case 2:  return CONTENT_71_80;
        case 3:  return CONTENT_81_85;
        case 4:  return CONTENT_86_90;
    }
}

/**
 * @brief Translate a raw 5.4.8 client DifficultyID into the core's internal mode.
 *
 * MapDifficulty.dbc is keyed on Difficulty.dbc ids, which for instances START AT 1,
 * while the Difficulty enum is the 0-based WotLK-era one. Stockades (map 34) has
 * exactly one row, DifficultyID 1, so a request for DUNGEON_DIFFICULTY_NORMAL (0)
 * found nothing, Player::GetAreaTriggerLockStatus() answered
 * AREA_LOCKSTATUS_MISSING_DIFFICULTY, and no instance in the game was enterable.
 * Continents were unaffected because world, battleground and arena maps are the
 * only ones that carry a 0 row.
 *
 * The direction here is client -> internal, which is what building the legacy index
 * needs. The id mapping is identical to the switch in BuildMapSpawnModeMasks and the
 * two must be changed together: a tier this function admits but that one drops
 * resolves at the area trigger and then instantiates with no spawns filed under its
 * mask, so the instance is entered completely empty. Challenge mode is exactly that
 * hazard, and the two switches agree by both OMITTING id 8 -- see below.
 *
 * Ids with no internal equivalent return -1 and are simply absent from the index:
 * LFR (7), flexible (14) and scenarios (11, 12). The Difficulty enum has no member
 * for any of them, so giving them one is a feature, not a mapping fix.
 *
 * Challenge mode (8) is deliberately NOT translated, and that is a reversal of an earlier
 * revision of this branch which mapped it to internal mode 2.
 *
 * All nine challenge dungeons ship a DifficultyID 8 row in MapDifficulty.dbc -- 959, 960, 961,
 * 962, 994, 1001, 1004, 1007 and 1011 -- so translating it puts internal mode 2 in the legacy
 * index and lets the client enter at that tier. The world database has no spawns to match:
 *
 *   maps 959/960/961/962   347/177/433/561 creatures, every one spawnMask 3 (bits 0 and 1)
 *   maps 994/1001/1004/1007/1011   no creature spawns at all, in any mode
 *
 * Nothing on those maps carries bit 2, so the instance instantiates and is completely empty.
 * That is exactly the divergence this file warns about elsewhere, one layer further down: the
 * earlier revision checked that ToInternalDifficulty and BuildMapSpawnModeMasks agreed with each
 * other, but agreeing about a mode the DATA never populates still yields an empty dungeon.
 * Note BuildMapSpawnModeMasks cannot fix this -- its output is a validation permission mask
 * (ObjectMgrCreatures.cpp rejects spawns carrying unsupported bits), not a spawn source.
 *
 * Refusing entry is better than admitting a player to an empty dungeon, so 8 stays unsupported
 * until spawn data exists. Re-enable it by returning 2 here once
 *   SELECT COUNT(*) FROM creature WHERE map IN (959,960,961,962,994,1001,1004,1007,1011)
 *     AND (spawnMask & 4) <> 0
 * is non-zero, and test an actual challenge map rather than trusting the mask.
 */
int32 ToInternalDifficulty(uint32 clientDifficultyId)
{
    switch (clientDifficultyId)
    {
        case 0:  return 0;                                  // continents / bg / arena
        case 1:  return 0;                                  // 5-man normal
        case 2:  return 1;                                  // 5-man heroic
        case 3:  return 0;                                  // raid 10 normal
        case 4:  return 1;                                  // raid 25 normal
        case 5:  return 2;                                  // raid 10 heroic
        case 6:  return 3;                                  // raid 25 heroic
        case 9:  return 0;                                  // legacy 40-player raids
        // case 8 (5-man challenge) intentionally absent -- see the note above; the world DB has
        // no bit-2 spawns on any challenge map, so admitting it yields an empty instance.
        default: return -1;                                 // challenge 8, LFR 7, flexible 14, scenarios 11/12
    }
}

/**
 * @brief Whether a DungeonEncounter.dbc row applies at an internal difficulty.
 *
 * DungeonEncounter.dbc keys on raw client DifficultyIDs like everything else in the
 * 5.4.8 DBCs, and comparing one directly against a Difficulty was wrong in both
 * directions. Of the 699 shipped rows only the 238 carrying id 0 ever matched, and
 * they matched for the wrong reason; the 264 rows for 5-man normal (id 1) were tested
 * against internal mode 1, which is HEROIC, so a normal clear credited nothing while a
 * heroic clear credited the normal encounter. Ids 5 and 6 (Sinestra, Ra-den) exceed
 * MAX_DIFFICULTY as raw values and could never match at all.
 *
 * Id 0 is a wildcard, not internal mode 0. 41 maps carry nothing but id 0 rows and 36
 * of those have more than one difficulty tier, so reading it as "normal only" would
 * stop every heroic run on them from ever crediting an encounter.
 */
/**
 * @brief The tier a client DifficultyID falls back to, mirroring Difficulty.dbc field 1.
 *
 * Hardcoded for the same reason ToInternalDifficulty is: Difficulty.dbc is never loaded by this
 * core, and at twelve static rows for 5.4.8 the table is not worth a store. Values read directly
 * from the shipped file; 0 means "no fallback", it is not a reference to id 0.
 *
 *   2 -> 1        5-man heroic falls back to 5-man normal
 *   5 -> 3        10-player heroic  -> 10-player normal
 *   6 -> 4        25-player heroic  -> 25-player normal
 *   7 -> 4        LFR               -> 25-player normal
 *   8 -> 2 -> 1   challenge mode    -> heroic -> normal
 *   11 -> 12      heroic scenario   -> normal scenario
 *
 * Ids 1, 3, 4, 9, 12 and 14 terminate.
 */
static uint32 ClientDifficultyFallback(uint32 clientDifficultyId)
{
    switch (clientDifficultyId)
    {
        case 2:  return 1;
        case 5:  return 3;
        case 6:  return 4;
        case 7:  return 4;
        case 8:  return 2;
        case 11: return 12;
        default: return 0;                                  // no fallback
    }
}

/**
 * @brief True when (mapId, clientDifficultyId) is a row the OLD raw-keyed reset scheduler could
 *        have written into `instance_reset`.
 *
 * A migration predicate, and deliberately not an accessor. The general raw lookup
 * (GetMapDifficultyDataByClientId) is banned tree-wide precisely so a raw row cannot reach runtime
 * code, and this must not be a way back in -- it answers a yes/no question about a stored key and
 * hands back no row.
 *
 * Needed because "out of internal range" is NOT evidence of the raw key space. An arbitrary
 * hand-edited value is also out of range, and treating it as proof would condemn a whole table of
 * valid reset times over one bad row. The old scheduler enumerated the RAW map and skipped
 * RaidDuration == 0, so a genuine stale row is exactly a raw reset-bearing (map, tier) pair: 136 of
 * them across 88 maps, values 2, 3, 4, 5, 6 and 9. Anything else out of range is just junk.
 *
 * @param mapId              the map the stored row names
 * @param clientDifficultyId the stored difficulty, read as a RAW client id
 * @return true if the raw map has this pair with a global reset
 */
bool IsLegacyRawResetKey(uint32 mapId, uint32 clientDifficultyId)
{
    MapDifficultyMap::const_iterator itr = sMapDifficultyMap.find(MAKE_PAIR32(mapId, clientDifficultyId));
    if (itr == sMapDifficultyMap.end())
    {
        return false;
    }

    return itr->second->RaidDuration != 0;
}

/**
 * @brief Whether a DungeonEncounter.dbc row applies at an internal difficulty on a given map.
 *
 * DungeonEncounter.dbc keys on raw client DifficultyIDs like everything else in the 5.4.8 DBCs,
 * and comparing one directly against a Difficulty was wrong in both directions. Of the 699
 * shipped rows only the 238 carrying id 0 ever matched, and they matched for the wrong reason;
 * the 264 rows for 5-man normal (id 1) were tested against internal mode 1, which is HEROIC, so a
 * normal clear credited nothing while a heroic clear credited the normal encounter. Ids 5 and 6
 * (Sinestra, Ra-den) exceed MAX_DIFFICULTY as raw values and could never match at all.
 *
 * Id 0 is a wildcard, not internal mode 0. 41 maps carry nothing but id 0 rows and 36 of those
 * have more than one difficulty tier, so reading it as "normal only" would stop every heroic run
 * on them from ever crediting an encounter.
 *
 * Straight equality was still wrong, though, because Difficulty.dbc defines fallback chains. Some
 * maps ship a tier whose encounters are tagged only for a LOWER tier, so an equality test credits
 * nothing at all there. Measured against the shipped DBCs, walking the chain recovers 30 rows
 * across four map/tier pairs: maps 189, 289, 309 and 598 at heroic, 6, 13, 10 and 1 rows, all
 * tagged id 1.
 *
 * An earlier revision said 32 rows across five pairs, the fifth being map 994 at challenge mode
 * reached through the full 8 -> 2 -> 1 chain. That gain existed only while ToInternalDifficulty
 * translated challenge mode; it does not now, so no map tier resolves to challenge and the chain
 * is never entered from there. ClientDifficultyFallback still records 8 -> 2 because that is what
 * Difficulty.dbc says, but the link is currently unreachable.
 *
 * The fallback is deliberately NOT unconditional. 33 map/tier pairs carry BOTH an exact row and a
 * fallback-reachable one, and a lower-tier row must not be allowed to answer for a tier that has
 * its own. sEncounterExactTiers records which map/tier pairs do.
 *
 * An earlier revision of this comment justified that guard by saying unconditional fallback would
 * "credit each boss twice". That is WRONG and was corrected on review:
 * DungeonPersistentState::UpdateEncounterState returns immediately after the first matching row,
 * so no kill can ever credit two encounters. The real hazard is narrower -- the FIRST matching row
 * wins, so without the guard a lower-tier row could answer ahead of the map's own row for that
 * tier and set the wrong Bit.
 *
 * Granularity is per map/tier rather than per encounter, and that is a deliberate simplification
 * with a measured basis. Reviewed as too coarse, on the argument that a mixed-tier map might carry
 * an encounter existing ONLY at the lower tier, which this guard would reject. Checked against the
 * shipped data per encounter Bit: of the 109 fallback-reachable rows the guard blocks across those
 * 33 pairs, every single one has its Bit already covered by an exact or wildcard row on the same
 * map and tier. Zero encounters are lost, so per-map-tier and per-encounter agree on all 5.4.8
 * data. If a future DBC ships a fallback-only encounter on a mixed-tier map this must become
 * per-Bit; the query above is how to tell.
 *
 * Verified against the DBCs, twice and independently: 30 row/tier pairs gained over plain
 * ToInternalDifficulty equality, 0 rows lost, 0 lower-tier rows admitted where an exact row
 * exists, and 0 encounters lost to the guard's granularity. (An earlier revision said 32 gained;
 * every way of counting it -- by row/tier pair, by distinct DBC row Id, and without requiring the
 * map to offer the tier -- gives 30.)
 *
 * Worth being plain about what that measurement means for the guard itself: at boss granularity it
 * is INERT on shipped 5.4.8.18414 data. Dropping it changes no encounter's creditability, because
 * each of the 109 rows it blocks has its Bit covered anyway. It is kept because it states what the
 * predicate means -- an exact row for a tier settles that tier -- and because without it those 109
 * rows match a tier the DBC assigned elsewhere. It is intent, not a bug fix.
 *
 * @param mapId                 the map the encounter belongs to
 * @param encounterDifficultyId raw DungeonEncounter.dbc DifficultyID
 * @param difficulty            the internal difficulty the group is running
 */
bool EncounterDifficultyMatches(uint32 mapId, uint32 encounterDifficultyId, Difficulty difficulty)
{
    if (encounterDifficultyId == 0)
    {
        return true;                                        // applies to every tier of the map
    }

    if (ToInternalDifficulty(encounterDifficultyId) == int32(difficulty))
    {
        return true;                                        // the map's own row for this tier
    }

    // An exact row exists for this tier, so the lower tiers are not this tier's encounters.
    if (sEncounterExactTiers.find(MAKE_PAIR32(mapId, uint32(difficulty))) != sEncounterExactTiers.end())
    {
        return false;
    }

    // Nothing exact: walk this map's tier down its fallback chain and see if the row sits on it.
    MapDifficultyEntry const* mapDiff = GetMapDifficultyData(mapId, difficulty);
    if (!mapDiff)
    {
        return false;
    }

    // Bounded, because an unbounded walk over a hand-maintained table is a startup hang waiting to
    // happen. The shipped chains are acyclic and at most two links long (8 -> 2 -> 1), and no
    // future Difficulty.dbc can change that -- the core never loads that file, so the only way to
    // introduce a cycle is to edit ClientDifficultyFallback itself. The bound is what makes that
    // edit a wrong answer instead of an infinite loop, and MAX_DIFFICULTY_FALLBACK_DEPTH is well
    // clear of the longest real chain.
    uint32 const MAX_DIFFICULTY_FALLBACK_DEPTH = 8;
    uint32 depth = 0;

    for (uint32 tier = ClientDifficultyFallback(mapDiff->DifficultyID); tier;
         tier = ClientDifficultyFallback(tier))
    {
        if (tier == encounterDifficultyId)
        {
            return true;
        }

        if (++depth >= MAX_DIFFICULTY_FALLBACK_DEPTH)
        {
            sLog.outError("EncounterDifficultyMatches: ClientDifficultyFallback chain from raw id %u "
                          "exceeded %u links -- it has a cycle. Treating map %u tier %u as no match.",
                          mapDiff->DifficultyID, MAX_DIFFICULTY_FALLBACK_DEPTH, mapId, uint32(difficulty));
            break;
        }
    }

    return false;
}

/**
 * @brief Records which map/tier pairs ship a non-wildcard encounter row of their own.
 *
 * Must run after DungeonEncounter.dbc is loaded. Wildcard (id 0) rows are excluded on purpose:
 * they apply everywhere, so a map carrying only wildcards has no tier-specific rows and should
 * still be allowed to fall back.
 */
static void BuildEncounterExactTierIndex()
{
    sEncounterExactTiers.clear();

    for (uint32 i = 0; i < sDungeonEncounterStore.GetNumRows(); ++i)
    {
        DungeonEncounterEntry const* entry = sDungeonEncounterStore.LookupEntry(i);
        if (!entry || entry->DifficultyID == 0)
        {
            continue;
        }

        int32 internal = ToInternalDifficulty(entry->DifficultyID);
        if (internal < 0)
        {
            continue;                                       // LFR, flexible, scenarios
        }

        sEncounterExactTiers.insert(MAKE_PAIR32(entry->MapID, uint32(internal)));
    }
}

/**
 * @brief Builds the legacy-keyed index that GetMapDifficultyData answers from.
 *
 * A static per-type translation is not enough. Some raids have no ID 3 row at all:
 * 25-player-only raids carry only ID 4, and legacy 40-player raids only ID 9. Asking
 * for the regular tier and translating it to a fixed 3 misses those maps entirely,
 * so they stay unenterable.
 *
 * The widening below serves the same purpose as the one in BuildMapSpawnModeMasks but
 * is NOT the identical predicate: that one fires only when a raid's mask is exactly
 * (1 << 1), this one whenever mode 0 is absent and some row maps to mode 1. They agree
 * on all 253 maps in 5.4.8 -- the seven rows=[4] raids -- and would diverge only on a
 * hypothetical raid carrying {4,5} or {4,6}. No such map ships.
 *
 * Where two rows map to the same internal mode -- a map carrying both ID 3 and ID 9,
 * say -- the lower client id wins, so the modern row is preferred over the legacy one.
 */
static void BuildMapDifficultyLegacyIndex()
{
    sMapDifficultyLegacyMap.clear();

    for (MapDifficultyMap::const_iterator itr = sMapDifficultyMap.begin(); itr != sMapDifficultyMap.end(); ++itr)
    {
        MapDifficultyEntry const* mapDiff = itr->second;
        int32 const mode = ToInternalDifficulty(mapDiff->DifficultyID);
        if (mode < 0 || mode >= MAX_DIFFICULTY)
        {
            continue;
        }

        uint32 const key = MAKE_PAIR32(mapDiff->MapID, uint32(mode));
        MapDifficultyMap::const_iterator existing = sMapDifficultyLegacyMap.find(key);
        if (existing == sMapDifficultyLegacyMap.end() ||
            mapDiff->DifficultyID < existing->second->DifficultyID)
        {
            sMapDifficultyLegacyMap[key] = mapDiff;
        }
    }

    // 25-player-only raids instantiate as internal mode 0. Without this a regular
    // lookup on Hyjal, Magtheridon, SSC, The Eye, Black Temple, Gruul or Sunwell
    // finds nothing and the map is refused at the area trigger.
    for (MapDifficultyMap::const_iterator itr = sMapDifficultyMap.begin(); itr != sMapDifficultyMap.end(); ++itr)
    {
        MapDifficultyEntry const* mapDiff = itr->second;
        MapEntry const* mapEntry = sMapStore.LookupEntry(mapDiff->MapID);
        if (!mapEntry || !mapEntry->IsRaid())
        {
            continue;
        }

        uint32 const regular = MAKE_PAIR32(mapDiff->MapID, uint32(REGULAR_DIFFICULTY));
        if (sMapDifficultyLegacyMap.find(regular) == sMapDifficultyLegacyMap.end() &&
            ToInternalDifficulty(mapDiff->DifficultyID) == 1)
        {
            sMapDifficultyLegacyMap[regular] = mapDiff;
        }
    }
}

/**
 * @brief Looks up the per-difficulty data row for a given map.
 *
 * MapDifficulty.dbc is keyed on Difficulty.dbc ids, which for instances START AT 1,
 * while the core's Difficulty enum is the 0-based WotLK-era one. Every instance
 * lookup therefore missed: Stockades (map 34) has exactly one row, DifficultyID 1,
 * and a request for DUNGEON_DIFFICULTY_NORMAL (0) found nothing, so
 * Player::GetAreaTriggerLockStatus answered AREA_LOCKSTATUS_MISSING_DIFFICULTY and
 * no instance in the game was enterable. Continents were unaffected because
 * world, battleground and arena maps are the only ones that carry a 0 row.
 *
 * @param mapId The map id.
 * @param difficulty The map difficulty, in the core's internal 0-based form.
 * @return Pointer to the MapDifficultyEntry, or NULL when no row matches.
 *
 * @note Nothing may carry a raw client DifficultyID across a runtime or persistence
 *       boundary. MapDifficulty.dbc is translated here, at load. Two other DBCs also
 *       store raw ids and each translates at its own boundary instead:
 *       LfgDungeons.dbc via ToInternalDifficulty in LFGMgr::CreateDungeonGroup, and
 *       DungeonEncounter.dbc via EncounterDifficultyMatches at both credit sites.
 *       Both of those were direct casts and both persisted the result.
 */
MapDifficultyEntry const* GetMapDifficultyData(uint32 mapId, Difficulty difficulty)
{
    MapDifficultyMap::const_iterator itr = sMapDifficultyLegacyMap.find(MAKE_PAIR32(mapId, difficulty));
    return itr != sMapDifficultyLegacyMap.end() ? itr->second : NULL;
}

/**
 * @brief The internal-mode index itself, for callers that must enumerate tiers.
 *
 * Only the reset scheduler needs this: it has to discover every (map, internal mode)
 * pair that carries a global reset in order to schedule one. Iterating the raw
 * sMapDifficultyMap instead is what made the reset system incoherent -- it keyed
 * `instance_reset`, the reset events and m_resetTimeByMapDifficulty on raw client
 * ids, while AddPersistentState, MovementHandler and DungeonPersistentState all read
 * those same structures with internal modes. Since no raw id except 0 equals its own
 * internal mode, 129 of the 143 reset-bearing tiers missed outright and the other 14
 * silently picked another tier's row.
 */
MapDifficultyMap const& GetMapDifficultyLegacyMap()
{
    return sMapDifficultyLegacyMap;
}

/**
 * @brief Builds per-map spawn-mode masks from the MapDifficulty data.
 *
 * Translates 5.4.8 client difficulty ids into the core's internal spawn
 * modes (0..MAX_DIFFICULTY-1), which is the convention used by the DB
 * spawn data and the runtime (Map::GetSpawnMode): continents (0) -> 0,
 * 5-man normal/heroic (1/2) -> 0/1, raid 10N/25N/10H/25H (3..6) -> 0..3,
 * legacy 40-player raids (9) -> 0. LFR (7), 5-man challenge (8), scenarios
 * (11, 12) and flexible (14) are not translated and are ignored -- for
 * challenge mode that is a deliberate choice rather than a missing enum
 * value, because no spawn on a challenge map carries bit 2. This switch and
 * ToInternalDifficulty must keep agreeing on exactly which ids they admit. 25-player-only raids (TBC) instantiate as spawn mode 0
 * internally and are widened accordingly. Map 0 has no MapDifficulty rows
 * in 4.x+ clients and is forced to the regular mask.
 *
 * The id mapping must stay identical to ToInternalDifficulty. Challenge mode
 * was dropped here while the difficulty index admitted it, which let a player
 * at dungeon difficulty 2 pass the area trigger on maps 959/960/961/962/994/
 * 1001/1004/1007/1011 and then arrive in an instance with no spawns filed
 * under mask bit 2. The shipped DB has no challenge spawns yet, so nothing
 * observable changed -- but the contract was wrong.
 *
 * @param spawnMasks Destination: map id -> allowed spawn-mode mask.
 */
void BuildMapSpawnModeMasks(std::map<uint32, uint32>& spawnMasks)
{
    spawnMasks.clear();

    for (MapDifficultyMap::const_iterator itr = sMapDifficultyMap.begin(); itr != sMapDifficultyMap.end(); ++itr)
    {
        MapDifficultyEntry const* mapDiff = itr->second;
        int32 mode = -1;

        switch (mapDiff->DifficultyID)
        {
            case 0:                                         // continents
                mode = 0;
                break;
            case 1:                                         // 5-man normal
            case 2:                                         // 5-man heroic
                mode = int32(mapDiff->DifficultyID) - 1;
                break;
            case 3:                                         // raid 10 normal
            case 4:                                         // raid 25 normal
            case 5:                                         // raid 10 heroic
            case 6:                                         // raid 25 heroic
                mode = int32(mapDiff->DifficultyID) - 3;
                break;
            case 9:                                         // legacy 40-player raids
                mode = 0;
                break;
            default:                                        // challenge (8) / LFR (7) / flexible (14) / scenarios (11, 12)
                // Challenge mode is unsupported here for the same reason as in
                // ToInternalDifficulty: no challenge map has a bit-2 spawn, so the tier cannot be
                // populated. These two switches must stay in step -- a mode one admits and the
                // other drops resolves at the area trigger and then instantiates with no spawns.
                break;
        }

        if (mode >= 0 && mode < MAX_DIFFICULTY)
        {
            spawnMasks[mapDiff->MapID] |= (1 << mode);
        }
    }

    // 25-player-only raids (TBC: Hyjal, Magtheridon, SSC, The Eye, Black
    // Temple, Gruul, Sunwell) instantiate as spawn mode 0 internally
    for (std::map<uint32, uint32>::iterator itr = spawnMasks.begin(); itr != spawnMasks.end(); ++itr)
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(itr->first);
        if (mapEntry && mapEntry->IsRaid() && itr->second == (1 << 1))
        {
            itr->second |= 1;
        }
    }

    // Map 0 was removed from dbc as of 4.x.x
    spawnMasks[0] = 1 << REGULAR_DIFFICULTY;
}

/**
 * @brief Picks a random NameGen.dbc name for a race/sex, or NULL if there is none.
 *
 * Backs the character-creation randomise button. The client sends
 * CMSG_RANDOMIZE_CHAR_NAME carrying the race and sex currently selected on screen and
 * waits for a name to drop into the edit box; without a reply the button does nothing.
 *
 * NameGen.dbc ships 12972 names across 13 races and both sexes -- 219 human male, 539 orc
 * male and so on -- so every creatable combination has candidates. A race/sex the DBC does
 * not cover returns NULL and the caller answers with the failure bit rather than inventing
 * a name, which is the one thing the client cannot recover from sensibly.
 */
std::string const* GetRandomCharacterName(uint32 race, uint32 sex)
{
    std::map<uint32, std::vector<std::string> >::const_iterator itr =
        sNameGenIndex.find(MAKE_PAIR32(race, sex));
    if (itr == sNameGenIndex.end() || itr->second.empty())
    {
        return NULL;
    }

    return &itr->second[urand(0, itr->second.size() - 1)];
}

PvPDifficultyEntry const* GetBattlegroundBracketByLevel(uint32 mapid, uint32 level)
{
    PvPDifficultyEntry const* maxEntry = NULL;              // used for level > max listed level case
    for (uint32 i = 0; i < sPvPDifficultyStore.GetNumRows(); ++i)
    {
        if (PvPDifficultyEntry const* entry = sPvPDifficultyStore.LookupEntry(i))
        {
            // skip unrelated and too-high brackets
            if (entry->MapID != mapid || entry->MinLevel > level)
            {
                continue;
            }

            // exactly fit
            if (entry->MaxLevel >= level)
            {
                return entry;
            }

            // remember for possible out-of-range case (search higher from existed)
            if (!maxEntry || maxEntry->MaxLevel < entry->MaxLevel)
            {
                maxEntry = entry;
            }
        }
    }

    return maxEntry;
}

PvPDifficultyEntry const* GetBattlegroundBracketById(uint32 mapid, BattleGroundBracketId id)
{
    for (uint32 i = 0; i < sPvPDifficultyStore.GetNumRows(); ++i)
        if (PvPDifficultyEntry const* entry = sPvPDifficultyStore.LookupEntry(i))
            if (entry->MapID == mapid && entry->GetBracketId() == id)
            {
                return entry;
            }

    return NULL;
}

/**
 * @brief Gets the talent tab pages for a class.
 *
 * @param cls The class id.
 * @return uint32 const* The three talent tab page ids for the class.
 */
uint32 const* GetTalentTabPages(uint32 cls)
{
    return sTalentTabPages[cls];
}

uint32 GetPowerIndexByClass(uint32 powerType, uint32 classId)
{
    return PowersByClass[classId][powerType];
}

std::vector<uint32> const* GetTalentTreeMasterySpells(uint32 talentTree)
{
    TalentTreeSpellsMap::const_iterator itr = sTalentTreeMasterySpellsMap.find(talentTree);
    if (itr == sTalentTreeMasterySpellsMap.end())
    {
        return NULL;
    }

    return &itr->second;
}

std::vector<uint32> const* GetTalentTreePrimarySpells(uint32 talentTree)
{
    TalentTreeSpellsMap::const_iterator itr = sTalentTreePrimarySpellsMap.find(talentTree);
    if (itr == sTalentTreePrimarySpellsMap.end())
    {
        return NULL;
    }

    return &itr->second;
}

uint32 GetTalentTreeRolesMask(uint32 talentTree)
{
    TalentTreeRolesMap::const_iterator itr = sTalentTreeRolesMap.find(talentTree);
    if (itr == sTalentTreeRolesMap.end())
    {
        return 0;
    }

    return itr->second;
}

/**
 * @brief Checks whether a point lies inside an area trigger volume.
 *
 * @param atEntry The area trigger definition.
 * @param mapid The current map id.
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @param z The Z coordinate.
 * @param delta Extra tolerance applied to the trigger bounds.
 * @return true if the point is inside the trigger; otherwise false.
 */
bool IsPointInAreaTriggerZone(AreaTriggerEntry const* atEntry, uint32 mapid, float x, float y, float z, float delta)
{
    if (mapid != atEntry->ContinentID)
    {
        return false;
    }

    if (atEntry->Radius > 0)
    {
        // if we have radius check it
        float dist2 = (x - atEntry->x) * (x - atEntry->x) + (y - atEntry->y) * (y - atEntry->y) + (z - atEntry->z) * (z - atEntry->z);
        if (dist2 > (atEntry->Radius + delta) * (atEntry->Radius + delta))
        {
            return false;
        }
    }
    else
    {
        // we have only extent

        // rotate the players position instead of rotating the whole cube, that way we can make a simplified
        // is-in-cube check and we have to calculate only one point instead of 4

        // 2PI = 360, keep in mind that ingame orientation is counter-clockwise
        double rotation = 2 * M_PI - atEntry->Box_yaw;
        double sinVal = sin(rotation);
        double cosVal = cos(rotation);

        float playerBoxDistX = x - atEntry->x;
        float playerBoxDistY = y - atEntry->y;

        float rotPlayerX = float(atEntry->x + playerBoxDistX * cosVal - playerBoxDistY * sinVal);
        float rotPlayerY = float(atEntry->y + playerBoxDistY * cosVal + playerBoxDistX * sinVal);

        // box edges are parallel to coordiante axis, so we can treat every dimension independently :D
        float dz = z - atEntry->z;
        float dx = rotPlayerX - atEntry->x;
        float dy = rotPlayerY - atEntry->y;
        if ((fabs(dx) > atEntry->Box_length / 2 + delta) ||
                (fabs(dy) > atEntry->Box_width / 2 + delta) ||
                (fabs(dz) > atEntry->Box_height / 2 + delta))
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Gets the race id associated with a creature display model.
 *
 * @param model_id The creature model display id.
 * @return uint32 The race id, or 0 if no race data is available.
 */
uint32 GetCreatureModelRace(uint32 model_id)
{
    CreatureDisplayInfoEntry const* displayEntry = sCreatureDisplayInfoStore.LookupEntry(model_id);
    if (!displayEntry)
    {
        return 0;
    }
    CreatureDisplayInfoExtraEntry const* extraEntry = sCreatureDisplayInfoExtraStore.LookupEntry(displayEntry->ExtendedDisplayInfoID);
    return extraEntry ? extraEntry->DisplayRaceID : 0;
}

float GetCurrencyPrecision(uint32 currencyId)
{
    CurrencyTypesEntry const * entry = sCurrencyTypesStore.LookupEntry(currencyId);

    return entry ? entry->GetPrecision() : 1.0f;
}

// script support functions
 DBCStorage <SoundEntriesEntry>  const* GetSoundEntriesStore()   { return &sSoundEntriesStore;   }
 DBCStorage <SpellEntry>         const* GetSpellStore()          { return &sSpellStore;          }
 DBCStorage <SpellRangeEntry>    const* GetSpellRangeStore()     { return &sSpellRangeStore;     }
 DBCStorage <FactionEntry>       const* GetFactionStore()        { return &sFactionStore;        }
 DBCStorage <CreatureDisplayInfoEntry> const* GetCreatureDisplayStore() { return &sCreatureDisplayInfoStore; }
 DBCStorage <EmotesEntry>        const* GetEmotesStore()         { return &sEmotesStore;         }
 DBCStorage <EmotesTextEntry>    const* GetEmotesTextStore()     { return &sEmotesTextStore;     }
