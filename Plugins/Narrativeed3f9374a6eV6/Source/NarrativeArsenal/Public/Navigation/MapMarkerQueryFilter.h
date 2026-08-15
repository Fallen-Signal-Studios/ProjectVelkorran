// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "MapMarkerQueryFilter.generated.h"

/**
 * Custom query filter which supports longer range navigation
 */
UCLASS()
class NARRATIVEARSENAL_API UMapMarkerQueryFilter : public UNavigationQueryFilter
{
	GENERATED_BODY()

	virtual void InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const override;
};
