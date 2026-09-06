// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeCombatAbility.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SovGameplayAbility_DominionHound.generated.h"

class ACharacter;
class ANarrativeNPCController;
class ASovDominionHandler;
class UAbilitySystemComponent;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UCharacterMovementComponent;
class UGameplayEffect;
class UNarrativeAbilitySystemComponent;

/** One authored bite presentation and its native, server-owned attack timing. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovDominionHoundBiteVariant
{
	GENERATED_BODY()

	/** Optional presentation montage. Damage never depends on animation notifies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Animation")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Animation", meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Animation")
	FName StartSection = NAME_None;

	/** Time from ability activation to the authoritative bite sweep. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float ImpactDelay = 0.22f;

	/** Recovery owned by gameplay code after the sweep. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float RecoveryAfterImpact = 0.42f;
};

/**
 * Server-authoritative shell shared by the Dominion hound's physical attacks.
 *
 * Native timers own impact and recovery, swept traces own contact, and every
 * hit is routed through Narrative's damage execution. Montages are optional
 * presentation only, so a missing notify or non-ticking server AnimInstance
 * can never suppress damage or strand the attack lane.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Dominion Hound Attack Base"))
class PROJECTVELKORRAN_API USovGameplayAbility_DominionHoundAttackBase : public UNarrativeCombatAbility
{
	GENERATED_BODY()

public:
	USovGameplayAbility_DominionHoundAttackBase();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Identity")
	FGameplayTag GetHoundAbilityTag() const { return AbilityIdentityTag; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Identity")
	FGameplayTag GetConfiguredInputTag() const { return InputTag; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Damage")
	float GetDamageAmount() const { return DamageAmount; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Damage")
	float GetPoiseDamageAmount() const { return PoiseDamageAmount; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Cost")
	bool RequiresAmmoForAttack() const { return bRequiresAmmo; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Damage")
	FGameplayTagContainer GetAttackClassifications() const { return AttackClassifications; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Damage")
	FGameplayTagContainer GetDamageChannels() const { return DamageChannels; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Targeting")
	float GetMinimumAttackRange() const { return MinimumAttackRange; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Targeting")
	float GetMaximumAttackRange() const { return MaximumAttackRange; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Timing")
	float GetConfiguredImpactDelay() const { return ImpactDelay; }

	/** Target selected by the authoritative attack activation, if still active. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Sovereign|Dominion Hound|Targeting")
	AActor* GetCurrentAttackTarget() const { return AttackTarget.Get(); }

	/** True when Sever interrupts this attack's active execution. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Command Link")
	bool IsInterruptedByCommandLinkSever() const
	{
		return bInterruptedByCommandLinkSever;
	}

	/** True when this attack is available only while a command link is active. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Command Link")
	bool RequiresActiveCommandLink() const
	{
		return bRequiresActiveCommandLink;
	}

	/** True when activation also requires the Handler's tag-plus-native dispatch. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Command Link")
	bool RequiresHandlerOrderAuthorization() const
	{
		return bRequiresHandlerOrderAuthorization;
	}

	/** True when activation must hold one of the target's attacker slots. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Attack Tokens")
	bool RequiresNarrativeAttackToken() const
	{
		return bRequiresNarrativeAttackToken;
	}

	/** True when the actual GAS activation container requires an active link. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Command Link")
	bool HasActiveCommandLinkActivationRequirement() const;

	/** True when the actual GAS activation container requires a native Handler order. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Command Link")
	bool HasHandlerOrderAuthorizationActivationRequirement() const;

	/** True when the actual GAS activation container blocks a severed link. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Command Link")
	bool BlocksCommandLinkSeverAtActivation() const;

	/** True when weapon-equipping state prevents a Hound attack. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|State")
	bool BlocksWeaponEquippingAtActivation() const;

	/** Uses this ability's exact GAS blocker container against the supplied ASC. */
	bool HasAnyActivationBlockingState(
		const UAbilitySystemComponent* AbilitySystem) const;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual float GetAttackDamage_Implementation() const override;
	virtual bool HasRequiredAttackConfiguration() const;
	virtual void PrepareAttack();
	virtual void BeginAttackPayload();
	virtual void StopOwnedMovement();

	/** Guards continuations against synchronous End-and-reactivate callbacks. */
	bool IsActivationEpochCurrent(uint64 ExpectedEpoch) const;
	uint64 GetActiveActivationEpoch() const { return ActivationEpoch; }
	bool IsEndingHoundAbility() const { return bEndingAbility; }

	/** Starts the payload exactly once, including through re-entrant BP events. */
	bool TryBeginAttackPayload();

	/** Stops payload work and enters native recovery. */
	void FinishAttackPayload();

	/** Cancels the complete lifecycle and performs movement/timer cleanup. */
	void CancelHoundAttack();

	bool CanContinueAttackPayload() const;

	/** Selects the optional montage and native timings used this activation. */
	void ConfigureActiveAttack(
		UAnimMontage* Montage,
		float PlayRate,
		FName StartSection,
		float InImpactDelay,
		float InRecoveryAfterImpact);

	/** Current target direction, with actor-forward fallback. */
	FVector ResolveAttackDirection(const FVector& FromLocation) const;

	/** Configured skeletal socket, or the avatar-local fallback point. */
	FVector ResolveAttackProbeLocation() const;

	/**
	 * Sweeps once, applies damage to at most one new hostile, and records it in
	 * the activation hit ledger before synchronous damage callbacks run.
	 */
	bool ApplyAttackSweep(
		const FVector& Start,
		const FVector& End,
		float Radius,
		bool& bOutBlockedByWorld);

	AActor* GetAttackTarget() const { return AttackTarget.Get(); }

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Identity")
	FGameplayTag AbilityIdentityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Damage")
	FGameplayTagContainer DamageChannels;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Damage")
	FGameplayTagContainer AttackClassifications;

	/** Instant effect routed through UNarrativeDamageExecCalc. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Damage", meta = (ClampMin = "0.0"))
	float DamageAmount = 16.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Damage", meta = (ClampMin = "0.0"))
	float PoiseDamageAmount = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumAttackRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumAttackRange = 400.0f;

	/** By default hounds only acquire player-controlled hostile characters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Targeting")
	bool bOnlyAcquirePlayerControlledTargets = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Trace")
	FName TraceSocketName = TEXT("MouthSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Trace", meta = (Units = "cm"))
	FVector FallbackTraceOffset = FVector(75.0f, 0.0f, 55.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceReach = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Trace", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRadius = 55.0f;

	/** Optional default montage used by charge and pounce. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Animation")
	FName MontageStartSection = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float ImpactDelay = 0.22f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float RecoveryAfterImpact = 0.42f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Timing", meta = (ClampMin = "0.1", Units = "s"))
	float MaximumActiveDuration = 2.0f;

	/** Specialist attacks opt into interruption when their command link is Severed. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Command Link")
	bool bInterruptedByCommandLinkSever = false;

	/** Specialist attacks opt into active-link authorization. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Command Link")
	bool bRequiresActiveCommandLink = false;

	/** Specialist moves may require the Handler's transient tag and native scope. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Command Link")
	bool bRequiresHandlerOrderAuthorization = false;

	/** Every Hound attack reserves or borrows its exact target's attacker slot. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Attack Tokens")
	bool bRequiresNarrativeAttackToken = false;

	/** Presentation hooks only. Gameplay remains wholly native and authoritative. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Dominion Hound|Presentation", meta = (DisplayName = "Hound Attack Started"))
	void ReceiveHoundAttackStarted(AActor* TargetActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Dominion Hound|Presentation", meta = (DisplayName = "Hound Attack Impact"))
	void ReceiveHoundAttackImpact();

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Dominion Hound|Presentation", meta = (DisplayName = "Hound Attack Ended"))
	void ReceiveHoundAttackEnded(bool bWasCancelled);

private:
	friend class ASovDominionHandler;

	/**
	 * Opens the native-only half of a Handler order authorization.
	 *
	 * The transient gameplay tag remains observable for GAS requirements, but
	 * cannot authorize Horn Charge by itself. Only ASovDominionHandler may open
	 * this scope around one exact TryActivateAbility call.
	 */
	void SetHandlerOrderDispatchInProgress(bool bInProgress)
	{
		bHandlerOrderDispatchInProgress = bInProgress;
	}

	AActor* FindBestAttackTarget(
		AActor* SourceActor,
		UAbilitySystemComponent* SourceAbilitySystem) const;

	bool IsValidAttackTarget(
		AActor* SourceActor,
		UAbilitySystemComponent* SourceAbilitySystem,
		UAbilitySystemComponent* TargetAbilitySystem) const;

	bool CanAcquireRequiredAttackToken(
		const FGameplayAbilityActorInfo* ActorInfo,
		AActor* TargetActor) const;
	bool AcquireRequiredAttackToken(
		const FGameplayAbilityActorInfo* ActorInfo,
		AActor* TargetActor);
	bool HasRequiredAttackTokenLease() const;
	void ReleaseClaimedAttackToken();

	bool ApplyPointDamage(
		const FHitResult& Hit,
		UAbilitySystemComponent* TargetAbilitySystem);
	uint64 AdvanceActivationEpoch();

	void StartAttackMontage();

	void HandleImpactTimer(uint64 ExpectedEpoch);
	void HandleRecoveryFinished(uint64 ExpectedEpoch);
	void HandleMaximumDurationExpired(uint64 ExpectedEpoch);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	void BindCancellationTags(UAbilitySystemComponent* AbilitySystem);
	void UnbindCancellationTags();
	void HandleCancellationTagChanged(FGameplayTag CallbackTag, int32 NewCount);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> AttackTarget;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

	UPROPERTY(Transient)
	TObjectPtr<ANarrativeNPCController> AttackTokenController;

	/** Exact target ASC whose finite attacker slot backs this direct attack. */
	UPROPERTY(Transient)
	TObjectPtr<UNarrativeAbilitySystemComponent> AttackTokenTargetAbilitySystem;

	TSet<TWeakObjectPtr<UAbilitySystemComponent>> HitTargets;
	TArray<TPair<FGameplayTag, FDelegateHandle>> CancellationTagHandles;
	FTimerHandle ImpactTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FTimerHandle MaximumDurationTimerHandle;
	double NextAllowedActivationTime = 0.0;
	uint64 AttackTokenLeaseSerial = 0;
	uint64 ActivationEpoch = 0;
	float ActiveMontagePlayRate = 1.0f;
	float ActiveImpactDelay = 0.22f;
	float ActiveRecoveryAfterImpact = 0.42f;
	FName ActiveMontageStartSection = NAME_None;
	bool bPayloadStarted = false;
	bool bPayloadFinished = false;
	bool bAbilityStarted = false;
	bool bEndingAbility = false;
	bool bNewlyClaimedAttackToken = false;
	bool bHandlerOrderDispatchInProgress = false;
};

/** Close-range Standard bite. Three authored variants avoid immediate repeats. */
UCLASS(Blueprintable, meta = (DisplayName = "Dominion Hound: Bite"))
class PROJECTVELKORRAN_API USovGameplayAbility_DominionHoundBite : public USovGameplayAbility_DominionHoundAttackBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_DominionHoundBite();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Bite")
	int32 GetBiteVariantCount() const { return BiteVariants.Num(); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Bite")
	int32 GetLastSelectedBiteVariantIndex() const { return LastSelectedVariantIndex; }

protected:
	virtual bool HasRequiredAttackConfiguration() const override;
	virtual void PrepareAttack() override;

	/** Normally three entries, corresponding to the hound's three bite clips. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Bite")
	TArray<FSovDominionHoundBiteVariant> BiteVariants;

private:
	int32 LastSelectedVariantIndex = INDEX_NONE;
};

/** Committed straight-line horn attack with continuous, swept hit detection. */
UCLASS(Blueprintable, meta = (DisplayName = "Dominion Hound: Horn Charge"))
class PROJECTVELKORRAN_API USovGameplayAbility_DominionHoundHornCharge : public USovGameplayAbility_DominionHoundAttackBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_DominionHoundHornCharge();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Charge")
	bool IsUsingNativeMovement() const { return bUseNativeMovement; }

	/** Mirrors the movement-state gate used by CanActivateAbility. */
	bool HasRequiredMovementStateForActivation(
		const AActor* AvatarActor) const;

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool HasRequiredAttackConfiguration() const override;
	virtual void BeginAttackPayload() override;
	virtual void StopOwnedMovement() override;

	/** Disable this when the authored montage supplies root-motion translation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Charge")
	bool bUseNativeMovement = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Charge", meta = (EditCondition = "bUseNativeMovement", ClampMin = "0.0", Units = "cm/s"))
	float ChargeSpeed = 1500.0f;

	/** Gameplay-active charge duration for native or montage-root-motion travel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Charge", meta = (ClampMin = "0.05", Units = "s"))
	float MaximumChargeDuration = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Charge", meta = (EditCondition = "bUseNativeMovement", ClampMin = "0.0", Units = "cm"))
	float MaximumChargeDistance = 1350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Charge", meta = (ClampMin = "0.005", Units = "s"))
	float MovementSweepInterval = 0.016f;

private:
	void UpdateCharge(uint64 ExpectedEpoch);

	TWeakObjectPtr<UCharacterMovementComponent> OwnedMovementComponent;
	FTimerHandle ChargeUpdateTimerHandle;
	FVector ChargeDirection = FVector::ForwardVector;
	FVector ChargeStartLocation = FVector::ZeroVector;
	FVector PreviousProbeLocation = FVector::ZeroVector;
	double ChargeStartTime = 0.0;
	float SavedMaxWalkSpeed = 0.0f;
	bool bOwnsChargeMovement = false;
};

/** Snapshot-target ballistic pounce with no in-flight homing. */
UCLASS(Blueprintable, meta = (DisplayName = "Dominion Hound: Pounce"))
class PROJECTVELKORRAN_API USovGameplayAbility_DominionHoundPounce : public USovGameplayAbility_DominionHoundAttackBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_DominionHoundPounce();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Hound|Pounce")
	bool IsUsingNativeMovement() const { return bUseNativeMovement; }

protected:
	virtual bool HasRequiredAttackConfiguration() const override;
	virtual void BeginAttackPayload() override;
	virtual void StopOwnedMovement() override;

	/** Disable this when the authored montage supplies root-motion translation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Pounce")
	bool bUseNativeMovement = true;

	/** Gameplay-active pounce duration for native or montage-root-motion travel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Pounce", meta = (ClampMin = "0.1", Units = "s"))
	float PounceFlightDuration = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Pounce", meta = (EditCondition = "bUseNativeMovement", ClampMin = "0.0", Units = "cm/s"))
	float MaximumLaunchSpeed = 3200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Pounce", meta = (EditCondition = "bUseNativeMovement", Units = "cm"))
	float TargetHeightOffset = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Hound|Pounce", meta = (ClampMin = "0.005", Units = "s"))
	float MovementSweepInterval = 0.016f;

private:
	void UpdatePounce(uint64 ExpectedEpoch);

	TWeakObjectPtr<UCharacterMovementComponent> OwnedMovementComponent;
	FTimerHandle PounceUpdateTimerHandle;
	FVector PounceDirection = FVector::ForwardVector;
	FVector PreviousProbeLocation = FVector::ZeroVector;
	double PounceStartTime = 0.0;
	float SavedAirControl = 0.0f;
	bool bOwnsPounceMovement = false;
};
