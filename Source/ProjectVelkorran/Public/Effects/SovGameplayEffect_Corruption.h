// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_Corruption.generated.h"

/**
 * Infinite shell used for exactly one component-owned corruption-band handle.
 * The component supplies the current band and umbrella tags dynamically on the
 * outgoing spec, then removes this exact handle when the band changes.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Corruption Band State"))
class PROJECTVELKORRAN_API USovGameplayEffect_CorruptionBandState : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USovGameplayEffect_CorruptionBandState();
};
