// Copyright Narrative Tools 2024. 


#include "Navigation/NavigationSubsystem.h"
#include <EngineUtils.h>
#include "Navigation/MapTileBounds.h"

UNavigationSubsystem::UNavigationSubsystem()
{

}

void UNavigationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{

	Super::OnWorldBeginPlay(InWorld);

	//Try pull our navdata from the maptilebounds should one exist 
	for (TActorIterator<AMapTileBounds> It(GetWorld()); It; ++It)
	{
		MapTileBounds = *It;

		if (MapTileBounds)
		{
			for (auto& POI : MapTileBounds->POIs)
			{
				POILookupMap.Add(POI.POITag, POI);
			}

			break;
		}
	}
}

bool UNavigationSubsystem::GetPointOfInterest(FPOIData& OutPointOfInterest, const FGameplayTag& POITag)
{
	if (POILookupMap.Contains(POITag))
	{
		OutPointOfInterest = POILookupMap[POITag];
		return true;
	}

	return false;
}

