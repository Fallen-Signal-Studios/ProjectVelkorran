// Copyright Narrative Tools 2024. 


#include "UnrealFramework/NarrativeGameMode.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Settings/NarrativeDeveloperSettings.h"
#include "Engine/World.h"
#include "Engine/TimerHandle.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "UObject/ObjectPtr.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

ANarrativeGameMode::ANarrativeGameMode()
{

}

void ANarrativeGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	//Initialize the save system so our save object is created. That way when we create players in PostLogin they have the save object and can load themselves. 
	if (UWorld* World = GetWorld())
	{
		if (UNarrativeSaveSubsystem* SaveSub = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			SaveSub->InitializeSaveSystem(*World);
		}
	}

}

FString ANarrativeGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	UE_LOG(LogTemp, Warning, TEXT("InitNewPlayer Options: %s Portal: %s"), *Options, *Portal);

	FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	
	// Init player's name - UE does this by default, but forces PC name which for our purposes isnt always what we want.
	FString InName = UGameplayStatics::ParseOption(Options, TEXT("CustomName")).Left(30);
	
	if (!InName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Applying custom name %s"), *InName);
		ChangeName(NewPlayerController, InName, false);
	}
	
	return ErrorMessage;
}

UPlayerDefinition* ANarrativeGameMode::GetPlayerDefinitionForController_Implementation(AController* InController)
{

	TArray<UPlayerDefinition*> Definitions = PlayerDefinitions;

#if WITH_EDITOR
	if (const UNarrativeDeveloperSettings* NarrativeDevSettings = GetDefault<UNarrativeDeveloperSettings>())
	{
		if (NarrativeDevSettings->PlayerDefinitionOverrides.Num() > 0)
		{
			Definitions = NarrativeDevSettings->PlayerDefinitionOverrides;
		}
	}
#endif 

	const int32 PIndex = GetNumPlayers() - 1;
	
	
	if (Definitions.IsValidIndex(PIndex))
	{
		return Definitions[PIndex];
	}
	else
	{
		//If only one def exists use that
		if (Definitions.IsValidIndex(0))
		{
			return Definitions[0];
		}
	}

	return nullptr; 
}

APawn* ANarrativeGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save default player pawns into a map
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	APawn* ResultPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);

	ANarrativePlayerCharacter* NChar = Cast<ANarrativePlayerCharacter>(ResultPawn);

	/**As players join the game, assign their player definition assets. */
	if (NChar)
	{
		if (UPlayerDefinition* PDef = GetPlayerDefinitionForController(NewPlayer))
		{
			const int32 PIndex = GetNumPlayers() - 1;
	
			UE_LOG(LogTemp, Warning, TEXT("Game mode assigned joiner PIndex %d and def %s"), PIndex, *GetNameSafe(PDef));
			NChar->SetPlayerDefinition(PDef);
		}
		else
		{
			UE_LOG(LogGameMode, Warning, TEXT("Couldnt get player definition for spawned player! "));
		}	
	}

	if (!ResultPawn)
	{
		UE_LOG(LogGameMode, Warning, TEXT("SpawnDefaultPawnAtTransform: Couldn't spawn Pawn of type %s at %s"), *GetNameSafe(PawnClass), *SpawnTransform.ToHumanReadableString());
	}

	//Server registers here, clients register in OnRep_PlayerDefinition 
	if (NChar)
	{
		if (UNarrativeCharacterSubsystem* NPCSubsystem = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
		{
			UE_LOG(LogTemp, Warning, TEXT("REGISTERING CHARACTER on server %s"), *GetNameSafe(NChar));
			NPCSubsystem->RegisterCharacter(NChar);
		}
	}

	return ResultPawn;
}

void ANarrativeGameMode::RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot)
{
	Super::RestartPlayerAtPlayerStart(NewPlayer, StartSpot);

	//UE doesn't set the transform, we want that
	if (NewPlayer)
	{
		if (APawn* Pawn = NewPlayer->GetPawn())
		{
			Pawn->SetActorLocation(StartSpot->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);

			// Override default Set initial control rotation to starting rotation rotation
			NewPlayer->ClientSetRotation(StartSpot->GetActorRotation(), true);

			FRotator NewControllerRot = StartSpot->GetActorRotation();
			NewControllerRot.Roll = 0.f;
			NewPlayer->SetControlRotation(NewControllerRot);
		}
	}
}

void ANarrativeGameMode::ProcessServerTravel(const FString& URL, bool bAbsolute)
{
#if WITH_SERVER_CODE

	UE_LOG(LogGameMode, Log, TEXT("ProcessServerTravel: %s"), *URL);
	UWorld* World = GetWorld();
	check(World);
	FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(World);
	
	// Compute the next URL, and pull the map out of it. This handles short->long package name conversion
	FURL NextURL = FURL(&WorldContext.LastURL, *URL, bAbsolute ? TRAVEL_Absolute : TRAVEL_Relative);
	
	if (NextURL.HasOption((TEXT("LevelTransition"))))
	{
		UE_LOG(LogGameMode, Warning, TEXT("SERVER WAS ASKED FOR LEVEL TRANSITION, SAVE EVERYONES GAME AND HOPE FOR THE BEST!!! HOLD ON..."));
		if (UNarrativeSaveSubsystem* SaveSub = World->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			//Create save files for every player in the game. 
			for (FConstPlayerControllerIterator PCIt = World->GetPlayerControllerIterator(); PCIt; ++PCIt)
			{
				APlayerController* PC = PCIt->Get();

				if (PC)
				{
					SaveSub->CreatePlayerOnlySave(PC);
				}
			}
		}
	}

	//Now our players have save files, we can savely travel - when the new map loads the players will have a file to pull their data from. 
	Super::ProcessServerTravel(URL, bAbsolute);
	
#endif // WITH_SERVER_CODE
	
}
