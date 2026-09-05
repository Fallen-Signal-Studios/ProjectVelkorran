// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_CorruptionBand.generated.h"

/** Owns a single exposure-state tag; never changes input, camera, damage, or story state. */
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_CorruptionBand : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovGameplayEffect_CorruptionBand();
};
