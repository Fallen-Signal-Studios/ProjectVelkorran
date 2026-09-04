// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Effects/SovGameplayEffect_CorruptionBand.h"
USovGameplayEffect_CorruptionBand::USovGameplayEffect_CorruptionBand()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	StackingType = EGameplayEffectStackingType::None;
	bExecutePeriodicEffectOnApplication = false;
}
