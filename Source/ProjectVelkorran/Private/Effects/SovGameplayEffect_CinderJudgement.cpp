// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Effects/SovGameplayEffect_CinderJudgement.h"

#include "GAS/NarrativeDamageExecCalc.h"

USovGameplayEffect_CinderJudgementDamage::
	USovGameplayEffect_CinderJudgementDamage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition DamageExecution;
	DamageExecution.CalculationClass = UNarrativeDamageExecCalc::StaticClass();
	Executions.Add(DamageExecution);
}
