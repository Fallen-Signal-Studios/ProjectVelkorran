// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SovGuardComponent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSovGuardStateSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovGuardResultSignature,
	const FSovDamageResult&, Result);

/**
 * Tarrik's project-owned guard lifecycle and reward bridge.
 *
 * Damage routing evaluates the frontal plane, attack class, Stamina, and
 * mitigation transactionally. This component owns input-duration state,
 * perfect/counter windows, Echo rewards, and Blueprint presentation events.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovGuardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovGuardComponent();

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Guard")
	bool InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Guard")
	bool IsInitialized() const;

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Guard")
	bool BeginGuard();

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Guard")
	void EndGuard();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Guard")
	bool IsGuarding() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Guard")
	bool IsPerfectDefenseWindowOpen() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Guard")
	bool IsCounterWindowOpen() const;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Guard")
	FSovGuardStateSignature OnGuardStarted;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Guard")
	FSovGuardStateSignature OnGuardEnded;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Guard")
	FSovGuardResultSignature OnGuardImpact;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Guard")
	FSovGuardResultSignature OnPerfectDefense;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Guard")
	FSovGuardResultSignature OnGuardBroken;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Guard")
	FSovGuardResultSignature OnCounterLanded;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Guard|Tuning", meta = (ClampMin = "0.0"))
	float PerfectDefenseWindow = 0.16f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Guard|Tuning", meta = (ClampMin = "0.0"))
	float CounterWindowDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Guard|Tuning", meta = (ClampMin = "0.0"))
	float GuardBreakDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Guard|Tuning", meta = (ClampMin = "0.0"))
	float PerfectGuardEchoReward = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Guard|Tuning", meta = (ClampMin = "0.0"))
	float GuardCounterEchoReward = 10.f;

private:
	void TryInitializeFromOwner();

	UFUNCTION()
	void HandleOwnerASCInitialized();

	void UninitializeFromAbilitySystem();
	void ClosePerfectDefenseWindow();
	void CloseCounterWindow();
	void ClearGuardBrokenState();
	void BroadcastPendingGuardBroken();
	void OpenCounterWindow();
	void SetOwnedLooseTag(const FGameplayTag& Tag, bool bShouldApply, bool& bAppliedFlag);

	UFUNCTION()
	void HandleDamageResolvedAsTarget(const FSovDamageResult& Result);

	UFUNCTION()
	void HandleDamageResolvedAsSource(const FSovDamageResult& Result);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FTimerHandle PerfectDefenseTimerHandle;
	FTimerHandle CounterWindowTimerHandle;
	FTimerHandle GuardBrokenTimerHandle;

	bool bAppliedGuardingTag = false;
	bool bAppliedPerfectDefenseTag = false;
	bool bAppliedCounterWindowTag = false;
	bool bAppliedGuardBrokenTag = false;
	FSovDamageResult PendingGuardBrokenResult;
};
