// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_DominionHound.generated.h"

/**
 * Instant damage shell shared by Dominion hound melee abilities.
 *
 * The ability owns hit context, damage channels, guard class, and SetByCaller
 * magnitudes. This class only supplies the lifetime policy and Narrative
 * damage-execution contract so Blueprint children may add presentation cues
 * without replacing authoritative combat behavior.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Dominion Hound Damage"))
class PROJECTVELKORRAN_API USovGameplayEffect_DominionHoundDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USovGameplayEffect_DominionHoundDamage();

	/** Exposes the invariant protected by the native automation contract. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Damage")
	bool UsesNarrativeDamageExecution() const;
};
