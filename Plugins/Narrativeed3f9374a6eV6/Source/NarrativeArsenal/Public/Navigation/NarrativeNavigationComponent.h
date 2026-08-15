// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include <Engine/DataTable.h>
#include <GameplayTagContainer.h>
#include "NarrativeSavableComponent.h"
#include "Navigation/NavigationSubsystem.h"
#include "Navigation/MapTileBounds.h"
#include "NarrativeNavigationComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarkerAdded, class UMapMarker*, Marker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarkerRemoved, class UMapMarker*, Marker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDiscoverPointOfInterest, const FGameplayTag&, POITag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFastTravelRequested, const FPOIData&, POI);

/**
Add this to your PlayerController.

To enable navigation, add this to your controller, and you're away.  */
UCLASS( ClassGroup=(Narrative), DisplayName = "Navigation Component", meta=(BlueprintSpawnableComponent) )
class NARRATIVEARSENAL_API UNarrativeNavigationComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()

protected:

	friend class UNavigationMarkerComponent;
	friend class UMapMarker;

	// Sets default values for this component's properties
	UNarrativeNavigationComponent();

	//Save interface 
	void PrepareForSave_Implementation() override;
	void Load_Implementation() override;

	UPROPERTY(SaveGame)
	TArray<FTransform> SavedCustomMarkerTransforms; 

	//The current map layer we're supposed to display - marked savegame so we can restore when player loads back in. 
	UPROPERTY(SaveGame)
	FGameplayTag CurrentMapLayer;


	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	virtual bool AddMarker(class UMapMarker* MapMarker);

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	virtual bool RemoveMarker(class UMapMarker* MapMarker);

	UFUNCTION(BlueprintCallable, Category = "Navigation")
	void SelectMarker(class UMapMarker* Marker);

	/**Contains all the POI's we've found */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, SaveGame, Category = "Point Of Interest")
	FGameplayTagContainer DiscoveredPOIs;

public:

	//Return a POI that has previously been cached. 
	UFUNCTION(BlueprintCallable, Category = "Point of Interest")
	bool GetPointOfInterest(FPOIData& OutPointOfInterest, UPARAM(meta = (Categories = "Navigator.PointOfInterest"))const FGameplayTag& POITag);

	//Find POI that is nearest to the given location
	UFUNCTION(BlueprintPure, Category = "Point of Interest")
	bool GetNearestPOIToPoint(FPOIData& OutPointOfInterest, const FVector& TestLocation);

	//Set the layer of this map tile for the given domains 
	UFUNCTION(BlueprintCallable, Category = "Map Layers")
	bool SetMapLayer(UPARAM(meta = (Categories = "Narrative.UI.Navigator.MapLayer"))FGameplayTag NewLayer, UPARAM(meta = (Categories = "Navigator.NavigatorTypes")) FGameplayTagContainer Domains);

	//Set a POI as having been "discovered" by the player
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	virtual void DiscoverPOI(const FGameplayTag& POITag);

	//Check if we've found the given POI in the world
	UFUNCTION(BlueprintPure, Category = "Navigation")
	virtual bool HasDiscoveredPOI(const FGameplayTag& POITag) const;

	//Place a custom waypoint in the world at the given transform. bAddToSaveFile will ensure the marker is added to our save file 
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	virtual class UCustomWaypointMarker* PlaceCustomWaypoint(const FTransform& Transform);

	//Place a custom waypoint in the world at the given transform. bAddToSaveFile will ensure the marker is added to our save file 
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	virtual void RemoveCustomWaypoint(class UCustomWaypointMarker* Waypoint);

	//Our custom waypoints that we've placed. These are serialized to disk. s
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Map Configuration")
	TArray<class UCustomWaypointMarker*> CustomWaypoints;

	//How many unreal units wide (cm) is your map image? Enter it here. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Configuration")
	float MapWidth;

	//What position in the world is the center of your map? Enter that position here. (ie where was the camera located that you took your map shot from)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Configuration")
	FVector2D MapOrigin;

	//The max number of custom waypoints we're allowed to place in the world 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map Configuration")
	int32 MaxCustomWaypoints;

	//The bounds we've found in the world, should one exist 
	UPROPERTY(BlueprintReadOnly, Category = "Map Configuration")
	class AMapTileBounds* MapTileBounds;

	//All of the markers currently being tracked by this navigation component
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TArray<class UMapMarker*> Markers;

	//We cache map tiles seperately for quick access
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TSet<class UMapTileMarker*> MapTiles;

	//POI markers 
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TMap<FGameplayTag, class UPOIMapMarker*> POIMarkers;

	//Quick lookup map for pois
	UPROPERTY(BlueprintReadOnly, Category = "Navigation")
	TMap<FGameplayTag, FPOIData> POILookupMap;

	//Fires when a marker is added to our navigation
	UPROPERTY(BlueprintAssignable, Category = "Navigation")
	FOnMarkerAdded OnMarkerAdded;

	//Fires when a marker is removed from our navigation 
	UPROPERTY(BlueprintAssignable, Category = "Navigation")
	FOnMarkerRemoved OnMarkerRemoved;
	
	//Fires when a point of interest has been discovered, in case you want to add a message to the UI or something similar 
	UPROPERTY(BlueprintAssignable, Category = "Navigation")
	FOnDiscoverPointOfInterest OnPOIDiscovered;

	//Fires when a fast travel has been requested - each games fast travel implemtation may be game specific. 
	UPROPERTY(BlueprintAssignable, Category = "Navigation")
	FOnFastTravelRequested OnFastTravelRequested;
};
