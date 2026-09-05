// Copyright Narrative Tools 2024. 


#include "Subsystems/NarrativeSaveSubsystem.h"
#include "NarrativeSave.h"
#include "UObject/StrongObjectPtr.h"
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

FNarrativeInitialSaveRequested UNarrativeSaveSubsystem::OnInitialSaveRequested;

bool UNarrativeSaveSubsystem::UpdateSaveObject(const bool bSkipRecordCreation)
{
	if (bSkipRecordCreation)
	{
		if (!NarrativeSaveGame)
		{ NarrativeSaveGame = Cast<UNarrativeSave>(UGameplayStatics::CreateSaveGameObject(GetSaveGameClass())); }
		return NarrativeSaveGame != nullptr;
	}
	UNarrativeSave* Candidate = nullptr;
	if (!CaptureSaveObject(Candidate)) { return false; }
	NarrativeSaveGame = Candidate;
	return true;
}

bool UNarrativeSaveSubsystem::CaptureSaveObject(UNarrativeSave*& OutSnapshot)
{
	OutSnapshot = nullptr;
	if (!GetWorld() || bIsCurrentlyLoading || bIsCapturingSnapshot) { return false; }
	TGuardValue<bool> CaptureGuard(bIsCapturingSnapshot, true);
	UNarrativeSave* Candidate = NarrativeSaveGame
		? DuplicateObject<UNarrativeSave>(NarrativeSaveGame, this)
		: Cast<UNarrativeSave>(UGameplayStatics::CreateSaveGameObject(GetSaveGameClass()));
	TStrongObjectPtr<UNarrativeSave> KeepCandidate(Candidate);
	if (Candidate)
	{
		//Main menu logic uses this to remember which map we had loaded
		Candidate->LevelName = UGameplayStatics::GetCurrentLevelName(GetWorld(), true);

		TSet<AActor*> SkipActors;
		TSet<FGuid> CapturedGuids;

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

					if (!CreateActorRecord(PS, PlayerData.PlayerStateData)
						|| !CreateActorRecord(PC, PlayerData.ControllerData)
						|| !CreateActorRecord(Pawn, PlayerData.PawnData)) { return false; }

					for (const FGuid Guid : { PlayerData.PlayerStateData.ActorGUID, PlayerData.ControllerData.ActorGUID, PlayerData.PawnData.ActorGUID })
					{
						if (Guid.IsValid())
						{ if (CapturedGuids.Contains(Guid)) { return false; } CapturedGuids.Add(Guid); }
					}
					SkipActors.Add(PS);
					SkipActors.Add(PC);
					SkipActors.Add(PC->GetPawn());

					//Don't save the controller or player state transforms, we don't really need to do this and at worst it may cause bugs 
					PlayerData.PlayerStateData.Transform = FTransform::Identity;
					PlayerData.ControllerData.Transform = FTransform::Identity;

					Candidate->PlayerData = PlayerData;

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
							if (const auto* OwnerPolicy = Cast<INarrativeSavableActor>(Actor); OwnerPolicy && !OwnerPolicy->ShouldSaveWorldRecord())
							{ Candidate->RecordMap.Remove(ActorGUID); continue; }
							if (CapturedGuids.Contains(ActorGUID)) { return false; }
							CapturedGuids.Add(ActorGUID);
							//Create a record of the actor and any components its needs saved 
							FNarrativeActorRecord SaveActor;
							if (!CreateActorRecord(Actor, SaveActor)) { return false; }

							Candidate->RecordMap.Add(ActorGUID, SaveActor);
							FailedUnloadedRecords.Remove(ActorGUID);
						}
					}
				}
			}
		}

		if (!FailedUnloadedRecords.IsEmpty()) { return false; }
		OutSnapshot = Candidate;
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

	if (!UpdateSaveObject()) { return false; }

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

bool UNarrativeSaveSubsystem::Load(const FString& SaveName, const int32 Slot)
{
	UNarrativeSave* Candidate = Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromSlot(SaveName, Slot));
	if (!LoadFromSnapshot(Candidate)) { return false; }
	CurrentSaveName = SaveName;
	CurrentSaveSlot = Slot;
	return true;
}

bool UNarrativeSaveSubsystem::LoadFromSnapshot(UNarrativeSave* Snapshot)
{
    if (!Snapshot || !GetWorld() || bIsCurrentlyLoading || bIsCapturingSnapshot) { return false; }
    TGuardValue<bool> LoadingGuard(bIsCurrentlyLoading, true);
    TStrongObjectPtr<UNarrativeSave> KeepSnapshot(Snapshot), Previous(NarrativeSaveGame);
    struct FRestoreJob
    {
        TWeakObjectPtr<AActor> Actor;
        FNarrativeActorRecord Record;
        ENarrativeRestorePhase Phase = ENarrativeRestorePhase::Interactables;
        bool bSpawn = false;
    };
    TArray<FRestoreJob> Jobs;
    TSet<FGuid> FoundRecords;
    for (FActorIterator It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor) || !Actor->Implements<UNarrativeStableActor>()) { continue; }
        const auto* OwnerPolicy = Cast<INarrativeSavableActor>(Actor);
        if (OwnerPolicy && !OwnerPolicy->ShouldSaveWorldRecord()) { continue; }
        const FGuid Guid = INarrativeStableActor::Execute_GetActorGUID(Actor);
        if (const FNarrativeActorRecord* Record = Snapshot->RecordMap.Find(Guid))
        {
            if (FoundRecords.Contains(Guid) || !ValidateRecordForActor(Actor, *Record)) { return false; }
            FoundRecords.Add(Guid);
            auto& Job = Jobs.AddDefaulted_GetRef(); Job.Actor = Actor; Job.Record = *Record;
            Job.Phase = OwnerPolicy ? OwnerPolicy->GetSaveRestorePhase() : Record->RestorePhase;
        }
    }
    // Copy before callbacks: destruction/load delegates may change the live record map.
    for (const auto& Pair : Snapshot->RecordMap)
    {
        const auto& Record = Pair.Value;
        if (!Pair.Key.IsValid() || Pair.Key != Record.ActorGUID || !Record.IsValid()
            || static_cast<uint8>(Record.RestorePhase) > static_cast<uint8>(ENarrativeRestorePhase::MissionResume)) { return false; }
        if (!FoundRecords.Contains(Record.ActorGUID) && !Record.bNetStartup && Record.bNeedsDynamicSpawn && !Record.bDestroyed)
        {
            UClass* Class = Record.ActorSoftClass.LoadSynchronous();
            if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))
            { if (Record.bOptional) { UE_LOG(LogSaveSystem, Warning, TEXT("Skipping unavailable optional actor %s"), *Record.ActorName.ToString()); continue; } return false; }
            const auto* DefaultActor = Cast<AActor>(Class->GetDefaultObject());
            auto& Job = Jobs.AddDefaulted_GetRef(); Job.Record = Record; Job.bSpawn = true;
            const auto* Policy = Cast<INarrativeSavableActor>(DefaultActor);
            Job.Phase = Policy ? Policy->GetSaveRestorePhase() : Record.RestorePhase;
        }
    }
    Jobs.Sort([](const FRestoreJob& A, const FRestoreJob& B)
    {
        if (A.Phase != B.Phase) { return A.Phase < B.Phase; }
        return A.Record.ActorGUID.ToString(EGuidFormats::Digits) < B.Record.ActorGUID.ToString(EGuidFormats::Digits);
    });
    NarrativeSaveGame = Snapshot;
    OnBeginLoad.Broadcast();
    for (const FRestoreJob& Job : Jobs)
    {
        if (!(Job.bSpawn ? LoadDynamicRecord(Job.Record) : LoadActorFromRecord(Job.Actor.Get(), Job.Record)))
        { NarrativeSaveGame = Previous.Get(); return false; }
    }
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
	const APlayerState* PS = IsValid(PC) ? PC->GetPlayerState<APlayerState>() : nullptr;
	return PS && CreatePlayerOnlySaveInSlot(PC, PS->GetPlayerName());
}

bool UNarrativeSaveSubsystem::CreatePlayerOnlySaveInSlot(APlayerController* PC, const FString& SlotName)
{
	APlayerState* PS = IsValid(PC) ? PC->GetPlayerState<APlayerState>() : nullptr;
	APawn* Pawn = IsValid(PC) ? PC->GetPawn() : nullptr;
	if (bSavingDisabled || !IsValid(PS) || !IsValid(Pawn) || !PC->HasAuthority() || SlotName.IsEmpty()) { return false; }
	FNarrativeSavePlayer Records;
	if (!CreateActorRecord(PS, Records.PlayerStateData)
		|| !CreateActorRecord(PC, Records.ControllerData)
		|| !CreateActorRecord(Pawn, Records.PawnData)) { return false; }
	// Preserve configured save-subclass data (including character creator data).
	// This isolated slot object must never replace the live world save object.
	UNarrativeSave* PlayerSave = Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!PlayerSave)
	{ PlayerSave = Cast<UNarrativeSave>(UGameplayStatics::CreateSaveGameObject(GetSaveGameClass())); }
	if (!PlayerSave) { return false; }
	Records.PlayerStateData.Transform = FTransform::Identity;
	Records.ControllerData.Transform = FTransform::Identity;
	PlayerSave->PlayerData = MoveTemp(Records);
	return UGameplayStatics::SaveGameToSlot(PlayerSave, SlotName, 0);
}

bool UNarrativeSaveSubsystem::ReadPlayerOnlySave(const FString& SlotName, FNarrativeSavePlayer& OutPlayerData) const
{
	if (SlotName.IsEmpty()) { return false; }
	const UNarrativeSave* PlayerSave = Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!PlayerSave || !PlayerSave->PlayerData.IsValid()
		|| !PlayerSave->PlayerData.PlayerStateData.IsValid() || !PlayerSave->PlayerData.ControllerData.IsValid()) { return false; }
	OutPlayerData = PlayerSave->PlayerData;
	return true;
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
				return LoadActorFromRecord(Actor, NarrativeSaveGame->RecordMap[ActorGUID]);
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

				if (!CreateActorRecord(Actor, NewRecord)) { return false; }
				NarrativeSaveGame->RecordMap.Add(NewRecord.ActorGUID, NewRecord);
				FailedUnloadedRecords.Remove(ActorGUID);
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
		UClass* Class = Record.ActorSoftClass.LoadSynchronous();
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract)) { return false; }

		//TODO Just load an actor if its grid tile is currently loaded 
		AActor* Actor = GetWorld()->SpawnActorDeferred<AActor>(Class, Record.Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (IsValid(Actor))
		{
			Actor->FinishSpawning(Record.Transform);
			if (LoadActorFromRecord(Actor, Record)) { return true; }
			if (IsValid(Actor)) { Actor->Destroy(); }
			return false;
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

bool UNarrativeSaveSubsystem::CreateActorRecord(AActor* Actor, FNarrativeActorRecord& ActorRecord) const
{
	if (!IsValid(Actor)) { return false; }
	FNarrativeActorRecord Candidate;
	if (Actor->Implements<UNarrativeSavableActor>())
	{
		Candidate.ActorGUID = INarrativeStableActor::Execute_GetActorGUID(Actor);
		if (!Candidate.ActorGUID.IsValid()) { return false; }
		Candidate.bNeedsDynamicSpawn = INarrativeSavableActor::Execute_ShouldRespawn(Actor);
		if (!IsValid(Actor)) { return false; }
		Candidate.bOptional = INarrativeSavableActor::Execute_IsOptionalSaveRecord(Actor);
		if (!IsValid(Actor)) { return false; }
		INarrativeSavableActor::Execute_PrepareForSave(Actor);
		if (!IsValid(Actor)) { return false; }
	}
	Candidate.ActorName = Actor->GetFName();
	Candidate.ActorSoftClass = Actor->GetClass();
    if (const auto* Policy = Cast<INarrativeSavableActor>(Actor)) { Candidate.RestorePhase = Policy->GetSaveRestorePhase(); }
	Candidate.bNetStartup = Actor->IsNetStartupActor();
	Candidate.Transform = FTransform::Identity;
	if (Actor->GetRootComponent() && Actor->GetRootComponent()->Mobility == EComponentMobility::Movable)
	{ Candidate.Transform = Actor->GetActorTransform(); }
	FMemoryWriter MemWriter(Candidate.ByteData);
	FObjectAndNameAsStringProxyArchive Ar(MemWriter, true);
	Ar.ArIsSaveGame = true;
	Actor->Serialize(Ar);
	if (Ar.IsError() || !IsValid(Actor)) { return false; }
	TInlineComponentArray<UActorComponent*> Components(Actor);
	for (UActorComponent* Component : Components)
	{
		if (!IsValid(Component) || !Component->Implements<UNarrativeSavableComponent>()) { continue; }
		INarrativeSavableComponent::Execute_PrepareForSave(Component);
		if (!IsValid(Actor) || !IsValid(Component)) { return false; }
		FNarrativeSaveComponent Record;
		Record.ComponentName = Component->GetFName();
		Record.ComponentClass = Component->GetClass();
        if (const auto* Policy = Cast<INarrativeSavableComponent>(Component)) { Record.RestorePhase = Policy->GetSaveRestorePhase(); }
		Record.bOptional = INarrativeSavableComponent::Execute_IsOptionalSaveRecord(Component);
		if (!IsValid(Actor) || !IsValid(Component)) { return false; }
		FMemoryWriter Writer(Record.ByteData);
		FObjectAndNameAsStringProxyArchive ComponentAr(Writer, true);
		ComponentAr.ArIsSaveGame = true;
		Component->Serialize(ComponentAr);
		if (ComponentAr.IsError() || !IsValid(Actor) || !IsValid(Component)) { return false; }
		Candidate.SavedComponents.Add(MoveTemp(Record));
	}
	ActorRecord = MoveTemp(Candidate); // Never replace a caller's last good record on capture failure.
	return true;
}

bool UNarrativeSaveSubsystem::ValidateRecordForActor(const AActor* Actor, const FNarrativeActorRecord& Record) const
{
    if (!IsValid(Actor) || !Record.IsValid() || static_cast<uint8>(Record.RestorePhase) > static_cast<uint8>(ENarrativeRestorePhase::MissionResume)
        || Record.Transform.ContainsNaN()) { return false; }
    if (!Record.ActorSoftClass.IsNull())
    {
        UClass* Class = Record.ActorSoftClass.LoadSynchronous();
        if (!Class || !Actor->IsA(Class)) { return false; }
    }
    TSet<FName> SeenNames;
    TInlineComponentArray<UActorComponent*> Components(Actor);
    for (const auto& Saved : Record.SavedComponents)
    {
        if (Saved.ComponentName.IsNone() || SeenNames.Contains(Saved.ComponentName)
            || static_cast<uint8>(Saved.RestorePhase) > static_cast<uint8>(ENarrativeRestorePhase::MissionResume)) { return false; }
        SeenNames.Add(Saved.ComponentName);
        const UActorComponent* Found = nullptr;
        for (UActorComponent* Component : Components)
        { if (IsValid(Component) && Component->GetFName() == Saved.ComponentName && Component->Implements<UNarrativeSavableComponent>()) { Found = Component; break; } }
        UClass* SavedClass = Saved.ComponentClass.IsNull() ? nullptr : Saved.ComponentClass.LoadSynchronous();
        if ((!Found || (!Saved.ComponentClass.IsNull() && (!SavedClass || !Found->IsA(SavedClass)))) && !Saved.bOptional) { return false; }
    }
    return true;
}

bool UNarrativeSaveSubsystem::LoadActorFromRecord(AActor* Actor, const FNarrativeActorRecord& ActorRecord) const
{
	if (!ValidateRecordForActor(Actor, ActorRecord)) { return false; }
	if (ActorRecord.bNetStartup && ActorRecord.bDestroyed) { return Actor->Destroy(); }
	if (!ActorRecord.Transform.Equals(FTransform::Identity)
		&& Actor->GetRootComponent() && Actor->GetRootComponent()->Mobility == EComponentMobility::Movable)
	{ Actor->SetActorTransform(ActorRecord.Transform, false, nullptr, ETeleportType::TeleportPhysics); }
	if (!IsValid(Actor)) { return false; }
	FMemoryReader Reader(ActorRecord.ByteData);
	FObjectAndNameAsStringProxyArchive Ar(Reader, true);
	Ar.ArIsSaveGame = true;
	Actor->Serialize(Ar);
	if (Ar.IsError() || !IsValid(Actor)) { return false; }
	if (Actor->Implements<UNarrativeSavableActor>())
	{
		INarrativeSavableActor::Execute_SetActorGUID(Actor, ActorRecord.ActorGUID);
		if (!IsValid(Actor)) { return false; }
		RefreshStableActorIdentity(Actor);
		INarrativeSavableActor::Execute_Load(Actor);
		if (!IsValid(Actor)) { return false; }
	}
	TArray<FNarrativeSaveComponent> OrderedComponents = ActorRecord.SavedComponents;
    const auto CurrentPhase = [Actor](const FNarrativeSaveComponent& Record)
    {
        TInlineComponentArray<UActorComponent*> Components(Actor);
        for (UActorComponent* Component : Components)
        {
            if (IsValid(Component) && Component->GetFName() == Record.ComponentName)
            { if (const auto* Policy = Cast<INarrativeSavableComponent>(Component)) { return Policy->GetSaveRestorePhase(); } }
        }
        return Record.RestorePhase;
    };
    OrderedComponents.StableSort([&CurrentPhase](const FNarrativeSaveComponent& A, const FNarrativeSaveComponent& B)
    {
        return CurrentPhase(A) < CurrentPhase(B); // Neutral legacy components preserve their previous relative order.
    });
    for (const FNarrativeSaveComponent& Record : OrderedComponents)
    {
		bool bFoundComponent = false;
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (UActorComponent* Component : Components)
		{
			if (!IsValid(Component) || Component->GetFName() != Record.ComponentName
				|| !Component->Implements<UNarrativeSavableComponent>()) { continue; }
			bFoundComponent = true;
			if (!Record.ComponentClass.IsNull())
			{
				UClass* SavedClass = Record.ComponentClass.LoadSynchronous();
				if (!SavedClass || !Component->IsA(SavedClass))
				{ if (Record.bOptional) { break; } return false; }
			}
			FMemoryReader ComponentReader(Record.ByteData);
			FObjectAndNameAsStringProxyArchive ComponentAr(ComponentReader, true);
			ComponentAr.ArIsSaveGame = true;
			Component->Serialize(ComponentAr);
			if (ComponentAr.IsError() || !IsValid(Actor) || !IsValid(Component)) { return false; }
			INarrativeSavableComponent::Execute_Load(Component);
			if (!IsValid(Actor) || !IsValid(Component)) { return false; }
			break;
		}
		if (!bFoundComponent)
		{
			if (!Record.bOptional) { return false; }
			UE_LOG(LogSaveSystem, Warning, TEXT("Skipping missing optional save component %s"), *Record.ComponentName.ToString());
		}
	}
	return true;
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

						if (ActorGUID.IsValid()) { QuickLookupMap.FindOrAdd(ActorGUID, Actor); }
					}
				}
			}


            UNarrativeSave* InitialSnapshot = nullptr;
            bool bOverrideRequested = false;
            OnInitialSaveRequested.Broadcast(InWorld, InitialSnapshot, bOverrideRequested);
            if (bOverrideRequested)
            {
                bInitialLoadFailed = !LoadFromSnapshot(InitialSnapshot);
                return;
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
				bInitialLoadFailed = !Load(SlotString, Slot);
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

void UNarrativeSaveSubsystem::RefreshStableActorIdentity(AActor* Actor) const
{
	if (!IsValid(Actor) || !Actor->Implements<UNarrativeStableActor>()) { return; }
	for (auto It = QuickLookupMap.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid() || It.Value().Get() == Actor) { It.RemoveCurrent(); }
	}
	const FGuid Guid = INarrativeStableActor::Execute_GetActorGUID(Actor);
	if (Guid.IsValid()) { QuickLookupMap.Add(Guid, Actor); }
}

void UNarrativeSaveSubsystem::OnActorSpawned(AActor* SpawnedActor)
{
	if (IsValid(SpawnedActor) && SpawnedActor->Implements<UNarrativeStableActor>())
	{
		const FGuid Guid = INarrativeStableActor::Execute_GetActorGUID(SpawnedActor);
		if (Guid.IsValid())
		{
			RefreshStableActorIdentity(SpawnedActor);
			OnStableActorSpawned.Broadcast(SpawnedActor, Guid);
		}
	}
}

void UNarrativeSaveSubsystem::OnActorDestroyed(class AActor* DestroyedActor)
{
	if (DestroyedActor && NarrativeSaveGame && !bIsCurrentlyLoading && GetWorld() && !GetWorld()->bIsTearingDown
		&& DestroyedActor->Implements<UNarrativeSavableActor>())
	{
		const FGuid Guid = INarrativeStableActor::Execute_GetActorGUID(DestroyedActor);
		if (Guid.IsValid())
		{
			if (DestroyedActor->IsNetStartupActor())
			{
				// A destroyed placed actor needs only an identity tombstone, not serialization of a dying UObject.
				auto& Record = NarrativeSaveGame->RecordMap.FindOrAdd(Guid);
				Record.ActorGUID = Guid; Record.ActorName = DestroyedActor->GetFName();
				Record.ActorSoftClass = DestroyedActor->GetClass(); Record.bNetStartup = true; Record.bDestroyed = true;
			}
			else { NarrativeSaveGame->RecordMap.Remove(Guid); }
		}
	}
	if (DestroyedActor)
	{
		if (DestroyedActor->Implements<UNarrativeStableActor>())
		{
			const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(DestroyedActor);

			// A staged replacement may already own this saved GUID. Destroying
			// the old actor must not evict the replacement's lookup entry.
			if (const TWeakObjectPtr<AActor>* Entry = QuickLookupMap.Find(ActorGUID))
			{ if (Entry->Get() == DestroyedActor) { QuickLookupMap.Remove(ActorGUID); } }
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
	if (InLevel && NarrativeSaveGame && InWorld == GetWorld())
	{
		for (auto& DestroyedActor : InLevel->Actors)
		{
			if (IsValid(DestroyedActor))
			{
				if (DestroyedActor->Implements<UNarrativeStableActor>())
				{
					const FGuid ActorGUID = INarrativeStableActor::Execute_GetActorGUID(DestroyedActor);
					const auto* OwnerPolicy = Cast<INarrativeSavableActor>(DestroyedActor);
					if (!bIsCurrentlyLoading && DestroyedActor->Implements<UNarrativeSavableActor>()
						&& (!OwnerPolicy || OwnerPolicy->ShouldSaveWorldRecord()) && !SaveSingleActor(DestroyedActor))
					{ FailedUnloadedRecords.Add(ActorGUID); UE_LOG(LogSaveSystem, Error, TEXT("Could not capture unloaded actor %s; save blocked until the actor recaptures or the checkpoint reloads."), *GetNameSafe(DestroyedActor)); }
					QuickLookupMap.Remove(ActorGUID);
				}
			}
		}
	}
}

void UNarrativeSaveSubsystem::LevelAddedToWorld(ULevel* InLevel, UWorld* InWorld)
{
	//OnWorldBeginPlay doesn't work as world partition actors dont seem to be loaded yet, so we also cache in here. 
	if (InLevel && NarrativeSaveGame && InWorld == GetWorld())
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
