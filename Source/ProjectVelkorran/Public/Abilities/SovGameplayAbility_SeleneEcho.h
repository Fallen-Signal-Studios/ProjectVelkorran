// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "Components/SovCommandLinkComponent.h"
#include "SovGameplayAbility_SeleneEcho.generated.h"

class UGameplayEffect;
class ANarrativeProjectile;

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

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	ESovSeleneEchoWeaponFamily GetEchoWeaponFamily() const { return WeaponFamily; }

protected:
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

	/**
	 * Releases the authority-owned command-link portion of the pulse immediately.
	 * Native code also calls this automatically at full charge and from a normal
	 * early ability end, so Blueprint payload graphs do not need to invoke it.
	 * Repeated calls during one activation are safe no-ops.
	 *
	 * @return Number of command links that completed a new Sever transition.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Echo Ability|Axiom")
	int32 ReleaseAxiomNullPulseCommandLinks();

	/**
	 * Attempts the command-link portion of Null Pulse against one command node
	 * already selected by the authoritative pulse payload. Shield collapse,
	 * recharge suppression, and device disable remain independently authored.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Echo Ability|Axiom")
	ESovCommandLinkSeverResolution TrySeverAxiomCommandLink(
		AActor* CommandNode,
		FSovCommandLinkSeverResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability|Axiom")
	float GetAxiomFullChargeDuration() const
	{
		return FMath::Max(FullChargeDuration, 0.0f);
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability|Axiom")
	float GetAxiomPulseRange(float ChargeAlpha) const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability|Axiom")
	float GetAxiomPulseHalfAngleDegrees(float ChargeAlpha) const;

#if WITH_AUTOMATION_TESTS
	static bool IsLocationInsideAxiomPulse(
		const FVector& PulseOrigin,
		const FVector& PulseDirection,
		const FVector& CandidateLocation,
		float PulseRange,
		float PulseHalfAngleDegrees);
#endif

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

	virtual bool HasRequiredPayloadConfiguration() const override;

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
	float GetAuthorityChargeAlpha() const;
	void HandleAxiomFullChargeReached();
	int32 ProcessAuthorityCommandLinkPulse(float ChargeAlpha);
	static bool IsLocationInsideDirectedPulse(
		const FVector& PulseOrigin,
		const FVector& PulseDirection,
		const FVector& CandidateLocation,
		float PulseRange,
		float PulseHalfAngleDegrees);

	FTimerHandle AxiomFullChargeTimerHandle;
	double AuthorityChargeStartTimeSeconds = 0.0;
	bool bAuthorityCommandLinkPulseProcessed = false;
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
