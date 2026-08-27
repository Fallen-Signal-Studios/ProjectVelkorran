// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeCombatAbility.h"
#include "TimerManager.h"
#include "SovGameplayAbility_ReformationDrone.generated.h"

class ASovReformationDroneGunshotPresentation;
class ASovReformationDroneRocketProjectile;
class UAbilitySystemComponent;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UGameplayEffect;

/**
 * Shared server-authoritative shell for integral Reformation drone weapons.
 *
 * These attacks are granted by an NPC Ability Configuration rather than a
 * Narrative weapon item. Native timers own release timing so dedicated server
 * gameplay never depends on animation notifies, while GAS montage playback
 * still replicates authored attack animation to clients.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Reformation Drone Weapon Base"))
class PROJECTVELKORRAN_API USovGameplayAbility_ReformationDroneWeaponBase : public UNarrativeCombatAbility
{
	GENERATED_BODY()

public:
	USovGameplayAbility_ReformationDroneWeaponBase();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Animation")
	UAnimMontage* GetAttackMontage() const { return AttackMontage.Get(); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Weapon")
	FGameplayTag GetDroneWeaponAbilityTag() const { return AbilityIdentityTag; }

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

	virtual bool HasRequiredPayloadConfiguration() const;
	virtual void ExecuteAutomaticPayload();

	/**
	 * Enters the payload phase exactly once. Manual Blueprint release calls and
	 * the native release timer share this gate so an authored authority event
	 * can safely beat the fallback timer without firing the weapon twice.
	 */
	bool TryBeginWeaponPayloadRelease();

	/** Marks the authoritative payload complete and starts authored recovery. */
	void NotifyPayloadFinished();

	/** Cancels the ability and clears every native release/recovery timer. */
	void CancelDroneWeaponAbility();

	/** Returns whether the source is still allowed to release another shot. */
	bool CanContinueWeaponPayload() const;

	/** Lets concrete weapon implementations observe base lifecycle state safely. */
	bool HasWeaponPayloadFinished() const { return bPayloadFinished; }

	/** Resolves a drone-mesh muzzle socket, then falls back to a local offset. */
	FTransform ResolveMuzzleTransform(int32 MuzzleIndex) const;

	/** Authority-owned focus/control-rotation aim trace. */
	FVector ResolveAuthorityAimPoint(float TraceDistance);

	/** Applies one point hit through the shared Sovereign damage execution. */
	bool ApplyPointDamage(
		const FHitResult& Hit,
		float Damage,
		float PoiseDamage) const;

	bool IsHostileTarget(const UAbilitySystemComponent* TargetAbilitySystem) const;
	bool IsTargetAlive(const UAbilitySystemComponent* TargetAbilitySystem) const;

	/** Native identity tag placed on every outgoing damage spec. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Identity")
	FGameplayTag AbilityIdentityTag;

	/** Damage channels attached to each outgoing damage spec. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Damage")
	FGameplayTagContainer DamageChannels;

	/** Guard/attack classifications attached to each outgoing damage spec. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Damage")
	FGameplayTagContainer AttackClassifications;

	/** Instant effect routed through UNarrativeDamageExecCalc. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Optional authored attack montage played through GAS on authority. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Animation")
	FName MontageStartSection = NAME_None;

	/** If true, native authority timing releases the payload automatically. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Timing")
	bool bAutoReleasePayload = true;

	/** Delay from activation to the authoritative release frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Timing", meta = (EditCondition = "bAutoReleasePayload", ClampMin = "0.0", Units = "s"))
	float PayloadReleaseDelay = 0.1f;

	/** Recovery after the final shot or projectile launch. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float PostFireRecovery = 0.2f;

	/** Authoritative per-ability cooldown independent of bot decision cadence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownDuration = 0.5f;

	/** Watchdog for an authored child that never releases or finishes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Timing", meta = (ClampMin = "0.1", Units = "s"))
	float MaximumActiveDuration = 5.0f;

	/** Integral weapon sockets on the drone skeletal mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Muzzle")
	TArray<FName> MuzzleSocketNames;

	/** Avatar-local fallback used when no configured socket exists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Muzzle", meta = (Units = "cm"))
	FVector FallbackMuzzleOffset = FVector(100.0f, 0.0f, 40.0f);

	/** Called on authority after commit, before native release timing begins. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Reformation Drone|Ability", meta = (DisplayName = "Drone Weapon Started"))
	void ReceiveDroneWeaponStarted();

	/** Called on authority when either native or authored release begins. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Reformation Drone|Ability", meta = (DisplayName = "Drone Weapon Payload Released"))
	void ReceiveDroneWeaponPayloadReleased();

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Reformation Drone|Ability", meta = (DisplayName = "Drone Weapon Ended"))
	void ReceiveDroneWeaponEnded(bool bWasCancelled);

private:
	UFUNCTION()
	void HandleAutomaticPayloadRelease();

	UFUNCTION()
	void HandleRecoveryFinished();

	UFUNCTION()
	void HandleMaximumDurationExpired();

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	void StartAttackMontage();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FTimerHandle PayloadReleaseTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FTimerHandle MaximumDurationTimerHandle;
	double NextAllowedActivationTime = 0.0;
	bool bPayloadStarted = false;
	bool bPayloadFinished = false;
	bool bAbilityStarted = false;
	bool bEndingAbility = false;
};

/** Server-owned hitscan burst with replicated per-shot presentation. */
UCLASS(Blueprintable, meta = (DisplayName = "Reformation Drone: Gunfire"))
class PROJECTVELKORRAN_API USovGameplayAbility_ReformationDroneGunfire : public USovGameplayAbility_ReformationDroneWeaponBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_ReformationDroneGunfire();

	/** Starts the native burst immediately; safe to call from an authored server release event. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Reformation Drone|Gunfire")
	void FireGunBurstFromAim();

protected:
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual bool HasRequiredPayloadConfiguration() const override;
	virtual void ExecuteAutomaticPayload() override;
	virtual float GetAttackDamage_Implementation() const override;

	/** Optional replicated cosmetic packet class; gameplay still fires when empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Gunfire|Presentation")
	TSubclassOf<ASovReformationDroneGunshotPresentation> GunshotPresentationClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Gunfire|Damage", meta = (ClampMin = "0.0"))
	float DamagePerShot = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Gunfire|Damage", meta = (ClampMin = "0.0"))
	float PoiseDamagePerShot = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Gunfire|Burst", meta = (ClampMin = "1", ClampMax = "30"))
	int32 BurstShotCount = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Gunfire|Burst", meta = (ClampMin = "0.01", Units = "s"))
	float TimeBetweenShots = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Gunfire|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumRange = 5000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Gunfire|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRadius = 0.0f;

	/** Full cone angle applied independently to each authoritative shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Gunfire|Targeting", meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
	float SpreadDegrees = 1.25f;

private:
	UFUNCTION()
	void FireNextBurstShot();

	void SpawnGunshotPresentation(
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const FHitResult* Hit,
		bool bDamagedTarget,
		int32 ShotIndex) const;

	FTimerHandle BurstTimerHandle;
	int32 ShotsFired = 0;
	bool bBurstStarted = false;
};

/** Server-spawned rocket carrying its own delayed radial damage payload. */
UCLASS(Blueprintable, meta = (DisplayName = "Reformation Drone: Rocket Launcher"))
class PROJECTVELKORRAN_API USovGameplayAbility_ReformationDroneRocketLauncher : public USovGameplayAbility_ReformationDroneWeaponBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_ReformationDroneRocketLauncher();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Reformation Drone|Rocket")
	ASovReformationDroneRocketProjectile* LaunchRocketFromAim();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Reformation Drone|Rocket")
	ASovReformationDroneRocketProjectile* LaunchRocket(
		const FTransform& SpawnTransform,
		FVector InitialVelocity,
		AActor* HomingTarget = nullptr);

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual bool HasRequiredPayloadConfiguration() const override;
	virtual void ExecuteAutomaticPayload() override;
	virtual float GetAttackDamage_Implementation() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Payload")
	TSubclassOf<ASovReformationDroneRocketProjectile> RocketClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float ExplosionRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Payload", meta = (ClampMin = "0.0"))
	float ExplosionDamage = 55.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Payload", meta = (ClampMin = "0.0"))
	float ExplosionPoiseDamage = 35.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Payload", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumExplosionDamageFraction = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Payload")
	bool bExplosionRequiresLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Launch", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RocketSpeed = 2600.0f;

	/** Distance of the authority-owned focus/control-rotation aim trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Launch", meta = (ClampMin = "0.0", Units = "cm"))
	float RocketAimTraceDistance = 8000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Launch", meta = (ClampMin = "0.0"))
	float RocketGravityScale = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Launch", meta = (ClampMin = "1.0", Units = "cm"))
	float RocketCollisionRadius = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Launch", meta = (ClampMin = "0.1", Units = "s"))
	float RocketFlightDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Launch", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaximumRocketSpeed = 5000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Launch", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumRocketSpawnDistance = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Homing")
	bool bEnableHoming = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Homing", meta = (EditCondition = "bEnableHoming", ClampMin = "0.0", Units = "cm/s^2"))
	float HomingAccelerationMagnitude = 6500.0f;

private:
	TSubclassOf<ASovReformationDroneRocketProjectile> ResolveRocketClass() const;
	AActor* ResolveHomingTarget() const;
	bool bRocketReleaseAttempted = false;
	int32 NextMuzzleIndex = 0;
};
