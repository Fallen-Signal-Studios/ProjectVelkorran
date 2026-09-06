// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SovShieldComponent.generated.h"

class UAbilitySystemComponent;
class UNarrativeAttributeSetBase;
class UNarrativeAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UNiagaraSystem;
class ANarrativeCharacter;
class ANarrativeCharacterVisual;
struct FOnAttributeChangeData;

/** One mesh temporarily owned by the character's shield overlay. */
USTRUCT()
struct PROJECTVELKORRAN_API FSovShieldOverlayBinding
{
	GENERATED_BODY()

	/** Weak because Narrative can replace the entire CharacterVisual at runtime. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMeshComponent> MeshComponent;

	/** Restored when the shield overlay releases this mesh. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PreviousOverlayMaterial;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovShieldChangedSignature,
	float, OldShield,
	float, NewShield,
	float, MaxShield);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSovShieldBrokenSignature);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovShieldVisualScalarChangedSignature,
	float, ShieldVisualScalar);

/**
 * Project-owned lifecycle controller for the regenerating Shield resource.
 *
 * UNarrativeAttributeSetBase remains authoritative storage and damage routing.
 * This component observes replicated Shield changes, owns the server recharge
 * schedule, and exposes a Blueprint-facing shield-break event. It never ticks:
 * one-shot and repeating world timers drive recharge timing.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovShieldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovShieldComponent();

	/**
	 * Binds to an ASC that owns UNarrativeAttributeSetBase. BeginPlay resolves the
	 * owner's ASC once as a compatibility fallback. Narrative character readiness
	 * supplies it explicitly, without polling.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Shield")
	bool InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

	/** Native restore seam: discard prior-attempt timers; preserve other systems' tag counts. */
	void ResetForCheckpoint();
	void SetCheckpointRestoreInProgress(bool bInProgress, bool bResumePassiveWork = true);
	bool IsCheckpointStateReconciled() const { return bCheckpointStateReconciled && IsInitialized(); }
	uint64 GetCheckpointRestoreGeneration() const { return CheckpointRestoreGeneration; }
	bool EndCheckpointRestore(uint64 Generation, bool bResumePassiveWork);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Shield")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Shield")
	float GetShield() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Shield")
	float GetMaxShield() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Shield")
	bool IsShieldBroken() const { return bShieldBroken; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Shield")
	bool IsRechargeBlocked() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Shield")
	bool IsRecharging() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Shield")
	float GetSecondsUntilRecharge() const;

	/** Current material scalar derived from Shield depletion. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Shield|Presentation")
	float GetShieldVisualScalar() const { return CurrentShieldVisualScalar; }

	/**
	 * Re-scan the owner and Narrative's runtime CharacterVisual. When a Shield
	 * Overlay Material is assigned, it is applied to every discovered character
	 * mesh. Otherwise, legacy material slots exposing ShieldScalarParameterName
	 * are discovered and driven in place.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Shield|Presentation")
	void RefreshShieldVisuals();

	/** Explicitly applies the configured shield overlay to a custom runtime mesh. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Shield|Presentation")
	bool RegisterShieldOverlayTarget(UMeshComponent* MeshComponent);

	/** Explicit registration path for runtime-spawned meshes or custom visuals. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Shield|Presentation")
	bool RegisterShieldMaterialTarget(UMeshComponent* MeshComponent, int32 MaterialIndex);

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Shield")
	FSovShieldChangedSignature OnShieldChanged;

	/** Fires once whenever Shield crosses from above zero to zero. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Shield")
	FSovShieldBrokenSignature OnShieldBroken;

	/** Fires when the depletion-derived material scalar changes. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Shield|Presentation")
	FSovShieldVisualScalarChangedSignature OnShieldVisualScalarChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Seconds after the latest Shield decrease before recharge may begin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Shield|Tuning", meta = (ClampMin = "0.0"))
	float RechargeDelay = 3.0f;

	/** Fraction of MaxShield restored each second. 0.05 means five percent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Shield|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RechargePercentPerSecond = 0.05f;

	/** Timer cadence used to sample continuous recharge; this is not an Actor tick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Shield|Tuning", meta = (ClampMin = "0.01"))
	float RechargeTimerInterval = 0.1f;

	/** Scalar parameter authored on any shield-reactive material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	FName ShieldScalarParameterName = TEXT("ShieldIntensity");

	/**
	 * Material rendered over Narrative's runtime-built character meshes. The
	 * component creates one per-character MID and drives ShieldScalarParameterName.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	TObjectPtr<UMaterialInterface> ShieldOverlayMaterial;

	/** Automatically apply ShieldOverlayMaterial after Narrative builds its appearance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	bool bAutoApplyShieldOverlayMaterial = true;

	/**
	 * Unreal provides one global overlay channel per mesh. When enabled, the
	 * shield temporarily replaces an existing overlay and restores it afterward.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	bool bOverrideExistingOverlayMaterials = true;

	/** Hide the overlay at zero Shield; it is restored automatically on recharge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	bool bHideShieldOverlayWhenBroken = true;

	/**
	 * Legacy fallback used only when ShieldOverlayMaterial is unassigned. Finds
	 * already-applied materials that expose ShieldScalarParameterName.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	bool bAutoDiscoverShieldMaterials = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	float FullShieldScalar = 0.0f;

	/** Value approached as Shield nears zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	float NearBreakShieldScalar = 1.0f;

	/** Value used after Shield reaches zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	float BrokenShieldScalar = 1.0f;

	/** Shapes the depletion response. One is linear; values above one bias toward break. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation", meta = (ClampMin = "0.01"))
	float ShieldScalarResponseExponent = 1.0f;

	/** Per-component shield-break burst. Null disables automatic Niagara playback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	TObjectPtr<UNiagaraSystem> ShieldBreakSystem;

	/** Local transform composed with the owning character transform for the break burst. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Shield|Presentation")
	FTransform ShieldBreakRelativeTransform = FTransform::Identity;

private:
	bool bRestoringCheckpoint = false;
	bool bCheckpointStateReconciled = false;
	uint64 CheckpointRestoreGeneration = 0;
	bool bChangingAbilitySystem = false;
	bool bEndingPlay = false;
	uint64 BindingGeneration = 0;
	uint64 BoundActorInfoEpoch = 0;
	int32 BoundReadyEpoch = 0;
	UFUNCTION()
	void HandleOwnerReadyEpochChanged(int32 ReadyEpoch);
	uint64 BoundLifeEpoch = 0;
	TWeakObjectPtr<const UNarrativeAttributeSetBase> BoundAttributes;
	bool IsCurrentOperation(uint64 Generation) const;
	bool ValidateBindingOrRetire();
	void HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	UFUNCTION()
	void HandleDeathStateChanged(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bDead);
	FTimerHandle ReviveRebindTimerHandle;
	uint64 ReviveRebindGeneration = 0;
	void TryInitializeFromOwner();

	UFUNCTION()
	void HandleOwnerASCInitialized();

	UFUNCTION()
	void HandleCharacterVisualInitialized(ANarrativeCharacter* Character);

	UFUNCTION()
	void HandleBaseAppearanceApplied();

	void UninitializeFromAbilitySystem();
	void ClearLifecycleTimers();

	void HandleShieldAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxShieldAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleRechargeBlockedTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UFUNCTION()
	void HandleDamageResolved(const FSovDamageResult& Result);

	void RecordShieldDamage();
	void ScheduleRechargeDelay(float DelaySeconds);
	void HandleRechargeDelayElapsed();
	void TryStartRecharge();
	void HandleRechargeTimerElapsed();
	void StopRecharge();

	void RefreshShieldBrokenState(float CurrentShield, bool bBroadcastBreak);
	void ApplyShieldBrokenTag();
	void RemoveShieldBrokenTag();
	void BindCharacterVisual(ANarrativeCharacterVisual* NewCharacterVisual);
	void ScheduleShieldVisualRefresh();
	void HandleDeferredShieldVisualRefresh();
	bool PrepareShieldOverlayMaterial();
	bool ShouldDisplayShieldOverlay() const;
	void ReconcileShieldOverlayVisibility();
	void DiscoverShieldOverlayTargets();
	void PruneShieldOverlayBindings();
	void ClearShieldOverlayTargets();
	void PruneShieldMaterialInstances();
	void UpdateShieldVisualScalar();
	void DiscoverShieldMaterialTargets();
	void SpawnShieldBreakSystem() const;
	bool MaterialExposesShieldScalar(const class UMaterialInterface* Material) const;

	bool CanWriteShield() const;
	float GetWorldTimeSeconds() const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<ANarrativeCharacterVisual> BoundCharacterVisual;

	/** Shared by all of this character's runtime appearance meshes. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShieldOverlayMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> AppliedShieldOverlayMaterial;

	UPROPERTY(Transient)
	TArray<FSovShieldOverlayBinding> ShieldOverlayBindings;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ShieldMaterialInstances;

	FDelegateHandle ShieldChangedDelegateHandle;
	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxShieldChangedDelegateHandle;
	FDelegateHandle RechargeBlockedTagChangedDelegateHandle;

	FTimerHandle RechargeDelayTimerHandle;
	FTimerHandle RechargeTimerHandle;
	FTimerHandle ShieldVisualRefreshTimerHandle;

	FGameplayTag ShieldBrokenTag;
	FGameplayTag RechargeBlockedTag;

	float LastShieldDamageWorldTime = 0.0f;
	float LastRechargeUpdateWorldTime = 0.0f;
	bool bHasRecordedShieldDamage = false;
	bool bRechargeDelayElapsed = false;
	bool bShieldBroken = false;
	bool bAppliedShieldBrokenTag = false;
	bool bWarnedMissingAttributeSet = false;
	bool bWarnedInvalidShieldOverlayMaterial = false;
	bool bShieldVisualRefreshPending = false;
	FName AppliedShieldScalarParameterName;
	float CurrentShieldVisualScalar = 0.0f;
};
