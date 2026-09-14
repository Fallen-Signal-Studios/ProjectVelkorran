// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeCombatAbility.h"
#include "TimerManager.h"
#include "SovGameplayAbility_Echo.generated.h"

class USovEchoComponent;
class UAbilitySystemComponent;
class UNarrativeAttributeSetBase;
class UWeaponItem;
class UAnimMontage;

/** How an Echo ability validates the weapon classes authored on its child. */
UENUM(BlueprintType)
enum class ESovEchoWeaponGatePolicy : uint8
{
	AnyAllowedWielded UMETA(DisplayName = "Any Allowed Wielded Weapon"),
	GrantingSourceMustBeWielded UMETA(DisplayName = "Granting Source Must Be Wielded")
};

/**
 * Shared, predicted GAS shell for character-specific Echo abilities.
 *
 * Echo is checked on the predicting owner and authority, then spent only by
 * authority through USovEchoComponent. Concrete native parents provide
 * identity and payload contracts; Blueprint children own authored montage,
 * target, projectile, GameplayEffect, cue, and recovery flow.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Sovereign Echo Ability Base"))
class PROJECTVELKORRAN_API USovGameplayAbility_EchoBase : public UNarrativeCombatAbility
{
	GENERATED_BODY()

public:
	USovGameplayAbility_EchoBase();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** Same cost/payload/weapon checks as CheckCost, with caller-owned diagnostics and no ability state writes.
	 * This is not CanActivateAbility: target-specific and Blueprint activation remain input-owned. */
	bool CheckEchoPresentationCost(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FString& OutReason, FGameplayTagContainer* OptionalRelevantTags = nullptr) const;

	virtual void ApplyCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	float GetMinimumEchoRequired() const { return FMath::Max(MinimumEchoRequired, 0.0f); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	float GetEchoCost() const { return FMath::Max(EchoCost, 0.0f); }

	/** Read-only shared weapon rule for resource feedback; activation still validates every other gate. */
	bool CanUseEchoWeaponContext(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
	{
		return ActorInfo && MeetsWeaponRequirement(Handle, ActorInfo);
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	float GetCurrentEcho() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	USovEchoComponent* GetEchoComponent() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	FText GetEchoAbilityDisplayName() const { return AbilityDisplayName; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	FText GetEchoAbilityDescription() const { return AbilityDescription; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability|Animation")
	FGameplayTag GetEchoAbilityAnimSetTag() const { return AbilityAnimSetTag; }

	/**
	 * Authority-only aim trace using the server pawn eye point and replicated
	 * controller rotation. Use this for expensive hitscan Echo payloads rather
	 * than trusting Narrative's unvalidated client target data.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo Ability|Targeting")
	FGameplayAbilityTargetDataHandle GetAuthorityAimTargetData(const FCombatTraceData& TraceData);

	/** Convenience wrapper for Blueprint-authored montage/task completion. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo Ability")
	void FinishEchoAbility(bool bWasCancelled = false);

protected:
	/** Native continuations must still own this exact paid execution. */
	bool IsCurrentEchoExecutionValid() const { return IsEchoActivationCurrent(EchoActivationEpoch); }
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

	/** Concrete native parents fail closed when required payload assets are empty. */
	virtual bool HasRequiredPayloadConfiguration() const { return true; }

	/** Concrete abilities may override the shared authored-weapon policy. */
	virtual bool MeetsWeaponRequirement(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;

	/** Runs after GAS commit on the predicting owner and authority. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Echo Ability", meta = (DisplayName = "Echo Ability Started"))
	void ReceiveEchoAbilityStarted(bool bIsAuthoritative);

	/** Owner-only hook for camera, rumble, and other non-gameplay presentation. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Echo Ability|Presentation", meta = (DisplayName = "Echo Ability Local Presentation"))
	void ReceiveEchoAbilityLocalPresentation();

	/** Runs only on authority after the Echo transaction succeeds. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Echo Ability", meta = (DisplayName = "Echo Ability Authority Committed"))
	void ReceiveEchoAbilityAuthorityCommitted(float EchoSpent);

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Echo Ability", meta = (DisplayName = "Echo Ability Ended"))
	void ReceiveEchoAbilityEnded(bool bWasCancelled);

	/** Minimum current meter value required before the ability can activate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cost", meta = (ClampMin = "0.0"))
	float MinimumEchoRequired = 0.0f;

	/** Echo removed on authoritative GAS commit. May be lower than the threshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cost", meta = (ClampMin = "0.0"))
	float EchoCost = 0.0f;

	/** Transaction/source tag sent through USovEchoComponent::OnEchoSpent. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cost")
	FGameplayTag EchoSpendTag;

	/** Concrete weapon item classes allowed to execute this ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Weapon")
	TArray<TSubclassOf<UWeaponItem>> AllowedWeaponClasses;

	/** Intentionally fails closed until AllowedWeaponClasses is authored. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Weapon")
	bool bRequiresAllowedWeapon = true;

	/** Universal actions accept any allowed wielded weapon; variants require their granting source. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Weapon")
	ESovEchoWeaponGatePolicy WeaponGatePolicy = ESovEchoWeaponGatePolicy::GrantingSourceMustBeWielded;

	/** Character identity tag required on the avatar ASC. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Identity")
	FGameplayTag RequiredCharacterTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Identity")
	FText AbilityDisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Identity", meta = (MultiLine = true))
	FText AbilityDescription;

	/** Key resolved through Narrative's active linked weapon animation layer. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Animation", meta = (Categories = "Narrative.Anim.AnimSets"))
	FGameplayTag AbilityAnimSetTag;

	/** Cosmetic A/B casts. Exactly two distinct montages; no payload notifies or root motion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Animation")
	TArray<TObjectPtr<UAnimMontage>> CastMontages;

	/** Failsafe for a Blueprint child that never ends its montage/task flow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Lifecycle", meta = (ClampMin = "0.0"))
	float MaximumActiveDuration = 5.0f;

private:
	void PlayAlternatingCastMontage();
	uint8 NextCastMontageIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveCastMontage;

	bool IsEchoActivationCurrent(uint64 Epoch) const;
	USovEchoComponent* ResolveEchoComponent(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool MeetsCharacterRequirement(const FGameplayAbilityActorInfo* ActorInfo) const;
	void AddEchoFailureTags(FGameplayTagContainer* OptionalRelevantTags) const;
	void BindCancellationTags(UAbilitySystemComponent* AbilitySystem);
	void UnbindCancellationTags();
	void HandleCancellationTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void HandleMaximumDurationExpired();

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;

	TMap<FGameplayTag, FDelegateHandle> CancellationTagHandles;
	FDelegateHandle BusyTagChangedHandle;
	FTimerHandle MaximumDurationTimerHandle;
	bool bEchoAbilityStarted = false;
	uint64 EchoActivationEpoch = 0;
	uint64 EchoActorInfoEpoch = 0;
	uint64 EchoLifeEpoch = 0;
	TWeakObjectPtr<const UNarrativeAttributeSetBase> EchoAttributes;
	TWeakObjectPtr<AActor> EchoActivationAvatar;
	TWeakObjectPtr<UAbilitySystemComponent> EchoActivationASC;
	FGameplayAbilitySpecHandle EchoActivationSpec;
	bool bEchoEndPending = false;
	bool bEndingEcho = false;
	mutable bool bAuthorityEchoSpendAttempted = false;
	mutable bool bAuthorityEchoSpendSucceeded = false;
	mutable FString LastActivationFailureReason;
};
