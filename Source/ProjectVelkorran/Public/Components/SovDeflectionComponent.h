// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "TimerManager.h"
#include "SovDeflectionComponent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSovDeflectionStateSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovDeflectionResultSignature,
	const FSovDamageResult&, Result);

/**
 * Selene's short, all-or-nothing precision defense.
 *
 * The component owns only the predicted timing tag and one-hit consumption.
 * Damage eligibility, facing, Stamina payment, and final routing remain one
 * authoritative transaction in Narrative's AttributeSet. There is deliberately
 * no held state, chip-damage branch, guard break, or counter-window behavior.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovDeflectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovDeflectionComponent();

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Deflection")
	bool InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Deflection")
	bool IsInitialized() const;

	/** Opens one precision window. Returns false when the action cannot start. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Deflection")
	bool BeginDeflection();

	/** Closes the current window without producing a successful Deflection. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Deflection")
	void EndDeflection();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Deflection")
	bool IsDeflectionWindowOpen() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Deflection")
	float GetPerfectDeflectionWindow() const
	{
		return FMath::Max(PerfectDeflectionWindow, 0.0f);
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Deflection")
	float GetMinimumDeflectionStartStamina() const
	{
		return FMath::Max(MinimumDeflectionStartStamina, 0.0f);
	}

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Deflection")
	FSovDeflectionStateSignature OnDeflectionStarted;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Deflection")
	FSovDeflectionStateSignature OnDeflectionWindowClosed;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Deflection")
	FSovDeflectionResultSignature OnPerfectDeflection;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Prototype timing from the Phase 1 protagonist-feel target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Deflection|Tuning", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float PerfectDeflectionWindow = 0.11f;

	/** Threshold to enter Deflection. The resolved hit pays the standard/heavy cost from combat settings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Deflection|Tuning", meta = (ClampMin = "0.0"))
	float MinimumDeflectionStartStamina = 8.0f;

private:
	friend class USovGameplayAbility_SeleneDeflection;
	bool BeginDeflectionInternal(int32 OwnedBusyContributions, uint64* OutWindowEpoch = nullptr);
	void EndOwnedDeflectionWindow(uint64 Epoch);
	bool HasInterruptionState(int32 OwnedBusyContributions) const;
	void BindInterruptionTags();
	void UnbindInterruptionTags();
	void HandleInterruptionTagChanged(FGameplayTag Tag, int32 NewCount);
	void TryInitializeFromOwner();

	UFUNCTION()
	void HandleOwnerASCInitialized();

	void UninitializeFromAbilitySystem();
	void CloseDeflectionWindow();
	void SetOwnedLooseTag(const FGameplayTag& Tag, bool bShouldApply, bool& bAppliedFlag);

	UFUNCTION()
	void HandleDamageResolvedAsTarget(const FSovDamageResult& Result);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPerfectDeflection(const FSovDamageResult& Result);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FTimerHandle DeflectionWindowTimerHandle;
	TMap<FGameplayTag, FDelegateHandle> InterruptionTagHandles;
	uint64 WindowEpoch = 0;
	int32 DeflectionOwnedBusyContributions = 0;
	bool bClosingWindow = false;
	bool bUninitializing = false;
	bool bAppliedDeflectingTag = false;
};
