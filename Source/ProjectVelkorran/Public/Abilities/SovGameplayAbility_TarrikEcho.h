// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "SovGameplayAbility_TarrikEcho.generated.h"

class UGameplayEffect;
class ANarrativeProjectile;
class ASovCinderStickyGrenadeProjectile;

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

protected:
	virtual bool HasRequiredPayloadConfiguration() const override;

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

protected:
	virtual bool HasRequiredPayloadConfiguration() const override;

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
