// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Framework/SovCampaignGameMode.h"

#include "Characters/SovTarrikCharacter.h"
#include "Framework/SovPlayerController.h"
#include "Framework/SovPlayerState.h"
#include "UnrealFramework/NarrativeGameState.h"

ASovCampaignGameMode::ASovCampaignGameMode()
{
	PlayerControllerClass = ASovPlayerController::StaticClass();
	PlayerStateClass = ASovPlayerState::StaticClass();
	GameStateClass = ANarrativeGameState::StaticClass();
	DefaultPawnClass = ASovTarrikCharacter::StaticClass();
}
