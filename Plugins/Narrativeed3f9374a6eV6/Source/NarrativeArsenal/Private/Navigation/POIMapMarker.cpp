// Copyright Narrative Tools 2025.


#include "Navigation/POIMapMarker.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/NavigatorGameplayTags.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"


#define LOCTEXT_NAMESPACE "POIMapMarker"

UPOIMapMarker::UPOIMapMarker(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	auto POIIconFinder = ConstructorHelpers::FObjectFinder<UTexture2D>(TEXT("/Script/Engine.Texture2D'/NarrativePro/Pro/Core/UI/Textures/Icons/T_Marker_Location.T_Marker_Location'"));
	if (POIIconFinder.Succeeded())
	{
		DefaultMarkerSettings.LocationIcon = POIIconFinder.Object;
	}

	//POIs shouldnt show up on minimaps, just world maps and compasses 
	FGameplayTagContainer DefaultPOIDomains;
	DefaultPOIDomains.AddTagFast(FNavigatorGameplayTags::Get().NavigatorTypes_Compass);
	DefaultPOIDomains.AddTagFast(FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap);
	SetDefaultDomains(DefaultPOIDomains);

}

void UPOIMapMarker::OnSelect_Implementation(class UNarrativeNavigationComponent* Selector)
{
	if (POI.bSupportsFastTravel && Selector)
	{
		Selector->OnFastTravelRequested.Broadcast(POI);
	}
}

FLinearColor UPOIMapMarker::GetMarkerColor_Implementation(class UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType) const
{
	FLinearColor Color = Super::GetMarkerColor_Implementation(Selector, NavigatorType);

	//Check if POI is discoverable and tint its alpha accordingly 
	if (Selector && POI.bIsDiscoverable)
	{
		if (Selector->HasDiscoveredPOI(POI.POITag))
		{
			Color.A = 1.f;
		}
		else
		{
			Color.A = .5f;
		}
	}

	return Color;
}

FText UPOIMapMarker::GetMarkerDisplayText_Implementation(class UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType, FText& OutSubtitleText) const
{
	FText DisplayText = Super::GetMarkerDisplayText_Implementation(Selector, NavigatorType, OutSubtitleText);

	if (Selector && POI.bSupportsFastTravel)
	{
		if (Selector->HasDiscoveredPOI(POI.POITag))
		{
			//TODO developer settings these strings 
			//OutSubtitleText = FText::FromString("Discovered Point Of Interest");
			return FText::Format(LOCTEXT("POIDiscoveredFormat", "{0} {1}"), DisplayText, LOCTEXT("DiscoveredText", "(Discovered)"));
		}
	}

	return DisplayText;
}

FText UPOIMapMarker::GetMarkerActionText_Implementation(class UNarrativeNavigationComponent* Selector) const
{
	if (POI.bSupportsFastTravel && CanInteract(Selector))
	{
		return LOCTEXT("FastTravelText", "Fast Travel");
	}

	return FText::GetEmpty();
}

bool UPOIMapMarker::CanInteract_Implementation(class UNarrativeNavigationComponent* Selector) const
{
	#if !UE_BUILD_SHIPPING
		return true; 
	#else 


	if (POI.bSupportsFastTravel && Selector)
	{
		if (Selector->HasDiscoveredPOI(POI.POITag))
		{
			return true;
		}
	}

	return false; 

	#endif
}

#undef LOCTEXT_NAMESPACE 