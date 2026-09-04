// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Effects/SovGameplayEffect_CinderGrenade.h"

#include "GAS/NarrativeDamageExecCalc.h"
#include "NarrativeGameplayTags.h"

namespace
{
	FGameplayEffectExecutionDefinition MakeNarrativeDamageExecution()
	{
		FGameplayEffectExecutionDefinition Execution;
		Execution.CalculationClass = UNarrativeDamageExecCalc::StaticClass();
		return Execution;
	}
}

USovGameplayEffect_CinderGrenadeExplosionDamage::
	USovGameplayEffect_CinderGrenadeExplosionDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	Executions.Add(MakeNarrativeDamageExecution());
}

USovGameplayEffect_CinderGrenadeBurn::USovGameplayEffect_CinderGrenadeBurn()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat DurationSetByCaller;
	DurationSetByCaller.DataTag = FNarrativeGameplayTags::Get().SetByCaller_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);

	Period = FScalableFloat(1.0f);
	bExecutePeriodicEffectOnApplication = false;
	Executions.Add(MakeNarrativeDamageExecution());

	// USovStatusComponent owns canonical Burn reapplication and exact effect
	// replacement. Keeping stacking policy here would create a second source of
	// truth and uses the UGameplayEffect::StackingType API deprecated in UE 5.7.
}
