// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Weapons/NarrativeProjectile.h"
#include "SovCinderStickyGrenadeProjectile.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UMaterialInterface;
class UNiagaraSystem;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class URadialForceComponent;
class USphereComponent;
class UStaticMeshComponent;
struct FGameplayEffectContextHandle;
class FLifetimeProperty;

/**
 * Server-authoritative gameplay projectile for Tarrik's Cinder Sticky Grenade.
 *
 * The native actor owns collision, sticking, fuse timing, target validation,
 * and GAS effect application. A Blueprint child should supply the mesh, audio,
 * Niagara, and other presentation through the cosmetic events below.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Cinder Sticky Grenade Projectile"))
class PROJECTVELKORRAN_API ASovCinderStickyGrenadeProjectile : public ANarrativeProjectile
{
	GENERATED_BODY()

public:
	ASovCinderStickyGrenadeProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Copies the paid payload into this actor before FinishSpawning is called. */
	void InitializeGrenade(
		UAbilitySystemComponent* InSourceAbilitySystem,
		AActor* InSourceAvatar,
		UObject* InDamageSourceObject,
		TSubclassOf<UGameplayEffect> InExplosionDamageEffectClass,
		TSubclassOf<UGameplayEffect> InBurnEffectClass,
		const FGameplayTag& InAbilityIdentityTag,
		float InEffectLevel,
		const FVector& InInitialVelocity,
		float InGravityScale,
		float InFuseDuration,
		float InExplosionRadius,
		float InExplosionDamage,
		float InExplosionPoiseDamage,
		float InMinimumDamageFraction,
		float InBurnDamagePerTick,
		float InBurnDuration,
		bool bInRequiresLineOfSight);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability|Cinder Sticky Grenade")
	bool IsStuck() const { return bIsStuck; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability|Cinder Sticky Grenade")
	bool HasDetonated() const { return bHasDetonated; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Components")
	TObjectPtr<UStaticMeshComponent> GrenadeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/**
	 * One-shot physics blast fired by the authoritative detonation. Select this
	 * component in a projectile Blueprint to tune strength, falloff, mass
	 * handling, and the physics object types that should be affected.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Components")
	TObjectPtr<URadialForceComponent> ExplosionRadialForce;

	/** Enables the radial physics impulse without changing GAS damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Physics")
	bool bApplyExplosionPhysicsImpulse = true;

	/** Multiplies the configured gameplay explosion radius for the physics blast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Physics", meta = (EditCondition = "bApplyExplosionPhysicsImpulse", ClampMin = "0.0"))
	float ExplosionPhysicsRadiusScale = 1.0f;

	/**
	 * Moves the radial-force origin below the grenade so nearby bodies receive
	 * an upward component instead of being driven flat along the ground.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Physics", meta = (EditCondition = "bApplyExplosionPhysicsImpulse", ClampMin = "0.0", Units = "cm"))
	float ExplosionPhysicsUpwardBias = 75.0f;

	/** Time retained after detonation so replicated cosmetic state reaches clients. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation", meta = (ClampMin = "0.1", Units = "s"))
	float DetonationCleanupDelay = 0.5f;

	/** Optional one-shot Niagara system spawned automatically at detonation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation")
	TObjectPtr<UNiagaraSystem> ExplosionNiagaraSystem;

	/** World scale used when spawning the configured explosion system. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation")
	FVector ExplosionNiagaraScale = FVector::OneVector;

	/** Optional scorch/debris decal projected onto the nearest world surface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation|Decal")
	TObjectPtr<UMaterialInterface> ExplosionDecalMaterial;

	/** Decal projection depth, width, and height. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	FVector ExplosionDecalSize = FVector(16.0f, 175.0f, 175.0f);

	/** Maximum distance searched for the nearest ground, wall, or other world surface. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionDecalSurfaceSearchDistance = 250.0f;

	/** Small separation from the hit surface to prevent depth fighting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionDecalSurfaceOffset = 1.0f;

	/** Time the decal remains fully visible before fading begins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "s"))
	float ExplosionDecalVisibleDuration = 5.0f;

	/** Fade-out duration after the fully visible period. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "s"))
	float ExplosionDecalFadeDuration = 1.0f;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Echo Ability|Presentation", meta = (DisplayName = "Cinder Grenade Launched"))
	void ReceiveGrenadeLaunched();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Echo Ability|Presentation", meta = (DisplayName = "Cinder Grenade Stuck"))
	void ReceiveGrenadeStuck(AActor* HitActor, FVector ImpactLocation, FVector ImpactNormal);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Echo Ability|Presentation", meta = (DisplayName = "Cinder Grenade Detonated"))
	void ReceiveGrenadeDetonated(FVector ExplosionLocation, FVector SurfaceNormal);

private:
	friend struct FSovBallisticAssistTestAccess;
	UFUNCTION()
	void HandleProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UFUNCTION()
	void OnRep_IsStuck();

	UFUNCTION()
	void OnRep_HasDetonated();

	UFUNCTION()
	void OnRep_InitialVelocity();

	void StartProjectileMovement();
	void DeactivateProjectile();
	void Detonate();
	void ApplyExplosionPhysicsImpulse();
	void ApplyExplosion();
	bool IsTargetAlive(const UAbilitySystemComponent* TargetAbilitySystem) const;
	bool HasExplosionLineOfSight(AActor* TargetActor) const;
	bool FindExplosionDecalSurface(FHitResult& OutSurfaceHit) const;
	void SpawnExplosionDecal();
	void PlayStuckPresentation();
	void PlayDetonationPresentation();

	UPROPERTY(Replicated)
	TObjectPtr<AActor> StuckActor;

	UPROPERTY(Replicated)
	FVector_NetQuantize StuckLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal StuckNormal = FVector::UpVector;

	UPROPERTY(ReplicatedUsing = OnRep_IsStuck)
	bool bIsStuck = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize DetonationLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing = OnRep_HasDetonated)
	bool bHasDetonated = false;

	/** Lets simulated proxies run a cosmetic arc between movement corrections. */
	UPROPERTY(ReplicatedUsing = OnRep_InitialVelocity)
	FVector_NetQuantize10 ReplicatedInitialVelocity = FVector::ZeroVector;

	/** Keeps simulated-proxy interpolation on the same ballistic arc as authority. */
	UPROPERTY(Replicated)
	float ReplicatedGravityScale = 1.0f;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;

	/** Deprecated payload slot retained so existing Blueprint spawn data remains loadable. */
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> BurnEffectClass;

	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;
	TWeakObjectPtr<AActor> SourceAvatar;
	TWeakObjectPtr<UObject> DamageSourceObject;
	FGameplayTag AbilityIdentityTag;
	float EffectLevel = 1.0f;
	float FuseDuration = 1.5f;
	float ExplosionRadius = 350.0f;
	float ExplosionDamage = 40.0f;
	float ExplosionPoiseDamage = 20.0f;
	float MinimumDamageFraction = 0.5f;
	float BurnDamagePerTick = 5.0f;
	float BurnDuration = 4.0f;
	bool bRequiresLineOfSight = true;
	bool bPayloadInitialized = false;
	bool bPlayedStuckPresentation = false;
	bool bPlayedDetonationPresentation = false;
	FTimerHandle FuseTimerHandle;
};
