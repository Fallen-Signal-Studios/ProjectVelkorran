// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "NarrativeHealExecCalc.generated.h"


class UObject;


/**
* Heal technique used by lyra - similar to damage effect 
 */
UCLASS()
class UNarrativeHealExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:

	UNarrativeHealExecution();

protected:

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
