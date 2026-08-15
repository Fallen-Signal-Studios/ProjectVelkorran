// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/MassVehicleSubsystem.h"

#include "MassCommonFragments.h"
#include "MassEntityUtils.h"
#include "MassEntityView.h"
#include "MassZoneGraphNavigationFragments.h"
#include "ZoneGraphQuery.h"
#include "Engine/World.h"
#include "ZoneGraphSubsystem.h"
#include "Vehicles/Mass/MassVehicle.h"
#include "Logging/LogVerbosity.h"
#include "VisualLogger/VisualLogger.h"

void UMassVehicleSubsystem::VehicleEnterLane(const FZoneGraphLaneHandle& Lane, const FMassEntityHandle& Vehicle)
{
	auto& VehiclesInlane = VehiclesInLanes.FindOrAdd(Lane);
	VehiclesInlane.VehiclesInLane.Add(Vehicle);
}

void UMassVehicleSubsystem::VehicleLeaveLane(const FZoneGraphLaneHandle& Lane, const FMassEntityHandle& Vehicle)
{
	auto& VehiclesInlane = VehiclesInLanes.FindOrAdd(Lane);
	VehiclesInlane.VehiclesInLane.Remove(Vehicle);
}

FMassEntityHandle UMassVehicleSubsystem::GetTailVehicle(const FZoneGraphLaneHandle& Lane) const
{
	auto OutVehicle = FMassEntityHandle();
	if (auto VehicleLane = VehiclesInLanes.Find(Lane))
	{
		if (!VehicleLane->VehiclesInLane.IsEmpty())
		{
			OutVehicle = VehicleLane->VehiclesInLane.Last();
		}
	}
	
	return OutVehicle;
}

float UMassVehicleSubsystem::GetAvailableSpace(const FZoneGraphLaneHandle& Lane, bool bIncludeIncomingVehicles) const
{
	auto TailVehicle = GetTailVehicle(Lane);
	auto VehicleLane = VehiclesInLanes.Find(Lane);

	auto ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(GetWorld());
	auto ZoneGraphStorage = ZoneGraphSubsystem->GetZoneGraphStorage(Lane.DataHandle);
	float LaneLength;
	UE::ZoneGraph::Query::GetLaneLength(*ZoneGraphStorage, Lane, LaneLength);
	
	if (VehicleLane)
	{
		float DistanceAlongLane = LaneLength;
		
		if (TailVehicle.IsValid())
		{
			auto TailVehicleView = FMassEntityView::TryMakeView(UE::Mass::Utils::GetEntityManagerChecked(*GetWorld()), TailVehicle);
			if (TailVehicleView.IsValid())
			{
				if (auto TailLaneLocation = TailVehicleView.GetFragmentDataPtr<FMassZoneGraphLaneLocationFragment>())
				{
					DistanceAlongLane = TailLaneLocation->DistanceAlongLane;
				}
			}
		}
		else if (VehicleLane->IncomingVehicles == 0)
		{
			// special exception
			// If there are no incoming vehicles and no tail vehicle, we want to let at least one vehicle into the lane
			// If we dont, its possible this lane will get filtered out. The only time this may be an issue is with small lanes.
			return FLT_MAX;
		}

		// @todo expose this var or at the very least calculate space based on distance between vehicles and vehicle space taken
		float VehicleSpace = bIncludeIncomingVehicles ? (VehicleLane->IncomingVehicles * 1000.f) : 0;
		return FMath::Max(0,  DistanceAlongLane - VehicleSpace);
	}

	return LaneLength;
}

void UMassVehicleSubsystem::ClaimLaneSpot(const FZoneGraphLaneHandle& Lane)
{
	auto& VehicleLane = VehiclesInLanes.FindOrAdd(Lane);
	VehicleLane.IncomingVehicles++;
}

void UMassVehicleSubsystem::UnclaimLaneSpot(const FZoneGraphLaneHandle& Lane)
{
	auto& VehicleLane = VehiclesInLanes.FindOrAdd(Lane);
	VehicleLane.IncomingVehicles = FMath::Max(0, VehicleLane.IncomingVehicles - 1);
}


void UMassVehicleSubsystem::DebugLane(const FZoneGraphLaneHandle& Lane)
{
#if ENABLE_VISUAL_LOG
	auto ZoneGraphSubsystem = UWorld::GetSubsystem<UZoneGraphSubsystem>(GetWorld());
	
	if (auto VehicleLane = VehiclesInLanes.Find(Lane))
	{
		FZoneGraphLaneLocation LaneAfterIntersectionLocation;
		ZoneGraphSubsystem->CalculateLocationAlongLane(Lane, 0, LaneAfterIntersectionLocation);
		
		auto TailVehicle = GetTailVehicle(Lane);
		if (TailVehicle.IsValid())
		{
			auto& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*GetWorld());
			auto EntityView = FMassEntityView::TryMakeView(EntityManager, TailVehicle);
			if (EntityView.IsValid())
			{
				const auto& TransformFragment = EntityView.GetFragmentData<FTransformFragment>();
				UE_VLOG_SEGMENT_THICK(this, LogMassVehicle, Log, TransformFragment.GetTransform().GetLocation(), LaneAfterIntersectionLocation.Position, FColor::Green, 10.f, TEXT("Space: %f | Incoming vehicles: %f"), GetAvailableSpace(Lane,true), VehicleLane->IncomingVehicles);
			}
		}
	}
#endif
}