// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "MapMarker.h"
#include "CustomWaypointMarker.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UCustomWaypointMarker : public UMapMarker
{
	GENERATED_BODY()
	
	UCustomWaypointMarker(const FObjectInitializer& ObjectInitializer);

	virtual FText GetMarkerActionText_Implementation(class UNarrativeNavigationComponent* Selector) const override;
	virtual void OnSelect_Implementation(class UNarrativeNavigationComponent* Selector) override;

};
