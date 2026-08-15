// Copyright Narrative Tools 2022. 

#include "Navigation/NavigationMarkerComponent.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/NavigatorGameplayTags.h"
#include <UObject/ConstructorHelpers.h>
#include <Engine/Texture2D.h>
#include <Engine/World.h>
#include <GameFramework/PlayerController.h>
#include <UObject/UObjectThreadContext.h>
#include "NarrativeArsenal.h"

#define LOCTEXT_NAMESPACE "NavigationMarker"


UNavigationMarkerComponent::UNavigationMarkerComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetAutoActivate(true);
}


void UNavigationMarkerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (IsActive())
	{
		RegisterMarker();
	}
}

void UNavigationMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	RemoveMarker();
}

void UNavigationMarkerComponent::Activate(bool bReset)
{
	Super::Activate(bReset);

	RegisterMarker();
}

void UNavigationMarkerComponent::Deactivate()
{
	Super::Deactivate();

	RemoveMarker();
}

void UNavigationMarkerComponent::DestroyComponent(bool bPromoteChildren)
{
	Super::DestroyComponent(bPromoteChildren);

	RemoveMarker();
}

void UNavigationMarkerComponent::RegisterMarker()
{
	//Because navigation is a local thing, we can safely use the local player controller
	if (MarkerObject)
	{
		MarkerObject->ActorOwner = GetOwner();
		MarkerObject->RegisterMarker();
	}
}

void UNavigationMarkerComponent::RemoveMarker()
{
	if (MarkerObject)
	{
		MarkerObject->RemoveMarker();
	}
}


#undef LOCTEXT_NAMESPACE 