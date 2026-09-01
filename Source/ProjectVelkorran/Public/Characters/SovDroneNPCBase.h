// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Components/SovDismembermentComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetSerialization.h"
#include "SovDroneNPCBase.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UNarrativeAbilitySystemComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * Non-humanoid replacement for the project NPC base's default dismemberment
 * component. Keeping the subobject in place preserves inherited serialization
 * while preventing mannequin bone rules and blood presentation on drones.
 */
UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovDroneDismembermentComponent final
	: public USovDismembermentComponent
{
	GENERATED_BODY()

public:
	USovDroneDismembermentComponent();
};

/**
 * Project-owned NPC base for hovering mechanical drones.
 *
 * The capsule remains a ground-navigation proxy. Hover variation is applied to
 * the visual mesh only, while death uses a replicated Niagara presentation and
 * optional server-authoritative Narrative GAS radial damage instead of humanoid
 * ragdoll. Inheriting from ASovNPCCharacterBase retains project combat sustain
 * drops without enabling bipedal dismemberment for the drone.
 */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovDroneNPCBase : public ASovNPCCharacterBase
{
	GENERATED_BODY()

public:
	ASovDroneNPCBase(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Authority seam for an authored fatal payload that already owns its blast.
	 * Call before applying the fatal self damage. Native Reformation self-destruct
	 * is also detected automatically from its committed presentation actor.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Drone|Death")
	void SuppressNextDeathExplosion();

	/** Rolls back suppression when an authored fatal payload fails to kill. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Drone|Death")
	void ClearDeathExplosionSuppression();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Drone|Death")
	bool IsDeathExplosionEnabledOnDeath() const
	{
		return bEnableDeathExplosionOnDeath;
	}

protected:
	virtual void BeginPlay() override;
	virtual void HandleDeath_Implementation(
		AActor* KilledActor,
		UNarrativeAbilitySystemComponent* KilledActorASC,
		bool bIsDead) override;
	virtual void SetRagdoll(bool bWantsRagdoll) override;

	/** Cosmetic vertical variation. The actor and navigation capsule stay grounded. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Flight")
	bool bEnableVerticalHoverVariation = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Flight", meta = (EditCondition = "bEnableVerticalHoverVariation", ClampMin = "0.0", Units = "cm"))
	float HoverVariationAmplitude = 16.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Flight", meta = (EditCondition = "bEnableVerticalHoverVariation", ClampMin = "0.0", Units = "Hz"))
	float HoverVariationFrequency = 0.65f;

	/** Safe default: ordinary drone deaths do not create a second combat payload. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death")
	bool bEnableDeathExplosionOnDeath = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death", meta = (EditCondition = "bEnableDeathExplosionOnDeath"))
	TObjectPtr<UNiagaraSystem> DeathExplosionNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death", meta = (EditCondition = "bEnableDeathExplosionOnDeath"))
	TObjectPtr<USoundBase> DeathExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death", meta = (EditCondition = "bEnableDeathExplosionOnDeath"))
	FVector DeathExplosionScale = FVector::OneVector;

	/** Instant Narrative damage effect used by the server-authoritative blast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Damage", meta = (EditCondition = "bEnableDeathExplosionOnDeath"))
	TSubclassOf<UGameplayEffect> DeathExplosionDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Damage", meta = (EditCondition = "bEnableDeathExplosionOnDeath", ClampMin = "0.0", Units = "cm"))
	float DeathExplosionRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Damage", meta = (EditCondition = "bEnableDeathExplosionOnDeath", ClampMin = "0.0"))
	float DeathExplosionDamage = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Damage", meta = (EditCondition = "bEnableDeathExplosionOnDeath", ClampMin = "0.0", ClampMax = "1.0"))
	float DeathExplosionMinimumDamageFraction = 0.25f;

	/** Collision channel used to prevent damage through solid level geometry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Damage", meta = (EditCondition = "bEnableDeathExplosionOnDeath"))
	TEnumAsByte<ECollisionChannel> DeathExplosionDamagePreventionChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Shutdown")
	bool bHideMeshOnDeath = true;

	/** Preserve the capsule's query channels for Narrative interaction while removing pawn obstruction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Shutdown")
	bool bKeepDeathInteractionTrace = true;

private:
	void TriggerDeathExplosion();
	void ApplyDeathShutdownState(bool bIsDead);
	bool HasCommittedNativeSelfDestruct() const;
	bool IsLivingHostileTarget(
		const UAbilitySystemComponent* TargetAbilitySystem,
		const AActor* TargetActor) const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayDeathExplosion(FVector_NetQuantize ExplosionLocation);

	FVector InitialMeshRelativeLocation = FVector::ZeroVector;
	FName InitialCapsuleCollisionProfile = NAME_None;
	FName InitialMeshCollisionProfile = NAME_None;
	TEnumAsByte<ECollisionEnabled::Type> InitialCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	TEnumAsByte<ECollisionEnabled::Type> InitialMeshCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	bool bInitialMeshHiddenInGame = false;
	bool bDeathExplosionTriggered = false;
	bool bSuppressNextDeathExplosion = false;
};
