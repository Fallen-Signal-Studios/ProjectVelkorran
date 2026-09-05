// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "GameplayTagContainer.h"
#include "SovStatusDefinition.generated.h"

class UGameplayEffect;
class UNiagaraSystem;
class USoundBase;

/** How long an applied status remains authoritative. */
UENUM(BlueprintType)
enum class ESovStatusDurationPolicy : uint8
{
	Timed UMETA(DisplayName = "Timed"),
	Infinite UMETA(DisplayName = "Infinite")
};

/** Deterministic behavior when a target already owns this status. */
UENUM(BlueprintType)
enum class ESovStatusReapplyPolicy : uint8
{
	Reject UMETA(DisplayName = "Reject Duplicate"),
	RefreshDuration UMETA(DisplayName = "Refresh Duration"),
	AddStack UMETA(DisplayName = "Add Stack"),
	ReplaceIfStronger UMETA(DisplayName = "Replace If Stronger")
};

/** Semantic checkpoint behavior; runtime effect handles are never serialized. */
UENUM(BlueprintType)
enum class ESovStatusCheckpointBehavior : uint8
{
	ClearOnCheckpoint UMETA(DisplayName = "Clear On Checkpoint"),
	PersistRemainingDuration UMETA(DisplayName = "Persist Remaining Duration"),
	PersistFullDuration UMETA(DisplayName = "Persist Full Duration")
};

/**
 * Data-owned contract for one status family.
 *
 * The component owns application, stacking, cleansing, and persistence. The
 * Gameplay Effect owns the actual GAS modifiers while this definition owns
 * the semantic rules required by the campaign TDD.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Sovereign Status Definition"))
class PROJECTVELKORRAN_API USovStatusDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Status")
	bool IsStructurallyValid() const;

	/** Schema for checkpoint and editor migration. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status", meta = (ClampMin = "1"))
	int32 SchemaVersion = 1;

	/** Exact Sov.Status.Apply.* request tag routed by the damage/status contract. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status", meta = (Categories = "Sov.Status.Apply"))
	FGameplayTag RequestTag;

	/** GAS-owned state tag that represents this status while active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status", meta = (Categories = "Sov.State.Status"))
	FGameplayTag StateTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Duration")
	ESovStatusDurationPolicy DurationPolicy = ESovStatusDurationPolicy::Timed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Duration", meta = (ClampMin = "0.0", ForceUnits = "s", EditCondition = "DurationPolicy == ESovStatusDurationPolicy::Timed"))
	float DefaultDuration = 1.0f;

	/** Informational period for authored periodic Gameplay Effects. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Duration", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float Period = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Stacking", meta = (ClampMin = "1"))
	int32 MaximumStacks = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Stacking")
	ESovStatusReapplyPolicy ReapplyPolicy = ESovStatusReapplyPolicy::RefreshDuration;

	/** Any matching tag rejects the request without changing the current status. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Defense")
	FGameplayTagContainer ImmunityTags;

	/** Matching resistance scales duration and magnitude instead of silently acting as immunity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Defense")
	FGameplayTagContainer ResistanceTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ResistantDurationMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ResistantMagnitudeMultiplier = 0.5f;

	/** Required exact cleanse tags that may remove this status. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Cleanse", meta = (Categories = "Sov.Status.Cleanse"))
	FGameplayTagContainer CleanseTags;

	/** Optional authored target requirements beyond the built-in device gate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Targeting")
	FGameplayTagContainer RequiredTargetTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Targeting")
	FGameplayTagContainer BlockedTargetTags;

	/** Device Disabled definitions opt into this explicit component-owned eligibility gate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Targeting")
	bool bRequiresDeviceEligibility = false;

	/** Tags granted by the active GE in addition to StateTag (movement/ability constraints, for example). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|AI and Animation")
	FGameplayTagContainer GrantedConstraintTags;

	/** Semantic hooks consumed by AI without hard-coding a concrete StateTree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|AI and Animation")
	FGameplayTagContainer AIReactionTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|AI and Animation")
	FGameplayTagContainer AnimationConstraintTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Hard Control")
	bool bHardCrowdControl = false;

	/** Applied through a separate, exactly tracked GE after expiry or cleanse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Hard Control", meta = (EditCondition = "bHardCrowdControl"))
	FGameplayTag RecoveryImmunityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Hard Control", meta = (ClampMin = "0.0", ClampMax = "2.5", ForceUnits = "s", EditCondition = "bHardCrowdControl"))
	float RecoveryImmunityDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|UI", meta = (ClampMin = "1"))
	int32 UIPriority = 0;

	/** Presentation semantics; gameplay never depends on these tags or assets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	FGameplayTag PresentationTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	FGameplayTag AccessibilityPresentationTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	TSoftObjectPtr<UNiagaraSystem> LoopingVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	TSoftObjectPtr<USoundBase> LoopingAudio;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Checkpoint")
	ESovStatusCheckpointBehavior CheckpointBehavior = ESovStatusCheckpointBehavior::ClearOnCheckpoint;

	/** Gameplay state/modifiers applied by GAS. A safe native shell is used when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Gameplay Effect")
	TSubclassOf<UGameplayEffect> EffectClass;
};
