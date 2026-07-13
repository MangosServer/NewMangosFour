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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_DBCSFRM_H
#define MANGOS_DBCSFRM_H

const char Achievementfmt[]= "niiissiiiiisiix";                     // 5.3.0 Checked
const char AchievementCriteriafmt[]="niiiiiiiixsiiiiixxxxxxx";      // 5.4.8: file is 23 fields / 96 B (col4 Quantity is int64 = 2 dwords); 23 chars can cover only 92 B -- trailing AdditionalConditionValue[2] is unreadable by design; loader tolerates the 4-byte tail.
const char AreaTableEntryfmt[] = "iiinixxxxxxxisiiiiixxxxxxxxxxx";  // TODO: NEED TO CONFIRM THIS
const char AreaGroupEntryfmt[] = "niiiiiii";                        // 5.3.0 Checked
const char AreaTriggerEntryfmt[]="nifffxxxfffffxxx";                // 5.4.8
const char ArmorLocationfmt[]="nfffff";                             // 5.3.0 Checked
const char AuctionHouseEntryfmt[]= "niiix";                         // 5.3.0 Checked
const char BankBagSlotPricesEntryfmt[] = "ni";                      // 5.3.0 Checked
const char BarberShopStyleEntryfmt[]= "nixxxiii";                   // TODO: 5.3.0 DIFFERENT
const char BattlemasterListEntryfmt[] = "niiiiiiiiiiiiiiiiixsiiiixxxxxx";
//                                    1        10        20        30        40        50        60        70        80        90        100       110       120       130       140       150       160
const char CharStartOutfitEntryfmt[]= "dbbbXiiiiiiiiiiiiiiiiiiiiiiiixxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";    // 5.3.0 packed byte header (304B client record)
const char CharTitlesEntryfmt[]="nxsxix";                           // 5.3.0 Checked
const char ChatChannelsEntryfmt[]="iixsx";                          // 5.3.0 Checked
// ChatChannelsEntryfmt, index not used (more compact store)
const char ChrClassesEntryfmt[]="nixsxxxixiiiixxxxx";               // TODO: NEED TO CONFIRM THIS
const char ChrRacesEntryfmt[]="nxixiixixxxxixsxxxxxixxxxxxxxxxxxxxx"; // 5.4.8
const char ChrClassesXPowerTypesfmt[]="nii";                        // 5.3.0 Checked
const char CinematicSequencesEntryfmt[] = "nxxxxxxxxx";             // 5.3.0 Checked
const char CreatureDisplayInfofmt[]="nixifxxxxxxxxxxxxxxx";         // TODO: NEED TO CONFIRM THIS
const char CreatureDisplayInfoExtrafmt[] = "nixxxxxxxxxxxxxxxxxxx"; // 5.3.0 Checked
const char CreatureFamilyfmt[]="nfifiiiiixsx";                      // 5.3.0 Checked
const char CreatureModelDatafmt[] = "nxxxxxxxxxxxxxxffxxxxxxxxxxxxxxxxx";
const char CreatureSpellDatafmt[] = "niiiixxxx";                    // 5.3.0 Checked
const char CreatureTypefmt[]="nxx";                                 // 5.3.0 Checked
const char CurrencyTypesfmt[]="nisxxxxiiixx";   // TODO: NEED TO CONFIRM THIS
const char DestructibleModelDataFmt[] = "nixxxixxxxixxxxixxxxixxx";
const char DungeonEncounterfmt[]="niiiisxxx";   // 5.4.8
const char DurabilityCostsfmt[] = "niiiiiiiiiiiiiiiiiiiiiiiiiiiii"; // TODO: 5.3.0 DIFFERENT
const char DurabilityQualityfmt[] = "nf";                           // TODO: 5.3.0 DIFFERENT
const char EmotesEntryfmt[]="nxxiiixx";                             // 5.3.0 Checked
const char EmotesTextEntryfmt[] = "nxixxxxxxxxxxxxxxxx";            // 5.3.0 Checked
const char FactionEntryfmt[]="niiiiiiiiiiiiiiiiiiffixsxxxx";        // TODO: NEED TO CONFIRM THIS
const char FactionTemplateEntryfmt[] = "niiiiiiiiiiiii";            // 5.3.0 Checked
const char GameObjectDisplayInfofmt[]="nsxxxxxxxxxxffffffxxx";      // 5.3.0 Checked
const char GemPropertiesEntryfmt[]="nixxix";                        // 5.3.0 Checked
const char GlyphPropertiesfmt[] = "niii";                           // 5.3.0 Checked
const char GlyphSlotfmt[] = "nii";                                  // 5.3.0 Checked
const char GtBarberShopCostBasefmt[]="xf";                          // 5.3.0 Checked
const char GtCombatRatingsfmt[]="xf";                               // 5.3.0 Checked
const char GtChanceToMeleeCritBasefmt[]="xf";                       // 5.3.0 Checked
const char GtChanceToMeleeCritfmt[]="xf";                           // 5.3.0 Checked
const char GtChanceToSpellCritBasefmt[]="xf";                       // 5.3.0 Checked
const char GtChanceToSpellCritfmt[]="xf";                           // 5.3.0 Checked
const char GtOCTBaseHPByClassfmt[]="df";                            // 5.3.0 Checked
const char GtOCTBaseMPByClassfmt[]="df";                            // 5.3.0 Checked
const char GtOCTClassCombatRatingScalarfmt[] = "df";                // 5.3.0 Checked

const char GtOCTHpPerStaminafmt[]="df";                             // 5.3.0 Checked
//const char GtOCTRegenHPfmt[]="xf";
const char GtRegenHPPerSptfmt[]="xf";
const char GtRegenMPPerSptfmt[]="xf";                               // 5.3.0 Checked
const char GtSpellScalingfmt[]="df";                                // 5.3.0 Checked
const char Holidaysfmt[] = "nxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; // 5.3.0 Checked
const char ItemArmorQualityfmt[]="nfffffffi";                       // 5.3.0 Checked
const char ItemArmorShieldfmt[]="nifffffff";                        // 5.3.0 Checked
const char ItemArmorTotalfmt[]="niffff";                            // 5.3.0 Checked
const char ItemBagFamilyfmt[]="nx";                                 // 5.3.0 Checked
const char ItemClassfmt[]="nifs";
// const char ItemDisplayTemplateEntryfmt[]="nxxxxxxxxxxixxxxxxxxxxx";
// const char ItemCondExtCostsEntryfmt[]="xiii";
const char ItemDamagefmt[]="nfffffffi";
const char ItemLimitCategoryEntryfmt[]="nxii";                      // 5.3.0 Checked
const char ItemRandomPropertiesfmt[]="nxiiiiis";                    // 5.3.0 Checked
const char ItemRandomSuffixfmt[]="nsxiiiiiiiiii";                   // 5.3.0 Checked
const char ItemReforgefmt[]="nifif";
const char ItemSetEntryfmt[]="dsxxxxxxxxxxxxxxxxxiiiiiiiiiiiiiiiiii"; // 5.3.0 Checked
const char LfgDungeonsEntryfmt[] = "isiiiiiiiiiisiiisiiiiiiiiiiii";
const char LiquidTypefmt[] = "nxxixixxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; // 5.3.0 Checked
const char LockEntryfmt[] = "niiiiiiiiiiiiiiiiiiiiiiiixxxxxxxx";    // 5.3.0 Checked
const char MailTemplateEntryfmt[]="nxs";                            // 5.3.0 Checked
const char MapEntryfmt[]="nsiiisissififfiiiii";                     // 5.4.8
const char MapDifficultyEntryfmt[]="niisiii";                       // 5.4.8 registry: idx6 LockID(uint), was s difficultyString
const char MovieEntryfmt[]="nxxxx";                                 // 5.4.8
const char MountCapabilityfmt[]="niiiiiii";
const char MountTypefmt[]="niiiiiiiiiiiiiiiiiiiiiiii";
const char NumTalentsAtLevelfmt[]="df";
const char OverrideSpellDatafmt[]="niiiiiiiiiixx";
const char QuestFactionRewardfmt[] = "niiiiiiiiii";
const char QuestSortEntryfmt[]="nx";                                // 5.3.0 Checked
const char QuestXPLevelfmt[] = "niiiiiiiiii";                       // 5.3.0 Checked
const char Phasefmt[]="nsi";                                        // 5.4.8 registry: idx1 Name(string), was i PhaseShift
const char PowerDisplayfmt[] = "nixxxx";
const char PvPDifficultyfmt[] = "diiiii";
const char RandomPropertiesPointsfmt[] = "niiiiiiiiiiiiiii";        // 5.3.0 Checked
const char ScalingStatDistributionfmt[]="niiiiiiiiiiiiiiiiiiiixi";  // 5.3.0 Checked
const char ScalingStatValuesfmt[]="iniiiiiixiiiiiiiiiiiiiixxxxxxxxxxxxxxxxxxxxxxxxxx";   // TODO: NEED TO CONFIRM THIS
const char SkillLinefmt[]="nisxixixx";                              // TODO: NEED TO CONFIRM THIS
const char SkillLineAbilityfmt[]="niiiiiiiiiixx";                    // 5.4.8 (18414): MoP removed excludeRace/excludeClass; columns 5+ shifted left two slots
const char SkillRaceClassInfofmt[]="diiiiixx";                      // 5.4.8
const char SoundEntriesfmt[]="nisxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"; // TODO: NEED TO CONFIRM THIS
const char SpellCastTimefmt[]="niii";                               // 5.3.0 Checked
const char SpellDurationfmt[] = "niii";                             // 5.3.0 Checked
const char SpellDifficultyfmt[] = "niiii";
//                          1        10        20        30        40        50        60        70        80        90        100       110       120       130       140       150       160
const char SpellEntryfmt[]="nssxxixxxiiiiiiiiiiiiiixi";             // 5.4.8
const char SpellAuraOptionsEntryfmt[]="dxxiiiixx";                  // TODO: NEED TO CONFIRM THIS
const char SpellAuraRestrictionsEntryfmt[]="dxxiiiiiiii";           // 5.4.8
const char SpellCastingRequirementsEntryfmt[]="dixxixi";            // 5.3.0 Checked
const char SpellCategoriesEntryfmt[]="dxxiiiiiix";                  // 5.4.8
const char SpellClassOptionsEntryfmt[]="dxiiiix";
const char SpellCooldownsEntryfmt[]="dxxiii";                       // 5.4.8
//                                1        10        20        30
const char SpellEffectEntryfmt[]="diifiiixfiiiiiifiifiiiiixiiiix";  // 5.4.8
const char SpellEquippedItemsEntryfmt[]="dxxiii";                   // 5.4.8
const char SpellInterruptsEntryfmt[]="dxxixixi";                    // 5.4.8
const char SpellLevelsEntryfmt[]="dxxiii";                          // 5.4.8
const char SpellPowerEntryfmt[]="xnxiiiiixxxxx";                    // 5.4.8
//const char SpellReagentsEntryfmt[]="diiiiiiiiiiiiiiii";
const char SpellScalingEntryfmt[]="diiiifiii";                      // 5.4.8 registry: idx6 CoefLevelBase(int) idx7 MaxLevel(uint), was ff
const char SpellShapeshiftEntryfmt[]="dixixx";                      // 5.3.0 Checked
const char SpellTargetRestrictionsEntryfmt[]="dxxfxiiii"; // 5.4.8
const char SpellTotemsEntryfmt[]="diiii";                           // 5.3.0 Checked
const char SpellFocusObjectfmt[]="nx";
const char SpellItemEnchantmentfmt[]="nxiiiiiiiiisiiiixxixxxxxxx";   // 5.4.8
const char SpellItemEnchantmentConditionfmt[] = "nbbbbbxxxxxbbbbbbbbbbiiiiiXXXXX"; // 5.4.8: file is 31 fields / 72 B; no {1,4}-byte 31-char fmt can sum to 72 (4a+b=72, a+b=31 has no integer solution) -- current fmt is the aligned maximal read; loader tolerates the tail.
const char SpellMiscfmt[]="dxxiiiiiiiiiiiiiiiiifiiiii";             // TODO: NEED TO CONFIRM THIS
const char SpellRadiusfmt[]="nfxxx";                                // 5.4.8
const char SpellRangefmt[]="nffffxxx";                              // 5.4.8
const char SpellRuneCostfmt[]="niiixi";                             // 5.4.8
const char SpellShapeshiftFormfmt[]="nxxiixiiixxiiiiiiiixx";        // 5.3.0 Checked
//const char StableSlotPricesfmt[] = "ni"; // removed
const char SummonPropertiesfmt[] = "niiiii";                        // 5.3.0 Checked
const char TalentEntryfmt[]="niiiiiiiiixxixxxxxx";
const char TalentTabEntryfmt[]="nxxiiixxiii";
const char TalentTreePrimarySpellsfmt[]="diix";
const char TaxiNodesEntryfmt[]="nifffsiixxxx";                      // TODO: NEED TO CONFIRM THIS
const char TaxiPathEntryfmt[] = "niii";                             // 5.3.0 Checked
const char TaxiPathNodeEntryfmt[] = "diiifffiiii";                  // 5.3.0 Checked
const char TotemCategoryEntryfmt[]="nxii";                          // 5.3.0 Checked
const char TransportAnimationEntryfmt[]="diixxxx";                  // 5.3.0 Checked
const char VehicleEntryfmt[]="nixffffiiiiiiiifffffffffffffffssssfifiixx"; // 16357
//                                1        10        20        30        40        50        60        70        80        90        100       110       120       130       140       150       160
const char VehicleSeatEntryfmt[]="niiffffffffffiiiiiifffffffiiifffiiiiiiiffiiiifxxxxxxxxxxxxxxxxxxxx"; // 5.4.8 registry: idx45 CameraEnteringDelay(float), was i m_flagsB
const char WMOAreaTableEntryfmt[]="niiixxxxxiixxxx";                // 5.3.0 Checked
const char WorldMapAreaEntryfmt[]="xinxffffixxxxx";                 // 5.3.0 Checked
const char WorldMapOverlayEntryfmt[]="nxiiiixxxxxxxxxx";            // 5.4.8
const char WorldSafeLocsEntryfmt[]="nifffxx";                       // 5.4.8
const char WorldPvPAreaEnrtyfmt[]="niiiiii";

#endif