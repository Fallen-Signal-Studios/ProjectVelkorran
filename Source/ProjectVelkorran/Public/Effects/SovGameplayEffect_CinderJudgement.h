// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_CinderJudgement.generated.h"

/**
 * Native instant damage shell for Cinder Judgement's impact and blast packets.
 *
 * The ability supplies the authoritative hit/origin, channels, guard class,
 * Shield pressure, Poise pressure, and radial falloff. Blueprint children may
 * add per-target cues without replacing the Narrative damage transaction.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Cinder Judgement Damage"))
class PROJECTVELKORRAN_API USovGameplayEffect_CinderJudgementDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USovGameplayEffect_CinderJudgementDamage();
};
