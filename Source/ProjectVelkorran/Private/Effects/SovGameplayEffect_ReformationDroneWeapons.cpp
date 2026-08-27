// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Effects/SovGameplayEffect_ReformationDroneWeapons.h"

#include "GAS/NarrativeDamageExecCalc.h"

USovGameplayEffect_ReformationDroneDamage::
	USovGameplayEffect_ReformationDroneDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition DamageExecution;
	DamageExecution.CalculationClass = UNarrativeDamageExecCalc::StaticClass();
	Executions.Add(DamageExecution);
}
