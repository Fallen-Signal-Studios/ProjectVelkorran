// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeCombatAbility.h"
#include "TimerManager.h"
#include "SovGameplayAbility_TarrikEcho.generated.h"

class USovEchoComponent;
class UAbilitySystemComponent;
class UWeaponItem;
class UGameplayEffect;
class ANarrativeProjectile;

/** Weapon context used to organize Tarrik's Echo loadout in UI and content. */
UENUM(BlueprintType)
enum class ESovTarrikEchoWeaponFamily : uint8
{
	Universal UMETA(DisplayName = "Either Weapon"),
	Velkorran UMETA(DisplayName = "Velkorran (Sword)"),
	Cinderline UMETA(DisplayName = "Cinderline")
};

/**
 * Shared, predicted GAS shell for Tarrik's weapon-context Echo abilities.
 *
 * Echo is checked on both the predicting owner and authority, but is spent only
 * by the authority through USovEchoComponent. Blueprint children own authored
 * montage, target-data, projectile, GameplayEffect, cue, and recovery flow by
 * implementing Echo Ability Started and ending the ability when that flow is
 * complete.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik Echo Ability Base"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikEchoBase : public UNarrativeCombatAbility
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikEchoBase();

	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	float GetMinimumEchoRequired() const { return FMath::Max(MinimumEchoRequired, 0.0f); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	float GetEchoCost() const { return FMath::Max(EchoCost, 0.0f); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	float GetCurrentEcho() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	USovEchoComponent* GetEchoComponent() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	FText GetEchoAbilityDisplayName() const { return AbilityDisplayName; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	FText GetEchoAbilityDescription() const { return AbilityDescription; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	ESovTarrikEchoWeaponFamily GetEchoWeaponFamily() const { return WeaponFamily; }

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

	/**
	 * Runs on the predicting owner and the authority after GAS commit succeeds.
	 * Follow Narrative's normal predicted montage/target-data pattern here.
	 */
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

	/**
	 * Concrete weapon item classes allowed to execute this ability. Weapon
	 * variants validate the granting source is still wielded; the universal
	 * grenade accepts either currently wielded allowed class.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Weapon")
	TArray<TSubclassOf<UWeaponItem>> AllowedWeaponClasses;

	/** Intentionally fails closed until AllowedWeaponClasses is authored. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Weapon")
	bool bRequiresAllowedWeapon = true;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Identity")
	ESovTarrikEchoWeaponFamily WeaponFamily = ESovTarrikEchoWeaponFamily::Universal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Identity")
	FText AbilityDisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Identity", meta = (MultiLine = true))
	FText AbilityDescription;

	/** Key resolved through Narrative's active linked weapon animation layer. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Animation", meta = (Categories = "Narrative.Anim.AnimSets"))
	FGameplayTag AbilityAnimSetTag;

	/** Failsafe for a Blueprint child that never ends its montage/task flow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Lifecycle", meta = (ClampMin = "0.0"))
	float MaximumActiveDuration = 5.0f;

private:
	USovEchoComponent* ResolveEchoComponent(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool MeetsWeaponRequirement(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const;
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
	mutable bool bAuthorityEchoSpendAttempted = false;
	mutable bool bAuthorityEchoSpendSucceeded = false;
};

/** Sword signature: radial damage/Poise pressure, knockback, and a short ward. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinder Slam"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderSlam : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderSlam();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> RadialDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ProtectiveWardEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float SlamRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 900.0f;
};

/** Sword release: a slung inferno projectile with direct damage and Burn. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Velkorran's Hunger"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikVelkorransHunger : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikVelkorransHunger();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<ANarrativeProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> DirectDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> BurnEffectClass;
};

/** Universal utility: a thrown Cinder charge that sticks, fuses, and explodes. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinder Sticky Grenade"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderStickyGrenade : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderStickyGrenade();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<ANarrativeProjectile> GrenadeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> BurnEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float FuseDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float ExplosionRadius = 350.0f;
};

/** Cinderline release: direct explosive shot with impact and radial damage. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinder Judgement"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderJudgement : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderJudgement();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> DirectDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float ExplosionRadius = 300.0f;
};

/** Cinderline signature: a penetrating shot followed by a chained burning line. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinderline Requiem"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderlineRequiem : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderlineRequiem();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> PenetratingDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> LineDetonationEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> BurnEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumRange = 12000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float LineDetonationRadius = 220.0f;
};
