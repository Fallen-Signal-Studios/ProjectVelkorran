// Copyright Narrative Tools 2022. 


#include "Navigation/CustomWaypointMarker.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/NavigatorGameplayTags.h"

UCustomWaypointMarker::UCustomWaypointMarker(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultMarkerSettings.MarkerTitleText = NSLOCTEXT("CustomWaypointMarker", "MarkerDisplayName", "Custom Waypoint");
	DefaultMarkerSettings.IconTint = FLinearColor(0.178507f, 0.538802f, 0.859375f,1.000000f);

	bPinToMapEdge = true;

	MarkerDomain.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Screenspace);
}

void UCustomWaypointMarker::OnSelect_Implementation(class UNarrativeNavigationComponent* Selector)
{
	Super::OnSelect_Implementation(Selector);

	if (Selector)
	{
		Selector->RemoveCustomWaypoint(this);
;	}
}

FText UCustomWaypointMarker::GetMarkerActionText_Implementation(class UNarrativeNavigationComponent* Selector) const
{
	return NSLOCTEXT("CustomWaypointMarker", "MarkerActionText", "Remove");
}
