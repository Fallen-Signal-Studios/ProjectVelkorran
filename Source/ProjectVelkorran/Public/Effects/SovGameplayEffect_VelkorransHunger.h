// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_VelkorransHunger.generated.h"

/**
 * Instant direct-damage shell used by Velkorran's Hunger.
 *
 * The projectile supplies damage, Poise pressure, Edge/Thermal channels, and
 * guard classification through the outgoing spec. This class owns only the
 * lifetime policy and Narrative damage execution contract.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Velkorran's Hunger Damage"))
class PROJECTVELKORRAN_API USovGameplayEffect_VelkorransHungerDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USovGameplayEffect_VelkorransHungerDamage();
};
