// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/TrafficLightIntersectionData.h"

#include "ZoneGraphQuery.h"
#include "Engine/World.h"
#include "Vehicles/Mass/TrafficLightSubsystem.h"

static const float MinMostlySquareAdjacentSideAngleDeg = 75.0f;
static const float MaxMostlySquareAdjacentSideCos = FMath::Cos(FMath::DegreesToRadians(MinMostlySquareAdjacentSideAngleDeg));

TConstArrayView<FTrafficIntersectionSide, int> FTrafficIntersectionSideHandle::GetIntersectionSides(
	UTrafficLightSubsystem* TrafficLightSubsystem) const
{
	check(IsValid())
	
	auto& Intersection = GetIntersection(TrafficLightSubsystem);
	return Intersection.IntersectionSides;
}

TArrayView<FTrafficIntersectionSide> FTrafficIntersectionSideHandle::GetMutableIntersectionSides(
	UTrafficLightSubsystem* TrafficLightSubsystem)
{
	check(IsValid())
	
	auto& Intersection = GetIntersection(TrafficLightSubsystem);
	return Intersection.IntersectionSides;
}

const FTrafficIntersectionSide& FTrafficIntersectionSideHandle::GetIntersectionSide(
	UTrafficLightSubsystem* TrafficLightSubsystem) const
{
	check(IsValid())
	auto Sides = GetIntersectionSides(TrafficLightSubsystem);

	return Sides[SideIndex];
}

FTrafficIntersectionSide& FTrafficIntersectionSideHandle::GetMutableIntersectionSide(
	UTrafficLightSubsystem* TrafficLightSubsystem)
{
	check(IsValid())
	auto Sides = GetMutableIntersectionSides(TrafficLightSubsystem);

	return Sides[SideIndex];
}

FTrafficLightIntersection& FTrafficIntersectionSideHandle::GetIntersection(
	UTrafficLightSubsystem* TrafficLightSubsystem) const
{
	check(IsValid())
	
	return TrafficLightSubsystem->RegisteredLaneData[ZoneGraphDataHandle.Index].Intersections[IntersectionIndex];
}

bool FTrafficIntersectionSideHandle::IsValid() const
{
	return ZoneGraphDataHandle.IsValid() && IntersectionIndex != INDEX_NONE;
}

FTrafficPeriod& FTrafficLightIntersection::IncrementCurrentPeriod()
{
	check(!TrafficPeriods.IsEmpty())
	
	CurrentPeriodIndex = (CurrentPeriodIndex + 1) % TrafficPeriods.Num();

	auto& CurrentPeriod = TrafficPeriods[CurrentPeriodIndex];
	RemainingPeriodDuration = CurrentPeriod.Duration;

	return CurrentPeriod;
}

bool FTrafficLightIntersection::IsSquareShaped() const
{
	// We are checking dot products to see if sides are mostly perpendicular. If so, we can make the assumption that this intersection is a square
	return
	IntersectionSides.Num() == 4 &&
	FMath::Abs(FVector::DotProduct(IntersectionSides[0].DirectionIntoIntersection, IntersectionSides[1].DirectionIntoIntersection)) <= MaxMostlySquareAdjacentSideCos &&
	FMath::Abs(FVector::DotProduct(IntersectionSides[1].DirectionIntoIntersection, IntersectionSides[2].DirectionIntoIntersection)) <= MaxMostlySquareAdjacentSideCos &&
	FMath::Abs(FVector::DotProduct(IntersectionSides[2].DirectionIntoIntersection, IntersectionSides[3].DirectionIntoIntersection)) <= MaxMostlySquareAdjacentSideCos &&
	FMath::Abs(FVector::DotProduct(IntersectionSides[3].DirectionIntoIntersection, IntersectionSides[0].DirectionIntoIntersection)) <= MaxMostlySquareAdjacentSideCos;
}

int FTrafficLightIntersection::GetSidesConnectingLanes(int StartSideIndex, int EndSideIndex, const FZoneGraphStorage& ZoneGraphStorage, TArray<FZoneGraphLaneHandle>& OutTrafficLanes) const
{
	if (StartSideIndex >= IntersectionSides.Num() || EndSideIndex >= IntersectionSides.Num())
	{
		return 0;
	}

	const auto& BeginSide = IntersectionSides[StartSideIndex];
	const auto& EndSide = IntersectionSides[EndSideIndex];

	const auto& StartInboundTrafficLanes = BeginSide.Lanes;
	for (auto& StartInboundTrafficLaneData : StartInboundTrafficLanes)
	{
		FZoneGraphLaneLocation EndLaneLocation;
		float LaneLength;
		
		UE::ZoneGraph::Query::GetLaneLength(ZoneGraphStorage, StartInboundTrafficLaneData, LaneLength);
		UE::ZoneGraph::Query::CalculateLocationAlongLane(ZoneGraphStorage, StartInboundTrafficLaneData, LaneLength, EndLaneLocation);
		
		if (FVector::DotProduct(EndSide.DirectionIntoIntersection, EndLaneLocation.Direction) < -0.8)
		{
			OutTrafficLanes.Add(StartInboundTrafficLaneData);
		}
	}
		
	return OutTrafficLanes.Num();
}

void FTrafficLightIntersection::SetOverrideIntersection(bool bShouldOverrideIntersection, UWorld* World)
{
	// Only continue if value is different
	if (bOverrideIntersection == bShouldOverrideIntersection) { return; }
	check(World);
	
	bOverrideIntersection = bShouldOverrideIntersection;
	auto ZoneGraphAnnotationSubsystem = UWorld::GetSubsystem<UZoneGraphAnnotationSubsystem>(World);

	if (!bOverrideIntersection)
	{
		// Reset override rules
		for (FTrafficIntersectionSide& IntersectionSide : IntersectionSides)
		{
			IntersectionSide.SideOverride = (uint8)EIntersectionSideRule::AllClosed;
		}
	}
	
	// By default, disable all intersection lanes when we are switching between overriding and default state
	FTrafficPeriod Period = FTrafficPeriod();
	for (const FTrafficIntersectionSide& IntersectionSide : IntersectionSides)
	{
		Period.Lanes.Append(IntersectionSide.Lanes);
	}
	
	auto PeriodEvent = FTrafficPeriodEvent();
	PeriodEvent.Period = Period;
	PeriodEvent.State = ELaneState::Closed;

	ZoneGraphAnnotationSubsystem->SendEvent(PeriodEvent);

	// Reset period index - this will allow us to apply a new period next tick
	CurrentPeriodIndex = INDEX_NONE;
}
