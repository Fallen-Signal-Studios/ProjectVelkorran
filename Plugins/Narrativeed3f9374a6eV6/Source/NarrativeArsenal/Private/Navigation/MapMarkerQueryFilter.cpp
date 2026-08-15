// Copyright Narrative Tools 2025.


#include "Navigation/MapMarkerQueryFilter.h"

void UMapMarkerQueryFilter::InitializeFilter(const ANavigationData& NavData, const UObject* Querier,
	FNavigationQueryFilter& Filter) const
{
	Super::InitializeFilter(NavData, Querier, Filter);

	Filter.SetMaxSearchNodes(8192);
}
