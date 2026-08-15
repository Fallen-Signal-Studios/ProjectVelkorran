// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Navigation/PathFollowingComponent.h"
#include "NarrativePathFollowingComp.generated.h"

/**
 * Custom path following component to allow for any extending of path following we require. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativePathFollowingComp : public UPathFollowingComponent
{
	GENERATED_BODY()
	
public:

	virtual void OnPathFinished(const FPathFollowingResult& Result) override;

	FVector CachedLastDestination;

};
