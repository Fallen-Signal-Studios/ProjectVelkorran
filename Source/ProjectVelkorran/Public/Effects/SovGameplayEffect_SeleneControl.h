// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GAS/NarrativeDamageExecCalc.h"
#include "SovGameplayEffect_SeleneControl.generated.h"

/** Known native shells: serialized legacy GE overrides are never executed by native payloads. */
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_SeleneDamage : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_SeleneDamage();
};
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_SeleneControl : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_SeleneControl();
};
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_SeleneFrostDOT : public USovGameplayEffect_SeleneControl
{
	GENERATED_BODY()
public:
	USovGameplayEffect_SeleneFrostDOT();
};
/** The execution, not a cosmetic listener, gates every frozen-only tick. */
UCLASS()
class PROJECTVELKORRAN_API USovSeleneFrozenDamageExecCalc : public UNarrativeDamageExecCalc
{
	GENERATED_BODY()
public:
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Params,
		FGameplayEffectCustomExecutionOutput& Output) const override;
};
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_SeleneFrozenDOT : public USovGameplayEffect_SeleneControl
{
	GENERATED_BODY()
public:
	USovGameplayEffect_SeleneFrozenDOT();
};
