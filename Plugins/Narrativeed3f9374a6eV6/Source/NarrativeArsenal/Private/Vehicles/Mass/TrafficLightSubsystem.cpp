// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/TrafficLightSubsystem.h"

#include "MassEntityView.h"
#include "MassSimulationSubsystem.h"
#include "MassZoneGraphNavigationFragments.h"
#include "ZoneGraphAnnotationSubsystem.h"
#include "ZoneGraphDelegates.h"
#include "ZoneGraphQuery.h"
#include "ZoneGraphSubsystem.h"
#include "Vehicles/Mass/MassVehicleSubsystem.h"
#include "Vehicles/Mass/TrafficLightIntersectionData.h"
#include "Vehicles/Mass/TrafficLightSettings.h"
#include "Vehicles/TrafficLights/TrafficLight.h"
#include "Logging/LogVerbosity.h"
#include "VisualLogger/VisualLogger.h"

DEFINE_LOG_CATEGORY(LogTrafficLight)

void UTrafficLightSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UMassSimulationSubsystem>();
	ZoneGraphSubsystem = Collection.InitializeDependency<UZoneGraphSubsystem>();
	ZoneGraphAnnotationSubsystem = Collection.InitializeDependency<UZoneGraphAnnotationSubsystem>();
	VehicleSubsystem = Collection.InitializeDependency<UMassVehicleSubsystem>();

	// Register existing data.
	for (const FRegisteredZoneGraphData& Registered : ZoneGraphSubsystem->GetRegisteredZoneGraphData())
	{
		if (Registered.bInUse && Registered.ZoneGraphData != nullptr)
		{
			PostZoneGraphDataAdded(Registered.ZoneGraphData);
		}
	}

	OnPostZoneGraphDataAddedHandle = UE::ZoneGraphDelegates::OnPostZoneGraphDataAdded.AddUObject(this, &ThisClass::PostZoneGraphDataAdded);
	OnPreZoneGraphDataRemovedHandle = UE::ZoneGraphDelegates::OnPreZoneGraphDataRemoved.AddUObject(this, &ThisClass::PreZoneGraphDataRemoved);
}

TStatId UTrafficLightSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTrafficLightSubsystem, STATGROUP_Tickables);
}

void UTrafficLightSubsystem::Tick(float DeltaTime)
{
	// Cycle through all intersections and progress their periods
	for (FTrafficLightData& LaneData : RegisteredLaneData)
	{
		for (FTrafficLightIntersection& Intersection : LaneData.Intersections)
		{
			// If the intersection is being overriden, we simply dont perform any logic
			if (Intersection.bOverrideIntersection) { continue; }
			
			// No periods for intersection
			if (Intersection.TrafficPeriods.IsEmpty()) { continue; }
			
			// Initialize period
			const FTrafficPeriod* CurrentPeriod = nullptr;
			if (Intersection.CurrentPeriodIndex == INDEX_NONE)
			{
				CurrentPeriod = &Intersection.IncrementCurrentPeriod();
				UpdateRegisteredTrafficLights(Intersection);

				auto PeriodEvent = FTrafficPeriodEvent();
				PeriodEvent.Period = *CurrentPeriod;
				PeriodEvent.State = ELaneState::Open;

				ZoneGraphAnnotationSubsystem->SendEvent(PeriodEvent);

				continue;
			}
			else
			{
				CurrentPeriod = Intersection.GetCurrentPeriod();
			}
			
			Intersection.RemainingPeriodDuration -= DeltaTime;

			const UTrafficLightSettings* TrafficSettings = GetDefault<UTrafficLightSettings>();
			if (Intersection.RemainingPeriodDuration <= TrafficSettings->LaneCloseAdvanceTime)
			{
				// send close event (ahead of time if specified)
				// @todo Right now this may get called multiple times but no logic should break as a result
				
				auto PeriodEvent = FTrafficPeriodEvent();
				PeriodEvent.Period = *CurrentPeriod;
				PeriodEvent.State = ELaneState::Closed;
				
				ZoneGraphAnnotationSubsystem->SendEvent(PeriodEvent);
			}

			// Advance to next period if the intersection is not blocked
			
			if (Intersection.RemainingPeriodDuration <= 0)
			{
				FVector SideAverage = FVector::ZeroVector;
				for (const FTrafficIntersectionSide& IntersectionSide : Intersection.IntersectionSides)
				{
					SideAverage += IntersectionSide.SideLocation;
				}
				SideAverage /= Intersection.IntersectionSides.Num();
			
				auto IntersectionArea = FBox::BuildAABB(SideAverage, FVector(2000.f));
			
				TArray<FMassEntityHandle> ObstacleHandles;
				VehicleSubsystem->VehicleObstacles.Query(IntersectionArea, ObstacleHandles);

				// @todo this can probably be a entity query
				bool bIsIntersectionOccupied = false;
				auto& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*GetWorld());
				for (const FMassEntityHandle& ObstacleHandle : ObstacleHandles)
				{
					FMassEntityView ObstacleView = FMassEntityView(EntityManager, ObstacleHandle);

					if (auto LaneFragment = ObstacleView.GetFragmentDataPtr<FMassZoneGraphLaneLocationFragment>())
					{
						if (Intersection.GetLanes().Contains(LaneFragment->LaneHandle))
						{
							bIsIntersectionOccupied = true;
							break;
						}
					}
				}

				// Early exit for occupied intersection
				if (bIsIntersectionOccupied) { continue; }
				
				auto PeriodEvent = FTrafficPeriodEvent();
				CurrentPeriod = &Intersection.IncrementCurrentPeriod();
				UpdateRegisteredTrafficLights(Intersection);

				// Send event for next period
				PeriodEvent.Period = *CurrentPeriod;
				PeriodEvent.State = ELaneState::Open;

				ZoneGraphAnnotationSubsystem->SendEvent(PeriodEvent);
			}
		}
	}
}

void UTrafficLightSubsystem::PostZoneGraphDataAdded(const AZoneGraphData* ZoneGraphData)
{
	const UWorld* World = GetWorld();

	// Only consider valid graph from our world
	if (ZoneGraphData == nullptr || ZoneGraphData->GetWorld() != World)
	{
		return;
	}

	const FZoneGraphStorage& Storage = ZoneGraphData->GetStorage();
	const int32 Index = Storage.DataHandle.Index;

	if (Index >= RegisteredLaneData.Num())
	{
		RegisteredLaneData.SetNum(Index + 1);
	}

	FTrafficLightData& LaneData = RegisteredLaneData[Index];
	if (LaneData.DataHandle != Storage.DataHandle)
	{
		// Initialize lane data if here the first time.
		BuildLaneData(LaneData, Storage);

		// Now that we have lane/intersection data, we can start implementing traffic lights
		for (int i=0;i<LaneData.Intersections.Num();i++)
		{
			FTrafficLightIntersection& Intersection = LaneData.Intersections[i];

			// not really an intersection if we have <= 1 side (probably a mistake while authoring level)
			if (Intersection.IntersectionSides.Num() <= 1)
			{
				continue;
			}

			// Ensure all intersection lanes are closed on initialization
			{
				FTrafficPeriodEvent PeriodEvent = FTrafficPeriodEvent();

				TArray<FZoneGraphLaneHandle> DisabledLanes;
				for (int SideIndex=0;SideIndex<Intersection.IntersectionSides.Num();SideIndex++)
				{
					DisabledLanes.Append(Intersection.IntersectionSides[SideIndex].Lanes);
				}
				PeriodEvent.Period = FTrafficPeriod(DisabledLanes, -1, EIntersectionSideRule::AllClosed);
				PeriodEvent.State = ELaneState::Closed;
			
				ZoneGraphAnnotationSubsystem->SendEvent(PeriodEvent);
			}
			
			// If we are a 4-sided intersection, we can implement 2 periods

			// For simplicity, if we are not a 4 sided + square intersection, we let each intersection side go for 1 period
			// If we have a 4 sided + square intersection, we can implement more complex periods (ex. letting opposite sides go simultaneously)

			if (Intersection.IntersectionSides.Num() == 4
				&& Intersection.IsSquareShaped())
			{
				for (int S=0;S<Intersection.IntersectionSides.Num();S++)
				{
					const uint32 SLeft = (S + 1) % 4;
					const uint32 SOpposite = (S + 2) % 4;
					const uint32 SRight = (S + 3) % 4;

					// Lanes that will be needed to build periods
					
					TArray<FZoneGraphLaneHandle> SideToOppositeAndRight;
					Intersection.GetSidesConnectingLanes(S, SOpposite, Storage, SideToOppositeAndRight);
					Intersection.GetSidesConnectingLanes(S, SRight, Storage, SideToOppositeAndRight);

					TArray<FZoneGraphLaneHandle> OppositeToSideAndLeft;
					Intersection.GetSidesConnectingLanes(SOpposite, S, Storage, OppositeToSideAndLeft);
					Intersection.GetSidesConnectingLanes(SOpposite, SLeft, Storage, OppositeToSideAndLeft);

					TArray<FZoneGraphLaneHandle> SideAll = Intersection.IntersectionSides[S].Lanes;
					TArray<FZoneGraphLaneHandle> OppositeAll = Intersection.IntersectionSides[SOpposite].Lanes;

					// Side to opposite and opposite to side (including right turns)
					{
						auto& Period = Intersection.TrafficPeriods.Add_GetRef(FTrafficPeriod());

						Period.Lanes.Append(SideToOppositeAndRight);
						Period.Lanes.Append(OppositeToSideAndLeft);
						Period.Duration = 15.f;
						Period.LanesCoveredMask = EIntersectionSideRule::StraightOpen | EIntersectionSideRule::RightOpen;
					}
					
					// side to all directions
					{
						auto& Period = Intersection.TrafficPeriods.Add_GetRef(FTrafficPeriod());

						Period.Lanes.Append(SideAll);
						Period.Duration = 10.f;
						Period.LanesCoveredMask = EIntersectionSideRule::AllDirectionsOpen;
					}

					// opposite to all directions
					{
						auto& Period = Intersection.TrafficPeriods.Add_GetRef(FTrafficPeriod());

						Period.Lanes.Append(OppositeAll);
						Period.Duration = 10.f;
						Period.LanesCoveredMask = EIntersectionSideRule::AllDirectionsOpen;
					}
				}
			}
			else
			{
				for (int k=0;k<Intersection.IntersectionSides.Num();k++)
				{
					auto& Period = Intersection.TrafficPeriods.Add_GetRef(FTrafficPeriod());

					Period.Lanes.Append(Intersection.IntersectionSides[k].Lanes);
					Period.Duration = 10.f;
				}
			}
		}
	}
}

void UTrafficLightSubsystem::PreZoneGraphDataRemoved(const AZoneGraphData* ZoneGraphData)
{
}

void UTrafficLightSubsystem::BuildLaneData(FTrafficLightData& LaneData, const FZoneGraphStorage& Storage)
{
	LaneData.DataHandle = Storage.DataHandle;
	LaneData.Intersections.Empty();
	
	const UTrafficLightSettings* TrafficSettings = GetDefault<UTrafficLightSettings>();
	
	checkf(ZoneGraphAnnotationSubsystem != nullptr, TEXT("ZoneGraphAnnotationSubsystem should be initialized from the subsystem collection dependencies."));

	TArray<FZoneGraphLinkedLane> Links;
	for (int32 LaneIndex = 0; LaneIndex < Storage.Lanes.Num(); ++LaneIndex)
	{
		const FZoneLaneData& ZoneLaneData = Storage.Lanes[LaneIndex];

		// If we run into an intersection lane, simply try to add that intersection to the list
		if (ZoneLaneData.Tags.Contains(TrafficSettings->IntersectionTag))
		{
			LaneData.FindOrAddIntersection(ZoneLaneData.ZoneIndex);
		}
		else
		{
			// If we run into a normal lane, see if it connects to an intersection
			// We will use this logic to add a side to the intersection
			// @note For now we make the assumption that only 1 lane will enter the intersection on one side. In the future, we may want more than 1 lane
			
			TArray<FZoneGraphLinkedLane> LinkedLanes;
			UE::ZoneGraph::Query::GetLinkedLanes(Storage, LaneIndex, EZoneLaneLinkType::Outgoing, EZoneLaneLinkFlags::None, EZoneLaneLinkFlags::None, LinkedLanes);
			
			int ZoneIndex = INDEX_NONE;
			for (const FZoneGraphLinkedLane& LinkedLane : LinkedLanes)
			{
				const FZoneLaneData& LinkZoneLaneData = Storage.Lanes[LinkedLane.DestLane.Index];

				// If the linked lane is an intersection, store zoneindex so we can later add a side to the intersection
				if (LinkZoneLaneData.Tags.Contains(TrafficSettings->IntersectionTag))
				{
					ZoneIndex = LinkZoneLaneData.ZoneIndex;
					break;
				}
			}

			if (ZoneIndex != INDEX_NONE)
			{
				auto& Intersection = LaneData.FindOrAddIntersection(ZoneIndex);

				// Add side to the intersection
				auto& Side = Intersection.IntersectionSides.Add_GetRef(FTrafficIntersectionSide());
				
				// Add intersection lanes to side
				for (const FZoneGraphLinkedLane& LinkedLane : LinkedLanes)
				{
					Side.Lanes.Add(LinkedLane.DestLane);

					// Naive solution - get tangent of first point in lane
					// @todo take out of for loop
					auto& SideLaneData = Storage.Lanes[LinkedLane.DestLane.Index];
					Side.DirectionIntoIntersection = Storage.LaneTangentVectors[SideLaneData.PointsBegin];
					Side.SideLocation = Storage.LanePoints[SideLaneData.PointsBegin];
				}

				// Add side to grid for use within traffic light visualization
				
				FTrafficIntersectionSideHandle Container(Storage.DataHandle, LaneData.Intersections.IndexOfByKey(ZoneIndex), Intersection.IntersectionSides.Num()-1);

				FBox IntersectionSideBounds = FBox::BuildAABB(Side.SideLocation, FVector(0.f));
#if ENABLE_VISUAL_LOG
				UE_VLOG_BOX(this, LogTrafficLight, Verbose, IntersectionSideBounds, FColor::Green, TEXT("Intersection Side"));
#endif
				IntersectionSidesGrid.Add(Container, IntersectionSideBounds);
			}
		}
 	}

	// Sort all intersection sides so they are in a clockwise direction
	// This will be useful when we implement traffic lights

	for (FTrafficLightIntersection& TrafficLightIntersection : LaneData.Intersections)
	{
		TrafficLightIntersection.SortSides();
	}
	
	for (const FTrafficLightData& Data : RegisteredLaneData)
	{
		for (const FTrafficLightIntersection& Intersection : Data.Intersections)
		{
			Intersection.DrawDebug(*GetWorld(), Storage);
		}
	}
}

void UTrafficLightSubsystem::RegisterTrafficLight(ATrafficLight* TrafficLight)
{
	if (TrafficLight)
	{
		RegisteredTrafficLights.Emplace(TrafficLight);
	}
}

void UTrafficLightSubsystem::UpdateRegisteredTrafficLights(const FTrafficLightIntersection& UpdatedIntersection)
{
	for (const TWeakObjectPtr<ATrafficLight>& RegisteredTrafficLight : RegisteredTrafficLights)
	{
		if (RegisteredTrafficLight.IsValid() && RegisteredTrafficLight->CachedIntersectionSide.IsValid())
		{
			auto& Intersection = RegisteredTrafficLight->CachedIntersectionSide.GetIntersection(this);
			if (Intersection == UpdatedIntersection)
			{
				RegisteredTrafficLight->OnPeriodUpdated();
			}
		}
	}
}
