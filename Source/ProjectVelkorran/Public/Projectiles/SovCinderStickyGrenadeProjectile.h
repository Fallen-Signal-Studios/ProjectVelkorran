// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Weapons/NarrativeProjectile.h"
#include "SovCinderStickyGrenadeProjectile.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
struct FGameplayEffectContextHandle;
struct FLifetimeProperty;

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

	/** Time retained after detonation so replicated cosmetic state reaches clients. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation", meta = (ClampMin = "0.1", Units = "s"))
	float DetonationCleanupDelay = 0.5f;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Echo Ability|Presentation", meta = (DisplayName = "Cinder Grenade Launched"))
	void ReceiveGrenadeLaunched();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Echo Ability|Presentation", meta = (DisplayName = "Cinder Grenade Stuck"))
	void ReceiveGrenadeStuck(AActor* HitActor, FVector ImpactLocation, FVector ImpactNormal);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Echo Ability|Presentation", meta = (DisplayName = "Cinder Grenade Detonated"))
	void ReceiveGrenadeDetonated(FVector ExplosionLocation, FVector SurfaceNormal);

private:
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
	void ApplyExplosion();
	void ApplyBurn(UAbilitySystemComponent* TargetAbilitySystem, const FGameplayEffectContextHandle& Context);
	bool IsTargetAlive(const UAbilitySystemComponent* TargetAbilitySystem) const;
	bool HasExplosionLineOfSight(AActor* TargetActor) const;
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

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;

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
