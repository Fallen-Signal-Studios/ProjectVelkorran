// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/SovCombatTypes.h"
#include "TimerManager.h"
#include "Weapons/NarrativeProjectile.h"
#include "SovReformationDroneRocketProjectile.generated.h"

class AActor;
class FLifetimeProperty;
class UAbilitySystemComponent;
class UAudioComponent;
class UCameraShakeBase;
class UGameplayEffect;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class URadialForceComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

/** Initial flight data replicated as one unit for late-relevant clients. */
USTRUCT()
struct FSovReformationDroneRocketFlightState
{
	GENERATED_BODY()

	/** Immutable muzzle frame used by launch one-shots on every proxy. */
	UPROPERTY()
	FVector_NetQuantize10 LaunchLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator LaunchRotation = FRotator::ZeroRotator;

	/** Synchronized server clock used to suppress stale launch one-shots. */
	UPROPERTY()
	float ServerLaunchTime = 0.0f;

	UPROPERTY()
	FVector_NetQuantize10 InitialVelocity = FVector::ZeroVector;

	UPROPERTY()
	float GravityScale = 0.0f;

	UPROPERTY()
	TObjectPtr<AActor> HomingTarget = nullptr;

	UPROPERTY()
	float HomingAccelerationMagnitude = 0.0f;
};

/** One replicated resolution packet prevents partially ordered impact cosmetics. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovReformationDroneRocketResolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket")
	bool bResolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket")
	bool bExpired = false;

	/** Perfect Guard or an exhausted reflection budget dissipated the projectile without blast damage. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket")
	bool bAbsorbed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket")
	TObjectPtr<AActor> HitActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket")
	FVector_NetQuantize Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket")
	FVector_NetQuantizeNormal Normal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket")
	FName HitBone = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket")
	bool bDamagedAnyTarget = false;
};

/**
 * Replicated, server-authoritative rocket used by Reformation drone attacks.
 *
 * Authority owns collision, homing, hostile-target validation, radial falloff,
 * line of sight, Poise pressure, and GAS application. Every presentation slot
 * is optional and runs from replicated flight/resolution state on clients.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Reformation Drone Rocket Projectile"))
class PROJECTVELKORRAN_API ASovReformationDroneRocketProjectile : public ANarrativeProjectile
{
	GENERATED_BODY()

public:
	ASovReformationDroneRocketProjectile();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Copies the paid authoritative payload before FinishSpawning is called. */
	void InitializeRocket(
		UAbilitySystemComponent* InSourceAbilitySystem,
		AActor* InSourceAvatar,
		UObject* InDamageSourceObject,
		TSubclassOf<UGameplayEffect> InExplosionDamageEffectClass,
		const FGameplayTag& InAbilityIdentityTag,
		const FGameplayTagContainer& InDamageChannels,
		const FGameplayTagContainer& InAttackClassifications,
		float InEffectLevel,
		const FVector& InInitialVelocity,
		float InGravityScale,
		float InCollisionRadius,
		float InFlightDuration,
		float InExplosionRadius,
		float InExplosionDamage,
		float InExplosionPoiseDamage,
		float InMinimumDamageFraction,
		bool bInRequiresLineOfSight,
		AActor* InHomingTarget,
		float InHomingAccelerationMagnitude);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Rocket")
	bool HasResolved() const { return Resolution.bResolved; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Rocket")
	bool ExpiredInFlight() const
	{
		return Resolution.bResolved && Resolution.bExpired;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Rocket")
	FSovReformationDroneRocketResolution GetResolution() const
	{
		return Resolution;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	/** Assign the authored rocket art on a Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Components")
	TObjectPtr<UStaticMeshComponent> RocketMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Optional persistent trail component configured by the flight slots below. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Components")
	TObjectPtr<UNiagaraComponent> RocketTrailComponent;

	/** Optional persistent spatial flight loop. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Components")
	TObjectPtr<UAudioComponent> FlightAudioComponent;

	/** Select this component in a child Blueprint to tune the physical blast. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Components")
	TObjectPtr<URadialForceComponent> ExplosionRadialForce;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Physics")
	bool bApplyExplosionPhysicsImpulse = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Physics", meta = (EditCondition = "bApplyExplosionPhysicsImpulse", ClampMin = "0.0"))
	float ExplosionPhysicsRadiusScale = 1.0f;

	/** Lowers the force origin so floor-bound bodies receive some upward impulse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Physics", meta = (EditCondition = "bApplyExplosionPhysicsImpulse", ClampMin = "0.0", Units = "cm"))
	float ExplosionPhysicsUpwardBias = 65.0f;

	/** Time retained after resolution so the replicated presentation reaches clients. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation", meta = (ClampMin = "0.1", Units = "s"))
	float ResolutionCleanupDelay = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Flight")
	TObjectPtr<UNiagaraSystem> TrailNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Flight")
	FTransform TrailRelativeTransform = FTransform::Identity;

	/** Optional one-shot muzzle flash/backblast spawned with the rocket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Flight")
	TObjectPtr<UNiagaraSystem> LaunchNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Flight")
	FTransform LaunchNiagaraRelativeTransform = FTransform::Identity;

	/** Late-relevant proxies skip muzzle-only launch art after this age. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Flight", meta = (ClampMin = "0.0", Units = "s"))
	float LaunchOneShotMaximumAge = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Flight")
	TObjectPtr<USoundBase> LaunchSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Flight")
	TObjectPtr<USoundBase> FlightLoopSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Flight", meta = (ClampMin = "0.0"))
	float FlightLoopVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Flight", meta = (ClampMin = "0.01"))
	float FlightLoopPitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Flight")
	FTransform FlightAudioRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact")
	TObjectPtr<UNiagaraSystem> ExplosionNiagaraSystem;

	/** Composed with a frame whose X axis points along the replicated surface normal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact")
	FTransform ExplosionNiagaraRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Camera Shake")
	TSubclassOf<UCameraShakeBase> ExplosionCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Camera Shake", meta = (EditCondition = "ExplosionCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionCameraShakeInnerRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Camera Shake", meta = (EditCondition = "ExplosionCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionCameraShakeOuterRadius = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Camera Shake", meta = (EditCondition = "ExplosionCameraShakeClass != nullptr", ClampMin = "0.0"))
	float ExplosionCameraShakeFalloff = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Camera Shake", meta = (EditCondition = "ExplosionCameraShakeClass != nullptr"))
	bool bOrientCameraShakeTowardExplosion = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Decal")
	TObjectPtr<UMaterialInterface> ExplosionDecalMaterial;

	/** Decal projection depth, width, and height. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	FVector ExplosionDecalSize = FVector(18.0f, 190.0f, 190.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionDecalSurfaceSearchDistance = 275.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionDecalSurfaceOffset = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "s"))
	float ExplosionDecalVisibleDuration = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Impact|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "s"))
	float ExplosionDecalFadeDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Dissipation")
	TObjectPtr<UNiagaraSystem> DissipationNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Dissipation")
	FTransform DissipationNiagaraRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Rocket|Presentation|Dissipation")
	TObjectPtr<USoundBase> DissipationSound;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Reformation Drone|Rocket|Presentation", meta = (DisplayName = "Drone Rocket Launched"))
	void ReceiveRocketLaunched();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Reformation Drone|Rocket|Presentation", meta = (DisplayName = "Drone Rocket Impacted"))
	void ReceiveRocketImpacted(
		AActor* HitActor,
		FVector ImpactLocation,
		FVector ImpactNormal,
		FName HitBone,
		bool bDamagedAnyTarget);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Reformation Drone|Rocket|Presentation", meta = (DisplayName = "Drone Rocket Dissipated"))
	void ReceiveRocketDissipated(FVector DissipationLocation);

private:
	UFUNCTION()
	void HandleProjectileOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UFUNCTION()
	void OnRep_FlightState();

	UFUNCTION()
	void OnRep_Resolution();

	bool ResolveDirectDefense(const FHitResult& Hit);
	void ResumeReflectedFlight();
	UFUNCTION() void ReceiveDirectDefense(const FSovDamageResult& Result);
	friend struct FSovProjectileDefenseTestAccess;
	void StartProjectileMovement();
	void ConfigureHoming();
	void DeactivateProjectile();
	void ExpireProjectile();
	void ResolveImpact(const FHitResult& Hit);
	void ResolveExpired(bool bAbsorbed = false);
	bool ApplyExplosion(const FVector& ExplosionLocation, const FHitResult* DirectHit);
	void ApplyExplosionPhysicsImpulse(const FVector& ExplosionLocation);
	bool IsTargetAlive(const UAbilitySystemComponent* TargetAbilitySystem) const;
	bool HasExplosionLineOfSight(
		AActor* TargetActor,
		UAbilitySystemComponent* TargetAbilitySystem,
		const FVector& ExplosionLocation,
		const FVector& ExplosionNormal,
		AActor* DirectHitActor) const;
	bool FindExplosionDecalSurface(FHitResult& OutSurfaceHit) const;
	void SpawnExplosionDecal();
	void PlayLaunchPresentation();
	void StopFlightPresentation();
	void PlayResolutionPresentation();

	UPROPERTY(ReplicatedUsing = OnRep_FlightState)
	FSovReformationDroneRocketFlightState FlightState;

	UPROPERTY(ReplicatedUsing = OnRep_Resolution)
	FSovReformationDroneRocketResolution Resolution;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;

	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;
	TWeakObjectPtr<AActor> SourceAvatar;
	TWeakObjectPtr<UObject> DamageSourceObject;
	FGameplayTag AbilityIdentityTag;
	FGameplayTagContainer DamageChannels;
	FGameplayTagContainer AttackClassifications;
	float EffectLevel = 1.0f;
	float CollisionRadius = 14.0f;
	float FlightDuration = 5.0f;
	float ExplosionRadius = 450.0f;
	float ExplosionDamage = 55.0f;
	float ExplosionPoiseDamage = 35.0f;
	float MinimumDamageFraction = 0.3f;
	bool bRequiresLineOfSight = true;
	bool bPayloadInitialized = false;
	bool bCollisionArmed = false;
	bool bResolving = false;
	bool bFlightExpiredWhileResolving = false;
	bool bPlayedLaunchPresentation = false;
	bool bPlayedResolutionPresentation = false;
	FTimerHandle FlightTimerHandle;
	const FGameplayEffectContext* PendingDirectContext = nullptr;
	TWeakObjectPtr<UAbilitySystemComponent> DirectDamageTarget;
	FSovDamageResult DirectResult;
	bool bReceivedDirectResult = false;
	uint8 ReflectionCount = 0;
	/** A reflected rocket can be intercepted once more, then dissipates, preventing infinite ping-pong. */
	UPROPERTY(EditDefaultsOnly, Category="Sovereign|Reformation Drone|Rocket", meta=(ClampMin="0", ClampMax="4"))
	uint8 MaximumReflections = 1;

};
