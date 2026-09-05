// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeCombatAbility.h"
#include "TimerManager.h"
#include "SovGameplayAbility_DominionHandler.generated.h"

class ASovDominionHandler;
class UAbilitySystemComponent;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

/**
 * Server-authoritative order that asks one linked Dominion hound to Horn Charge.
 *
 * The Handler snapshots the command-link instance at activation, then validates
 * that same instance again when the native issue timer expires. Animation is
 * optional presentation only: neither a missing montage nor an absent notify
 * can issue, duplicate, or strand the gameplay order.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Dominion Handler: Command Hound"))
class PROJECTVELKORRAN_API USovGameplayAbility_DominionHandlerCommandHound
	: public UNarrativeCombatAbility
{
	GENERATED_BODY()

public:
	// The dispatched Hound owns the attack token; the order itself does not reserve one.
	virtual bool RequiresBotAttackToken_Implementation() const override { return false; }

	USovGameplayAbility_DominionHandlerCommandHound();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Identity")
	FGameplayTag GetHandlerCommandAbilityTag() const
	{
		return AbilityIdentityTag;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Identity")
	FGameplayTag GetConfiguredInputTag() const { return InputTag; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Command Link")
	bool RequiresActiveCommandLink() const
	{
		return bRequiresActiveCommandLink;
	}

	/** True when the actual GAS activation container requires an active link. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Command Link")
	bool HasActiveCommandLinkActivationRequirement() const;

	/** True when the actual GAS activation container blocks a severed link. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Command Link")
	bool BlocksCommandLinkSeverAtActivation() const;

	/** True when weapon-equipping state prevents a command wind-up. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|State")
	bool BlocksWeaponEquippingAtActivation() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Cost")
	bool RequiresAmmoForCommand() const { return bRequiresAmmo; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Cost")
	bool HasGameplayEffectCost() const
	{
		return GetCostGameplayEffect() != nullptr;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Cost")
	bool HasGameplayEffectCooldown() const
	{
		return GetCooldownGameplayEffect() != nullptr;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Timing")
	float GetCommandIssueDelay() const { return CommandIssueDelay; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Timing")
	float GetPostIssueRecovery() const { return PostIssueRecovery; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Timing")
	float GetCommandCooldownDuration() const { return CommandCooldownDuration; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Timing")
	float GetMaximumActiveDuration() const { return MaximumActiveDuration; }

	/** The immutable link instance captured at the start of this activation. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Sovereign|Dominion Handler|Command Link")
	FGuid GetCapturedLinkInstanceId() const { return CapturedLinkInstanceId; }

	/** Initial candidate selected for anticipation presentation. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Sovereign|Dominion Handler|Command Link")
	AActor* GetPendingCommandHound() const { return PendingHound.Get(); }

	/** Hound that accepted the order, or null before a successful issue. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Sovereign|Dominion Handler|Command Link")
	AActor* GetOrderedHound() const { return OrderedHound.Get(); }

	/** Target resolved by the accepted Hound ability, when one is available. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Sovereign|Dominion Handler|Command Link")
	AActor* GetCommandTarget() const { return CommandTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Animation")
	UAnimMontage* GetCommandMontage() const { return CommandMontage.Get(); }

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

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Identity")
	FGameplayTag AbilityIdentityTag;

	/** Gameplay contract mirrored by ActivationRequiredTags. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Command Link")
	bool bRequiresActiveCommandLink = true;

	/** Optional replicated GAS montage; gameplay issue timing remains native. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Animation")
	TObjectPtr<UAnimMontage> CommandMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Animation")
	FName MontageStartSection = NAME_None;

	/** Anticipation time before the Handler authoritatively issues the order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float CommandIssueDelay = 0.35f;

	/** Native recovery retained after a successful hound activation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float PostIssueRecovery = 0.45f;

	/** Applied only after a linked hound accepts the Horn Charge activation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float CommandCooldownDuration = 5.5f;

	/** Cancels malformed authored children that never complete their lifecycle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Timing", meta = (ClampMin = "0.1", Units = "s"))
	float MaximumActiveDuration = 1.5f;

	/** Authority hook for anticipation, facing, bark, and command VFX. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Dominion Handler|Presentation", meta = (DisplayName = "Hound Order Started"))
	void ReceiveHoundOrderStarted(AActor* IntendedHound, FGuid LinkInstanceId);

	/** Authority hook after the exact linked hound accepts Horn Charge. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Dominion Handler|Presentation", meta = (DisplayName = "Hound Order Issued"))
	void ReceiveHoundOrderIssued(
		AActor* Hound,
		AActor* ChargeTarget,
		FGuid LinkInstanceId);

	/** Authority hook for a candidate/link that became invalid during anticipation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Dominion Handler|Presentation", meta = (DisplayName = "Hound Order Failed"))
	void ReceiveHoundOrderFailed(AActor* IntendedHound, FGuid LinkInstanceId);

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Dominion Handler|Presentation", meta = (DisplayName = "Hound Order Ended"))
	void ReceiveHoundOrderEnded(bool bWasCancelled, bool bOrderWasIssued);

private:
	bool HasRequiredCommandConfiguration() const;
	bool CanContinueCommand() const;
	bool IsActivationEpochCurrent(uint64 ExpectedEpoch) const;
	uint64 AdvanceActivationEpoch();
	void CancelCommandAbility();
	void StartCommandMontage();

	void HandleCommandIssueTimer(uint64 ExpectedEpoch);
	void HandleRecoveryFinished(uint64 ExpectedEpoch);
	void HandleMaximumDurationExpired(uint64 ExpectedEpoch);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	void BindCancellationTags(UAbilitySystemComponent* AbilitySystem);
	void UnbindCancellationTags();
	void HandleCancellationTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	bool FinishDeferredEndIfRequested();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

	UPROPERTY(Transient)
	TWeakObjectPtr<ASovDominionHandler> Handler;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PendingHound;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> OrderedHound;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CommandTarget;

	TArray<TPair<FGameplayTag, FDelegateHandle>> CancellationTagHandles;
	FTimerHandle CommandIssueTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FTimerHandle MaximumDurationTimerHandle;
	FGuid CapturedLinkInstanceId;
	double NextAllowedActivationTime = 0.0;
	uint64 ActivationEpoch = 0;
	bool bIssueAttempted = false;
	bool bOrderIssued = false;
	bool bAbilityStarted = false;
	bool bEndingAbility = false;
	bool bDispatchInProgress = false;
	bool bDeferredEndRequested = false;
	bool bDeferredEndReplicate = false;
	bool bDeferredEndWasCancelled = false;
};
