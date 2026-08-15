// Copyright Narrative Tools 2024. 


#include "Subsystems/NarrativeSaveSubsystem.h"
#include "NarrativeSave.h"
#include "Kismet/GameplayStatics.h"
#include <EngineUtils.h>
#include "NarrativeSavableActor.h"
#include "SaveSystemDeveloperSettings.h"
#include <Serialization/ObjectAndNameAsStringProxyArchive.h>
#include "NarrativeSavableComponent.h"
#include <GameFramework/PlayerState.h>
#include <GameFramework/PlayerController.h>
#include <GameFramework/GameModeBase.h>
#include <Engine/World.h>
#include "NarrativeLogChannels.h"
#include "TimerManager.h"
#include <Serialization/MemoryWriter.h>
#include <Serialization/MemoryReader.h>
#include <Serialization/ObjectAndNameAsStringProxyArchive.h>



UNarrativeSaveSubsystem::UNarrativeSaveSubsystem()
{
	CurrentSaveName = "";
	CurrentSaveSlot = -1;
}

bool UNarrativeSaveSubsystem::UpdateSaveObject(const bool bSkipRecordCreation)
{
	if (!NarrativeSaveGame)
	{
		NarrativeSaveGame = Cast<UNarrativeSave>(UGameplayStatics::CreateSaveGameObject(GetSaveGameClass()));
	}

	if (bSkipRecordCreation)
	{
		return true;
	}

	if (NarrativeSaveGame)
	{
		//Main menu logic uses this to remember which map we had loaded
		NarrativeSaveGame->LevelName = UGameplayStatics::GetCurrentLevelName(GetWorld(), true);

		TSet<AActor*> SkipActors;

		//Players are saved differently - we use their player id as storage and roll their different actors into one save object 
		for (FConstPlayerControllerIterator PCIt = GetWorld()->GetPlayerControllerIterator(); PCIt; ++PCIt)
		{
			APlayerController* PC = PCIt->Get();

			if (PC)
			{
				APlayerState* PS = PC->GetPlayerState<APlayerState>();
				APawn* Pawn = PC->GetPawn();

				if (PS && Pawn)
				{
					//FString PIDString = PS->GetUniqueId().ToString();

					FNarrativeSavePlayer PlayerData;

					CreateActorRecord(PS, PlayerData.PlayerStateData);
					CreateActorRecord(PC, PlayerData.ControllerData);
					CreateActorRecord(PC->GetPawn(), PlayerData.PawnData);

					SkipActors.Add(PS);
					SkipActors.Add(PC);
					SkipActors.Add(PC->GetPawn());

					//Don't save the controller or player state transforms, we don't really need to do this and at worst it may cause bugs 
					PlayerData.PlayerStateData.Transform = FTransform::Identity;
					PlayerData.ControllerData.Transform = FTransform::Identity;

					NarrativeSaveGame->PlayerData = PlayerData;

					//Breaking for now, you could add more if you wanted to support splitscreen 
					break;
				}
			}
		}

		// Iterate the entire world of actors, and create a record for each 
		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AActor* Actor = *It;

			//Skip player pawns 
			if (!SkipActors.Contains(Actor))
			{
				if (Actor)
				{
					//Cast method doesn't work with BP interfaces 
					if (Actor->Implements<UNarrativeStableActor>())
					{
						const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(Actor);

						//Returning none guid means we don't want saved. 
						if (ActorGUID.IsValid())
						{
							//Create a record of the actor and any components its needs saved 
							FNarrativeActorRecord SaveActor;
							CreateActorRecord(Actor, SaveActor);

							NarrativeSaveGame->RecordMap.Add(ActorGUID, SaveActor);
						}
					}
				}
			}
		}

		return true; 
	}

	return false;
}

bool UNarrativeSaveSubsystem::Save(const FString& SaveName /*= "NarrativeSave"*/, const int32 Slot /*= 0*/)
{
	if (bSavingDisabled)
	{
		return false; 
	}

	UpdateSaveObject();

	bool bSaved = false;

	if (NarrativeSaveGame)
	{
		bSaved = UGameplayStatics::SaveGameToSlot(NarrativeSaveGame, SaveName, Slot);

		if(bSaved)
		{
			CurrentSaveName = SaveName;
			CurrentSaveSlot = Slot;
		}
		else
		{
			CurrentSaveName = "";
			CurrentSaveSlot = -1;
		}
	}

	return bSaved;

}

bool UNarrativeSaveSubsystem::Load(const FString& SaveName /*= "NarrativeSave"*/, const int32 Slot /*= 0*/)
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveName, Slot))
	{
		return false;
	}

	OnBeginLoad.Broadcast();
	bIsCurrentlyLoading = true; 

	NarrativeSaveGame = Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromSlot(SaveName, Slot));

	if (NarrativeSaveGame)
	{
		CurrentSaveName = SaveName;
		CurrentSaveSlot = Slot;

		TSet<FGuid> FoundRecords;

		// Iterate the entire world of actors
		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AActor* Actor = *It;

			if (Actor)
			{
				//Cast method doesn't work with BP interfaces 
				if (Actor->Implements<UNarrativeStableActor>())
				{
					const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(Actor);
					
					if (ActorGUID.IsValid())
					{
						if (NarrativeSaveGame->RecordMap.Contains(ActorGUID))
						{
							LoadActorFromRecord(Actor, NarrativeSaveGame->RecordMap[ActorGUID]);
							FoundRecords.Add(ActorGUID);
						}
					}
				}
			}
		}

		/*In Narrative Pro, dynamic respawns are kept to a minimum. Dynamic actors are actors that aren't placed in the level - they were spawned via game code.
		For example if you drop a pickup on the ground and save, we need to respawn that pickup where you dropped it. 

		These are somewhat problematic - if the player drops an item 5km away from where we are that part of the map won't be loaded in - we could remember what grid cell 
		it was in and load it when that cell is loaded back in via WP, but this is out of the scope of the save subsystem at present.

		For this reason, we just dynamically respawn dropped pickups
		*/
		TArray<FNarrativeActorRecord> DynamicActors;

		for (auto& RecordKVP : NarrativeSaveGame->RecordMap)
		{
			FNarrativeActorRecord Record = RecordKVP.Value;

			if (!FoundRecords.Contains(Record.ActorGUID))
			{
				//Don't netstartups, only dynamic - WP will spawn them when they become relevant 
				if (!Record.bNetStartup && Record.bNeedsDynamicSpawn)
				{
					DynamicActors.Add(Record);
				}
			}
		}

		//Disable dynamic actors for now until we have a good system in place with non-dynamics 
		for (auto& Record : DynamicActors)
		{
			LoadDynamicRecord(Record);
		}
		
	}
	else
	{
		CurrentSaveName = "";
		CurrentSaveSlot = -1;
	}

	bIsCurrentlyLoading = false; 
	OnFinishedLoad.Broadcast();


	return true;
}

bool UNarrativeSaveSubsystem::DeleteSave(const FString& SaveName /*= "NarrativeSaveData"*/, const int32 Slot /*= 0*/)
{
	if (UGameplayStatics::DeleteGameInSlot(SaveName, Slot))
	{
		//If we deleted the current save clear the invalid cached stuff 
		if (SaveName.Equals(CurrentSaveName))
		{
			CurrentSaveName = "";
			CurrentSaveSlot = -1;
			NarrativeSaveGame = nullptr;
		}

		return true;
	}
	return false;
}

void UNarrativeSaveSubsystem::LoadPlayerData()
{
	if (NarrativeSaveGame && NarrativeSaveGame->PlayerData.IsValid())
	{
		bIsCurrentlyLoading = true;

		const FNarrativeSavePlayer& PlayerSave = NarrativeSaveGame->PlayerData;

		//Players are saved differently - we use their player id as storage and roll their different actors into one save object 
		for (FConstPlayerControllerIterator PCIt = GetWorld()->GetPlayerControllerIterator(); PCIt; ++PCIt)
		{
			APlayerController* PC = PCIt->Get();
			if (PC)
			{
				APlayerState* PS = PC->GetPlayerState<APlayerState>();
				APawn* Pawn = PC->GetPawn();
				if (PS && Pawn)
				{

					const bool bIsLevelTransition = UGameplayStatics::ParseOption(OptionString, "LevelTransition").Len() > 0;

					//If we're loading because we transitioned levels, we want to load our players data back in, but not do the transform, since that doesn't make any sense - we'll be in a new position now. 
					const FTransform OldPawnTransform = Pawn->GetActorTransform();

					//Next load the players pawn 
					LoadActorFromRecord(Pawn, PlayerSave.PawnData);

					if (bIsLevelTransition)
					{
						Pawn->SetActorTransform(OldPawnTransform);
					}

					//Load player state next - it has our players items, and granting these requires our pawn to be ready
					LoadActorFromRecord(PS, PlayerSave.PlayerStateData);

					//Load our PC last - it has our quests which require everything to be ready so waypoints are added to the right actor etc. 
					LoadActorFromRecord(PC, PlayerSave.ControllerData);
					break;
				}
			}
		}

		bIsCurrentlyLoading = false; 
	}
}

bool UNarrativeSaveSubsystem::CreatePlayerOnlySave(APlayerController* PC)
{
	if (PC)
	{
		APlayerState* PS = PC->GetPlayerState<APlayerState>();
		APawn* Pawn = PC->GetPawn();
		
		UNarrativeSave* NewPlayerSave  = Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromSlot(PS->GetPlayerName(), 0));

		if (!NewPlayerSave)
		{
			NewPlayerSave  = Cast<UNarrativeSave>(UGameplayStatics::CreateSaveGameObject(GetSaveGameClass()));
		}
		
		if (NewPlayerSave)
		{

			if (PS && Pawn)
			{
				FNarrativeSavePlayer PlayerData;

				CreateActorRecord(PS, PlayerData.PlayerStateData);
				CreateActorRecord(PC, PlayerData.ControllerData);
				CreateActorRecord(PC->GetPawn(), PlayerData.PawnData);

				//Don't save the controller or player state transforms, we don't really need to do this and at worst it may cause bugs 
				PlayerData.PlayerStateData.Transform = FTransform::Identity;
				PlayerData.ControllerData.Transform = FTransform::Identity;

				NewPlayerSave->PlayerData = PlayerData;

				FString RoleStr = PC->HasAuthority() ? "Server" : "Client";
				UE_LOG(LogSaveSystem, Warning, TEXT("%s CREATING PLAYER-ONLY SAVE FOR %s"), *RoleStr, *PS->GetPlayerName());
				
				//Save the player their own file using their PlayerName as the save type. For narrative users making their own game, you'll want to modify this. 
				return UGameplayStatics::SaveGameToSlot(NewPlayerSave, PS->GetPlayerName(), 0);
			}
		}
	}
	
	return false; 
}

bool UNarrativeSaveSubsystem::LoadPlayerOnlySave(APlayerController* PC)
{
	if (PC)
	{
		APlayerState* PS = PC->GetPlayerState<APlayerState>();
		APawn* Pawn = PC->GetPawn();
		if (PS && Pawn)
		{
			if (UNarrativeSave* PlayerSave = NarrativeSaveGame = Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromSlot(PS->GetPlayerName(), 0)))
			{
				bIsCurrentlyLoading = true;
				
				const bool bIsLevelTransition = UGameplayStatics::ParseOption(OptionString, "LevelTransition").Len() > 0;

				//If we're loading because we transitioned levels, we want to load our players data back in, but not do the transform, since that doesn't make any sense - we'll be in a new position now. 
				const FTransform OldPawnTransform = Pawn->GetActorTransform();
				const FNarrativeSavePlayer& SavePlayer = NarrativeSaveGame->PlayerData;

				FString RoleStr = PC->HasAuthority() ? "Server" : "Client";
				UE_LOG(LogSaveSystem, Warning, TEXT("%s LOADING PLAYER-ONLY SAVE FOR %s"), *RoleStr, *PS->GetPlayerName());
				
				
				//Next load the players pawn 
				LoadActorFromRecord(Pawn, SavePlayer.PawnData);

				if (bIsLevelTransition)
				{
					Pawn->SetActorTransform(OldPawnTransform);
				}

				//Load player state next - it has our players items, and granting these requires our pawn to be ready
				LoadActorFromRecord(PS, SavePlayer.PlayerStateData);

				//Load our PC last - it has our quests which require everything to be ready so waypoints are added to the right actor etc. 
				LoadActorFromRecord(PC, SavePlayer.ControllerData);
				bIsCurrentlyLoading = false;
				return true;
			}

		}
	}

	return false; 
}

bool UNarrativeSaveSubsystem::DeletePlayerOnlySave(APlayerController* PC)
{
	if (PC)
	{
		APlayerState* PS = PC->GetPlayerState<APlayerState>();
		APawn* Pawn = PC->GetPawn();
		if (PS && Pawn)
		{
			return UGameplayStatics::DeleteGameInSlot(PS->GetPlayerName(), 0);
		}
	}

	return false;
}

bool UNarrativeSaveSubsystem::MultiplayerSave(const FString& SaveName)
{
	//Create save files for every player in the game. 
	for (FConstPlayerControllerIterator PCIt = GetWorld()->GetPlayerControllerIterator(); PCIt; ++PCIt)
	{
		APlayerController* PC = PCIt->Get();

		if (PC)
		{
			CreatePlayerOnlySave(PC);
		}
	}

	return Save(SaveName);
}

bool UNarrativeSaveSubsystem::MultiplayerLoad(const FString& SaveName, const bool bChangeMap/*=true*/)
{
	FString LevelName;

	//Typically you'd want the server to actually load the level that was open at the time of save also. 
	if (bChangeMap)
	{
		if (UNarrativeSave* LoadedSaveGame = Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromSlot(SaveName, 0)))
		{
			LevelName = LoadedSaveGame->LevelName;
		}
	}
	else
	{
		LevelName = UGameplayStatics::GetCurrentLevelName(GetWorld(), true);
	}
	
	FString Cmd = FString::Printf(TEXT("servertravel %s?SaveGameName=%s"), *LevelName, *SaveName);
	
	if (UGameplayStatics::DoesSaveGameExist(SaveName, 0))
	{
		return false;
	}

	//Route the travel through a PC in the game so we can use servertravel command which resolves incomplete map name etc and is less strict then GetWorld()->ServerTravel. 
	for (FConstPlayerControllerIterator PCIt = GetWorld()->GetPlayerControllerIterator(); PCIt; ++PCIt)
	{
		APlayerController* PC = PCIt->Get();

		if (PC)
		{
			PC->ConsoleCommand(Cmd);

			break;
		}

	}

	return true;
	
	//LEGACY but may be useful for someone - We used to not re-load the level however this wasn't really ideal as made loading world much more complex and needed edgecases fixed. 
	/*//Load the data for all players in the game. 
	for (FConstPlayerControllerIterator PCIt = GetWorld()->GetPlayerControllerIterator(); PCIt; ++PCIt)
	{
		APlayerController* PC = PCIt->Get();

		if (PC)
		{
			LoadPlayerOnlySave(PC);
		}
	}
    
    //For multiplayer loading we actually need to res
	return Load(SaveName);*/
}

void UNarrativeSaveSubsystem::DeferredLoadPlayerData()
{
	LoadPlayerData();
}

bool UNarrativeSaveSubsystem::LoadSingleActor(class AActor* Actor)
{
	if (Actor && NarrativeSaveGame)
	{
		if (Actor->Implements<UNarrativeSavableActor>())
		{
			const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(Actor);

			if (ActorGUID.IsValid() && NarrativeSaveGame->RecordMap.Contains(ActorGUID))
			{
				LoadActorFromRecord(Actor, NarrativeSaveGame->RecordMap[ActorGUID]);

				return true;
			}
		}
	}

	return false;
}

bool UNarrativeSaveSubsystem::SaveSingleActor(class AActor* Actor)
{
	if (Actor && NarrativeSaveGame)
	{
		if (Actor->Implements<UNarrativeSavableActor>())
		{
			const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(Actor);

			if (ActorGUID.IsValid())
			{
				FNarrativeActorRecord NewRecord;

				if (CreateActorRecord(Actor, NewRecord))
				{
					NarrativeSaveGame->RecordMap.Add(NewRecord.ActorGUID, NewRecord);
				}

				return true;
			}
		}
	}

	return false;
}

bool UNarrativeSaveSubsystem::RemoveSingleActor(class AActor* Actor)
{
	if (Actor && NarrativeSaveGame)
	{
		if (Actor->Implements<UNarrativeSavableActor>())
		{
			const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(Actor);

			//Netstartup actors need a record with destroyed flag, we can't just remove the record 
			if (Actor->IsNetStartupActor() && !NarrativeSaveGame->RecordMap.Contains(ActorGUID))
			{
				SaveSingleActor(Actor);
			}

			if (ActorGUID.IsValid() && NarrativeSaveGame->RecordMap.Contains(ActorGUID))
			{
				FNarrativeActorRecord& Record = NarrativeSaveGame->RecordMap[ActorGUID];

				//If actor is net startup its record should always match. If not something has gone badly wrong 
				check(Record.bNetStartup == Actor->IsNetStartupActor());

				//If record is netstartup, we need a destroyed flag. If not, we can just remove the record entirely. 
				if (Record.bNetStartup)
				{
					if (!Record.bDestroyed)
					{
						Record.bDestroyed = true;
						UE_LOG(LogSaveSystem, Verbose, TEXT("Marked net startup actor %s destroyed! Marking as destroyed. "), *GetNameSafe(Actor));
					}
				}
				else
				{
					NarrativeSaveGame->RecordMap.Remove(ActorGUID);
				}

				return true; 
			}
		}
	}

	return false;
}

bool UNarrativeSaveSubsystem::DoesRecordExist(const FGuid& RecordGUID) const
{
	if (NarrativeSaveGame)
	{
		return NarrativeSaveGame->RecordMap.Contains(RecordGUID);
	}

	return false; 
}

bool UNarrativeSaveSubsystem::LoadDynamicRecord(const FGuid& RecordGUID)
{
	if (NarrativeSaveGame && NarrativeSaveGame->RecordMap.Contains(RecordGUID))
	{
		return LoadDynamicRecord(NarrativeSaveGame->RecordMap[RecordGUID]);
	}

	return false;
}

bool UNarrativeSaveSubsystem::LoadDynamicRecord(const FNarrativeActorRecord& Record)
{
	if (Record.IsValid() && Record.bNeedsDynamicSpawn)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.bNoFail = true;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		UClass* Class = Record.ActorSoftClass.LoadSynchronous();

		//TODO Just load an actor if its grid tile is currently loaded 
		AActor* Actor = GetWorld()->SpawnActorDeferred<AActor>(Class, Record.Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (IsValid(Actor))
		{
			Actor->FinishSpawning(Record.Transform);
			LoadActorFromRecord(Actor, Record);

			return true;
		}
	}

	return false;
}

void UNarrativeSaveSubsystem::SetSavingDisabled(const bool bShouldDisable)
{
	bSavingDisabled = bShouldDisable;
}

bool UNarrativeSaveSubsystem::IsNewGame() const
{
	if (NarrativeSaveGame)
	{
		return !NarrativeSaveGame->PlayerData.IsValid();
	}

	return true;
}

bool UNarrativeSaveSubsystem::IsLoading() const
{
	return bIsCurrentlyLoading; 
	//return GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(TimerHandle_DeferredLoadPlayerData);
}

AActor* UNarrativeSaveSubsystem::LookupActorByGUID(const FGuid& SearchGUID)
{
	if (SearchGUID.IsValid() && QuickLookupMap.Contains(SearchGUID))
	{
		return QuickLookupMap[SearchGUID].Get();
	}

	return nullptr; 
}

void UNarrativeSaveSubsystem::PostInitialize()
{
	Super::PostInitialize();

	//Bind to world actor spawned/destroyed. The thinking with this is we want to save/load 
	if (UWorld* World = GetWorld())
	{
		FOnActorSpawned::FDelegate ActorSpawned = FOnActorSpawned::FDelegate::CreateUObject(this, &UNarrativeSaveSubsystem::OnActorSpawned);
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(ActorSpawned);

		FOnActorSpawned::FDelegate ActorPreSpawned = FOnActorSpawned::FDelegate::CreateUObject(this, &UNarrativeSaveSubsystem::OnActorPrespawned);
		ActorPrespawnedHandle = World->AddOnActorPreSpawnInitialization(ActorPreSpawned);

		FOnActorDestroyed::FDelegate ActorDestroyed = FOnActorDestroyed::FDelegate::CreateUObject(this, &UNarrativeSaveSubsystem::OnActorDestroyed);
		ActorDestroyedHandle = World->AddOnActorDestroyedHandler(ActorDestroyed);

		FWorldDelegates::PreLevelRemovedFromWorld.AddUObject(this, &UNarrativeSaveSubsystem::PreLevelRemovedFromWorld);
		FWorldDelegates::LevelAddedToWorld.AddUObject(this,  &UNarrativeSaveSubsystem::LevelAddedToWorld);



		FWorldDelegates::OnPostWorldCreation.AddUObject(this, &UNarrativeSaveSubsystem::OnPostWorldCreation);
		FWorldDelegates::OnPreWorldFinishDestroy.AddUObject(this, &UNarrativeSaveSubsystem::OnPreWorldFinishDestroy);
	}
}

void UNarrativeSaveSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	//InitializeSaveSystem(InWorld);

	Super::OnWorldBeginPlay(InWorld);
}

bool UNarrativeSaveSubsystem::CreateActorRecord(class AActor* Actor, FNarrativeActorRecord& ActorRecord) const
{
	if (Actor)
	{
		//Because player data doesn't implement this interface, we don't actually exit out for non-interface actors, playerstate, controller etc need this.
		if (Actor->Implements<UNarrativeSavableActor>())
		{
			ActorRecord.ActorGUID = INarrativeStableActor::Execute_GetActorGUID(Actor);
			if (!ActorRecord.ActorGUID.IsValid())
			{
				//checkf(ActorRecord.ActorGUID.IsValid(), TEXT("Actor GUID returned wasn't valid. Have you implemented GetActorGUID()? "));
				return false;
			}

			ActorRecord.bNeedsDynamicSpawn = INarrativeSavableActor::Execute_ShouldRespawn(Actor);
		}

		UE_LOG(LogSaveSystem, Verbose, TEXT("Creating new record for actor %s under guid %s... "), *GetNameSafe(Actor), *ActorRecord.ActorGUID.ToString());

		ActorRecord.ActorName = Actor->GetFName();

		//Dont save non-movable actors - if designers move these around we do not want peoples save files negating those changes 
		if (Actor->GetRootComponent() && Actor->GetRootComponent()->Mobility == EComponentMobility::Movable)
		{
			ActorRecord.Transform = Actor->GetActorTransform();
		}

		ActorRecord.ActorSoftClass = Actor->GetClass();
		ActorRecord.bNetStartup = Actor->IsNetStartupActor();

		if (Actor->Implements<UNarrativeSavableActor>())
		{
			//Tell the component it is about to be saved so it can generate any save data it requires 
			INarrativeSavableActor::Execute_PrepareForSave(Actor);
		}

		FMemoryWriter MemWriter(ActorRecord.ByteData);
		FObjectAndNameAsStringProxyArchive Ar(MemWriter, true);
		Ar.ArIsSaveGame = true;

		Actor->Serialize(Ar);

		//Store any savable components in the record 
		ActorRecord.SavedComponents.Empty();

		for (auto& ActorComponent : Actor->GetComponents())
		{
			if (ActorComponent && ActorComponent->Implements<UNarrativeSavableComponent>())
			{
				//Tell the component it is about to be saved so it can generate any save data it requires 
				INarrativeSavableComponent::Execute_PrepareForSave(ActorComponent);

				FNarrativeSaveComponent SaveComponent;
				SaveComponent.ComponentName = ActorComponent->GetFName();

				FMemoryWriter CompMemWriter(SaveComponent.ByteData);

				FObjectAndNameAsStringProxyArchive CompAr(CompMemWriter, true);
				CompAr.ArIsSaveGame = true;

				ActorComponent->Serialize(CompAr);

				ActorRecord.SavedComponents.Add(SaveComponent);
			}
		}
		return true;
	}

	return false;
}

void UNarrativeSaveSubsystem::LoadActorFromRecord(class AActor* Actor, const FNarrativeActorRecord& ActorRecord) const
{
	if (Actor)
	{
		if (ActorRecord.bNetStartup && ActorRecord.bDestroyed)
		{
			Actor->Destroy();
			UE_LOG(LogSaveSystem, Verbose, TEXT("Destroying removed netstartup actor %s from record under guid %s... "), *GetNameSafe(Actor), *ActorRecord.ActorGUID.ToString());
			return;
		}

		//If the record contains a valid transform, we can set the actors transform. 
		if (!ActorRecord.Transform.Equals(FTransform::Identity))
		{
			if (Actor->GetRootComponent() && Actor->GetRootComponent()->Mobility == EComponentMobility::Movable)
			{
				Actor->SetActorTransform(ActorRecord.Transform, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}

		UE_LOG(LogSaveSystem, Verbose, TEXT("Loading actor %s from record under guid %s... "), *GetNameSafe(Actor), *ActorRecord.ActorGUID.ToString());

		FMemoryReader MemReader(ActorRecord.ByteData);

		FObjectAndNameAsStringProxyArchive Ar(MemReader, true);
		Ar.ArIsSaveGame = true;

		// Convert binary array back into actor's variables
		Actor->Serialize(Ar);

		//This function can be called on non-interface actors - the player pawn data doesnt implement the interface! 
		if (Actor->Implements<UNarrativeSavableActor>())
		{
			INarrativeSavableActor::Execute_SetActorGUID(Actor, ActorRecord.ActorGUID);
			//Let the actor define what it needs to do once its serialized UPROPERTY(SaveGame) variables are restored 
			INarrativeSavableActor::Execute_Load(Actor);
		}

		//Iterate the actors saved components and load those back in too. 
		for (FNarrativeSaveComponent SaveComponent : ActorRecord.SavedComponents)
		{
			for (UActorComponent* ActorComponent : Actor->GetComponents())
			{
				if (ActorComponent && ActorComponent->GetFName() == SaveComponent.ComponentName)
				{
					if (ActorComponent->Implements<UNarrativeSavableComponent>())
					{
						FMemoryReader ComponentMemReader(SaveComponent.ByteData);

						FObjectAndNameAsStringProxyArchive CompAr(ComponentMemReader, true);
						CompAr.ArIsSaveGame = true;
						// Convert binary array back into actor's variables
						ActorComponent->Serialize(CompAr);

						INarrativeSavableComponent::Execute_Load(ActorComponent);

						//We've loaded the saved comp back in, find next one 
						break;
					}
				}
			}
		}
	}
}

void UNarrativeSaveSubsystem::InitializeSaveSystem(UWorld& InWorld)
{
	if (UWorld* World = GetWorld())
	{
		//OnWorldBeginPlay is called before all actors begin play. This is nice - we can set their data so its all ready for BeginPlay(). 
		if (AGameModeBase* GM = World->GetAuthGameMode())
		{
			OptionString = GM->OptionsString;

			QuickLookupMap.Empty();
			// Iterate the entire world of actors, and create a lookup map key
			for (FActorIterator It(GetWorld()); It; ++It)
			{
				AActor* Actor = *It;

				//Skip player pawns 
				if (Actor)
				{
					//Cast method doesn't work with BP interfaces 
					if (Actor->Implements<UNarrativeStableActor>())
					{
						FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(Actor);

						QuickLookupMap.FindOrAdd(ActorGUID, Actor);
					}
				}
			}


			FString SlotString = "";

			//Store whether we come here from the main menu, or did we come here from changing from one map to another? 
			bool bIsLevelTransition = false; 
			int32 Slot = 0;

			if (OptionString.Len())
			{
				SlotString = UGameplayStatics::ParseOption(OptionString, "SaveGameName");
				bIsLevelTransition = UGameplayStatics::ParseOption(OptionString, "LevelTransition").Len() > 0;
			}
			/*else if (World->WorldType == EWorldType::PIE)
			{
				if (const USaveSystemDeveloperSettings* SaveDevSettings = GetDefault<USaveSystemDeveloperSettings>())
				{
					if (SaveDevSettings->bAutoLoadFirstSaveInEditor)
					{
						//In editor, lets auto load the first save slot provided we're in that level
						UNarrativeSave* LoadedSaveGame = Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromSlot("NarrativeSave0", Slot));

						if (LoadedSaveGame)
						{
							if (LoadedSaveGame->LevelName == UGameplayStatics::GetCurrentLevelName(GetWorld(), true))
							{
								SlotString = "NarrativeSave0";
							}
						}
					}
				}
			}*/

			if (UGameplayStatics::DoesSaveGameExist(SlotString, Slot))
			{
				//If we're came here from transitioning levels, we need to load everything back as it was, with the exception of our characters transform, since the new map will want us placed somewhere else. 
				Load(SlotString, Slot);
			}
			else //If we don't have a save to load we're a new game and should make an object, but not put anything in it yet
			{	
				UpdateSaveObject(true);
			}
		}
	}
}

void UNarrativeSaveSubsystem::OnActorPrespawned(class AActor* SpawnedActor)
{
	//if (SpawnedActor && NarrativeSaveGame)
	//{
	//	if (SpawnedActor->Implements<UNarrativeSavableActor>())
	//	{
	//		const FGuid ActorGUID = INarrativeSavableActor::Execute_GetActorGUID(SpawnedActor);

	//		if (NarrativeSaveGame->RecordMap.Contains(ActorGUID))
	//		{
	//			UE_LOG(LogSaveSystem, Warning, TEXT("Actor %s was prespawned and had a record! "), *GetNameSafe(SpawnedActor));
	//		}
	//	}
	//}
} 

void UNarrativeSaveSubsystem::OnActorSpawned(class AActor* SpawnedActor)
{
	if (SpawnedActor)
	{
		if (SpawnedActor->Implements<UNarrativeStableActor>())
		{
			const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(SpawnedActor);

			QuickLookupMap.FindOrAdd(ActorGUID, SpawnedActor);
			OnStableActorSpawned.Broadcast(SpawnedActor, ActorGUID);
		}
	}
}

void UNarrativeSaveSubsystem::OnActorDestroyed(class AActor* DestroyedActor)
{
	if (DestroyedActor)
	{
		if (DestroyedActor->Implements<UNarrativeStableActor>())
		{
			const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(DestroyedActor);

			QuickLookupMap.Remove(ActorGUID);
			OnStableActorDestroyed.Broadcast(DestroyedActor, ActorGUID);
		}
	}
	//if (DestroyedActor && NarrativeSaveGame)
	//{
	//	if (DestroyedActor->Implements<UNarrativeSavableActor>())
	//	{
	//		const FGuid ActorGUID = INarrativeSavableActor::Execute_GetActorGUID(DestroyedActor);

	//		/**When an actor is destroyed, remove it from the save file, so it wont be loaded. Net startup actors need to keep an record around,
	//		because the world will try load them back in and we need to check their record to see whether they should or not. */
	//		if (NarrativeSaveGame->RecordMap.Contains(ActorGUID))
	//		{
	//			FNarrativeActorRecord& Record = NarrativeSaveGame->RecordMap[ActorGUID];

	//			if (Record.bNetStartup)
	//			{
	//				if (!Record.bDestroyed)
	//				{
	//					Record.bDestroyed = true;
	//					UE_LOG(LogSaveSystem, Warning, TEXT("Savable actor %s was destroyed! Marking as destroyed. "), *GetNameSafe(DestroyedActor));
	//				}
	//			}
	//			else
	//			{
	//				const bool bNeedsPermanentRecord = INarrativeSavableActor::Execute_NeedsPermanentRecord(DestroyedActor);

	//				if (bNeedsPermanentRecord)
	//				{
	//					//If a permanent actor is destroyed, not only do we not want to remove its record - we actually want to save its record as its about to be gone. 
	//					FNarrativeActorRecord NewRecord;
	//					if (CreateActorRecord(DestroyedActor, NewRecord))
	//					{
	//						NarrativeSaveGame->RecordMap.Add(NewRecord.ActorGUID, NewRecord);
	//					}
	//					UE_LOG(LogSaveSystem, Warning, TEXT("Actor %s required a permanent record, so we've kept its record. "), *GetNameSafe(DestroyedActor));
	//				}
	//				else
	//				{
	//					NarrativeSaveGame->RecordMap.Remove(ActorGUID);
	//				}
	//			}
	//		}
	//		else if(DestroyedActor->IsNetStartupActor())
	//		{
	//			FNarrativeActorRecord NewRecord;
	//			if (CreateActorRecord(DestroyedActor, NewRecord))
	//			{
	//				if (NewRecord.bNetStartup)
	//				{
	//					NewRecord.bDestroyed = true;
	//				}

	//				NarrativeSaveGame->RecordMap.Add(NewRecord.ActorGUID, NewRecord);
	//			}
	//		}
	//	}
	//}
}

void UNarrativeSaveSubsystem::PreLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld)
{
	if (InLevel && NarrativeSaveGame)
	{
		for (auto& DestroyedActor : InLevel->Actors)
		{
			if (IsValid(DestroyedActor))
			{
				if (DestroyedActor->Implements<UNarrativeStableActor>())
				{
					const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(DestroyedActor);

					QuickLookupMap.Remove(ActorGUID);
				}
			}
		}
	}
}

void UNarrativeSaveSubsystem::LevelAddedToWorld(ULevel* InLevel, UWorld* InWorld)
{
	//OnWorldBeginPlay doesn't work as world partition actors dont seem to be loaded yet, so we also cache in here. 
	if (InLevel && NarrativeSaveGame)
	{
		for(int32 i = InLevel->Actors.Num() - 1; i >= 0; --i)
		{
			if (InLevel->Actors.IsValidIndex(i))
			{
				AActor* SpawnedActor = InLevel->Actors[i];

				if (SpawnedActor)
				{
					if (SpawnedActor->Implements<UNarrativeStableActor>())
					{
						const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(SpawnedActor);

						QuickLookupMap.FindOrAdd(ActorGUID, SpawnedActor);
					}
				}
			}
		}
	}
}

void UNarrativeSaveSubsystem::OnPreWorldFinishDestroy(UWorld* World)
{

}

void UNarrativeSaveSubsystem::OnPostWorldCreation(UWorld* World)
{

}

TSubclassOf<class UNarrativeSave> UNarrativeSaveSubsystem::GetSaveGameClass() const
{
	TSubclassOf<class UNarrativeSave> SaveClass = UNarrativeSave::StaticClass();

	if (const USaveSystemDeveloperSettings* SaveDevSettings = GetDefault<USaveSystemDeveloperSettings>())
	{
		if (SaveDevSettings->SaveGameClass.IsValid())
		{
			SaveClass = SaveDevSettings->SaveGameClass.TryLoadClass<UNarrativeSave>();
		}
	}

	return SaveClass;
}
