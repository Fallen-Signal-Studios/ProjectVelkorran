// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_CinderGrenade.generated.h"

/**
 * Instant damage shell used by Cinder Sticky Grenade explosions.
 *
 * The projectile supplies damage, radial falloff, Poise pressure, channels,
 * and guard classification through the outgoing spec. This class only owns
 * the lifetime policy and Narrative damage execution contract.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Cinder Grenade Explosion Damage"))
class PROJECTVELKORRAN_API USovGameplayEffect_CinderGrenadeExplosionDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USovGameplayEffect_CinderGrenadeExplosionDamage();
};

/**
 * Duration/periodic damage shell used by Cinder Sticky Grenade Burn.
 *
 * Duration and tick damage are supplied by SetByCaller magnitudes. Burn ticks
 * once per second, do not execute immediately on application, and refresh
 * rather than stack when reapplied by the same source.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Cinder Grenade Burn"))
class PROJECTVELKORRAN_API USovGameplayEffect_CinderGrenadeBurn : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USovGameplayEffect_CinderGrenadeBurn();
};
