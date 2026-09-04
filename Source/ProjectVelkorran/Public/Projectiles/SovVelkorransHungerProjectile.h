// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Weapons/NarrativeProjectile.h"
#include "SovVelkorransHungerProjectile.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UNiagaraSystem;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
struct FGameplayEffectContextHandle;
class FLifetimeProperty;

/**
 * Server-authoritative blade-wave projectile for Velkorran's Hunger.
 *
 * The native actor owns movement, collision, hostile-target validation, GAS
 * damage, Burn application, and replicated resolution. A Blueprint child owns
 * its mesh, trail, audio, and any additional cosmetic presentation.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Velkorran's Hunger Projectile"))
class PROJECTVELKORRAN_API ASovVelkorransHungerProjectile : public ANarrativeProjectile
{
	GENERATED_BODY()

public:
	ASovVelkorransHungerProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Copies the paid payload into this actor before FinishSpawning is called. */
	void InitializeHungerProjectile(
		UAbilitySystemComponent* InSourceAbilitySystem,
		AActor* InSourceAvatar,
		UObject* InDamageSourceObject,
		TSubclassOf<UGameplayEffect> InDirectDamageEffectClass,
		TSubclassOf<UGameplayEffect> InBurnEffectClass,
		const FGameplayTag& InAbilityIdentityTag,
		float InEffectLevel,
		const FVector& InInitialVelocity,
		float InGravityScale,
		float InCollisionRadius,
		float InFlightDuration,
		float InDirectDamage,
		float InDirectPoiseDamage,
		float InBurnDamagePerTick,
		float InBurnDuration);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability|Velkorran's Hunger")
	bool HasResolvedImpact() const { return bHasResolvedImpact; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Components")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Time retained after resolution so replicated cosmetic state reaches clients. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation", meta = (ClampMin = "0.1", Units = "s"))
	float ResolutionCleanupDelay = 0.5f;

	/** Optional Niagara system spawned automatically on a target or world impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation")
	TObjectPtr<UNiagaraSystem> ImpactNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation")
	FVector ImpactNiagaraScale = FVector::OneVector;

	/** Optional Niagara system spawned when the wave reaches its flight limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation")
	TObjectPtr<UNiagaraSystem> DissipationNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Presentation")
	FVector DissipationNiagaraScale = FVector::OneVector;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Echo Ability|Presentation", meta = (DisplayName = "Hunger Projectile Launched"))
	void ReceiveHungerProjectileLaunched();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Echo Ability|Presentation", meta = (DisplayName = "Hunger Projectile Impacted"))
	void ReceiveHungerProjectileImpacted(
		AActor* HitActor,
		FVector ImpactLocation,
		FVector ImpactSurfaceNormal,
		bool bDamagedTarget);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Echo Ability|Presentation", meta = (DisplayName = "Hunger Projectile Dissipated"))
	void ReceiveHungerProjectileDissipated(FVector DissipationLocation);

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
	void OnRep_HasResolvedImpact();

	UFUNCTION()
	void OnRep_InitialVelocity();

	void StartProjectileMovement();
	void DeactivateProjectile();
	void ExpireProjectile();
	void ResolveImpact(
		AActor* HitActor,
		const FVector& InResolvedLocation,
		const FVector& InResolvedNormal,
		bool bDamagedTarget,
		bool bInExpired);
	bool ApplyDirectHit(UAbilitySystemComponent* TargetAbilitySystem, const FVector& HitLocation);
	bool IsHostileTarget(const UAbilitySystemComponent* TargetAbilitySystem) const;
	bool IsTargetAlive(const UAbilitySystemComponent* TargetAbilitySystem) const;
	void PlayResolutionPresentation();

	UPROPERTY(Replicated)
	TObjectPtr<AActor> ResolvedActor;

	UPROPERTY(Replicated)
	FVector_NetQuantize ResolvedLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal ResolvedNormal = FVector::UpVector;

	UPROPERTY(Replicated)
	bool bResolvedDamage = false;

	UPROPERTY(Replicated)
	bool bExpired = false;

	UPROPERTY(ReplicatedUsing = OnRep_HasResolvedImpact)
	bool bHasResolvedImpact = false;

	/** Lets simulated proxies run a cosmetic path between movement corrections. */
	UPROPERTY(ReplicatedUsing = OnRep_InitialVelocity)
	FVector_NetQuantize10 ReplicatedInitialVelocity = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float ReplicatedGravityScale = 0.0f;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DirectDamageEffectClass;

	/** Deprecated payload slot retained so existing Blueprint spawn data remains loadable. */
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> BurnEffectClass;

	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystem;
	TWeakObjectPtr<AActor> SourceAvatar;
	TWeakObjectPtr<UObject> DamageSourceObject;
	FGameplayTag AbilityIdentityTag;
	float EffectLevel = 1.0f;
	float CollisionRadius = 35.0f;
	float FlightDuration = 1.25f;
	float DirectDamage = 65.0f;
	float DirectPoiseDamage = 30.0f;
	float BurnDamagePerTick = 5.0f;
	float BurnDuration = 4.0f;
	bool bPayloadInitialized = false;
	bool bCollisionArmed = false;
	bool bResolvingImpact = false;
	bool bPlayedResolutionPresentation = false;
	FTimerHandle FlightTimerHandle;
};
