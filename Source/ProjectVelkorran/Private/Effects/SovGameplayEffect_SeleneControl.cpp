// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Effects/SovGameplayEffect_SeleneControl.h"
#include "AbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

USovGameplayEffect_SeleneDamage::USovGameplayEffect_SeleneDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UNarrativeDamageExecCalc::StaticClass();
	Executions.Add(Execution);
}
USovGameplayEffect_SeleneControl::USovGameplayEffect_SeleneControl()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	FSetByCallerFloat Duration;
	Duration.DataTag = FNarrativeGameplayTags::Get().SetByCaller_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(Duration);
	StackingType = EGameplayEffectStackingType::None;
	bExecutePeriodicEffectOnApplication = false;
}
USovGameplayEffect_SeleneFrostDOT::USovGameplayEffect_SeleneFrostDOT()
{
	Period = FScalableFloat(1.0f);
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UNarrativeDamageExecCalc::StaticClass();
	Executions.Add(Execution);
}
USovGameplayEffect_SeleneFrozenDOT::USovGameplayEffect_SeleneFrozenDOT()
{
	Period = FScalableFloat(1.0f);
	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = USovSeleneFrozenDamageExecCalc::StaticClass();
	Executions.Add(Execution);
}
void USovSeleneFrozenDamageExecCalc::Execute_Implementation(
	const FGameplayEffectCustomExecutionParameters& Params, FGameplayEffectCustomExecutionOutput& Output) const
{
	const UAbilitySystemComponent* Target = Params.GetTargetAbilitySystemComponent();
	if (IsValid(Target) && Target->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Status_Frozen))
	{
		Super::Execute_Implementation(Params, Output);
	}
}
