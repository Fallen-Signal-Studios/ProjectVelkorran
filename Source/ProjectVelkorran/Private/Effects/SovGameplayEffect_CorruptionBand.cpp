// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Effects/SovGameplayEffect_CorruptionBand.h"
#include "GAS/NarrativeAttributeSetBase.h"
USovGameplayEffect_CorruptionBand::USovGameplayEffect_CorruptionBand()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	StackingType = EGameplayEffectStackingType::None;
	bExecutePeriodicEffectOnApplication = false;
	FGameplayModifierInfo Vulnerability;
	Vulnerability.Attribute = UNarrativeAttributeSetBase::GetDamageResistanceAttribute();
	Vulnerability.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Resistance; Resistance.DataName = TEXT("Corruption.Resistance");
	Vulnerability.ModifierMagnitude = FGameplayEffectModifierMagnitude(Resistance);
	Modifiers.Add(Vulnerability);
	FGameplayModifierInfo Recovery;
	Recovery.Attribute = UNarrativeAttributeSetBase::GetStaminaRegenRateAttribute();
	Recovery.ModifierOp = EGameplayModOp::Multiplicitive;
	FSetByCallerFloat Regen; Regen.DataName = TEXT("Corruption.StaminaRegenScale");
	Recovery.ModifierMagnitude = FGameplayEffectModifierMagnitude(Regen);
	Modifiers.Add(Recovery);
}
