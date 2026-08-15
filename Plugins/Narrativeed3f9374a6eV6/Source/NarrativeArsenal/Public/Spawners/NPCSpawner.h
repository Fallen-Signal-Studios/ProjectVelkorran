// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "NPCSpawnComponent.h"
#include "NPCSpawnVisualizerComponent.h"
#include "Spawners/SpawnerBase.h"
#include "SaveSystemStatics.h"
#include "NPCSpawner.generated.h"

/**
 * Same as SpawnerBase, just has convinience functionality related to spawning NPCs. 
 */
UCLASS(Blueprintable, Placeable)
class NARRATIVEARSENAL_API ANPCSpawner : public ASpawnerBase
{
	GENERATED_BODY()
	
public:

	ANPCSpawner();

	//Get all the NPCs spawned here - TODO we can make this more efficient by caching NPCs. 
	UFUNCTION(BlueprintPure, Category = "NPC Spawner")
	void GetSpawnedNPCs(TArray<ANarrativeNPCCharacter*>& OutNPCs);


	//TODO make a function OnNPCsReady which is called once NPCs are not only spawned but fully loaded in and ready to do things. 

#if WITH_EDITOR
	// Creates a new npc spawner component for the actor
	UNPCSpawnComponent* CreateNPCSpawner();
	
	/** Returns Valid if this object has data validation rules set up for it and the data for this object is valid. Returns Invalid if it does not pass the rules. Returns NotValidated if no rules are set for this object. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UNPCSpawnVisualizerComponent* NPCSpawnVisualizer;

	virtual void OnConstruction(const FTransform& Transform) override;
#endif
};
