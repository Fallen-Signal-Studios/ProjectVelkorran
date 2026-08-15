// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NPCActivityConfiguration.generated.h"

/**
 * Defines what activities an NPC can do, and what goal generators they will have.
 */
UCLASS(Blueprintable, BlueprintType)
class NARRATIVEARSENAL_API UNPCActivityConfiguration : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UNPCActivityConfiguration();

	//How often we want to re-score our goals, which may change our current activity. Faster rescore rates will use more performance.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activity Group")
	float RescoreInterval;

	//The activities to grant the NPC
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activity Group")
	TArray<TSubclassOf<class UNPCActivity>> DefaultActivities; 

	//The goal generators the NPC can use to generate goals - you can add your own goals manually via AC->AddGoal(), goals do not have to be added via generators 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Activity Group")
	TArray<TSubclassOf<class UNPCGoalGenerator>> GoalGenerators; 

};
