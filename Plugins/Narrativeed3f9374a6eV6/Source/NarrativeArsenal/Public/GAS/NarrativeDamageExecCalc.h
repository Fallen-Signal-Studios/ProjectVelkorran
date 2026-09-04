// Copyright Narrative Tools 2024.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "NarrativeDamageExecCalc.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectSpec;

/**
 * Resolves raw attack power into one final incoming-damage value.
 *
 * Shield and Health routing deliberately happens in UNarrativeAttributeSetBase so
 * each hit produces exactly one authoritative damage notification, including hits
 * that break Shield and overflow into Health.
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeDamageExecCalc : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	/** Shared transaction gate, also used by direct meta-attribute effects. */
	static bool ShouldRejectTransaction(const UAbilitySystemComponent* SourceASC, const UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& Spec);

	UNarrativeDamageExecCalc();

	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
