// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MapMarker.h"
#include "MapTileBounds.h"
#include "POIMapMarker.generated.h"

/**
 * Special map markers that are created for each POI in the game. They are "discoverable" and support being fast-travelled to when clicked on.
 */
UCLASS()
class NARRATIVEARSENAL_API UPOIMapMarker : public UMapMarker
{
	GENERATED_BODY()
	
public: 

	UPOIMapMarker(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintReadOnly, Category = "POIs")
	FPOIData POI; 

	virtual void OnSelect_Implementation(class UNarrativeNavigationComponent* Selector) override;
	virtual bool CanInteract_Implementation(class UNarrativeNavigationComponent* Selector) const override;
	FText GetMarkerActionText_Implementation(class UNarrativeNavigationComponent* Selector) const override;

	virtual FText GetMarkerDisplayText_Implementation(class UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType, FText& OutSubtitleText) const override;
	virtual FLinearColor GetMarkerColor_Implementation(class UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType) const override;

};
