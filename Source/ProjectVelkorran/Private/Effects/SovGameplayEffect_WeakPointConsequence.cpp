// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Effects/SovGameplayEffect_WeakPointConsequence.h"
#include "NarrativeGameplayTags.h"
USovGameplayEffect_WeakPointTimedConsequence::USovGameplayEffect_WeakPointTimedConsequence()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat Duration;
	Duration.DataTag = FNarrativeGameplayTags::Get().SetByCaller_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
	StackingType = EGameplayEffectStackingType::None;
}
USovGameplayEffect_WeakPointPermanentConsequence::USovGameplayEffect_WeakPointPermanentConsequence()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	StackingType = EGameplayEffectStackingType::None;
}
