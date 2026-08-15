// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Character/PlayerDefinition.h"
#include "NarrativeGameMode.generated.h"

/**
 * Default Game Mode class for Narrative pro. 
 */
UCLASS()
class NARRATIVEARSENAL_API ANarrativeGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
protected:

	ANarrativeGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual void RestartPlayerAtPlayerStart(AController* NewPlayer, AActor* StartSpot) override;
	
	virtual void ProcessServerTravel(const FString& URL, bool bAbsolute = false) override; 
	
	/** Returns default player definition class for a controller joining the game. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category=Classes)
	UPlayerDefinition* GetPlayerDefinitionForController(AController* InController);

	/** By default, Narrative assigns each player a definition using this list for each joining player. If you need different functionality, simply override GetPlayerDefinitionForController */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Classes)
	TArray<TObjectPtr<UPlayerDefinition>> PlayerDefinitions;

};
