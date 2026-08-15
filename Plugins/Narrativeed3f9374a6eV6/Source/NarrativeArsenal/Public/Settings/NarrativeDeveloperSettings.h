// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "AI/NPCDefinition.h"
#include "NarrativeDeveloperSettings.generated.h"

/**
 * Local developer settings per user. Contains useful tweaks devs may want locally, but don't want to propogate to other users 
 */
UCLASS(config=EditorPerProjectUserSettings)
class NARRATIVEARSENAL_API UNarrativeDeveloperSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()
	
public:

	UNarrativeDeveloperSettings();

	// We'll only allow NPCs in this list to spawn. This can be useful for checking 
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category="Debug")
	TArray<TObjectPtr<class UNPCDefinition>> NPCAllowList; 

	// We'll use this as the player definition instead of the game mode specified definition 
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category="Debug")
	TArray<TObjectPtr<class UPlayerDefinition>> PlayerDefinitionOverrides; 
};
