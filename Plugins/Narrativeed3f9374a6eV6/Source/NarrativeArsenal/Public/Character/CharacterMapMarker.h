// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/MapMarker.h"
#include <GenericTeamAgentInterface.h>
#include <GameplayTagContainer.h>
#include "CharacterMapMarker.generated.h"

/**
 * Map marker that shows NarrativeCharacters on the map. Colors differently based on whether marker is our neutral, hostile, etc. 
 */
UCLASS()
class NARRATIVEARSENAL_API UCharacterMapMarker : public UMapMarker
{
	GENERATED_BODY()
	
	public: 

	UCharacterMapMarker(const FObjectInitializer& ObjectInitializer);

	virtual FLinearColor GetMarkerColor_Implementation(class UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType) const;
	virtual void OnMarkerAdded_Implementation(class UNarrativeNavigationComponent* OwnerNavComp) override;
	virtual void OnMarkerRemoved_Implementation(class UNarrativeNavigationComponent* OwnerNavComp) override;

};
