// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GAS/SovCombatTypes.h"
#include "Combat/SovNativeDamageReceipt.h"

class UAbilitySystemComponent;
struct FCollisionQueryParams;

/** Copied at release; projectiles never retain an instanced gameplay ability. */
struct PROJECTVELKORRAN_API FSovSelenePayloadContext
{
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
	TWeakObjectPtr<AActor> SourceAvatar;
	TWeakObjectPtr<UObject> SourceObject;
	FGameplayTag AbilityTag;
	float Level = 1.0f;
};

namespace SovSelenePayload
{
	PROJECTVELKORRAN_API bool ValidSource(const FSovSelenePayloadContext& Context);
	PROJECTVELKORRAN_API AActor* ResolveTarget(AActor* Actor);
	PROJECTVELKORRAN_API bool EligibleTarget(const FSovSelenePayloadContext& Context, AActor* Target);
	PROJECTVELKORRAN_API void IgnoreSource(FCollisionQueryParams& Params, AActor* Source);
	PROJECTVELKORRAN_API bool Aim(const FSovSelenePayloadContext& Context, FVector& Origin, FVector& Direction);
	PROJECTVELKORRAN_API bool Visible(const FSovSelenePayloadContext& Context, FVector Origin, AActor* Target, FVector Point);
	/** Returns status acceptance from the single resolved damage transaction. */
	PROJECTVELKORRAN_API bool Damage(const FSovSelenePayloadContext& Context, AActor* Target,
		const FHitResult* Hit, float BaseDamage, float Poise, float Scalar = 1.0f, bool bRequestControl = false);
	/** Area stasis explicitly bypasses guard/deflection; callers of weapon damage must use Damage's receipt. */
	PROJECTVELKORRAN_API bool Control(const FSovSelenePayloadContext& Context, AActor* Target,
		float Duration, float RefreezeLockout, bool bRequestFreeze);
	PROJECTVELKORRAN_API bool HasStatus(AActor* Target, FGameplayTag Tag);
	PROJECTVELKORRAN_API void FrostDOT(const FSovSelenePayloadContext& Context, AActor* Target,
		float DamagePerTick, float Duration, bool bFrozenOnly);
}
