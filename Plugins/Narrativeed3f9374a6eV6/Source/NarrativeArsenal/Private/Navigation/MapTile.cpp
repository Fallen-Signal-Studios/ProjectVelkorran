// Copyright Narrative Tools 2025.


#include "Navigation/MapTile.h"
#include "Navigation/NavigatorGameplayTags.h"

UMapTileMarker::UMapTileMarker(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultMarkerSettings.bOverride_bShowActorRotation = true;
	DefaultMarkerSettings.bShowActorRotation = false; 
	WorldMapOverrideSettings.bOverride_IconSize = false; 

	FGameplayTagContainer DefaultMapTileDomains;
	DefaultMapTileDomains.AddTagFast(FNavigatorGameplayTags::Get().NavigatorTypes_Minimap);
	DefaultMapTileDomains.AddTagFast(FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap);
	SetDefaultDomains(DefaultMapTileDomains);

	ZOrder = 0;

}

bool UMapTileMarker::CanInteract_Implementation(class UNarrativeNavigationComponent* Selector) const
{
	return false; 
}
