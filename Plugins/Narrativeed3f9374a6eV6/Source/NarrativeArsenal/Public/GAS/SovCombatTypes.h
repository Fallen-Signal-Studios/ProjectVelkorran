// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "SovCombatTypes.generated.h"

/** One authoritative, ordered result for a Sovereign damage transaction. */
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FSovDamageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayTagContainer DamageChannels;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayTagContainer AttackClassifications;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayTagContainer RequestedStatusTags;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FName HitZone = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float BaseDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float ResolvedDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float RequestedShieldDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AppliedShieldDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AppliedHealthDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AppliedPoiseDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AppliedStaminaDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float ShieldBypassRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bGuarded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bPerfectDefense = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bGuardBroken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bShieldWasTargeted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bShieldBroken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bPoiseBroken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bFatal = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bShouldRestartShieldRecharge = false;

	FGameplayEffectContextHandle EffectContext;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovDamageResolvedSignature,
	const FSovDamageResult&, Result);
