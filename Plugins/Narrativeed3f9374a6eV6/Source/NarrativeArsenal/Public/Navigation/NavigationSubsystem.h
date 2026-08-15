// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include <GameplayTagContainer.h>
#include "MapTileBounds.h"
#include "NavigationSubsystem.generated.h"

/**
 * Subsystem for navigator, at the moment just used for caching POIs. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNavigationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public: 

	UNavigationSubsystem();

	/** Called when world is ready to start gameplay before the game mode transitions to the correct state and call BeginPlay on all actors */
	virtual void OnWorldBeginPlay(UWorld& InWorld);

	//Return a POI that has previously been cached. 
	UFUNCTION(BlueprintCallable, Category = "Point of Interest")
	bool GetPointOfInterest(FPOIData& OutPointOfInterest, UPARAM(meta = (Categories = "Navigator.PointOfInterest"))const FGameplayTag& POITag);

	//Quick lookup map for pois
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TMap<FGameplayTag, FPOIData> POILookupMap;

	//The bounds we've found in the world, should one exist 
	UPROPERTY(BlueprintReadOnly, Category = "Map Configuration")
	class AMapTileBounds* MapTileBounds;

};
