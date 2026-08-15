// Copyright Narrative Tools 2025.


#include "Vehicles/TrafficLights/TrafficLight.h"

#include "Vehicles/Mass/TrafficLightSubsystem.h"
#include "Engine/World.h"

// Sets default values
ATrafficLight::ATrafficLight()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

void ATrafficLight::BeginPlay()
{
	Super::BeginPlay();

	CacheIntersectionSide();

	if (UTrafficLightSubsystem* TrafficLightSubsystem = UWorld::GetSubsystem<UTrafficLightSubsystem>(GetWorld()))
	{
		TrafficLightSubsystem->RegisterTrafficLight(this);
	}
}

void ATrafficLight::CacheIntersectionSide()
{
	FVector WorldSideLocation = GetActorTransform().TransformPosition(SideLocation);
	UTrafficLightSubsystem* TrafficLightSubsystem = UWorld::GetSubsystem<UTrafficLightSubsystem>(GetWorld());

	TArray<FTrafficIntersectionSideHandle> Sides;
	FBox Extent = FBox::BuildAABB(WorldSideLocation, QueryExtent);
	TrafficLightSubsystem->IntersectionSidesGrid.Query(Extent, Sides);

#if ENABLE_VISUAL_LOG
	//UE_VLOG_BOX(this, LogTrafficLight, Verbose, Extent, FColor::Yellow, TEXT("Traffic Intersection Query"));
#endif 

	if (Sides.IsEmpty())
	{
		UE_LOG(LogTrafficLight, Error, TEXT("Unable to cache traffic intersection side!"));
		return;
	}
	
	Sides.Sort([&WorldSideLocation, &TrafficLightSubsystem](const FTrafficIntersectionSideHandle& Container1, const FTrafficIntersectionSideHandle& Container2)
	{
		const FTrafficIntersectionSide& Side1 = Container1.GetIntersectionSide(TrafficLightSubsystem);
		const FTrafficIntersectionSide& Side2 = Container2.GetIntersectionSide(TrafficLightSubsystem);
		
		return FVector::DistSquared2D(Side1.SideLocation, WorldSideLocation) < FVector::DistSquared2D(Side2.SideLocation, WorldSideLocation);
	});
	
#if ENABLE_VISUAL_LOG
	for (int i=0;i<Sides.Num();i++)
	{
		const FTrafficIntersectionSideHandle& TrafficIntersectionSideHandle = Sides[i];
		FColor Color = i == 0 ? FColor::Red : FColor::Yellow;
		//UE_VLOG_LOCATION(this, LogTrafficLight, Verbose, TrafficIntersectionSideHandle.GetIntersectionSide(TrafficLightSubsystem).SideLocation, 20.f, Color, TEXT("Potential Intersection Side"));
	}
#endif

	CachedIntersectionSide = Sides[0];

#if ENABLE_VISUAL_LOG
	//UE_VLOG_ARROW(this, LogTrafficLight, Verbose, GetActorLocation(), Sides[0].GetIntersectionSide(TrafficLightSubsystem).SideLocation, FColor::Yellow, TEXT("Traffic Light -> Intersection Side"));
#endif 
}

void ATrafficLight::OnPeriodUpdated_Implementation()
{
}

float ATrafficLight::GetPeriod(uint8 Rule) const
{
	if (!CachedIntersectionSide.IsValid())
	{
		UE_LOG(LogTrafficLight, Error, TEXT("Intersection Side is not cached/valid! Please run CacheIntersectionSide before using GetPeriod()"));
		return 0.f;
	}

	UTrafficLightSubsystem* TrafficLightSubsystem = UWorld::GetSubsystem<UTrafficLightSubsystem>(GetWorld());

	FTrafficLightIntersection& Intersection = CachedIntersectionSide.GetIntersection(TrafficLightSubsystem);
	const FTrafficPeriod* CurrentPeriod = Intersection.GetCurrentPeriod();

	// If our intersection is overriden, we need to look at the overriden value
	if (Intersection.bOverrideIntersection)
	{
		const FTrafficIntersectionSide& Side = CachedIntersectionSide.GetIntersectionSide(TrafficLightSubsystem);
		if (EnumHasAnyFlags((EIntersectionSideRule)Side.SideOverride, (EIntersectionSideRule)Rule))
		{
			return FLT_MAX;
		}
		else
		{
			return 0.f;
		}
	}

	// While this shouldnt really happen, this may cause error spam at start of play
	if (!CurrentPeriod)
	{
		return 0.f;
	}

	// We can assume that if the current period contains one of the sides in the cached intersection side, the current period is working with this traffic light
	bool bValidPeriod = false;

	const TArray<FZoneGraphLaneHandle>& Lanes = CurrentPeriod->Lanes;
	const FTrafficIntersectionSide& CachedSide = CachedIntersectionSide.GetIntersectionSide(TrafficLightSubsystem);
	for (int i=0;i<Lanes.Num();i++)
	{
		const FZoneGraphLaneHandle& Lane = Lanes[i];
		bool bPassesLaneFiler = EnumHasAnyFlags(CurrentPeriod->LanesCoveredMask, (EIntersectionSideRule)Rule);
		if (CachedSide.Lanes.Contains(Lane) && bPassesLaneFiler)
		{
			bValidPeriod = true;
			break;
		}
	}

	if (bValidPeriod)
	{
		return Intersection.RemainingPeriodDuration;
	}

	return 0.f;
}

