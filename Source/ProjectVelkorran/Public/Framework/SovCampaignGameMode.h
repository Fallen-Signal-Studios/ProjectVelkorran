// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeGameMode.h"
#include "SovCampaignGameMode.generated.h"

/** Project-owned campaign framework while retaining Narrative's save/definition flow. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovCampaignGameMode : public ANarrativeGameMode
{
	GENERATED_BODY()

public:
	ASovCampaignGameMode();
	/** Each campaign map assigns exactly one mission asset; class and definition are resolved together. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Campaign")
	TObjectPtr<class USovCampaignDefinition> InitialMission;

protected:
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal = TEXT("")) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual UPlayerDefinition* GetPlayerDefinitionForController_Implementation(AController* InController) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
