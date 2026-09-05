// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Framework/SovCampaignGameMode.h"

#include "Characters/SovTarrikCharacter.h"
#include "Framework/SovPlayerController.h"
#include "Framework/SovPlayerState.h"
#include "UnrealFramework/NarrativeGameState.h"

#include "Campaign/SovCampaignDefinition.h"
#include "Save/SovSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Character/PlayerDefinition.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

ASovCampaignGameMode::ASovCampaignGameMode()
{
	PlayerControllerClass = ASovPlayerController::StaticClass();
	PlayerStateClass = ASovPlayerState::StaticClass();
	GameStateClass = ANarrativeGameState::StaticClass();
	DefaultPawnClass = ASovTarrikCharacter::StaticClass();
}

FString ASovCampaignGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!Error.IsEmpty() || !InitialMission) { return Error; }
	ASovPlayerController* PC = Cast<ASovPlayerController>(NewPlayerController);
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!PC || !Save) { return TEXT("Campaign requires SovPlayerController and NarrativeSaveSubsystem."); }
	if (Save->DidInitialLoadFail()) { return TEXT("The requested campaign save could not be validated."); }
	if (USovSaveSubsystem* Slots = GetGameInstance()->GetSubsystem<USovSaveSubsystem>())
	{ if (!Slots->ValidatePendingWorld(*GetWorld(), Error)) { return Error; } }
	FNarrativeSavePlayer Records;
	const bool bTravel = UGameplayStatics::HasOption(OptionsString, TEXT("SovCampaignTransition"));
	bool bHasRecords = false;
	if (bTravel)
	{
		bHasRecords = Save->ReadPlayerOnlySave(ASovPlayerController::TravelSaveSlot(), Records);
		if (!bHasRecords) { return TEXT("Campaign travel record is missing or invalid."); }
	}
	else if (Save->GetSaveObject() && Save->GetSaveObject()->PlayerData.IsValid())
	{
		Records = Save->GetSaveObject()->PlayerData;
		bHasRecords = true;
	}
	if (!PC->StageCampaignLoad(InitialMission, bHasRecords ? &Records : nullptr, bTravel, Error)) { return Error; }
	return FString();
}

UClass* ASovCampaignGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const ASovPlayerController* PC = Cast<ASovPlayerController>(InController);
	const FGameplayTag Lead = PC ? PC->GetPendingProtagonist() : FGameplayTag();
	return InitialMission ? InitialMission->ResolvePawnClass(Lead.IsValid() ? Lead : InitialMission->Protagonist).LoadSynchronous() : Super::GetDefaultPawnClassForController_Implementation(InController);
}

UPlayerDefinition* ASovCampaignGameMode::GetPlayerDefinitionForController_Implementation(AController* InController)
{
	const ASovPlayerController* PC = Cast<ASovPlayerController>(InController);
	const FGameplayTag Lead = PC ? PC->GetPendingProtagonist() : FGameplayTag();
	return InitialMission ? InitialMission->ResolvePlayerDefinition(Lead.IsValid() ? Lead : InitialMission->Protagonist).LoadSynchronous() : Super::GetPlayerDefinitionForController_Implementation(InController);
}

APawn* ASovCampaignGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	if (!InitialMission) { return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, SpawnTransform); }
	FString Error;
	ASovPlayerController* PC = Cast<ASovPlayerController>(NewPlayer);
	if (!PC || !ASovPlayerController::ValidateMissionPawn(InitialMission, Error, PC->GetPendingProtagonist()))
	{ UE_LOG(LogTemp, Error, TEXT("Campaign spawn rejected: %s"), *Error); return nullptr; }
	ASovPlayerCharacterBase* Pawn = GetWorld()->SpawnActorDeferred<ASovPlayerCharacterBase>(
		InitialMission->ResolvePawnClass(PC->GetPendingProtagonist()).Get(), SpawnTransform, PC, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Pawn) { return nullptr; }
	Pawn->SetFlags(RF_Transient);
	if (!Pawn->PrepareCampaignInitialization(InitialMission->ResolvePlayerDefinition(PC->GetPendingProtagonist()).Get())) { Pawn->Destroy(); return nullptr; }
	Pawn->FinishSpawning(SpawnTransform);
	if (!IsValid(Pawn)) { return nullptr; }
	if (UNarrativeCharacterSubsystem* Characters = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
	{ Characters->RegisterCharacter(Pawn); }
	PC->InitializeCampaignPawn(Pawn);
	return Pawn;
}

AActor* ASovCampaignGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (InitialMission && !InitialMission->EntryPlayerStartTag.IsNone())
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{ if (It->PlayerStartTag == InitialMission->EntryPlayerStartTag) { return *It; } }
		UE_LOG(LogTemp, Error, TEXT("Campaign entry PlayerStart '%s' is missing."), *InitialMission->EntryPlayerStartTag.ToString());
		return nullptr;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}
