// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeGameMode.h"
#include "SovCampaignGameMode.generated.h"

/** Project-owned campaign framework while retaining Narrative's save/definition flow. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovCampaignGameMode : public ANarrativeGameMode
{
	GENERATED_BODY()

public:
	ASovCampaignGameMode();
};
