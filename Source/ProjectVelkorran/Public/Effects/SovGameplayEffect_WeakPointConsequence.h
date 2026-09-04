// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_WeakPointConsequence.generated.h"

/** Per-zone owned tag lifetime. Ability block counts are bound to the same active handle. */
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_WeakPointTimedConsequence : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_WeakPointTimedConsequence();
};

UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_WeakPointPermanentConsequence : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_WeakPointPermanentConsequence();
};
