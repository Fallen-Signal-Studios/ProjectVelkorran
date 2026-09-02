// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Effects/SovGameplayEffect_DominionHound.h"

#include "GAS/NarrativeDamageExecCalc.h"

USovGameplayEffect_DominionHoundDamage::USovGameplayEffect_DominionHoundDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition DamageExecution;
	DamageExecution.CalculationClass = UNarrativeDamageExecCalc::StaticClass();
	Executions.Add(DamageExecution);
}

bool USovGameplayEffect_DominionHoundDamage::
	UsesNarrativeDamageExecution() const
{
	return DurationPolicy == EGameplayEffectDurationType::Instant
		&& Executions.Num() == 1
		&& Executions[0].CalculationClass.Get()
			== UNarrativeDamageExecCalc::StaticClass();
}
