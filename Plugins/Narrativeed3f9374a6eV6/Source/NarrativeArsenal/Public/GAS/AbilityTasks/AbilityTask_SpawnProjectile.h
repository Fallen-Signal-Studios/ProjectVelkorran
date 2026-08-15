// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Weapons/NarrativeProjectile.h"
#include "AbilityTask_SpawnProjectile.generated.h"

/**
 * Spawns a Narrative Projectile, and waits for it to generate target data. Similar to WaitTargetData, but simplified a lot. 
 */
UCLASS()
class NARRATIVEARSENAL_API UAbilityTask_SpawnProjectile : public UAbilityTask
{
	GENERATED_UCLASS_BODY()
	
	//Ability Task listens to this to broadcast projectile hitting something. 
	UPROPERTY(BlueprintAssignable, Category = "Projectile")
	FProjectileTargetDataDelegate OnTargetData;

	//Calls when projectile was destroyed. 
	UPROPERTY(BlueprintAssignable, Category = "Projectile")
	FProjectileTargetDataDelegate OnDestroyed;

	/** Spawns target actor and waits for it to return valid data or to be canceled. */
	UFUNCTION(BlueprintCallable, meta=(HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true", HideSpawnParms="Instigator", DisplayName = "Spawn Projectile"), Category="Ability|Tasks")
	static UAbilityTask_SpawnProjectile* SpawnProjectile(UGameplayAbility* OwningAbility, FName TaskInstanceName, TSubclassOf<ANarrativeProjectile> Class, FTransform ProjectileSpawnTransform);

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"), Category = "Abilities")
	virtual bool BeginSpawningActor(UGameplayAbility* OwningAbility, TSubclassOf<ANarrativeProjectile> Class, ANarrativeProjectile*& SpawnedActor);

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"), Category = "Abilities")
	virtual void FinishSpawningActor(UGameplayAbility* OwningAbility, ANarrativeProjectile* SpawnedActor);

protected:

	virtual bool ShouldSpawnProjectile() const;
	virtual void InitializeProjectile(ANarrativeProjectile* SpawnedActor) const;
	virtual void FinalizeProjectile(ANarrativeProjectile* SpawnedActor) const;

	virtual void Activate() override;
	virtual void OnDestroy(bool AbilityEnded) override;

	UFUNCTION()
	virtual void OnProjectileTargetData(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	virtual void OnProjectileDestroyed(AActor* DestroyedActor);

	UPROPERTY()
	TSubclassOf<ANarrativeProjectile> ProjectileClass;

	UPROPERTY()
	FTransform ProjectileSpawnTransform;

	/** The Projectile that we spawned */
	UPROPERTY()
	TObjectPtr<ANarrativeProjectile> Projectile;
};
