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

/** Authoritative outcome returned by the project-owned status resolver. */
UENUM(BlueprintType)
enum class ESovStatusApplicationResult : uint8
{
	Applied,
	Refreshed,
	RejectedInvalidRequest,
	RejectedAuthority,
	RejectedDeadTarget,
	RejectedImmune,
	RejectedIneligibleTarget,
	RejectedWeakerExisting,
	RejectedDuplicate
};

/**
 * One exact status application request emitted after a Sovereign damage
 * transaction resolves. Damage-delivered requests reuse that transaction's
 * identity, making (RequestId, StatusTag) a stable deduplication key.
 */
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FSovStatusApplicationRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	FGuid RequestId;

	/** Exact Sov.Status.Apply.* tag requested by the source effect. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	FGameplayTag StatusTag;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	float Magnitude = 1.f;

	/** Zero delegates duration selection to the status definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	float Duration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	float EffectLevel = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	FGameplayEffectContextHandle Context;

	/**
	 * Filtered Sov.Ability.* source identity retained by periodic status
	 * payloads without copying damage channels or Status.Apply tags.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	FGameplayTagContainer SourceAbilityTags;

	/** True when this request was gated by positive applied damage or Poise. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	bool bRequiresAppliedDamage = false;
};

/** One authoritative, ordered result for a Sovereign damage transaction. */
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FSovDamageResult
{
	GENERATED_BODY()

	/** Unique identity for this resolved application, even when an Effect Context is reused. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGuid TransactionId;

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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovStatusApplicationRequestedSignature,
	const FSovStatusApplicationRequest&, Request);
