// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_CinderWard.generated.h"
/** A short source-owned resistance ward. Does not mutate current/max Shield. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API USovGameplayEffect_CinderWard : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_CinderWard();
};
