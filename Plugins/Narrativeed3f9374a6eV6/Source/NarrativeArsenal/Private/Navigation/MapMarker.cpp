// Copyright Narrative Tools 2025. 


#include "Navigation/MapMarker.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/NavigatorGameplayTags.h"
#include <Engine/Texture2D.h>
#include <Engine/World.h>
#include <UObject/ConstructorHelpers.h>
#include <GameFramework/PlayerController.h>
#include <UObject/UObjectThreadContext.h>

#include "ArsenalStatics.h"
#include "NarrativeArsenal.h"
#include "NavigationSystem.h"
#include "Navigation/MapMarkerQueryFilter.h"
#include "Widgets/NarrativeWidgetHelpers.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "MapMarker"

DEFINE_LOG_CATEGORY_STATIC(LogMapMarker, Log, All);

UMapMarker::UMapMarker(const FObjectInitializer& ObjectInitializer)
{
	DefaultMarkerSettings.bOverride_LocationDisplayName = true;
	DefaultMarkerSettings.bOverride_bShowActorRotation = true;
	DefaultMarkerSettings.bOverride_LocationIcon = true;
	DefaultMarkerSettings.bOverride_IconTint = true;
	DefaultMarkerSettings.bOverride_IconSize = true;
	DefaultMarkerSettings.bOverride_IconOffset = true;

	//The default UI that ships with navigator really benefits from a couple of overrides! Compass icons and screen space markers should be a little bigger
	CompassOverrideSettings.bOverride_IconSize = true;
	CompassOverrideSettings.IconSize = FVector2D(40.f, 40.f);
	ScreenspaceOverrideSettings.bOverride_IconSize = true;
	ScreenspaceOverrideSettings.IconSize = FVector2D(50.f, 50.f);
	WorldMapOverrideSettings.bOverride_IconSize = true;
	WorldMapOverrideSettings.IconSize = FVector2D(30.f);

	DefaultMarkerSettings.MarkerTitleText = LOCTEXT("NavigatorLocationDisplayName", "Location Marker");
	DefaultMarkerSettings.LocationIcon = nullptr;
	DefaultMarkerSettings.IconTint = FLinearColor(1.f, 1.f, 1.f);
	DefaultMarkerSettings.IconSize = FVector2D(20.f, 20.f);

	MarkerStartFadeOutDistance = 13500.f;
	MarkerStartFadeInDistance = 15000.f;
	bPinToMapEdge = false;
	bWantsOnPaint = false;

	ZOrder = 2;

	CachedLocation = FVector(TNumericLimits<double>::Max());
	
	auto LocationIconFinder = ConstructorHelpers::FObjectFinder<UTexture2D>(TEXT(
		"/Script/Engine.Texture2D'/NarrativePro/Pro/Core/UI/Textures/Icons/T_Marker_Location.T_Marker_Location'"));
	if (LocationIconFinder.Succeeded())
	{
		DefaultMarkerSettings.LocationIcon = LocationIconFinder.Object;
	}

	//Markers should show up on these navigators by default - screen space shouldnt be default 
	MarkerDomain.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Compass);
	MarkerDomain.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Minimap);
	MarkerDomain.AddTag(FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap);
}

void UMapMarker::RegisterMarker()
{
	//Because navigation is a local thing, we can safely use the local player controller
	for (FConstPlayerControllerIterator Iter = GetWorld()->GetPlayerControllerIterator(); Iter; ++Iter)
	{
		if (APlayerController* PC = Iter->Get())
		{
			if (PC->IsLocalController())
			{
				if (UNarrativeNavigationComponent* NavComp = Cast<UNarrativeNavigationComponent>(PC->GetComponentByClass(UNarrativeNavigationComponent::StaticClass())))
				{
					if (NavComp->AddMarker(this))
					{
						OnMarkerAdded(NavComp);
					}
				}
			}
		}
	}
}

void UMapMarker::RemoveMarker()
{
	//Because navigation is a local thing, we can safely use the local player controller
	for (FConstPlayerControllerIterator Iter = GetWorld()->GetPlayerControllerIterator(); Iter; ++Iter)
	{
		if (APlayerController* PC = Iter->Get())
		{
			if (PC->IsLocalController())
			{
				if (UNarrativeNavigationComponent* NavComp = Cast<UNarrativeNavigationComponent>(PC->GetComponentByClass(UNarrativeNavigationComponent::StaticClass())))
				{
					if (NavComp->RemoveMarker(this))
					{
						OnMarkerRemoved(NavComp);
					}
				}
			}
		}
	}
}

void UMapMarker::SetDrawMarkerPathEnabled(const bool Enabled)
{
	bWantsOnPaint = Enabled;
}

void UMapMarker::OnOwnerDestroyed(AActor* DestroyedActor)
{
	RemoveMarker();
}

void UMapMarker::OnMarkerAdded_Implementation(class UNarrativeNavigationComponent* OwnerNavComp)
{
	// check that we want the path drawn in the first place	
	if (ActorOwner)
	{
		ActorOwner->OnDestroyed.AddUniqueDynamic(this, &UMapMarker::OnOwnerDestroyed);
		InitializeBreadcrumb(OwnerNavComp->GetOwner());
	}
}

void UMapMarker::OnMarkerRemoved_Implementation(class UNarrativeNavigationComponent* OwnerNavComp)
{
	// we call this when the marker is removed each time.
	// this is because it could be possible to set the marker to not draw, and it still is listed
	SetDrawMarkerPathEnabled(false);
	CleanupBreadcrumb();
}

FNavigationMarkerSettings UMapMarker::GetMarkerSettings(const FGameplayTag& NavigatorType) const
{
	FNavigationMarkerSettings Settings = DefaultMarkerSettings;
	FNavigationMarkerSettings Overrides = CompassOverrideSettings;

	if (NavigatorType == FNavigatorGameplayTags::Get().NavigatorTypes_Compass)
	{
		Overrides = CompassOverrideSettings;
	}
	else if(NavigatorType == FNavigatorGameplayTags::Get().NavigatorTypes_Worldmap)
	{
		Overrides = WorldMapOverrideSettings;
	}
	else if (NavigatorType == FNavigatorGameplayTags::Get().NavigatorTypes_Minimap)
	{
		Overrides = MinimapOverrideSettings;
	}
	else if (NavigatorType == FNavigatorGameplayTags::Get().NavigatorTypes_Screenspace)
	{
		Overrides = ScreenspaceOverrideSettings;
	}

	if (Overrides.bOverride_LocationIcon)
	{
		Settings.LocationIcon = Overrides.LocationIcon;
	}

	if (Overrides.bOverride_IconTint)
	{
		Settings.IconTint = Overrides.IconTint;
	}

	if (Overrides.bOverride_IconSize)
	{
		Settings.IconSize = Overrides.IconSize;
	}

	if (Overrides.bOverride_IconOffset)
	{
		Settings.IconOffset = Overrides.IconOffset;
	}

	if (Overrides.bOverride_bShowActorRotation)
	{
		Settings.bShowActorRotation = Overrides.bShowActorRotation;
	}

	return Settings;
}

void UMapMarker::RefreshMarker()
{
	OnRefreshRequired.Broadcast();
}

FText UMapMarker::GetMarkerActionText_Implementation(class UNarrativeNavigationComponent* Selector) const
{
	return DefaultMarkerActionText;
}

FText UMapMarker::GetMarkerDisplayText_Implementation(class UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType, FText& OutSubtitleText) const
{
	auto MarkerSettings = GetMarkerSettings(NavigatorType);
	OutSubtitleText = MarkerSettings.MarkerSubtitleText;

	return MarkerSettings.MarkerTitleText;
}

FLinearColor UMapMarker::GetMarkerColor_Implementation(class UNarrativeNavigationComponent* Selector, const FGameplayTag& NavigatorType) const
{
	return GetMarkerSettings(NavigatorType).IconTint;
}

bool UMapMarker::CanInteract_Implementation(class UNarrativeNavigationComponent* Selector) const
{
	return true; 
}

void UMapMarker::OnSelect_Implementation(class UNarrativeNavigationComponent* Selector)
{

}

void UMapMarker::SetDefaultDomains(const FGameplayTagContainer& DefaultDomains)
{
	MarkerDomain = DefaultDomains;
}

void UMapMarker::SetZOrder(const int32 NewZOrder)
{
	ZOrder = NewZOrder;
}

void UMapMarker::SetDomains(const FGameplayTagContainer& InMarkerDomain)
{
	//If we're in constructor we don't want to remove and update markers 
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	if (ThreadContext.IsInConstructor > 0)
	{
		UE_LOG(LogNarrativeNavigator, Warning, TEXT("UNavigationMarkerComponent::SetDomain called from a constructor, you should call SetDefaultDomains instead. "));
		SetDefaultDomains(InMarkerDomain);
		return;
	}

	MarkerDomain = InMarkerDomain;
}

void UMapMarker::AddDomains(const FGameplayTagContainer& NewMarkerDomains)
{
	MarkerDomain.AppendTags(NewMarkerDomains);

	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	if (ThreadContext.IsInConstructor > 0)
	{
		return;
	}

}

void UMapMarker::RemoveDomains(const FGameplayTagContainer& RemoveDomains)
{
	MarkerDomain.RemoveTags(RemoveDomains);

	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	if (ThreadContext.IsInConstructor > 0)
	{
		return;
	}

}

FTransform UMapMarker::GetMarkerTransform() const
{
	if (ActorOwner)
	{
		return ActorOwner->GetActorTransform();
	}

	return MarkerTransform;
}

int32 UMapMarker::GetMarkerZOrder() const
{
	//Map tiles are 0 so icons need to be 1 so they show up in front 
	return ZOrder;
}

FVector2D UMapMarker::GetMarkerMapLocalPosition(const FVector2D MapOrigin, const FVector2D MapPan) const
{
	return (MapOrigin - FVector2D(GetMarkerTransform().GetLocation())) + MapPan;
}

FVector2D UMapMarker::GetMarkerTopLeftLocalPosition(FMarkerOnPaintData& OnPaintData) const
{
	return UNarrativeWidgetHelpers::TransformLocalSpace(
		OnPaintData.ParentGeometry,
		OnPaintData.MapGeometry,
		GetMarkerMapLocalPosition(OnPaintData.MapOrigin, OnPaintData.MapPan));
}

void UMapMarker::DrawBreadcrumb(FPaintContext& Context, FMarkerOnPaintData& OnPaintData) const
{
	if (!bDrawBreadcrumbs)
	{
		return;
	}

	if (!OwnerNavActor.IsValid())
	{
		return;
	}
	
	TRACE_CPUPROFILER_EVENT_SCOPE(UMapMarker::DrawBreadcrumbs)
	
	float DistSq = 0.f;
	int NearestKey = PathCurve.FindNearest(OwnerNavActor->GetActorLocation(), DistSq);
	

	TArray<FVector2f> PathPoints;

	// Start at player location
	PathPoints.Emplace(UNarrativeWidgetHelpers::WorldLocationToPaint(OnPaintData, OwnerNavActor->GetActorLocation()));

	// Get closest relevant key to player
	// We are essentially asking if we should start at the closest point to the player, or the next one
	int RelevantKey = NearestKey;
	
	FVector NearestPoint = PathCurve.Eval(NearestKey);
	FVector NextPoint = PathCurve.Eval(NearestKey + DistanceBetweenPoints);

	FVector PlayerToNearestDir = (OwnerNavActor->GetActorLocation()-NearestPoint).GetSafeNormal();
	FVector NearestToNextDir = (NearestPoint-NextPoint).GetSafeNormal();

	if (FVector::DotProduct(PlayerToNearestDir, NearestToNextDir) < 0.f)
	{
		RelevantKey = NearestKey + DistanceBetweenPoints;
	}

	// Since the points are added at an interval, we can get which point to start at by doing the reverse
	// Now we just add them to the path
	int StartIndex = FMath::CeilToInt((float)RelevantKey / DistanceBetweenPoints);
	for (int i=StartIndex;i<NavPath.Num();i++)
	{
		PathPoints.Emplace(UNarrativeWidgetHelpers::WorldLocationToPaint(OnPaintData, NavPath[i]));
	}

	// Add the destination marker
	PathPoints.Emplace(UNarrativeWidgetHelpers::WorldLocationToPaint(OnPaintData, GetMarkerTransform().GetLocation()));

	// Now draw the path
	UArsenalStatics::DrawDashedLine(Context, PathPoints, FLinearColor::Gray, 5.f, 10.f);
}

void UMapMarker::MarkerOnPaint_Implementation(FPaintContext& Context, FMarkerOnPaintData& OnPaintData) const
{
	DrawBreadcrumb(Context, OnPaintData);
}

void UMapMarker::SetDrawBreadcrumbs(bool bCanDrawBreadcrumbs)
{
	if (bDrawBreadcrumbs != bCanDrawBreadcrumbs)
	{
		bDrawBreadcrumbs = bCanDrawBreadcrumbs;
	}
}

void UMapMarker::InitializeBreadcrumb(AActor* NavActor)
{
	OwnerNavActor = NavActor;
	GetWorld()->GetTimerManager().SetTimer(RefreshPathTimerHandle, FTimerDelegate::CreateUObject(this, &UMapMarker::UpdateBreadcrumbNavPath), UpdateNavPathRate, true);
	UpdateBreadcrumbNavPath();
}

void UMapMarker::CleanupBreadcrumb()
{
	if (!GetWorld()) { return; }
	
	GetWorld()->GetTimerManager().ClearTimer(RefreshPathTimerHandle);
	
	if (auto NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSys->AbortAsyncFindPathRequest(NavRequestID);
	}
}

void UMapMarker::AsyncPathFinished(uint32 QueryID, ENavigationQueryResult::Type QueryResult,
	TSharedPtr<FNavigationPath> NavigationPath)
{
	switch (QueryResult)
	{
	case ENavigationQueryResult::Success:
		GeneratePath(QueryResult, NavigationPath);
		break;
	default:
		UE_LOG(LogMapMarker, Warning, TEXT("Unable to calculate path for %s Breadcrumbs: %s"), *GetName(), *UEnum::GetValueAsString(QueryResult))
		break;
	}

	// Reset query so that another can be called
	NavRequestID = INVALID_NAVQUERYID;
}

void UMapMarker::GeneratePath(ENavigationQueryResult::Type QueryResult,
	TSharedPtr<FNavigationPath> NavigationPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMapMarker::GeneratePath)
	
	UE_CLOG(NavigationPath->DidSearchReachedLimit(), LogMapMarker, Warning, TEXT("%s nav path reached search limit, path may not be complete. Consider adjusting MaxSearchNodes in UMapMarkerQueryFilter to support longer paths: Is Partial: %hs"), *GetName(), NavigationPath->IsPartial() ? "Yes" : "No");
	
	PathCurve.Reset();
	NavPath.Reset();
		
	// Add points to curve
		
	float AccumulatedDistance = 0.f;
	for (int i=0;i<NavigationPath->GetPathPoints().Num();i++)
	{
		const FVector& Position = NavigationPath->GetPathPointLocation(i).Position;

		if (i != 0)
		{
			const FVector& LastPos = NavigationPath->GetPathPointLocation(i-1).Position;
			AccumulatedDistance += FVector::Dist(Position, LastPos);
		}
			
		int32 PointID = PathCurve.AddPoint(AccumulatedDistance, Position);
		PathCurve.Points[PointID].InterpMode = CIM_CurveAuto;
	}
	PathCurve.AutoSetTangents();

	// Track points on curve at set interval
	int TotalPoints = AccumulatedDistance / DistanceBetweenPoints;

	for (int i=0;i<TotalPoints;i++)
	{
		NavPath.Emplace(PathCurve.Eval(i*DistanceBetweenPoints));
	}
}

void UMapMarker::UpdateBreadcrumbNavPath()
{
	if (!bDrawBreadcrumbs) { return; }
	if (!OwnerNavActor.IsValid()) { return; }
	if (!bWantsOnPaint) { return; } // Only update navpath breadcrumb if we will be drawn
	
	if (NavRequestID != INVALID_NAVQUERYID)
	{
		UE_LOG(LogMapMarker, Verbose, TEXT("Already have async path queried, ignoring duplicate call until finished"));
		return;
	}

	// Only update if we have moved significantly
	if (FVector::Dist(OwnerNavActor->GetActorLocation(), CachedLocation) < UpdateNavDistanceThreshhold)
	{
		return;
	}

	CachedLocation = OwnerNavActor->GetActorLocation();
	
	auto NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance() : nullptr;

	if (!NavData)
	{
		UE_LOG(LogMapMarker, Warning, TEXT("Nav data is not valid, unable to calculate breadcrumb for %s"), *GetName());
		return;
	}

	// Custom query filter so longer paths can be generated
	const FSharedConstNavQueryFilter QueryFilter = UNavigationQueryFilter::GetQueryFilter(*NavData, OwnerNavActor.Get(), UMapMarkerQueryFilter::StaticClass());
	
	FPathFindingQuery Query{
		OwnerNavActor.Get(),
		*NavData,
		OwnerNavActor->GetActorLocation(),
		GetMarkerTransform().GetLocation(),
		QueryFilter,
		0,
		TNumericLimits<FVector::FReal>::Max(),
		false};
	
	NavRequestID = NavSys->FindPathAsync(FNavAgentProperties(), Query, FNavPathQueryDelegate::CreateUObject(this, &UMapMarker::AsyncPathFinished), EPathFindingMode::Hierarchical);
}

#undef LOCTEXT_NAMESPACE 
