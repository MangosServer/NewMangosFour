/*
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

#include "Common.h"
#include "DBCStructure.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "SharedDefines.h"

int32 SpellEntry::CalculateSimpleValue(SpellEffectIndex eff) const
{
    if (SpellEffectEntry const* effectEntry = GetSpellEffectEntry(ID, eff))
    {
        return effectEntry->CalculateSimpleValue();
    }
    return 0;
}

ClassFamilyMask const& SpellEntry::GetEffectSpellClassMask(SpellEffectIndex eff) const
{
    if (SpellEffectEntry const* effectEntry = GetSpellEffectEntry(ID, eff))
    {
        return effectEntry->EffectSpellClassMask;
    }

    static ClassFamilyMask const emptyCFM;

    return emptyCFM;
}

SpellAuraOptionsEntry const* SpellEntry::GetSpellAuraOptions() const
{
    return AuraOptionsID ? sSpellAuraOptionsStore.LookupEntry(AuraOptionsID) : NULL;
}

SpellAuraRestrictionsEntry const* SpellEntry::GetSpellAuraRestrictions() const
{
    return AuraRestrictionsID ? sSpellAuraRestrictionsStore.LookupEntry(AuraRestrictionsID) : NULL;
}

SpellCastingRequirementsEntry const* SpellEntry::GetSpellCastingRequirements() const
{
    return CastingRequirementsID ? sSpellCastingRequirementsStore.LookupEntry(CastingRequirementsID) : NULL;
}

SpellCategoriesEntry const* SpellEntry::GetSpellCategories() const
{
    return CategoriesID ? sSpellCategoriesStore.LookupEntry(CategoriesID) : NULL;
}

SpellClassOptionsEntry const* SpellEntry::GetSpellClassOptions() const
{
    return ClassOptionsID ? sSpellClassOptionsStore.LookupEntry(ClassOptionsID) : NULL;
}

SpellCooldownsEntry const* SpellEntry::GetSpellCooldowns() const
{
    return CooldownsID ? sSpellCooldownsStore.LookupEntry(CooldownsID) : NULL;
}

SpellEffectEntry const* SpellEntry::GetSpellEffect(SpellEffectIndex eff) const
{
    return GetSpellEffectEntry(ID, eff);
}

SpellEquippedItemsEntry const* SpellEntry::GetSpellEquippedItems() const
{
    return EquippedItemsID ? sSpellEquippedItemsStore.LookupEntry(EquippedItemsID) : NULL;
}

SpellInterruptsEntry const* SpellEntry::GetSpellInterrupts() const
{
    return InterruptsID ? sSpellInterruptsStore.LookupEntry(InterruptsID) : NULL;
}

SpellLevelsEntry const* SpellEntry::GetSpellLevels() const
{
    return LevelsID ? sSpellLevelsStore.LookupEntry(LevelsID) : NULL;
}

SpellPowerEntry const* SpellEntry::GetSpellPower() const
{
#if defined(CATA)
    return SpellPowerId ? sSpellPowerStore.LookupEntry(SpellPowerId) : NULL;
#elif defined(MISTS)
    return sSpellPowerStore.LookupEntry(ID);
#endif
}

SpellReagentsEntry const* SpellEntry::GetSpellReagents() const
{
    return ReagentsID ? sSpellReagentsStore.LookupEntry(ReagentsID) : NULL;
}

SpellScalingEntry const* SpellEntry::GetSpellScaling() const
{
    return ScalingID ? sSpellScalingStore.LookupEntry(ScalingID) : NULL;
}

SpellShapeshiftEntry const* SpellEntry::GetSpellShapeshift() const
{
    return ShapeshiftID ? sSpellShapeshiftStore.LookupEntry(ShapeshiftID) : NULL;
}

SpellTargetRestrictionsEntry const* SpellEntry::GetSpellTargetRestrictions() const
{
    return TargetRestrictionsID ? sSpellTargetRestrictionsStore.LookupEntry(TargetRestrictionsID) : NULL;
}

SpellTotemsEntry const* SpellEntry::GetSpellTotems() const
{
    return TotemsID ? sSpellTotemsStore.LookupEntry(TotemsID) : NULL;
}

SpellMiscEntry const* SpellEntry::GetSpellMisc() const
{
    return MiscID ? sSpellMiscStore.LookupEntry(MiscID) : NULL;
}

uint32 SpellEntry::GetManaCost() const
{
    SpellPowerEntry const* power = GetSpellPower();
    return power ? power->ManaCost : 0;
}

uint32 SpellEntry::GetPreventionType() const
{
    SpellCategoriesEntry const* cat = GetSpellCategories();
    return cat ? cat->PreventionType : 0;
}

uint32 SpellEntry::GetCategory() const
{
    SpellCategoriesEntry const* cat = GetSpellCategories();
    return cat ? cat->Category : 0;
}

uint32 SpellEntry::GetStartRecoveryTime() const
{
    SpellCooldownsEntry const* cd = GetSpellCooldowns();
    return cd ? cd->StartRecoveryTime : 0;
}

uint32 SpellEntry::GetMechanic() const
{
    SpellCategoriesEntry const* cat = GetSpellCategories();
    return cat ? cat->Mechanic : 0;
}

uint32 SpellEntry::GetRecoveryTime() const
{
    SpellCooldownsEntry const* cd = GetSpellCooldowns();
    return cd ? cd->RecoveryTime : 0;
}

uint32 SpellEntry::GetCategoryRecoveryTime() const
{
    SpellCooldownsEntry const* cd = GetSpellCooldowns();
    return cd ? cd->CategoryRecoveryTime : 0;
}

uint32 SpellEntry::GetStartRecoveryCategory() const
{
    SpellCategoriesEntry const* cat = GetSpellCategories();
    return cat ? cat->StartRecoveryCategory : 0;
}

uint32 SpellEntry::GetSpellLevel() const
{
    SpellLevelsEntry const* levels = GetSpellLevels();
    return levels ? levels->SpellLevel : 0;
}

int32 SpellEntry::GetEquippedItemClass() const
{
    SpellEquippedItemsEntry const* items = GetSpellEquippedItems();
    return items ? items->EquippedItemClass : -1;
}

SpellFamily SpellEntry::GetSpellFamilyName() const
{
    SpellClassOptionsEntry const* classOpt = GetSpellClassOptions();
    return classOpt ? SpellFamily(classOpt->SpellFamilyName) : SPELLFAMILY_GENERIC;
}

uint32 SpellEntry::GetDmgClass() const
{
    SpellCategoriesEntry const* cat = GetSpellCategories();
    return cat ? cat->DefenseType : 0;
}

uint32 SpellEntry::GetDispel() const
{
    SpellCategoriesEntry const* cat = GetSpellCategories();
    return cat ? cat->DispelType : 0;
}

uint32 SpellEntry::GetMaxAffectedTargets() const
{
    SpellTargetRestrictionsEntry const* target = GetSpellTargetRestrictions();
    return target ? target->MaxAffectedTargets : 0;
}

uint32 SpellEntry::GetStackAmount() const
{
    SpellAuraOptionsEntry const* aura = GetSpellAuraOptions();
    return aura ? aura->CumulativeAura : 0;
}

uint32 SpellEntry::GetManaCostPercentage() const
{
    SpellPowerEntry const* power = GetSpellPower();
    // NOTE (MoP port-sweep): 'ManaPerSecond' is the 5.4.8 client name for SpellPower column 6; this
    // accessor's byte offset is unchanged from pre-alignment. The true percentage cost is the client
    // 'PowerCostPct' float (column 9), which this core does not model yet, so the value returned here
    // is the per-second column, not a percentage. Semantic fix deferred to the MoP port-sweep track.
    return power ? power->ManaPerSecond : 0;
}

uint32 SpellEntry::GetProcCharges() const
{
    SpellAuraOptionsEntry const* aura = GetSpellAuraOptions();
    return aura ? aura->ProcCharges : 0;
}

uint32 SpellEntry::GetProcChance() const
{
    SpellAuraOptionsEntry const* aura = GetSpellAuraOptions();
    return aura ? aura->ProcChance : 0;
}

uint32 SpellEntry::GetMaxLevel() const
{
    SpellLevelsEntry const* levels = GetSpellLevels();
    return levels ? levels->MaxLevel : 0;
}

uint32 SpellEntry::GetTargetAuraState() const
{
    SpellAuraRestrictionsEntry const* aura = GetSpellAuraRestrictions();
    return aura ? aura->TargetAuraState : 0;
}

uint32 SpellEntry::GetManaPerSecond() const
{
    SpellPowerEntry const* power = GetSpellPower();
    // NOTE (MoP port-sweep): 'PowerDisplayID' is the 5.4.8 client name for SpellPower column 7; byte
    // offset unchanged from pre-alignment. The true per-second drain is the client 'ManaPerSecond'
    // (column 6, consumed above by GetManaCostPercentage). Semantic fix deferred to the port-sweep track.
    return power ? power->PowerDisplayID : 0;
}

uint32 SpellEntry::GetRequiresSpellFocus() const
{
    SpellCastingRequirementsEntry const* castReq = GetSpellCastingRequirements();
    return castReq ? castReq->RequiresSpellFocus : 0;
}

uint32 SpellEntry::GetSpellEffectIdByIndex(SpellEffectIndex index) const
{
    SpellEffectEntry const* effect = GetSpellEffect(index);
    return effect ? effect->Effect : SPELL_EFFECT_NONE;
}

uint32 SpellEntry::GetAuraInterruptFlags() const
{
    SpellInterruptsEntry const* interrupt = GetSpellInterrupts();
    return interrupt ? interrupt->AuraInterruptFlags : 0;
}

uint32 SpellEntry::GetEffectImplicitTargetAByIndex(SpellEffectIndex index) const
{
    SpellEffectEntry const* effect = GetSpellEffect(index);
    return effect ? effect->EffectImplicitTargetA : TARGET_NONE;
}

int32 SpellEntry::GetAreaGroupId() const
{
    SpellCastingRequirementsEntry const* castReq = GetSpellCastingRequirements();
    return castReq ? castReq->AreaGroupId : 0;
}

uint32 SpellEntry::GetFacingCasterFlags() const
{
    SpellCastingRequirementsEntry const* castReq = GetSpellCastingRequirements();
    return castReq ? castReq->FacingCasterFlags : 0;
}

uint32 SpellEntry::GetBaseLevel() const
{
    SpellLevelsEntry const* levels = GetSpellLevels();
    return levels ? levels->BaseLevel : 0;
}

uint32 SpellEntry::GetInterruptFlags() const
{
    SpellInterruptsEntry const* interrupt = GetSpellInterrupts();
    return interrupt ? interrupt->InterruptFlags : 0;
}

uint32 SpellEntry::GetTargetCreatureType() const
{
    SpellTargetRestrictionsEntry const* target = GetSpellTargetRestrictions();
    return target ? target->TargetCreatureType : 0;
}

int32 SpellEntry::GetEffectMiscValue(SpellEffectIndex index) const
{
    SpellEffectEntry const* effect = GetSpellEffect(index);
    return effect ? effect->EffectMiscValue : 0;
}

uint32 SpellEntry::GetStances() const
{
    SpellShapeshiftEntry const* ss = GetSpellShapeshift();
    return ss ? ss->ShapeshiftMask : 0;
}

uint32 SpellEntry::GetStancesNot() const
{
    SpellShapeshiftEntry const* ss = GetSpellShapeshift();
    return ss ? ss->ShapeshiftExclude : 0;
}

uint32 SpellEntry::GetProcFlags() const
{
    SpellAuraOptionsEntry const* aura = GetSpellAuraOptions();
    return aura ? aura->ProcTypeMask : 0;
}

uint32 SpellEntry::GetChannelInterruptFlags() const
{
    SpellInterruptsEntry const* interrupt = GetSpellInterrupts();
    return interrupt ? interrupt->ChannelInterruptFlags : 0;
}

uint32 SpellEntry::GetManaCostPerLevel() const
{
    SpellPowerEntry const* power = GetSpellPower();
    return power ? power->ManaCostPerLevel : 0;
}

uint32 SpellEntry::GetCasterAuraState() const
{
    SpellAuraRestrictionsEntry const* aura = GetSpellAuraRestrictions();
    return aura ? aura->CasterAuraState : 0;
}

uint32 SpellEntry::GetTargets() const
{
    SpellTargetRestrictionsEntry const* target = GetSpellTargetRestrictions();
    return target ? target->Targets : 0;
}

uint32 SpellEntry::GetEffectApplyAuraNameByIndex(SpellEffectIndex index) const
{
    SpellEffectEntry const* effect = GetSpellEffect(index);
    return effect ? effect->EffectAura : 0;
}
uint32 SpellEntry::GetAttributes() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->Attributes : 0;
}

uint32 SpellEntry::GetAttributesEx() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx : 0;
}

uint32 SpellEntry::GetAttributesEx2() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx2 : 0;
}

uint32 SpellEntry::GetAttributesEx3() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx3 : 0;
}

uint32 SpellEntry::GetAttributesEx4() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx4 : 0;
}

uint32 SpellEntry::GetAttributesEx5() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx5 : 0;
}

uint32 SpellEntry::GetAttributesEx6() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx6 : 0;
}

uint32 SpellEntry::GetAttributesEx7() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx7 : 0;
}

uint32 SpellEntry::GetAttributesEx8() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx8 : 0;
}

uint32 SpellEntry::GetAttributesEx9() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx9 : 0;
}

uint32 SpellEntry::GetAttributesEx10() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx10 : 0;
}

uint32 SpellEntry::GetAttributesEx11() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx11 : 0;
}

uint32 SpellEntry::GetAttributesEx12() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx12 : 0;
}

uint32 SpellEntry::GetAttributesEx13() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->AttributesEx13 : 0;
}

uint32 SpellEntry::GetCastingTimeIndex() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->CastingTimeIndex : 0;
}

uint32 SpellEntry::GetDurationIndex() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->DurationIndex : 0;
}

uint32 SpellEntry::GetRangeIndex() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->RangeIndex : 0;
}

float SpellEntry::GetSpeed() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->Speed : 0.0f;
}

uint32 SpellEntry::GetSpellVisual(int idx) const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->SpellVisual[idx] : 0;
}

uint32 SpellEntry::GetSpellIconID() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->SpellIconID : 0;
}

uint32 SpellEntry::GetActiveIconID() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->ActiveIconID : 0;
}

uint32 SpellEntry::GetSchoolMask() const
{
    SpellMiscEntry const* misc = GetSpellMisc();
    return misc ? misc->SchoolMask : 0;
}

uint32 SpellEntry::GetPowerType() const
{
    SpellPowerEntry const* power = GetSpellPower();
    return power ? power->PowerType : 0;
}

