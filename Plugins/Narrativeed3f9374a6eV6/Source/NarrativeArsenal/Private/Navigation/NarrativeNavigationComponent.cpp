// Copyright Narrative Tools 2022. 


#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/NavigationMarkerComponent.h"
#include "Navigation/MapTileBounds.h"
#include "Navigation/MapTile.h"
#include <EngineUtils.h>
#include <Kismet/GameplayStatics.h>
#include <Net/UnrealNetwork.h>
#include "Navigation/POIMapMarker.h"
#include "Navigation/CustomWaypointMarker.h"
#include "NarrativeGameplayTags.h"

// Sets default values for this component's properties
UNarrativeNavigationComponent::UNarrativeNavigationComponent()
{
	SetIsReplicatedByDefault(true);
	MapWidth = 5000.f;
	MapOrigin = FVector2D::ZeroVector;
	MaxCustomWaypoints = 5;
	CurrentMapLayer = FNarrativeGameplayTags::Get().UI_Navigator_MapLayer_Default;
}


void UNarrativeNavigationComponent::BeginPlay()
{
	Super::BeginPlay();

	//Try pull our navdata from the maptilebounds should one exist 
	for (TActorIterator<AMapTileBounds> It(GetWorld()); It; ++It)
	{
		MapTileBounds = *It;

		if (MapTileBounds)
		{
			MapWidth = MapTileBounds->MapWidth;
			MapOrigin = FVector2D(MapTileBounds->GetActorLocation());

			break;
		}
	}

	//Create map tiles from the tile bounds set
	if (MapTileBounds)
	{
		for (auto& MapTileData : MapTileBounds->TileSet.Tiles)
		{
			if (UMapTileMarker* MapTileMarker = NewObject<UMapTileMarker>(this))
			{
				UTexture2D* TileImage = MapTileData.TileImage;

				if (MapTileData.TileImages.Contains(FNarrativeGameplayTags::Get().UI_Navigator_MapLayer_Default))
				{
					TileImage = MapTileData.TileImages[FNarrativeGameplayTags::Get().UI_Navigator_MapLayer_Default];
				}

				MapTileMarker->DefaultMarkerSettings.LocationIcon = TileImage;
				MapTileMarker->DefaultMarkerSettings.IconSize = FVector2D(MapTileBounds->TileSet.TileImageSize);
				MapTileMarker->MarkerTransform = FTransform(MapTileData.TileLocation);
				MapTiles.Add(MapTileMarker);

				AddMarker(MapTileMarker);
			}
		}

		for (auto& POI : MapTileBounds->POIs)
		{
			POILookupMap.Add(POI.POITag, POI);

			if (POI.bNeedsMapMarker)
			{
				if (UPOIMapMarker* POIMarker = NewObject<UPOIMapMarker>(this))
				{
					POIMarker->POI = POI;
					POIMarker->MarkerTransform = FTransform(POI.POILocation);
					POIMarker->DefaultMarkerSettings.MarkerTitleText = POI.POIDisplayName;
					POIMarker->DefaultMarkerSettings.LocationIcon = POI.MapMarkerIcon; 

					POIMarkers.Add(POI.POITag, POIMarker);

					AddMarker(POIMarker);
				}
			}

		}
	}

}

void UNarrativeNavigationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UNarrativeNavigationComponent, DiscoveredPOIs, COND_OwnerOnly);
}

bool UNarrativeNavigationComponent::AddMarker(class UMapMarker* MapMarker)
{
	if (MapMarker)
	{
		Markers.Add(MapMarker);
		OnMarkerAdded.Broadcast(MapMarker);
		return true;
	}

	return false;
}

bool UNarrativeNavigationComponent::RemoveMarker(class UMapMarker* MapMarker)
{
	if (MapMarker)
	{
		Markers.Remove(MapMarker);
		OnMarkerRemoved.Broadcast(MapMarker);
		return true;
	}
	return false;
}

void UNarrativeNavigationComponent::SelectMarker(class UMapMarker* Marker)
{
	if (APlayerController* OwnerPC = Cast<APlayerController>(GetOwner()))
	{
		if (Marker && Marker->CanInteract(this))
		{
			Marker->OnSelect(this);
			Marker->OnSelected.Broadcast(this);
		}
	}
}

bool UNarrativeNavigationComponent::GetPointOfInterest(FPOIData& OutPointOfInterest, const FGameplayTag& POITag)
{
	if (POILookupMap.Contains(POITag))
	{
		OutPointOfInterest = POILookupMap[POITag];
		return true;
	}

	return false;
}

bool UNarrativeNavigationComponent::GetNearestPOIToPoint(FPOIData& OutPointOfInterest, const FVector& TestLocation)
{

	if (POILookupMap.Num() <= 0)
	{
		return false; 
	}
	
	FPOIData BestPOI;
	float BestDistance = -1.f;

	for (auto& POIKVP : POILookupMap)
	{
		FPOIData& POI = POIKVP.Value;

		const float Dist = FVector::Dist2D(POI.POILocation, TestLocation);

		if (Dist < BestDistance || BestDistance == -1.f)
		{
			BestPOI = POI;
			BestDistance = Dist;
		}
	}

	OutPointOfInterest = BestPOI;
	return BestDistance != -1.f;
}

bool UNarrativeNavigationComponent::SetMapLayer(FGameplayTag NewLayer, FGameplayTagContainer Domains)
{
	if (MapTileBounds)
	{
		CurrentMapLayer = NewLayer;

		//Iterate the map tiles and swap the image to the specified layer. TODO domain specific. 
		int32 i = 0;
		for (auto& MapTile : MapTiles)
		{
			if (MapTile && MapTileBounds->TileSet.Tiles.IsValidIndex(i))
			{
				FMapTile& Tile = MapTileBounds->TileSet.Tiles[i];

				if (Tile.TileImages.Contains(NewLayer))
				{
					MapTile->DefaultMarkerSettings.LocationIcon = Tile.TileImages[NewLayer];
				}
				else
				{
					MapTile->DefaultMarkerSettings.LocationIcon = Tile.TileImage;
				}

				MapTile->RefreshMarker();
			}
			++i;
		}
	}

	return true;
}

void UNarrativeNavigationComponent::DiscoverPOI(const FGameplayTag& POITag)
{
	if (!DiscoveredPOIs.HasTag(POITag))
	{
		DiscoveredPOIs.AddTagFast(POITag);
		OnPOIDiscovered.Broadcast(POITag);

		//Find the marker in the world that represents this POI and refresh it 
		if (POIMarkers.Contains(POITag))
		{
			POIMarkers[POITag]->RefreshMarker();
		}
	}
}

bool UNarrativeNavigationComponent::HasDiscoveredPOI(const FGameplayTag& POITag) const
{
	return DiscoveredPOIs.HasTag(POITag);
}

UCustomWaypointMarker* UNarrativeNavigationComponent::PlaceCustomWaypoint(const FTransform& Transform)
{
	if (CustomWaypoints.Num() + 1 <= MaxCustomWaypoints)
	{
		if (UCustomWaypointMarker* Waypoint = NewObject<UCustomWaypointMarker>(this))
		{
			Waypoint->MarkerTransform = Transform;
			AddMarker(Waypoint);

			CustomWaypoints.Add(Waypoint);
			return Waypoint;
		}
	}

	return nullptr;
}

void UNarrativeNavigationComponent::RemoveCustomWaypoint(class UCustomWaypointMarker* Waypoint)
{
	if (Waypoint)
	{
		RemoveMarker(Waypoint);
		CustomWaypoints.Remove(Waypoint);
	}
}

void UNarrativeNavigationComponent::PrepareForSave_Implementation()
{
	for (auto& Waypoint : CustomWaypoints)
	{
		if (Waypoint)
		{
			SavedCustomMarkerTransforms.Add(Waypoint->MarkerTransform);
		}
	}
}

void UNarrativeNavigationComponent::Load_Implementation()
{
	//Destroy our existing waypoints, and load the ones in the save file  
	for (auto& Waypoint : CustomWaypoints)
	{
		if (Waypoint)
		{
			RemoveMarker(Waypoint);
		}
	}
	CustomWaypoints.Empty();

	for (auto& MarkerTransform : SavedCustomMarkerTransforms)
	{
		PlaceCustomWaypoint(MarkerTransform);
	}

	/** Our player data loads after the marker data has loaded, so the markers won't have the correct color yet. Go through the markers we've found, and ask them for an update. 
	TODO in the long term, having a system for LoadingPhases may be a cleaner solution to these sorts of hacks, but so far we've been able to get by fine without a phased system - could look into this in future however*/
	if (UNavigationSubsystem* NavSubsystem = GetWorld()->GetSubsystem<UNavigationSubsystem>())
	{
		for (auto& POI : DiscoveredPOIs)
		{
			//Find the marker in the world that represents this POI and refresh it 
			FPOIData POIData;

			if (GetPointOfInterest(POIData, POI))
			{
				if (POIMarkers.Contains(POI))
				{
					if (UMapMarker* Marker = POIMarkers[POI])
					{
						Marker->RefreshMarker();
					}
				}
			}
		}
	}

	SetMapLayer(CurrentMapLayer, FGameplayTagContainer::EmptyContainer);

	//DiscoveredPOIs = NavigatorSave->DiscoveredPOIs;
	// 
	//for (TActorIterator<ANavigatorPointOfInterest> It(GetWorld()); It; ++It)
	//{
	//	if (It)
	//	{
	//		if (ANavigatorPointOfInterest* POI = *It)
	//		{
	//			POI->SetDiscovered(HasDiscoveredPOI(POI));
	//		}
	//	}
	//}

}

