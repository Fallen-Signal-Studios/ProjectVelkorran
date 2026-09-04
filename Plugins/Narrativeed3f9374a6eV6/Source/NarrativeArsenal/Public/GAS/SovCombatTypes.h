// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "SovCombatTypes.generated.h"

/** The action-state defense that intercepted a Sovereign damage transaction. */
UENUM(BlueprintType)
enum class ESovDefenseKind : uint8
{
	None UMETA(DisplayName = "None"),
	Guard UMETA(DisplayName = "Guard"),
	Deflection UMETA(DisplayName = "Deflection")
};

/** One authoritative, ordered result for a Sovereign damage transaction. */
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FSovDamageResult
{
	GENERATED_BODY()

	/** Unique identity for this resolved application, even when an Effect Context is reused. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGuid TransactionId;

	/** Authority-produced identity shared by one attack's victims. Invalid when no trusted receipt exists. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGuid AttackId;

	/** Periodic effect delivery (including its initial execution), captured from the authoritative spec. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bPeriodicDamage = false;

	/** Target state captured before Stamina, damage, break, death, or presentation callbacks. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayTagContainer TargetTagsBeforeDamage;

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

	/** True only when this transaction passed action-state defense for a status request. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bStatusApplicationRequested = false;

	/** Poise payload before Guard/Deflection and recovery-floor routing. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float RequestedPoiseDamage = 0.f;


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

	/** Requested Health damage beyond the target's remaining Health. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float HealthOverkillDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AppliedPoiseDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AppliedStaminaDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float ShieldBypassRatio = 0.f;

	/** Exact defense policy that intercepted the hit. None means no defense action succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	ESovDefenseKind DefenseKind = ESovDefenseKind::None;

	/** Legacy Guard compatibility flag. Deflection deliberately leaves this false. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bGuarded = false;

	/** Blueprint convenience mirror for DefenseKind == Deflection. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bDeflected = false;

	/** True for a correctly timed Guard or Deflection. Consult DefenseKind for the action. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bPerfectDefense = false;

	/** Stable source-policy mirror used to prevent Echo-spend damage from refunding Echo. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bFromEchoAbility = false;

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

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayEffectContextHandle EffectContext;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovDamageResolvedSignature,
	const FSovDamageResult&, Result);
