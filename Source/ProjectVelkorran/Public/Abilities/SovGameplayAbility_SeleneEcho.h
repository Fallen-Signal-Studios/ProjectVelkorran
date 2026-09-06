// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "Components/SovCommandLinkComponent.h"
#include "Combat/SovSelenePayload.h"
#include "SovGameplayAbility_SeleneEcho.generated.h"

class UGameplayEffect;
class ANarrativeProjectile;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_WaitInputPress;
class ASovSeleneCombatProjectile;
class AWeaponVisual;

/** Weapon context used to organize Selene's Echo loadout in UI and content. */
UENUM(BlueprintType)
enum class ESovSeleneEchoWeaponFamily : uint8
{
	Universal UMETA(DisplayName = "Any Selene Weapon"),
	Verity UMETA(DisplayName = "Verity"),
	Staccato UMETA(DisplayName = "Staccato"),
	Axiom UMETA(DisplayName = "Axiom")
};

/** Selene adapter over the shared authoritative Echo lifecycle. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Selene Echo Ability Base"))
class PROJECTVELKORRAN_API USovGameplayAbility_SeleneEchoBase : public USovGameplayAbility_EchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_SeleneEchoBase();
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	ESovSeleneEchoWeaponFamily GetEchoWeaponFamily() const { return WeaponFamily; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	bool CanExecuteNativePayload(uint32 Epoch) const;
	bool ContinueNativePayload(uint32 Epoch);
	FSovSelenePayloadContext MakeNativePayloadContext() const;
	uint32 NativePayloadEpoch = 0;
	TWeakObjectPtr<UWeaponItem> NativeSourceWeapon;

	/** Result presentation only: gameplay is already applied by authority. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Echo Ability|Presentation")
	void ReceiveNativeSelenePayloadReleased(AActor* PayloadActor, FVector Origin, FVector Direction);

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Identity")
	ESovSeleneEchoWeaponFamily WeaponFamily = ESovSeleneEchoWeaponFamily::Universal;
};

/** Universal area lockdown: a delayed Stasis field with Freeze and cryothermal DoT. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Selene: Stillpoint Grenade"))
class PROJECTVELKORRAN_API USovGameplayAbility_SeleneStillpointGrenade : public USovGameplayAbility_SeleneEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_SeleneStillpointGrenade();

protected:
	virtual bool HasRequiredPayloadConfiguration() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float DetonationDamage = 0.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float FrozenDamagePerSecond = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<ANarrativeProjectile> GrenadeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> DetonationDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ChillEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> FreezeEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> FrozenDamageOverTimeEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ResistantTargetDamageOverTimeEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float FuseDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float StasisRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float StasisDuration = 3.5f;

	/** Prevents a persistent field from repeatedly hard-freezing the same target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float RefreezeLockout = 3.0f;
};

/** Verity signature: a steerable outbound arc with manual or timed recall. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Selene: Dispatch"))
class PROJECTVELKORRAN_API USovGameplayAbility_SeleneDispatch : public USovGameplayAbility_SeleneEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_SeleneDispatch();

protected:
	virtual bool HasRequiredPayloadConfiguration() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;


	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Authored Verity items identify only their own equipped/holstered visual. Inventory is never removed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation")
	TArray<TSubclassOf<UWeaponItem>> VerityWeaponClasses;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float DamagePerLeg = 70.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float PoiseDamagePerLeg = 35.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float ShatterBonusPoise = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<ANarrativeProjectile> ReturningVerityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> OutboundDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ReturnDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> FrozenShatterEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float MaximumOutboundDuration = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumOutboundDistance = 2400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm/s"))
	float OutboundSpeed = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ReturnSpeed = 2600.0f;

	/** Server-clamped steering rate; the client never supplies projectile position. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", ClampMax = "720.0", Units = "deg/s"))
	float MaximumSteeringDegreesPerSecond = 180.0f;
private:
	UFUNCTION()
	void HandleDispatchRecallPressed(float TimeWaited);
	UFUNCTION()
	void HandleDispatchFinished(ASovSeleneCombatProjectile* Projectile, bool bReturned);
	void UpdateDispatch(uint32 Epoch);
	void RestoreDispatchPresentation();
	UPROPERTY(Transient)
	TObjectPtr<ASovSeleneCombatProjectile> ActiveDispatch;
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputPress> RecallInputTask;
	TMap<TWeakObjectPtr<AWeaponVisual>, bool> HiddenVerityVisuals;
	FActiveGameplayEffectHandle VerityAbsentEffect;
	TWeakObjectPtr<UAbilitySystemComponent> DispatchSourceASC;
	TWeakObjectPtr<UWeaponItem> DispatchMainWeapon;
	TWeakObjectPtr<UWeaponItem> DispatchOffWeapon;
	FTimerHandle DispatchWatchdog;
	uint32 DispatchTaskEpoch = 0;
	bool bDispatchRecallPending = false;
	bool bEndingDispatch = false;

};

/** Staccato precision technique: one empowered shot with deterministic control. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Selene: Staccato Zero"))
class PROJECTVELKORRAN_API USovGameplayAbility_SeleneStaccatoZero : public USovGameplayAbility_SeleneEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_SeleneStaccatoZero();

protected:
	virtual bool HasRequiredPayloadConfiguration() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float BaseShotDamage = 60.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float ShotPoiseDamage = 30.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.01"))
	float FreezeDuration = 2.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float RefreezeLockout = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> EmpoweredShotDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> FreezeEffectClass;

	/** Applied instead of hard Freeze when target immunity/CC tier rejects it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ResistantTargetChillEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumRange = 15000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.75f;
};

/** Axiom anti-shield technique: charged Disruption pressure and suppression. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Selene: Axiom Null Pulse"))
class PROJECTVELKORRAN_API USovGameplayAbility_SeleneAxiomNullPulse : public USovGameplayAbility_SeleneEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_SeleneAxiomNullPulse();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** Release uses the authority clock and aim; callers cannot supply targets or charge. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Echo Ability|Axiom")
	bool ReleaseAxiomNullPulseFromAim();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability|Axiom")
	float GetAxiomChargeAlpha() const;

	/**
	 * Attempts the command-link portion of Null Pulse against one command node
	 * authorized by the native release. Calls outside that transaction fail closed.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Echo Ability|Axiom")
	ESovCommandLinkSeverResolution TrySeverAxiomCommandLink(
		AActor* CommandNode,
		FSovCommandLinkSeverResult& OutResult);

protected:
	virtual bool HasRequiredPayloadConfiguration() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Authority-only cosmetic result. Gameplay has already completed. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Echo Ability|Axiom|Presentation")
	void ReceiveAxiomPulseReleased(FVector Origin, FVector Direction, float ChargeAlpha,
		float Range, float HalfAngleDegrees, int32 ShieldTargets,
		int32 DisabledDevices, int32 SeveredLinks);

	/** Biological enemies are never disabled unless their exact archetype opts in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Axiom")
	TArray<TSubclassOf<AActor>> AdditionalDisableTargetClasses;

	/** Boss devices resist hard shutdown by default. Shield/link rules remain separate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Axiom")
	bool bAllowBossDeviceDisable = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Lifecycle", meta = (ClampMin = "0.0", Units = "s"))
	float PostPulseRecovery = 0.25f;

	/** Must route through NarrativeDamageExecCalc with HealthCoefficient set to zero. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ShieldDisruptionDamageEffectClass;

	/** Duration GE should grant Sov.State.Shield.RechargeBlocked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ShieldRechargeBlockEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> DeviceDisableEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float FullChargeDuration = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumPulseRange = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumPulseRange = 4000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float MinimumPulseHalfAngleDegrees = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float MinimumShieldSuppressionDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float MaximumPulseHalfAngleDegrees = 22.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float MaximumShieldSuppressionDuration = 4.0f;

private:
	friend struct FSovAxiomNullPulseTestAccess;
	bool IsCurrentAxiomActivation(uint32 Epoch) const;
	bool ContinueAxiomRelease(uint32 Epoch);
	bool CanReleaseAxiomPulse() const;
	bool IsAxiomTargetInPulse(AActor* Target) const;
	bool HasAxiomLineOfSight(AActor* Target, const FVector& TargetPoint) const;
	bool IsAxiomTargetEligible(AActor* Target, UAbilitySystemComponent* TargetASC) const;
	bool ApplyAxiomShieldCollapse(UAbilitySystemComponent* TargetASC);
	bool ApplyAxiomDurationEffect(UAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> EffectClass, FGameplayTag GrantedTag, float Duration);
	bool IsAxiomDeviceEligible(AActor* Target, UAbilitySystemComponent* TargetASC) const;
	void ClearAxiomTasksAndTimers();
	void StartAxiomRecovery(uint32 Epoch);

	UFUNCTION()
	void HandleAxiomInputReleased(float TimeHeld);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitInputRelease> AxiomInputReleaseTask;
	TWeakObjectPtr<UWeaponItem> ExpectedWeapon;
	TWeakObjectPtr<AActor> AuthorizedCommandNode;
	FTimerHandle AxiomFullChargeTimer;
	FTimerHandle AxiomRecoveryTimer;
	uint32 ActivationEpoch = 0;
	uint32 ReleaseTaskEpoch = 0;
	double ChargeStartWorldTime = 0.0;
	bool bNativeLifecycleReady = false;
	bool bPulseReleased = false;
	FVector ReleaseOrigin = FVector::ZeroVector;
	FVector ReleaseDirection = FVector::ForwardVector;
	float ReleasedChargeAlpha = 0.0f;
	float ReleasedRange = 0.0f;
	float ReleasedHalfAngle = 0.0f;
};

/** Verity lane technique: an advancing wave that Chills and conditionally Freezes. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Selene: Verity's Wake"))
class PROJECTVELKORRAN_API USovGameplayAbility_SeleneVeritysWake : public USovGameplayAbility_SeleneEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_SeleneVeritysWake();

protected:
	virtual bool HasRequiredPayloadConfiguration() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;


	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float WaveDamage = 50.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float WavePoiseDamage = 20.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.01"))
	float ControlDuration = 3.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float FrostDamagePerSecond = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<ANarrativeProjectile> WaveClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> WaveDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ChillEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> FreezeEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> FrostDamageOverTimeEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float WaveRange = 2200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float WaveWidth = 500.0f;

	/** Centerline hits or already-Chilled targets Freeze deterministically. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float GuaranteedFreezeCenterlineWidth = 120.0f;
};
