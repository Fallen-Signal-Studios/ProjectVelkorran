// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SovPoiseComponent.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

UENUM(BlueprintType)
enum class ESovPoiseState : uint8
{
	Stable UMETA(DisplayName = "Stable"),
	Pressured UMETA(DisplayName = "Pressured"),
	Broken UMETA(DisplayName = "Broken"),
	Recovering UMETA(DisplayName = "Recovering")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovPoiseChangedSignature,
	float, OldPoise,
	float, NewPoise,
	float, MaxPoise);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSovPoiseStateChangedSignature,
	ESovPoiseState, PreviousState,
	ESovPoiseState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSovPoiseBrokenSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSovPoiseRecoveredSignature);

/**
 * Project-owned lifecycle controller for Poise and stagger resistance.
 *
 * UNarrativeAttributeSetBase remains authoritative storage and consumes the
 * transient PoiseDamage meta attribute. This component observes replicated
 * Poise changes, owns regeneration and break-recovery timing on the server,
 * publishes semantic state tags, and exposes Blueprint-facing events.
 * It never ticks; world timers drive lifecycle updates.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovPoiseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovPoiseComponent();

	/**
	 * Binds to an ASC that owns UNarrativeAttributeSetBase. BeginPlay resolves the
	 * owner's ASC once as a compatibility fallback. Narrative readiness supplies
	 * it explicitly without polling.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Poise")
	bool InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	float GetPoise() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	float GetMaxPoise() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	ESovPoiseState GetPoiseState() const { return PoiseState; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	bool IsPoiseBroken() const { return PoiseState == ESovPoiseState::Broken; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	bool IsPoiseRecovering() const { return PoiseState == ESovPoiseState::Recovering; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	bool IsPoisePressured() const { return PoiseState == ESovPoiseState::Pressured; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	bool IsRegenerationBlocked() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	bool IsRegenerating() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	float GetSecondsUntilRegeneration() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	float GetSecondsUntilBreakRecovery() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Poise")
	float GetSecondsUntilRecoveryComplete() const;

	/**
	 * Completes a Broken reaction, refills Poise, and begins the Recovering window.
	 * Animation or ability logic may call this when its authored reaction ends; the
	 * fallback timer calls it automatically if no explicit completion arrives.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Poise")
	bool RecoverFromPoiseBreak();

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Poise")
	FSovPoiseChangedSignature OnPoiseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Poise")
	FSovPoiseStateChangedSignature OnPoiseStateChanged;

	/** Fires once whenever Poise crosses from above zero to zero. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Poise")
	FSovPoiseBrokenSignature OnPoiseBroken;

	/** Fires when a Broken reaction resets Poise and enters Recovering. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Poise")
	FSovPoiseRecoveredSignature OnPoiseRecovered;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Seconds without Poise loss before regeneration may begin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Poise|Tuning", meta = (ClampMin = "0.0"))
	float RegenerationDelay = 2.0f;

	/** Fraction of MaxPoise restored each second. 0.20 means twenty percent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Poise|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RegenerationPercentPerSecond = 0.20f;

	/** Timer cadence used to sample continuous regeneration; this is not an Actor tick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Poise|Tuning", meta = (ClampMin = "0.01"))
	float RegenerationTimerInterval = 0.1f;

	/** At or below this fraction of MaxPoise, a non-broken character is Pressured. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Poise|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PressuredThresholdPercent = 0.50f;

	/** Safety duration before a Broken reaction automatically resolves. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Poise|Tuning", meta = (ClampMin = "0.0"))
	float BrokenFallbackDuration = 0.8f;

	/** Post-break window that prevents Poise from reaching zero again. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Poise|Tuning", meta = (ClampMin = "0.0"))
	float RecoveryImmunityDuration = 1.5f;

private:
	void TryInitializeFromOwner();

	UFUNCTION()
	void HandleOwnerASCInitialized();

	void UninitializeFromAbilitySystem();
	void ClearLifecycleTimers();

	void HandlePoiseAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxPoiseAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleRegenerationBlockedTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleReplicatedStateTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	void RecordPoiseDamage();
	void ScheduleRegenerationDelay(float DelaySeconds);
	void HandleRegenerationDelayElapsed();
	void TryStartRegeneration();
	void HandleRegenerationTimerElapsed();
	void StopRegeneration();

	void EnterBrokenState(bool bBroadcastChanges);
	void ScheduleBrokenFallback();
	void HandleBrokenFallbackElapsed();
	void ScheduleRecoveryEnd();
	void HandleRecoveryElapsed();

	ESovPoiseState DeterminePoiseState() const;
	void RefreshPoiseState(bool bBroadcastChanges);
	void SetPoiseState(ESovPoiseState NewState, bool bBroadcastChanges);
	void UpdateOwnedStateTags(ESovPoiseState NewState);
	void RemoveOwnedStateTags();
	void SetOwnedLooseTag(const FGameplayTag& Tag, bool bShouldApply, bool& bAppliedFlag);
	void SetPoiseInternal(float NewPoise);

	bool CanWritePoise() const;
	float GetWorldTimeSeconds() const;
	float GetTimerRemaining(const FTimerHandle& TimerHandle) const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle PoiseChangedDelegateHandle;
	FDelegateHandle MaxPoiseChangedDelegateHandle;
	FDelegateHandle RegenerationBlockedTagChangedDelegateHandle;
	FDelegateHandle BrokenTagChangedDelegateHandle;
	FDelegateHandle RecoveringTagChangedDelegateHandle;

	FTimerHandle RegenerationDelayTimerHandle;
	FTimerHandle RegenerationTimerHandle;
	FTimerHandle BrokenFallbackTimerHandle;
	FTimerHandle RecoveryTimerHandle;

	FGameplayTag PressuredTag;
	FGameplayTag BrokenTag;
	FGameplayTag RecoveringTag;
	FGameplayTag RegenerationBlockedTag;

	float LastPoiseDamageWorldTime = 0.0f;
	float LastRegenerationUpdateWorldTime = 0.0f;
	ESovPoiseState PoiseState = ESovPoiseState::Stable;
	bool bHasRecordedPoiseDamage = false;
	bool bRegenerationDelayElapsed = false;
	bool bAppliedPressuredTag = false;
	bool bAppliedBrokenTag = false;
	bool bAppliedRecoveringTag = false;
	bool bWarnedMissingAttributeSet = false;
};
