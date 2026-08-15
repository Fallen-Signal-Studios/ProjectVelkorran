// Copyright Narrative Tools 2024. 


#include "Spawners/NPCSpawnComponent.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NPCDefinition.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include <Kismet/GameplayStatics.h>
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Spawners/SpawnerBase.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

UNPCSpawnComponent::UNPCSpawnComponent()
{
	UntetherDistance = 10000.f;
	bDontSpawnIfPreviouslyKilled =	true;
	bWasKilled = false; 

	SetAutoActivate(true);

	USaveSystemStatics::CreateSaveGuid(NPCSaveGUID);
}

bool UNPCSpawnComponent::ShouldSpawnActor()
{

	if (!Super::ShouldSpawnActor())
	{
		return false; 
	}

	if (bDontSpawnIfPreviouslyKilled && bWasKilled)
	{
		return false; 	
	}

	//Need a definition set 
	if (!IsValid(NPCToSpawn))
	{
		return false;
	}


	return true;
}

bool UNPCSpawnComponent::ShouldDespawnActor()
{
	if (!Super::ShouldDespawnActor())
	{
		return false; 
	}

	return true;
}

class AActor* UNPCSpawnComponent::SpawnActor_Implementation()
{
	if (ASpawnerBase* SpawnerOwner = Cast<ASpawnerBase>(GetOwner()))
	{
		if (NPCToSpawn)
		{
			if (UNarrativeCharacterSubsystem* NPCSubsystem = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
			{
				const FTransform SpawnTransform = GetComponentTransform();

				ANarrativeNPCCharacter* NPC = nullptr;

				//This spawns NPC previously tethered to a player. Try find the NPC using the tether, and untether from the player
				//In a networked game, we'd probably want the NetID of the tetherer so we can look them up? Networked World Partition needs more thought. 
				if (bTetheredToPlayer)
				{
					const FGuid NPCGuid = NPCToSpawn->bAllowMultipleInstances ? NPCSaveGUID : NPCToSpawn->UniqueNPCGUID;

					if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
					{
						FNPCTether NPCTether;

						bTetheredToPlayer = false; 

						if (PC->GetTether(NPCGuid, NPCTether))
						{
							NPC = NPCTether.NPCCharacter;

							PC->UntetherNPC(NPC);
						}
						else //If we cant find the NPC via the tether, then it got untethered at some point. We'll spawn it back in...
						{
							//TODO want to set NPC transform back to spawn transform if the NPC was abandoned. 
							
							//But only if it has a save record. No save record means the NPC was killed and removed their record.  
							if (UNarrativeSaveSubsystem* SaveSub = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
							{
								if (SaveSub->DoesRecordExist(NPCGuid))
								{
									NPC = NPCSubsystem->SpawnNPC(NPCToSpawn, SpawnTransform, SpawnParams);
								}
								else
								{
									bWasKilled = true; 
								}
							}
						}
					}
				}
				else
				{
					NPC = NPCSubsystem->SpawnNPC(NPCToSpawn, SpawnTransform, SpawnParams);
				}

				if (IsValid(NPC))
				{
					NPC->SpawnInfo.OwningSpawnerGUID = INarrativeSavableActor::Execute_GetActorGUID(SpawnerOwner); //SpawnerOwner->SpawnerSaveGUID;
					NPC->SpawnInfo.SpawnName = GetFName(); //TODO is there a rename resistant way to reget this? soft ref? 
					NPC->SpawnInfo.OwningSpawn = this;
					NPC->SpawnInfo.SpawnTransform = SpawnTransform;

					//If NPC is non unique, spawn assigns its save GUID. This way, non-unique NPCs save records are tied to their spawn, which is a nice way to remember non-unique NPCs. 
					if (NPCToSpawn->bAllowMultipleInstances)
					{
						NPC->SpawnInfo.SpawnAssignedSaveGUID = NPCSaveGUID;
					}

					if (bDontSpawnIfPreviouslyKilled)
					{	
						if (UNarrativeAbilitySystemComponent* NASC = Cast<UNarrativeAbilitySystemComponent>(NPC->GetAbilitySystemComponent()))
						{
							if (IsValid(NASC) && IsValid(this))
							{
								NASC->OnDeathStateChanged.AddUniqueDynamic(this, &UNPCSpawnComponent::SpawnedNPCDied);
							}
						}
					}

					if (OptionalGoal)
					{
						if (UNPCActivityComponent* NPCAC = NPC->GetActivityComponent())
						{
							NPCAC->AddGoal(OptionalGoal);
						}
					}

					//When NPC spawns itself in, it will spawn into its location it was at when it unspawned. 
					// We may want logic to check LastSpawnTime and reset. 
					//NPC->SetActorTransform(SpawnTransform);

					return NPC;
				}
			}
		}
	}

	return nullptr; 
}

bool UNPCSpawnComponent::TryDespawnActor()
{
	if (ANarrativeNPCCharacter* SpawnedNPC = Cast<ANarrativeNPCCharacter>(SpawnedActor))
	{
		/*If the player is within tether range, dont despawn the NPC, and tether it to our local player controller so it can manage it. 
		For multiplayer games, we should consider new server WP settings in 5.4, and iterate all world PCs.  */
		if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
		{
			if (APawn* ControlledPawn = PC->GetPawn())
			{	
				if (ControlledPawn->GetDistanceTo(SpawnedNPC) < UntetherDistance)
				{
					if (PC->TetherNPC(SpawnedNPC))
					{
 						bTetheredToPlayer = true;
						return true;
					}
				}
			}
		}
	}

	return RemoveActor();
}

bool UNPCSpawnComponent::RemoveActor()
{
	if (ANarrativeNPCCharacter* NPCCharacter = Cast<ANarrativeNPCCharacter>(SpawnedActor.Get()))
	{
		if (ANarrativeNPCController* NPCController = Cast<ANarrativeNPCController>(NPCCharacter->GetController()))
		{
			NPCController->CleanUp(0.f);
		}
		else
		{
			NPCCharacter->Destroy();
		}

		SpawnedActor.Reset();
		return true;
	}

	return false; 
}

FString UNPCSpawnComponent::GetEditorLabel_Implementation() const
{
	return IsValid(NPCToSpawn) ? NPCToSpawn->NPCName.ToString() : "No NPC Selected";
}

ANarrativeNPCCharacter* UNPCSpawnComponent::GetSpawnedNPC() const
{
	return Cast<ANarrativeNPCCharacter>(SpawnedActor.Get());
}

void UNPCSpawnComponent::SpawnedNPCDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead)
{
	//Was killed will be saved to disk so we can prevent spawning this NPC in again 
	if (bIsDead && KilledActor == SpawnedActor.Get())
	{
		bWasKilled = true;
	}
}

#if WITH_EDITOR
void UNPCSpawnComponent::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// Invalidate duplicated GUID so we can generate another one
	if (DuplicateMode == EDuplicateMode::Normal)
	{
		NPCSaveGUID.Invalidate();
		USaveSystemStatics::CreateSaveGuid(NPCSaveGUID);
	}
}
#endif
