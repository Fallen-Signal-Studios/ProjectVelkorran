// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "SovEchoComponent.generated.h"

class UAbilitySystemComponent;
class UNarrativeAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovEchoChangedSignature,
	float, OldEcho,
	float, NewEcho,
	float, MaxEcho);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSovEchoThresholdChangedSignature,
	bool, bIsActive,
	float, CurrentEcho);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovEchoTransactionSignature,
	FGameplayTag, SourceTag,
	float, Amount,
	float, NewEcho);

/**
 * Project-owned lifecycle controller for the campaign's Echo resource.
 *
 * The Narrative attribute set remains the authoritative storage for Echo and
 * MaxEcho. This component owns the rules shared by both protagonists:
 *
 * - server-authoritative grants and spending;
 * - 0..MaxEcho clamping with no overflow;
 * - combat-activity tracking;
 * - inactivity decay toward the reserve floor;
 * - encounter-end normalization;
 * - Resonant and signature-ready threshold notifications.
 *
 * Character abilities and data decide which actions deserve Echo and how much.
 * Tarrik/Selene-specific award rules, chain tracking, source cooldowns, ability
 * costs, and joint Resonance handshakes deliberately do not live here.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovEchoComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovEchoComponent();

	/**
	 * Binds the component to an ASC that owns UNarrativeAttributeSetBase.
	 * BeginPlay resolves the owner's ASC automatically; this remains public for
	 * characters whose ASC becomes available later in their initialization flow.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo")
	bool InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo")
	float GetEcho() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo")
	float GetMaxEcho() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo")
	bool CanAffordEcho(float Cost) const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo")
	bool IsResonant() const { return bIsResonant; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo")
	bool IsSignatureReady() const { return bIsSignatureReady; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo")
	bool IsEncounterActive() const { return bEncounterActive; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo")
	float GetSecondsUntilDecay() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo")
	FGameplayTag GetLastActivityTag() const { return LastActivityTag; }

	/**
	 * Grants Echo for an action already validated by protagonist-specific logic.
	 * Returns the amount actually applied after clamping.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo", meta = (AutoCreateRefTerm = "SourceTag"))
	float AddEcho(float Amount, const FGameplayTag& SourceTag);

	/** Spends Echo atomically when the current amount can cover Cost. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo", meta = (AutoCreateRefTerm = "SpendTag"))
	bool TrySpendEcho(float Cost, const FGameplayTag& SpendTag);

	/**
	 * Applies an authored checkpoint value and restarts the inactivity clock.
	 * This bypasses neither clamping nor authority.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo")
	float RestoreEchoFromCheckpoint(float AuthoredValue);

	/** Starts encounter decay timing without changing the current Echo value. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo")
	void BeginEncounter();

	/**
	 * Ends the encounter and normalizes Echo to the supplied reserve. A negative
	 * override selects DefaultEncounterReserve.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo")
	float EndEncounter(float ReserveOverride = -1.0f);

	/**
	 * Resets the inactivity timer for guarding, disrupting, and other qualifying
	 * participation that does not necessarily grant Echo.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo", meta = (AutoCreateRefTerm = "ActivityTag"))
	void RecordCombatActivity(const FGameplayTag& ActivityTag);

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Echo")
	FSovEchoChangedSignature OnEchoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Echo")
	FSovEchoTransactionSignature OnEchoGranted;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Echo")
	FSovEchoTransactionSignature OnEchoSpent;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Echo")
	FSovEchoThresholdChangedSignature OnResonantStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Echo")
	FSovEchoThresholdChangedSignature OnSignatureReadyStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Echo above this value decays toward it after inactivity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Tuning", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float DecayFloor = 25.0f;

	/** Seconds without qualifying combat activity before decay begins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Tuning", meta = (ClampMin = "0.0"))
	float InactivityDelay = 6.0f;

	/** Echo points removed per second while inactivity decay is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Tuning", meta = (ClampMin = "0.0"))
	float DecayRate = 8.0f;

	/** Mission-default reserve applied when an encounter ends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Tuning", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float DefaultEncounterReserve = 25.0f;

	/** Echo threshold that activates the readable Resonant state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Tuning", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float ResonantThreshold = 75.0f;

	/** Echo threshold that makes the signature release available. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Tuning", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float SignatureReadyThreshold = 100.0f;

private:
	void TryInitializeFromOwner();

	UFUNCTION()
	void HandleOwnerASCInitialized();

	void UninitializeFromAbilitySystem();
	void SetEchoInternal(float NewEcho);
	void RefreshThresholdStates(float CurrentEcho, bool bBroadcastChanges);
	bool CanWriteEcho() const;
	float GetWorldTimeSeconds() const;

	void HandleEchoAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxEchoAttributeChanged(const FOnAttributeChangeData& ChangeData);

	UFUNCTION()
	void HandleDealtDamage(
		UNarrativeAbilitySystemComponent* DamagedAbilitySystem,
		float Damage,
		const FGameplayEffectSpec& EffectSpec);

	UFUNCTION()
	void HandleReceivedDamage(
		UNarrativeAbilitySystemComponent* DamageSourceAbilitySystem,
		float Damage,
		const FGameplayEffectSpec& EffectSpec);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle EchoChangedDelegateHandle;
	FDelegateHandle MaxEchoChangedDelegateHandle;

	FGameplayTag LastActivityTag;
	float LastActivityWorldTime = 0.0f;
	float LastDecayUpdateWorldTime = 0.0f;
	bool bEncounterActive = false;
	bool bIsResonant = false;
	bool bIsSignatureReady = false;
	bool bWarnedMissingAttributeSet = false;
};
