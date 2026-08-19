// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SovShieldComponent.generated.h"

class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UNiagaraSystem;
class ANarrativeCharacter;
class ANarrativeCharacterVisual;
struct FOnAttributeChangeData;

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
	 * Re-scan the owner, Narrative character visual, and attached visual actors
	 * for material slots that expose ShieldScalarParameterName.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Shield|Presentation")
	void RefreshShieldVisuals();

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

	/** Automatically find matching materials on the pawn and Narrative visual actors. */
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

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ShieldMaterialInstances;

	FDelegateHandle ShieldChangedDelegateHandle;
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
	bool bShieldVisualRefreshPending = false;
	float CurrentShieldVisualScalar = 0.0f;
};
