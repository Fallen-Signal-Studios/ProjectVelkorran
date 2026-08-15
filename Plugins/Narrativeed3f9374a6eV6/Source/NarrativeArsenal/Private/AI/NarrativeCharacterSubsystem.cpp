// Copyright Narrative Tools 2024. 


#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NPCDefinition.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include <EngineUtils.h>
#include <Engine/AssetManager.h>
#include <GameFramework/GameModeBase.h>
#include "Subsystems/NarrativeSaveSubsystem.h"
#include <Kismet/GameplayStatics.h>
#include "NarrativeLogChannels.h"
#include "AI/NarrativeNPCController.h"
#include "NarrativeWorldSettings.h"
#include "Settings/NarrativeDeveloperSettings.h"



static const FAutoConsoleCommandWithWorld DumpCharacterMapCommand(
	TEXT("n.charactersubsystem.DumpCharacterMap"),
	TEXT("Outputs the current Input Config for each player"),
	FConsoleCommandWithWorldDelegate::CreateStatic(&UNarrativeCharacterSubsystem::HandleDumpCharacterMap)
);

UNarrativeCharacterSubsystem::UNarrativeCharacterSubsystem()
{

}


void UNarrativeCharacterSubsystem::PostInitialize()
{
	Super::PostInitialize();
	
	//The save subsystem used to load all this itself, but we had a problem - we needed NPC data around before we loaded - so we've moved it here instead. TODO perhaps a more proper system to handle load ordering 
	if (UWorld* World = GetWorld())
	{
		FOnActorDestroyed::FDelegate ActorDestroyed = FOnActorDestroyed::FDelegate::CreateUObject(this, &UNarrativeCharacterSubsystem::OnActorDestroyed);
		ActorDestroyedHandle = World->AddOnActorDestroyedHandler(ActorDestroyed);

		FOnActorSpawned::FDelegate ActorSpawned = FOnActorSpawned::FDelegate::CreateUObject(this, &UNarrativeCharacterSubsystem::OnActorSpawned);
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(ActorSpawned);
	}
}

void UNarrativeCharacterSubsystem::Deinitialize()
{
	Super::Deinitialize();

	NPCMap.Empty();
	CharacterMap.Empty();
	ActorSpawnedHandle.Reset();
	ActorDestroyedHandle.Reset();
}

void UNarrativeCharacterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

bool UNarrativeCharacterSubsystem::DestroyNPC(ANarrativeNPCCharacter* NPC)
{
	if (NPC)
	{
		if (ANarrativeNPCController* NPCController = Cast<ANarrativeNPCController>(NPC->GetController()))
		{
			NPCController->CleanUp(0.f);
		}
		else
		{
			NPC->Destroy();
		}

		return true;
	}

	return false; 
}

class ANarrativeNPCCharacter* UNarrativeCharacterSubsystem::SpawnNPC(UNPCDefinition* NPCData, FTransform SpawnTransform /*= FTransform()*/, FNPCSpawnParams SpawnParams /*= FNPCSpawnParams()*/)
{
	if (NPCData)
	{
		if (UWorld* World = GetWorld())
		{

			//In editor we enforce an allow list for debugging purposes 
#if WITH_EDITOR

			if (const UNarrativeDeveloperSettings* NarrativeDevSettings = GetDefault<UNarrativeDeveloperSettings>())
			{
				if (NarrativeDevSettings->NPCAllowList.Num() > 0)
				{
					if (!NarrativeDevSettings->NPCAllowList.Contains(NPCData))
					{
						return nullptr; 
					}
				}
			}

#endif 
			//Only server types should spawn NPCs, which should then replicate 
			if (World->GetNetMode() < NM_Client)
			{
				if (!NPCData->NPCClassPath.IsValid())
				{
					//TODO should we async load new NPCs in, this is generally always loaded though atm 
					NPCData->NPCClassPath.LoadSynchronous();
				}

				//If the NPCs class path is valid, we can spawn it immediately. Otherwise, we'll need to wait for the asset manager to load it. 
				if (NPCData->NPCClassPath.IsValid())
				{
					return SpawnNPC_Internal(NPCData, SpawnTransform, SpawnParams);
				}
			}
		}
	}

	return nullptr;
}

class ANarrativeNPCCharacter* UNarrativeCharacterSubsystem::FindOrSpawnNPC(UNPCDefinition* NPCData, FTransform SpawnTransform)
{
	if (!IsValid(NPCData))
	{
		return nullptr;
	}

	ANarrativeNPCCharacter* NPC = FindNPC(NPCData);

	//If the NPC is invalid, it wasn't in the world, and so we need to make sure we spawn it. 
	if(!IsValid(NPC))
	{
		return SpawnNPC(NPCData, SpawnTransform);

	}

	return NPC;
}

class ANarrativeNPCCharacter* UNarrativeCharacterSubsystem::FindNPC(const UNPCDefinition* NPCData, bool& bOutSucceeded) const 
{
	if (IsValid(NPCData) && NPCMap.Contains(NPCData->NPCID))
	{
		for (auto& NPC : NPCMap[NPCData->NPCID].NPCs)
		{
			if (IsValid(NPC))
			{
				bOutSucceeded = true; 
				return NPC;
			}
		}
	}

	bOutSucceeded = false;
	return nullptr;
}

class ANarrativeNPCCharacter* UNarrativeCharacterSubsystem::FindNPC(const UNPCDefinition* NPCData) const
{
	if (IsValid(NPCData) && NPCMap.Contains(NPCData->NPCID))
	{
		for (auto& NPC : NPCMap[NPCData->NPCID].NPCs)
		{
			if (IsValid(NPC))
			{
				return NPC;
			}
		}
	}

	return nullptr;
}

class ANarrativeNPCCharacter* UNarrativeCharacterSubsystem::FindNPCByID(const FName& NPCID) const
{
	if (NPCMap.Contains(NPCID))
	{
		for (auto& NPC : NPCMap[NPCID].NPCs)
		{
			if (IsValid(NPC))
			{
				return NPC;
			}
		}
	}

	return nullptr;
}

class ANarrativeCharacter* UNarrativeCharacterSubsystem::FindCharacter(const UCharacterDefinition* CharacterDefinition) const
{
	if (CharacterDefinition)
	{
		return FindCharacterByID(CharacterDefinition->CharacterID);
	}

	return nullptr;
}

class ANarrativeCharacter* UNarrativeCharacterSubsystem::FindCharacterByID(const FName& CharacterID) const
{
	if (!CharacterID.IsNone())
	{
		if (CharacterMap.Contains(CharacterID))
		{
			for (auto& Char : CharacterMap[CharacterID].Characters)
			{
				if (IsValid(Char))
				{
					return Char;
				}
			}
		}
	}

	return nullptr;
}

void UNarrativeCharacterSubsystem::FindNPCs(const UNPCDefinition* NPCData, TArray<ANarrativeNPCCharacter*>& OutActors) const
{
	if (NPCData)
	{
		if (NPCMap.Contains(NPCData->NPCID))
		{
			OutActors = NPCMap[NPCData->NPCID].NPCs;
		}
	}
}

void UNarrativeCharacterSubsystem::FindCharacters(const UCharacterDefinition* CharacterDefinition, TArray<ANarrativeCharacter*>& OutActors) const
{
	if (CharacterDefinition)
	{
		if (CharacterMap.Contains(CharacterDefinition->CharacterID))
		{
			OutActors = CharacterMap[CharacterDefinition->CharacterID].Characters;
		}
	}
}

bool UNarrativeCharacterSubsystem::IsCharacterSpawned(const UCharacterDefinition* CharacterDefinition) const
{
	if (CharacterDefinition)
	{
		if (CharacterMap.Contains(CharacterDefinition->CharacterID))
		{
			return CharacterMap[CharacterDefinition->CharacterID].HasValidCharacters();
		}
	}
	
	return false; 
}


class ANarrativeNPCCharacter* UNarrativeCharacterSubsystem::SpawnNPC_Internal(UNPCDefinition* NPCData, const FTransform& SpawnTransform, const FNPCSpawnParams& SpawnParams)
{
	if (UWorld* World = GetWorld())
	{
		//Only server types should spawn NPCs, which should then replicate 
		if (World->GetNetMode() < NM_Client)
		{
			if (IsValid(NPCData))
			{	
				const TSubclassOf<ANarrativeNPCCharacter> NPCClass =  NPCData->NPCClassPath.LoadSynchronous();

				FNPCArray& NPCArray = NPCMap.FindOrAdd(NPCData->NPCID);
				FCharacterArray& CharArray = CharacterMap.FindOrAdd(NPCData->NPCID);

				//The NPC array already has an NPC spawned under this key. This instance cannot be created, lets destroy the NPC we tried to spawn 
				if (!NPCData->bAllowMultipleInstances && NPCArray.HasValidNPCs())
				{
					UE_LOG(LogNPCs, Warning, TEXT("UNarrativeNPCSubsystem::SpawnNPC_Internal prevented a duplicate of %s"), *NPCData->NPCName.ToString());
					return nullptr;
				}

				if (ANarrativeNPCCharacter* NPC = World->SpawnActorDeferred<ANarrativeNPCCharacter>(NPCClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn))
				{
					if (IsValid(NPC))
					{
						NPC->SpawnInfo.SpawnParams = SpawnParams;
						
						if (SpawnParams.bOverride_CharacterRandomSeed)
						{
							NPC->SetRandomSeed(SpawnParams.CharacterRandomSeed);
						}

						NPC->SetNPCDefinition(NPCData);
						NPC->FinishSpawning(SpawnTransform);

						if (NPC)
						{
							NPC->SpawnDefaultController();
						}

						RegisterCharacter(NPC);
						
						//NPCArray.NPCs.AddUnique(NPC);
						//CharArray.Characters.AddUnique(NPC);

						OnNPCSpawned.Broadcast(NPCData, NPC);

						return NPC;
					}

				}
			}
		}
	}
	return nullptr;
}

void UNarrativeCharacterSubsystem::RegisterCharacter(class ANarrativeCharacter* Character)
{
	if (IsValid(Character))
	{
		if (UCharacterDefinition* CharDef = Character->GetCharacterDefinition())
		{
			FCharacterArray& CharArray = CharacterMap.FindOrAdd(CharDef->CharacterID);

			CharArray.Characters.AddUnique(Character);

		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("NChar %s was spawned but didn't have valid chardef so we cannot map it. "), *GetNameSafe(Character));
		}

		if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(Character))
		{
			if (UNPCDefinition* NPCData =  NPC->GetNPCDefinition())
			{
				FNPCArray& NPCArray = NPCMap.FindOrAdd(NPCData->NPCID);

				NPCArray.NPCs.AddUnique(NPC);
			}
		}
	}
}

void UNarrativeCharacterSubsystem::OnNPCClassLoaded(FPrimaryAssetId LoadedId, const FTransform SpawnTransform, const FNPCSpawnParams& SpawnParams)
{
	if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		if (UNPCDefinition* NPCData = Cast<UNPCDefinition>(Manager->GetPrimaryAssetObject(LoadedId)))
		{
			//The class path should now be loaded thanks to bundles! 
			check(NPCData->NPCClassPath.IsValid());

			SpawnNPC_Internal(NPCData, SpawnTransform, SpawnParams);
		}
	}
}

void UNarrativeCharacterSubsystem::OnActorSpawned(class AActor* SpawnedActor)
{
	//NPCs and Player spawning logic both just call this manually as more efficient and we can ensure character state is ready 
	/*if (ANarrativeCharacter* NChar = Cast<ANarrativeCharacter>(SpawnedActor))
	{
		if (IsValid(NChar))
		{
			if (UCharacterDefinition* CharDef = NChar->GetCharacterDefinition())
			{
				FCharacterArray& CharArray = CharacterMap.FindOrAdd(CharDef);

				CharArray.Characters.AddUnique(NChar);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("NChar %s was spawned but didn't have valid chardef so we cannot map it. "), *GetNameSafe(NChar));
			}
		}
	}*/
}

void UNarrativeCharacterSubsystem::OnActorDestroyed(class AActor* DestroyedActor)
{
	if (ANarrativeCharacter* NChar = Cast<ANarrativeCharacter>(DestroyedActor))
	{
		if (UCharacterDefinition* CharDef = NChar->GetCharacterDefinition())
		{
			if (CharacterMap.Contains(CharDef->CharacterID))
			{
				CharacterMap[CharDef->CharacterID].Characters.Remove(NChar);
			}
			else
			{
				UE_LOG(LogNPCs, Warning, TEXT("UNarrativeNPCSubsystem::OnActorDestroyed tried removing character from map but it had null CharDef! "));
			}
		}

		//Remove the destroyed NPC from its map
		if (ANarrativeNPCCharacter* DestroyedNPC = Cast<ANarrativeNPCCharacter>(DestroyedActor))
		{
			if (UNPCDefinition* NPCData = DestroyedNPC->NPCDefinition)
			{
				if (NPCMap.Contains(NPCData->NPCID))
				{
					NPCMap[NPCData->NPCID].NPCs.Remove(DestroyedNPC);
				}
				else
				{
					UE_LOG(LogNPCs, Warning, TEXT("UNarrativeNPCSubsystem::OnActorDestroyed tried removing NPC from map but it had null NPCData! "));
				}
			}
		}
	}

}

void UNarrativeCharacterSubsystem::HandleDumpCharacterMap(UWorld* World)
{
	if (World)
	{
		if (UNarrativeCharacterSubsystem* CSS = World->GetSubsystem<UNarrativeCharacterSubsystem>())
		{
			for (auto& KVP : CSS->CharacterMap)
			{
				UE_LOG(LogNPCs, Warning, TEXT("Character Mapping %s - CHARACTERS (%d) : "), *KVP.Key.ToString(), KVP.Value.Characters.Num());

				for (auto& Char : KVP.Value.Characters)
				{
					UE_LOG(LogNPCs, Warning, TEXT("Character %s"), *GetNameSafe(Char));
				}
			}
		}
	}

}
