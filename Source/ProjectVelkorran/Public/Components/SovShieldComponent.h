// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SovShieldComponent.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovShieldChangedSignature,
	float, OldShield,
	float, NewShield,
	float, MaxShield);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSovShieldBrokenSignature);

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

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Shield")
	FSovShieldChangedSignature OnShieldChanged;

	/** Fires once whenever Shield crosses from above zero to zero. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Shield")
	FSovShieldBrokenSignature OnShieldBroken;

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

private:
	void TryInitializeFromOwner();

	UFUNCTION()
	void HandleOwnerASCInitialized();

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

	bool CanWriteShield() const;
	float GetWorldTimeSeconds() const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle ShieldChangedDelegateHandle;
	FDelegateHandle MaxShieldChangedDelegateHandle;
	FDelegateHandle RechargeBlockedTagChangedDelegateHandle;

	FTimerHandle RechargeDelayTimerHandle;
	FTimerHandle RechargeTimerHandle;

	FGameplayTag ShieldBrokenTag;
	FGameplayTag RechargeBlockedTag;

	float LastShieldDamageWorldTime = 0.0f;
	float LastRechargeUpdateWorldTime = 0.0f;
	bool bHasRecordedShieldDamage = false;
	bool bRechargeDelayElapsed = false;
	bool bShieldBroken = false;
	bool bAppliedShieldBrokenTag = false;
	bool bWarnedMissingAttributeSet = false;
};
