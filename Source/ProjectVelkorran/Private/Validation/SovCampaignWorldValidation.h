// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
class USovCampaignDefinition;
class UWorld;
namespace SovCampaignWorldValidation
{
	/** Authored configuration only: never initializes gameplay or runs BeginPlay. */
	PROJECTVELKORRAN_API int32 Validate(UWorld* World, const USovCampaignDefinition* Mission, bool bShippingValidation,
		TMap<FName, FString>* GlobalEncounterOwners = nullptr);
}
