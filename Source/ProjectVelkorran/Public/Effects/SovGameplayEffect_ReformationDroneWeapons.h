// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_ReformationDroneWeapons.generated.h"

/**
 * Instant damage shell shared by the Reformation drone's integral weapons.
 *
 * The ability or projectile owns hit context, damage channels, guard class,
 * falloff, and SetByCaller magnitudes. This class deliberately contains only
 * the lifetime policy and Narrative damage-execution contract so a Blueprint
 * child can add cues without replacing authoritative combat behavior.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Reformation Drone Weapon Damage"))
class PROJECTVELKORRAN_API USovGameplayEffect_ReformationDroneDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USovGameplayEffect_ReformationDroneDamage();
};
