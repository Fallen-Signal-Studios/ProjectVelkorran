// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Effects/SovGameplayEffect_CinderWard.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
USovGameplayEffect_CinderWard::USovGameplayEffect_CinderWard()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat Duration; Duration.DataTag = FNarrativeGameplayTags::Get().SetByCaller_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
	FGameplayModifierInfo Resistance;
	Resistance.Attribute = UNarrativeAttributeSetBase::GetDamageResistanceAttribute();
	Resistance.ModifierOp = EGameplayModOp::Additive;
	Resistance.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(50.f));
	Modifiers.Add(Resistance);
	StackingType = EGameplayEffectStackingType::AggregateBySource;
	StackLimitCount = 1;
	StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
}
