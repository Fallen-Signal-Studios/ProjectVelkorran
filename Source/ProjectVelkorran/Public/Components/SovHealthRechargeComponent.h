// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "TimerManager.h"
#include "SovHealthRechargeComponent.generated.h"

class UAbilitySystemComponent;
class UNarrativeAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovHealthChangedSignature,
	float, OldHealth,
	float, NewHealth,
	float, MaxHealth);

/**
 * Server-authoritative delayed Health recharge for player characters.
 *
 * The component is installed only by ASovPlayerCharacterBase. Replicated
 * Health attributes drive client presentation; world timers perform the
 * authoritative recharge without adding an Actor tick.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovHealthRechargeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovHealthRechargeComponent();

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Health")
	bool InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

	/** Restart recharge timing from restored current Health without a damage event. */
	void ResetForCheckpoint();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Health")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Health")
	float GetHealth() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Health")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Health")
	bool IsRecharging() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Health")
	float GetSecondsUntilRecharge() const;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Health")
	FSovHealthChangedSignature OnHealthChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Seconds without an applied hit before Health recharge may begin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Health|Recharge", meta = (ClampMin = "0.0"))
	float RechargeDelay = 5.0f;

	/** Fraction of MaxHealth restored each second. 0.10 means ten percent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Health|Recharge", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RechargePercentPerSecond = 0.10f;

	/** Timer cadence used to sample continuous recharge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Health|Recharge", meta = (ClampMin = "0.01"))
	float RechargeTimerInterval = 0.1f;

private:
	void TryInitializeFromOwner();

	UFUNCTION()
	void HandleOwnerASCInitialized();

	void UninitializeFromAbilitySystem();
	void ClearLifecycleTimers();
	void HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void HandleDamageResolved(const FSovDamageResult& Result);

	UFUNCTION()
	void HandleDeathStateChanged(
		AActor* KilledActor,
		UNarrativeAbilitySystemComponent* KilledActorASC,
		bool bIsDead);

	void RecordAppliedHit();
	void ScheduleRechargeDelay(float DelaySeconds);
	void HandleRechargeDelayElapsed();
	void TryStartRecharge();
	void HandleRechargeTimerElapsed();
	void StopRecharge();
	void ResetAtFullHealth();

	bool CanWriteHealth() const;
	float GetWorldTimeSeconds() const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
	FTimerHandle RechargeDelayTimerHandle;
	FTimerHandle RechargeTimerHandle;

	float LastAppliedHitWorldTime = 0.0f;
	float LastRechargeUpdateWorldTime = 0.0f;
	bool bHasRecordedAppliedHit = false;
	bool bRechargeDelayElapsed = false;
	bool bWarnedMissingAttributeSet = false;
};
