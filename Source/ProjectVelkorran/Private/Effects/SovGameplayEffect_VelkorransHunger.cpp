// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Effects/SovGameplayEffect_VelkorransHunger.h"

#include "GAS/NarrativeDamageExecCalc.h"

USovGameplayEffect_VelkorransHungerDamage::
	USovGameplayEffect_VelkorransHungerDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition DamageExecution;
	DamageExecution.CalculationClass = UNarrativeDamageExecCalc::StaticClass();
	Executions.Add(DamageExecution);
}
