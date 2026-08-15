// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/Actor.h"
#include "StableActorStatics.generated.h"

/**
 * Functions that simply working with FStableActors. 
 */
UCLASS()
class NARRATIVESAVESYSTEM_API UStableActorStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	//Create a stable actor from an actor reference 
	UFUNCTION(BlueprintPure, Category = "Stable Actor")
	static FStableActor MakeStableActor(AActor* Actor);

	//Create a stable actor from an actor reference 
	UFUNCTION(BlueprintPure, Category = "Stable Actor", meta = (WorldContext = "WorldContextObject"))
	static FStableActor MakeStableActorFromGUID(const UObject* WorldContextObject, const FGuid& StableActorGuid);

	/*Finds the stable actor in the world. Very efficient as looks actor up via GUID. Quite unwieldly function in CPP but really intended to be used from Quest blueprints.
	
	@bOutSucceeded Whether or not the stable actor was found in the level. 
	@Type Makes casting the stable actor easier than having to use a cast node. */
	UFUNCTION(BlueprintPure, Category = "Stable Actor", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "Type"))
	static class AActor* GetStableActor(const UObject* WorldContextObject, const FStableActor& StableActor, bool& bOutSucceeded, TSubclassOf<class AActor> Type = nullptr);
};
