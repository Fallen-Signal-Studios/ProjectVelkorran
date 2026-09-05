// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Effects/SovGameplayEffect_AxiomNullPulse.h"

#include "GAS/NarrativeDamageExecCalc.h"
#include "NarrativeGameplayTags.h"

namespace
{
	void ConfigureAxiomDuration(UGameplayEffect& Effect)
	{
		Effect.DurationPolicy = EGameplayEffectDurationType::HasDuration;
		FSetByCallerFloat Duration;
		Duration.DataTag = FNarrativeGameplayTags::Get().SetByCaller_Duration;
		Effect.DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
		Effect.Period = FScalableFloat(0.0f);
		Effect.bExecutePeriodicEffectOnApplication = false;
		// Every cast owns its own count. A short pulse cannot replace a long one.
		Effect.StackingType = EGameplayEffectStackingType::None;
	}
}

USovGameplayEffect_AxiomNullPulseDamage::USovGameplayEffect_AxiomNullPulseDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UNarrativeDamageExecCalc::StaticClass();
	Executions.Add(Execution);
}

USovGameplayEffect_AxiomShieldSuppression::USovGameplayEffect_AxiomShieldSuppression()
{
	ConfigureAxiomDuration(*this);
}

USovGameplayEffect_AxiomDeviceDisable::USovGameplayEffect_AxiomDeviceDisable()
{
	ConfigureAxiomDuration(*this);
}
