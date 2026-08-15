// Copyright Narrative Tools 2025.


#include "Character/CharacterMapMarker.h"
#include "Navigation/NavigationDeveloperSettings.h"
#include "ArsenalStatics.h"
#include <UObject/ConstructorHelpers.h>
#include <Engine/Texture2D.h>
#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/NavigatorGameplayTags.h"

UCharacterMapMarker::UCharacterMapMarker(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	auto LocationIconFinder = ConstructorHelpers::FObjectFinder<UTexture2D>(TEXT("/Script/Engine.Texture2D'/NarrativePro/Pro/Core/UI/Textures/Icons/T_Marker_PlayerIcon.T_Marker_PlayerIcon'"));

	if (LocationIconFinder.Succeeded())
	{
		DefaultMarkerSettings.LocationIcon = LocationIconFinder.Object;
	}

	//On compasses, the map marker should't be a triangle, this looks bad. Use the default one instead. 
	auto DefaultIconFinder = ConstructorHelpers::FObjectFinder<UTexture2D>(TEXT("/Script/Engine.Texture2D'/NarrativePro/Pro/Core/UI/Textures/Icons/T_Marker_Default.T_Marker_Default'"));
	if (DefaultIconFinder.Succeeded())
	{
		CompassOverrideSettings.bOverride_LocationIcon = true;
		CompassOverrideSettings.LocationIcon = DefaultIconFinder.Object;
	}

	DefaultMarkerSettings.bShowActorRotation = true;

	FGameplayTagContainer DefaultDomains;

	//DefaultDomains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Compass);
	//DefaultDomains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap);
	DefaultDomains.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Minimap);	

	SetDefaultDomains(DefaultDomains);
}

void UCharacterMapMarker::OnMarkerAdded_Implementation(class UNarrativeNavigationComponent* OwnerNavComp)
{
	Super::OnMarkerAdded_Implementation(OwnerNavComp);
}

void UCharacterMapMarker::OnMarkerRemoved_Implementation(class UNarrativeNavigationComponent* OwnerNavComp)
{
	Super::OnMarkerRemoved_Implementation(OwnerNavComp);
}

FLinearColor UCharacterMapMarker::GetMarkerColor_Implementation(class UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType) const
{
	if (Selector)
	{
		if (const UNavigationDeveloperSettings* Settings = GetDefault<UNavigationDeveloperSettings>())
		{
			//Our our player marker should be colored in our desired style. 
			if (APlayerController* PC = Cast<APlayerController>(Selector->GetOwner()))
			{
				if (PC->GetPawn() == ActorOwner)
				{
					return Settings->PlayerColor;
				}
			}

			ETeamAttitude::Type Attitude = UArsenalStatics::GetAttitude(ActorOwner, Selector->GetOwner());

			switch (Attitude)
			{
			case ETeamAttitude::Friendly:
			{
				return Settings->FriendlyColor;
			}
			break;
			case ETeamAttitude::Neutral:
			{
				return Settings->NeutralColor;
			}
			break;
			case ETeamAttitude::Hostile:
			{
				return Settings->HostileColor;
			}
			break;
			}

			return Settings->NeutralColor;
		}
	}
	return FLinearColor::White;
}
