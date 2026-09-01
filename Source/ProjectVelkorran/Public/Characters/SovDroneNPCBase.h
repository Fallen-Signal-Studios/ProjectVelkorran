// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "SovDroneNPCBase.generated.h"

class UGameplayEffect;
class UNiagaraSystem;
class USoundBase;

/**
 * Project-owned Narrative NPC base for hovering mechanical drones.
 *
 * The capsule remains a ground-navigation proxy. Hover variation is applied to
 * the visual mesh only, while death uses a replicated Niagara presentation and
 * server-authoritative Narrative GAS radial damage instead of humanoid ragdoll.
 */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovDroneNPCBase : public ANarrativeNPCCharacter
{
	GENERATED_BODY()

public:
	ASovDroneNPCBase(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleDeath_Implementation(
		AActor* KilledActor,
		class UNarrativeAbilitySystemComponent* KilledActorASC,
		bool bIsDead) override;
	virtual void SetRagdoll(bool bWantsRagdoll) override;

	/** Cosmetic vertical variation. The actor and navigation capsule stay grounded. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Flight")
	bool bEnableVerticalHoverVariation = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Flight", meta = (EditCondition = "bEnableVerticalHoverVariation", ClampMin = "0.0", Units = "cm"))
	float HoverVariationAmplitude = 16.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Flight", meta = (EditCondition = "bEnableVerticalHoverVariation", ClampMin = "0.0", Units = "Hz"))
	float HoverVariationFrequency = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death")
	TObjectPtr<UNiagaraSystem> DeathExplosionNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death")
	TObjectPtr<USoundBase> DeathExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death")
	FVector DeathExplosionScale = FVector::OneVector;

	/** Instant Narrative damage effect used by the server-authoritative blast. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Damage")
	TSubclassOf<UGameplayEffect> DeathExplosionDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Damage", meta = (ClampMin = "0.0", Units = "cm"))
	float DeathExplosionRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Damage", meta = (ClampMin = "0.0"))
	float DeathExplosionDamage = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeathExplosionMinimumDamageFraction = 0.25f;

	/** Collision channel used to prevent damage through solid level geometry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Damage")
	TEnumAsByte<ECollisionChannel> DeathExplosionDamagePreventionChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Shutdown")
	bool bHideMeshOnDeath = true;

	/** Preserve the capsule's query channels for Narrative interaction while removing pawn obstruction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Drone|Death|Shutdown")
	bool bKeepDeathInteractionTrace = true;

private:
	void TriggerDeathExplosion();
	void ApplyDeathShutdownState(bool bIsDead);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayDeathExplosion(FVector_NetQuantize ExplosionLocation);

	FVector InitialMeshRelativeLocation = FVector::ZeroVector;
	FName InitialCapsuleCollisionProfile = NAME_None;
	FName InitialMeshCollisionProfile = NAME_None;
	TEnumAsByte<ECollisionEnabled::Type> InitialCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	TEnumAsByte<ECollisionEnabled::Type> InitialMeshCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	bool bInitialMeshHiddenInGame = false;
	bool bDeathExplosionTriggered = false;
};
