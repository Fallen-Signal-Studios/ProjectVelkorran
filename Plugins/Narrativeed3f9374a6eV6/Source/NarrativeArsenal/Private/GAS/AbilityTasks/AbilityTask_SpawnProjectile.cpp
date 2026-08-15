// Copyright Narrative Tools 2025.


#include "GAS/AbilityTasks/AbilityTask_SpawnProjectile.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "AbilitySystemComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityTask_SpawnProjectile)

UAbilityTask_SpawnProjectile::UAbilityTask_SpawnProjectile(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}


UAbilityTask_SpawnProjectile* UAbilityTask_SpawnProjectile::SpawnProjectile(UGameplayAbility* OwningAbility, FName TaskInstanceName, TSubclassOf<ANarrativeProjectile> InProjectileClass, FTransform ProjectileSpawnTransform)
{
	UAbilityTask_SpawnProjectile* MyObj = NewAbilityTask<UAbilityTask_SpawnProjectile>(OwningAbility, TaskInstanceName);		//Register for task list here, providing a given FName as a key
	MyObj->ProjectileClass = InProjectileClass;
	MyObj->ProjectileSpawnTransform = ProjectileSpawnTransform;
	MyObj->Projectile = nullptr;
	return MyObj;
}

bool UAbilityTask_SpawnProjectile::BeginSpawningActor(UGameplayAbility* OwningAbility, TSubclassOf<ANarrativeProjectile> InProjectileClass, ANarrativeProjectile*& SpawnedActor)
{
	SpawnedActor = nullptr;

	if (Ability)
	{
		if (ShouldSpawnProjectile())
		{
			UClass* Class = *InProjectileClass;
			if (Class != nullptr)
			{
				if (UWorld* World = GEngine->GetWorldFromContextObject(OwningAbility, EGetWorldErrorMode::LogAndReturnNull))
				{
					APawn* OwnerPawn = Cast<APawn>(OwningAbility->GetActorInfo().AvatarActor);
					SpawnedActor = World->SpawnActorDeferred<ANarrativeProjectile>(Class, ProjectileSpawnTransform, OwnerPawn, OwnerPawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				}
			}

			if (SpawnedActor)
			{
				Projectile = SpawnedActor;
				InitializeProjectile(SpawnedActor);
			}
		}
	}

	return (SpawnedActor != nullptr);
}

void UAbilityTask_SpawnProjectile::FinishSpawningActor(UGameplayAbility* OwningAbility, ANarrativeProjectile* SpawnedActor)
{
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	if (ASC && IsValid(SpawnedActor))
	{
		check(Projectile == SpawnedActor);

		SpawnedActor->FinishSpawning(ProjectileSpawnTransform);

		FinalizeProjectile(SpawnedActor);
	}
}

bool UAbilityTask_SpawnProjectile::ShouldSpawnProjectile() const
{
	check(ProjectileClass);
	check(Ability);

	// Spawn the actor if this is a locally controlled ability (always) or if this is a replicating targeting mode.
	// (E.g., server will spawn this target actor to replicate to all non owning clients)

	const ANarrativeProjectile* CDO = CastChecked<ANarrativeProjectile>(ProjectileClass->GetDefaultObject());

	const bool bReplicates = CDO->GetIsReplicated();
	const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled();
	//const bool bShouldProduceTargetDataOnServer = CDO->ShouldProduceTargetDataOnServer;

	return (bReplicates || bIsLocallyControlled);
}

void UAbilityTask_SpawnProjectile::InitializeProjectile(ANarrativeProjectile* InProjectile) const
{
	if (Projectile)
	{
		Projectile->OnProjectileTargetData.AddDynamic(const_cast<UAbilityTask_SpawnProjectile*>(this), &UAbilityTask_SpawnProjectile::OnProjectileTargetData);
		Projectile->OnDestroyed.AddDynamic(const_cast<UAbilityTask_SpawnProjectile*>(this), &UAbilityTask_SpawnProjectile::OnProjectileDestroyed);
	}
}

void UAbilityTask_SpawnProjectile::FinalizeProjectile(ANarrativeProjectile* SpawnedActor) const
{

}

void UAbilityTask_SpawnProjectile::Activate()
{
	// Need to handle case where target actor was passed into task
	if (Ability && (ProjectileClass == nullptr))
	{
		if (Projectile)
		{
			ANarrativeProjectile* SpawnedActor = Projectile;
			ProjectileClass = SpawnedActor->GetClass();

			if (!IsValidChecked(this))
			{
				return;
			}

			if (ShouldSpawnProjectile())
			{
				InitializeProjectile(SpawnedActor);
				FinalizeProjectile(SpawnedActor);

				// Note that the call to FinalizeProjectile, this task could finish and our owning ability may be ended.
			}
			else
			{
				Projectile = nullptr;

				// We may need a better solution here.  We don't know the target actor isn't needed till after it's already been spawned.
				SpawnedActor->Destroy();
				SpawnedActor = nullptr;
			}
		}
		else
		{
			EndTask();
		}
	}
}

void UAbilityTask_SpawnProjectile::OnDestroy(bool AbilityEnded)
{
	//In Narrative we found it was nicer to just let the spawner destroy the projectile, rather than try keep GA alive for projectile duration. 
	//if (Projectile)
	//{
	//	Projectile->Destroy();
	//}

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_SpawnProjectile::OnProjectileTargetData(const FGameplayAbilityTargetDataHandle& Data)
{
	OnTargetData.Broadcast(Data);
}

void UAbilityTask_SpawnProjectile::OnProjectileDestroyed(AActor* DestroyedActor)
{
	if(DestroyedActor == Projectile)
	{
		OnDestroyed.Broadcast(FGameplayAbilityTargetDataHandle());
	}
}
