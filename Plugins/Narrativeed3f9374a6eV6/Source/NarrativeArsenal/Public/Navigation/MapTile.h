// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/MapMarker.h"
#include "MapTile.generated.h"

/**
 * Special type of map marker that is actually a maptile for the maps background image, and not an interactable marker. 
 */
UCLASS()
class NARRATIVEARSENAL_API UMapTileMarker : public UMapMarker
{
	GENERATED_BODY()
	
protected:

	UMapTileMarker(const FObjectInitializer& ObjectInitializer);

	virtual bool CanInteract_Implementation(class UNarrativeNavigationComponent* Selector) const override;

};
