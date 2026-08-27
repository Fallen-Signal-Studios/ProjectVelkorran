// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "SovGameplayAbility_TarrikEcho.generated.h"

class UGameplayEffect;
class ASovCinderStickyGrenadeProjectile;
class ASovCinderJudgementPresentation;
class ASovVelkorransHungerProjectile;

/** Weapon context used to organize Tarrik's Echo loadout in UI and content. */
UENUM(BlueprintType)
enum class ESovTarrikEchoWeaponFamily : uint8
{
	Universal UMETA(DisplayName = "Either Weapon"),
	Velkorran UMETA(DisplayName = "Velkorran (Sword)"),
	Cinderline UMETA(DisplayName = "Cinderline")
};

/** Compatibility-preserving Tarrik adapter over the shared Echo lifecycle. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik Echo Ability Base"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikEchoBase : public USovGameplayAbility_EchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikEchoBase();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	ESovTarrikEchoWeaponFamily GetEchoWeaponFamily() const { return WeaponFamily; }

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Identity")
	ESovTarrikEchoWeaponFamily WeaponFamily = ESovTarrikEchoWeaponFamily::Universal;
};

/** Sword signature: radial damage/Poise pressure, knockback, and a short ward. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinder Slam"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderSlam : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderSlam();

protected:
	virtual bool HasRequiredPayloadConfiguration() const override;

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

	/**
	 * Resolves the active Velkorran visual, traces the server-owned aim, and
	 * releases one authoritative blade-wave projectile. This is the normal
	 * Blueprint release-frame entry point.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Echo Ability|Payload")
	ASovVelkorransHungerProjectile* ReleaseVelkorransHungerFromAim();

	/**
	 * Advanced release path for callers with a custom server-validated spawn
	 * transform and velocity. Most children should use the aim-driven helper.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Echo Ability|Payload")
	ASovVelkorransHungerProjectile* ReleaseVelkorransHunger(
		const FTransform& SpawnTransform,
		FVector InitialVelocity);

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual bool HasRequiredPayloadConfiguration() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<ASovVelkorransHungerProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> DirectDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> BurnEffectClass;

	/** SetByCaller direct damage routed through UNarrativeDamageExecCalc. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float DirectDamage = 65.0f;

	/** Explicit Poise pressure applied by the blade-wave impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float DirectPoiseDamage = 30.0f;

	/** SetByCaller damage supplied to each tick of the shared Cinder Burn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float BurnDamagePerTick = 5.0f;

	/** SetByCaller duration supplied to the shared Cinder Burn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float BurnDuration = 4.0f;

	/** Socket on Velkorran's active weapon visual used for the release location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch")
	FName HungerReleaseSocketName = TEXT("HungerRelease");

	/** Avatar-local fallback used when Velkorran's visual or socket is unavailable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (Units = "cm"))
	FVector HungerFallbackSpawnOffset = FVector(95.0f, 20.0f, 75.0f);

	/** Distance of the authoritative camera/control-rotation aim trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.0", Units = "cm"))
	float HungerAimTraceDistance = 5000.0f;

	/** Native blade-wave travel speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.0", Units = "cm/s"))
	float HungerProjectileSpeed = 4200.0f;

	/** Gravity multiplier copied to the projectile; zero produces a straight wave. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.0"))
	float HungerProjectileGravityScale = 0.0f;

	/** Server clamp for custom launch velocities. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaximumHungerProjectileSpeed = 6000.0f;

	/** Authoritative collision radius of the traveling blade wave. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "1.0", Units = "cm"))
	float HungerProjectileCollisionRadius = 35.0f;

	/** Maximum flight time before the wave dissipates harmlessly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.1", Units = "s"))
	float HungerProjectileFlightDuration = 1.25f;

	/** Rejects a custom release transform that is not near Tarrik. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumHungerSpawnDistance = 600.0f;

private:
	TSubclassOf<ASovVelkorransHungerProjectile> ResolveHungerProjectileClass() const;
	TSubclassOf<UGameplayEffect> ResolveHungerDamageEffectClass() const;
	TSubclassOf<UGameplayEffect> ResolveHungerBurnEffectClass() const;
	bool bHungerReleaseAttempted = false;
};

/** Universal utility: a thrown Cinder charge that sticks, fuses, and explodes. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinder Sticky Grenade"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderStickyGrenade : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderStickyGrenade();

	/**
	 * Resolves Tarrik's throw socket, traces the server-owned aim, calculates a
	 * ballistic launch velocity, and releases one authoritative grenade.
	 * This is the normal Blueprint release-frame entry point.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Echo Ability|Payload")
	ASovCinderStickyGrenadeProjectile* ReleaseCinderStickyGrenadeFromAim();

	/**
	 * Advanced release path for callers that need to supply a custom server-
	 * validated transform and velocity. Most Blueprint children should call
	 * ReleaseCinderStickyGrenadeFromAim instead.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Echo Ability|Payload")
	ASovCinderStickyGrenadeProjectile* ReleaseCinderStickyGrenade(
		const FTransform& SpawnTransform,
		FVector InitialVelocity);

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual bool HasRequiredPayloadConfiguration() const override;
	virtual bool MeetsWeaponRequirement(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<ASovCinderStickyGrenadeProjectile> GrenadeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> BurnEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float FuseDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float ExplosionRadius = 350.0f;

	/** SetByCaller base damage routed through UNarrativeDamageExecCalc. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float ExplosionDamage = 40.0f;

	/** Explicit Poise pressure at the blast center before radial falloff. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float ExplosionPoiseDamage = 20.0f;

	/** Damage/Poise fraction retained at the edge of the blast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumExplosionDamageFraction = 0.5f;

	/** SetByCaller damage supplied to each tick of the authored Burn effect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float BurnDamagePerTick = 5.0f;

	/** SetByCaller duration supplied to the authored Burn effect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float BurnDuration = 4.0f;

	/** Character-mesh socket used as the authoritative release location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch")
	FName GrenadeThrowSocketName = TEXT("hand_r");

	/** Local-space spawn offset used when the configured socket cannot be found. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (Units = "cm"))
	FVector GrenadeFallbackSpawnOffset = FVector(45.0f, 20.0f, 65.0f);

	/** Distance of the authoritative camera/control-rotation aim trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.0", Units = "cm"))
	float GrenadeAimTraceDistance = 2500.0f;

	/** Fixed speed used by the native ballistic solver and zero-velocity fallback. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.0", Units = "cm/s"))
	float DefaultGrenadeLaunchSpeed = 1600.0f;

	/** Selects the higher of the two valid ballistic solutions when available. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch")
	bool bUseHighGrenadeThrowArc = false;

	/** Used only when the fixed-speed ballistic target is physically unreachable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "-89.0", ClampMax = "89.0", Units = "deg"))
	float GrenadeFallbackThrowPitch = 35.0f;

	/** Multiplier applied to world gravity by both the solver and projectile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.0"))
	float GrenadeGravityScale = 1.0f;

	/** Server clamp for authored launch velocity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaximumGrenadeLaunchSpeed = 3000.0f;

	/** Rejects an accidental release transform that is not near Tarrik. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Launch", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumGrenadeSpawnDistance = 300.0f;

	/** Prevents radial damage through blocking world geometry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	bool bExplosionRequiresLineOfSight = true;

private:
	TSubclassOf<ASovCinderStickyGrenadeProjectile> ResolveGrenadeClass() const;
	TSubclassOf<UGameplayEffect> ResolveExplosionDamageEffectClass() const;
	TSubclassOf<UGameplayEffect> ResolveBurnEffectClass() const;
	bool bGrenadeReleaseAttempted = false;
};

/** Cinderline release: direct explosive shot with impact and radial damage. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinder Judgement"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderJudgement : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderJudgement();

	/**
	 * Resolves Cinderline's muzzle and performs one authority-owned two-stage
	 * trace. The direct hit and controlled blast are both applied exactly once.
	 * A Blueprint release event may call this before the native fallback timer.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Echo Ability|Cinder Judgement")
	bool ReleaseCinderJudgementFromAim();

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

	/**
	 * Impact-origin actor and replicated cosmetic packet. A native fallback is
	 * always used when this slot is empty or its authored class cannot spawn.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Presentation")
	TSubclassOf<ASovCinderJudgementPresentation> PresentationClass;

	/** Must derive from the native Judgement damage shell; invalid classes fall back safely. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> DirectDamageEffectClass;

	/** Must derive from the native Judgement damage shell; invalid classes fall back safely. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float ExplosionRadius = 325.0f;

	/** Base impact damage before attack rating, mitigation, and hit-zone scaling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Direct Damage", meta = (ClampMin = "0.0"))
	float DirectDamage = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Direct Damage", meta = (ClampMin = "0.0"))
	float DirectPoiseDamage = 30.0f;

	/** Multiplier applied only while damage is absorbed by Shield. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Direct Damage", meta = (ClampMin = "0.0"))
	float DirectShieldCoefficient = 1.5f;

	/** Center damage of the controlled blast. A surviving direct target can receive both packets. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Explosion", meta = (ClampMin = "0.0"))
	float ExplosionDamage = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Explosion", meta = (ClampMin = "0.0"))
	float ExplosionPoiseDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Explosion", meta = (ClampMin = "0.0"))
	float ExplosionShieldCoefficient = 1.25f;

	/** Damage and Poise fraction retained at the outer edge of the blast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Explosion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumExplosionDamageFraction = 0.35f;

	/** Prevents radial damage through blocking world geometry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Explosion")
	bool bExplosionRequiresLineOfSight = true;

	/** When false, an unobstructed maximum-range shot dissipates without a blast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Explosion")
	bool bExplodeAtMaximumRange = false;

	/** Optional impulse applied to simulated props and ragdolls by the blast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Explosion|Physics")
	bool bApplyExplosionPhysicsImpulse = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Explosion|Physics", meta = (EditCondition = "bApplyExplosionPhysicsImpulse", ClampMin = "0.0", Units = "cm/s"))
	float ExplosionPhysicsImpulseStrength = 1600.0f;

	/** Lowers the impulse origin so nearby bodies receive a useful upward component. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Explosion|Physics", meta = (EditCondition = "bApplyExplosionPhysicsImpulse", ClampMin = "0.0", Units = "cm"))
	float ExplosionPhysicsUpwardBias = 35.0f;

	/** Socket on Cinderline's active weapon visual used as the trace origin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Targeting")
	FName MuzzleSocketName = TEXT("Muzzle");

	/** Avatar-local fallback used while the weapon visual is unavailable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Targeting", meta = (Units = "cm"))
	FVector FallbackMuzzleOffset = FVector(95.0f, 15.0f, 70.0f);

	/** Rejects a broken weapon-visual socket transform far from Tarrik. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumMuzzleDistance = 500.0f;

	/** Optional sphere radius for the authority-owned muzzle trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRadius = 0.0f;

	/** Native release timing keeps dedicated-server gameplay independent of an Anim Notify. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Timing")
	bool bAutoReleasePayload = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Timing", meta = (EditCondition = "bAutoReleasePayload", ClampMin = "0.0", Units = "s"))
	float PayloadReleaseDelay = 0.28f;

	/** Busy-state recovery after the shot has been resolved. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Cinder Judgement|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float PostReleaseRecovery = 0.45f;

private:
	UFUNCTION()
	void HandleAutomaticJudgementRelease();

	UFUNCTION()
	void HandleJudgementRecoveryFinished();

	TSubclassOf<UGameplayEffect> ResolveJudgementDirectEffectClass() const;
	TSubclassOf<UGameplayEffect> ResolveJudgementExplosionEffectClass() const;
	FTransform ResolveJudgementMuzzleTransform() const;
	FVector ResolveJudgementAuthorityAimPoint();
	bool ApplyJudgementDamage(
		class UAbilitySystemComponent* TargetAbilitySystem,
		const FGameplayEffectContextHandle& Context,
		TSubclassOf<UGameplayEffect> EffectClass,
		float Damage,
		float PoiseDamage,
		float ShieldCoefficient,
		float SourceModifier) const;
	int32 ApplyJudgementExplosion(
		const FVector& Origin,
		const FVector& SurfaceNormal,
		AActor* DirectHitActor,
		AActor* ExplosionDamageCauser) const;
	bool HasJudgementExplosionLineOfSight(
		const FVector& Origin,
		class UAbilitySystemComponent* TargetAbilitySystem,
		AActor* DirectHitActor) const;
	void ApplyJudgementPhysicsImpulse(
		const FVector& Origin,
		const FVector& SurfaceNormal) const;
	bool HasJudgementPhysicsLineOfSight(
		const FVector& Origin,
		class UPrimitiveComponent* TargetComponent) const;
	ASovCinderJudgementPresentation* SpawnDeferredJudgementPresentation(
		const FVector& TraceStart,
		const FVector& TraceEnd) const;
	void FinishJudgementPresentation(
		ASovCinderJudgementPresentation* Presentation,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const FHitResult* Hit,
		bool bBlastTriggered,
		bool bDirectDamageResolved,
		int32 RadialTargetsResolved) const;
	void BeginJudgementRecovery();

	FTimerHandle JudgementReleaseTimerHandle;
	FTimerHandle JudgementRecoveryTimerHandle;
	bool bJudgementReleaseAttempted = false;
};

/** Cinderline signature: a penetrating shot followed by a chained burning line. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinderline Requiem"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderlineRequiem : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderlineRequiem();

protected:
	virtual bool HasRequiredPayloadConfiguration() const override;

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
