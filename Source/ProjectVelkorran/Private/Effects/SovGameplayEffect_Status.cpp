// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Effects/SovGameplayEffect_Status.h"

#include "Sovereign/SovGameplayTags.h"

USovGameplayEffect_Status::USovGameplayEffect_Status()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat DurationSetByCaller;
	DurationSetByCaller.DataTag =
		FSovGameplayTags::Get().SetByCaller_Status_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);
}

USovGameplayEffect_StatusInfinite::USovGameplayEffect_StatusInfinite()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}
