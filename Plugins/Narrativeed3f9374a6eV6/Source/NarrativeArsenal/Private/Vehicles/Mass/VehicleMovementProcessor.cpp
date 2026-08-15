// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/VehicleMovementProcessor.h"

#include "ArsenalStatics.h"
#include "AudioMixerBlueprintLibrary.h"
#include "MassCommonFragments.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassZoneGraphNavigationFragments.h"
#include "ZoneGraphQuery.h"
#include "ZoneGraphSubsystem.h"
#include "MassLODFragments.h"
#include "MassMovementFragments.h"
#include "MassRepresentationFragments.h"
#include "ZoneGraphAnnotationSubsystem.h"
#include "Vehicles/Mass/MassVehicleSubsystem.h"
#include "Vehicles/Mass/TrafficLightSettings.h"
#include "MassExternalSubsystemTraits.h"
#include "AI/Mass/IncomingCollisionFragments.h"
#include "Vehicles/Mass/MassVehicle.h"
#include "Vehicles/Mass/VehicleFragments.h"

UVehicleMovementProcessor::UVehicleMovementProcessor()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement;
}

void UVehicleMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NextVehicleQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	NextVehicleQuery.AddSubsystemRequirement<UMassVehicleSubsystem>(EMassFragmentAccess::ReadOnly);
	NextVehicleQuery.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly);
	NextVehicleQuery.AddConstSharedRequirement<FVehicleSettingsFragment>();

	NextVehicleQuery.AddRequirement<FVehicleLocomotionFragment>(EMassFragmentAccess::ReadWrite);
	
	CalculateSpeedQuery.AddRequirement<FIncomingCollisionFragment>(EMassFragmentAccess::ReadOnly);
	CalculateSpeedQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	CalculateSpeedQuery.AddConstSharedRequirement<FVehicleSettingsFragment>();
	CalculateSpeedQuery.AddSubsystemRequirement<UMassVehicleSubsystem>(EMassFragmentAccess::ReadOnly);
	CalculateSpeedQuery.AddSubsystemRequirement<UZoneGraphSubsystem>(EMassFragmentAccess::ReadOnly);
	CalculateSpeedQuery.AddSubsystemRequirement<UZoneGraphAnnotationSubsystem>(EMassFragmentAccess::ReadOnly);
	
	CalculateSpeedQuery.AddRequirement<FVehicleLocomotionFragment>(EMassFragmentAccess::ReadWrite);

	CalculateLaneQuery.AddConstSharedRequirement<FVehicleSettingsFragment>();
	CalculateLaneQuery.AddSubsystemRequirement<UZoneGraphSubsystem>(EMassFragmentAccess::ReadOnly);
	CalculateLaneQuery.AddSubsystemRequirement<UZoneGraphAnnotationSubsystem>(EMassFragmentAccess::ReadOnly);
	
	CalculateLaneQuery.AddSubsystemRequirement<UMassVehicleSubsystem>(EMassFragmentAccess::ReadWrite);
	CalculateLaneQuery.AddRequirement<FVehicleLocomotionFragment>(EMassFragmentAccess::ReadWrite);
	CalculateLaneQuery.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadWrite);
	
	MoveVehicleQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	MoveVehicleQuery.AddSubsystemRequirement<UZoneGraphAnnotationSubsystem>(EMassFragmentAccess::ReadOnly);
	MoveVehicleQuery.AddSubsystemRequirement<UZoneGraphSubsystem>(EMassFragmentAccess::ReadOnly);
	MoveVehicleQuery.AddConstSharedRequirement<FVehicleSettingsFragment>();

	MoveVehicleQuery.AddSubsystemRequirement<UMassVehicleSubsystem>(EMassFragmentAccess::ReadWrite);
	MoveVehicleQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	MoveVehicleQuery.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadWrite);
	MoveVehicleQuery.AddRequirement<FVehicleLocomotionFragment>(EMassFragmentAccess::ReadWrite);
	MoveVehicleQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
}

void UVehicleMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Calculate time to collision using nearby obstacles
	NextVehicleQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const UMassVehicleSubsystem& VehicleSubsystem = Context.GetSubsystemChecked<UMassVehicleSubsystem>();
		TConstArrayView<FTransformFragment> TransformFragments = Context.GetFragmentView<FTransformFragment>();
		const FVehicleSettingsFragment& VehicleSettings = Context.GetConstSharedFragment<FVehicleSettingsFragment>();
		TConstArrayView<FMassZoneGraphLaneLocationFragment> LaneLocations = Context.GetFragmentView<FMassZoneGraphLaneLocationFragment>();

		TArrayView<FVehicleLocomotionFragment> VehicleLocomotionFragments = Context.GetMutableFragmentView<FVehicleLocomotionFragment>();

		for (int i=0;i<Context.GetNumEntities();i++)
		{
			const FTransform& Transform = TransformFragments[i].GetTransform();
			FVehicleLocomotionFragment& VehicleLocomotion = VehicleLocomotionFragments[i];
			const FMassZoneGraphLaneLocationFragment& LaneLocation = LaneLocations[i];

			TArray<FMassEntityHandle> NearbyObstacles;
			FBox Extent = FBox::BuildAABB(Transform.GetLocation(), FVector(VehicleSettings.ObstacleSearchRadius));
			VehicleSubsystem.VehicleObstacles.Query(Extent, NearbyObstacles);
			
			VehicleLocomotion.DistanceToNextVehicle = FLT_MAX;
			VehicleLocomotion.NextVehicleHandle.Reset();

			// Get vehicle in front of us (in the same/next lane)
			for (const FMassEntityHandle& NearbyObstacle : NearbyObstacles)
			{
				if (NearbyObstacle == Context.GetEntity(i)) { continue; }
				
				FMassEntityView ObstacleView = FMassEntityView(Context.GetEntityManagerChecked(), NearbyObstacle);
				
				// If the obstacle is a vehicle (on zonegraph)
				if (auto ObstacleLaneLocation = ObstacleView.GetFragmentDataPtr<FMassZoneGraphLaneLocationFragment>())
				{
					// check if we are on the same lane and the obstacle vehicle is ahead of us in the lane
					bool bSameLane = LaneLocation.LaneHandle == ObstacleLaneLocation->LaneHandle;
					bool bAheadOfVehicle = LaneLocation.DistanceAlongLane < ObstacleLaneLocation->DistanceAlongLane;
					float DistanceToVehicle = ObstacleLaneLocation->DistanceAlongLane - LaneLocation.DistanceAlongLane;

					// If there is a nearby vehicle on the next lane - we also need to check for that
					bool bIsInNextLane = VehicleLocomotion.NextLane == ObstacleLaneLocation->LaneHandle;
					float DistanceToNext = ObstacleLaneLocation->DistanceAlongLane + (LaneLocation.LaneLength - LaneLocation.DistanceAlongLane);
					
					if (bSameLane && bAheadOfVehicle && DistanceToVehicle < VehicleLocomotion.DistanceToNextVehicle)
					{
						VehicleLocomotion.DistanceToNextVehicle = DistanceToVehicle;
						VehicleLocomotion.NextVehicleHandle = NearbyObstacle;
					}
					else if (bIsInNextLane && DistanceToNext < VehicleLocomotion.DistanceToNextVehicle)
					{
						VehicleLocomotion.DistanceToNextVehicle = DistanceToNext;
						VehicleLocomotion.NextVehicleHandle = NearbyObstacle;
					}
				}
			}
		}
	});

	CalculateSpeedQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		TConstArrayView<FIncomingCollisionFragment> IncomingCollisionFragments = Context.GetFragmentView<FIncomingCollisionFragment>();
		TConstArrayView<FTransformFragment> TransformFragments = Context.GetFragmentView<FTransformFragment>();
		const FVehicleSettingsFragment& VehicleSettings = Context.GetConstSharedFragment<FVehicleSettingsFragment>();
		const UMassVehicleSubsystem& VehicleSubsystem = Context.GetSubsystemChecked<UMassVehicleSubsystem>();
		const UZoneGraphSubsystem& ZoneGraph = Context.GetSubsystemChecked<UZoneGraphSubsystem>();
		const UZoneGraphAnnotationSubsystem& AnnotationSubsystem = Context.GetSubsystemChecked<UZoneGraphAnnotationSubsystem>();
		const UTrafficLightSettings* TrafficLightSettings = GetDefault<UTrafficLightSettings>(); //@todo this is probably better within a shared fragment

		TArrayView<FVehicleLocomotionFragment> VehicleLocomotionFragments = Context.GetMutableFragmentView<FVehicleLocomotionFragment>();
		
		Context.ForEachEntityInChunk([&](FMassExecutionContext& MassContext, int32 EntityIndex)
		{
			FVehicleLocomotionFragment& VehicleLocomotion = VehicleLocomotionFragments[EntityIndex];
			const FIncomingCollisionFragment& IncomingCollisionFragment = IncomingCollisionFragments[EntityIndex];
			const FTransformFragment& TransformFragment = TransformFragments[EntityIndex];

			const FVector& Location = TransformFragment.GetTransform().GetLocation();
			float Speed = VehicleSettings.VehicleMaxSpeed;
			
			// We do one more speed adjustment to ensure we dont get too close to the vehicle in front of us (in the same/next lane).
			// This is because when turning directions, vehicles may not see that they are about to bump into each other
			CalculateSpeedFromObstacle(VehicleLocomotion.DistanceToNextVehicle, VehicleSettings.MinimumDistanceToNext, VehicleSettings.BrakingDistanceFromNext, VehicleSettings.NextVehicleAvoidanceBrakingPower, Speed);

			// If obstacle and next vehicle are different, also adjust speed for incoming obstacle
			if (VehicleLocomotion.NextVehicleHandle != IncomingCollisionFragment.IncomingEntity)
			{
				// Obstacle speed adjustment
				CalculateSpeedFromObstacle(IncomingCollisionFragment.DistanceToObstacle, VehicleSettings.MinimumDistanceToObstacle, VehicleSettings.BrakingDistanceFromObstacle, VehicleSettings.ObstacleAvoidanceBrakingPower, Speed);
			}
			
			// Incoming closed lane speed adjustment
			if (VehicleLocomotion.NextLane.IsValid())
			{
				auto Storage = ZoneGraph.GetZoneGraphStorage(VehicleLocomotion.NextLane.DataHandle);
				bool bStopAtLaneExit = false;
			
				auto Tags = AnnotationSubsystem.GetAnnotationTags(VehicleLocomotion.NextLane);
				if (Tags.Contains(TrafficLightSettings->ClosedTag) || !VehicleSettings.VehicleLaneFilter.Pass(Tags))
				{
					bStopAtLaneExit = true;
				}

				// If our next lane is an intersection, make sure we have room in the lane after the intersection (this is to prevent intersection freezes)
				// if there is no room, make sure we dont enter intersection
				// In addition, if the intersection will lead us to an invalid lane, we will also stop
				if (!bStopAtLaneExit && Tags.Contains(TrafficLightSettings->IntersectionTag))
				{
					FZoneGraphLinkedLane LinkedLane;
					ZoneGraph.GetFirstLinkedLane(VehicleLocomotion.NextLane, EZoneLaneLinkType::Outgoing, EZoneLaneLinkFlags::All, EZoneLaneLinkFlags::None, LinkedLane);

					if (LinkedLane.IsValid())
					{
						auto LinkedLaneTags = AnnotationSubsystem.GetAnnotationTags(VehicleLocomotion.NextLane);
						float Space = VehicleSubsystem.GetAvailableSpace(LinkedLane.DestLane, false);
						
						if (Space < 800.f)
						{
							bStopAtLaneExit = true;
						}
						
						if (!VehicleSettings.VehicleLaneFilter.Pass(LinkedLaneTags))
						{
							bStopAtLaneExit = true;
						}
					}
				}
				
				if (bStopAtLaneExit)
				{
					FZoneGraphLaneLocation ClosedLaneLocation;
					UE::ZoneGraph::Query::CalculateLocationAlongLane(*Storage, VehicleLocomotion.NextLane, 0, ClosedLaneLocation);

					CalculateSpeedFromObstacle(FVector::Dist(ClosedLaneLocation.Position, Location), VehicleSettings.ClosedLaneMinDistance, VehicleSettings.ClosedLaneBrakingDistance, VehicleSettings.ClosedLaneBrakingPower, Speed);
				}
			}

			// Update desired speed
			VehicleLocomotion.DesiredSpeed = FMath::Clamp(Speed, 0.f, VehicleSettings.VehicleMaxSpeed);
		});
	});

	CalculateLaneQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const FVehicleSettingsFragment& VehicleSettings = Context.GetConstSharedFragment<FVehicleSettingsFragment>();
		const UZoneGraphSubsystem& ZoneGraph = Context.GetSubsystemChecked<UZoneGraphSubsystem>();
		const UZoneGraphAnnotationSubsystem& AnnotationSubsystem = Context.GetSubsystemChecked<UZoneGraphAnnotationSubsystem>();
		const UTrafficLightSettings* TrafficLightSettings = GetDefault<UTrafficLightSettings>(); //@todo this is probably better within a shared fragment

		UMassVehicleSubsystem& VehicleSubsystem = Context.GetMutableSubsystemChecked<UMassVehicleSubsystem>();
		TArrayView<FVehicleLocomotionFragment> VehicleLocomotionFragments = Context.GetMutableFragmentView<FVehicleLocomotionFragment>();
		TArrayView<FMassZoneGraphLaneLocationFragment> LaneLocationFragments = Context.GetMutableFragmentView<FMassZoneGraphLaneLocationFragment>();
		
		Context.ForEachEntityInChunk([&](FMassExecutionContext& MassContext, int32 EntityIndex)
		{
			FVehicleLocomotionFragment& VehicleLocomotionFragment = VehicleLocomotionFragments[EntityIndex];
			FMassZoneGraphLaneLocationFragment& LaneLocation = LaneLocationFragments[EntityIndex];

			// If we have reached *near* the end and we dont have a next lane set yet, find a new lane to go to
			if (!VehicleLocomotionFragment.NextLane.IsValid() && LaneLocation.DistanceAlongLane >= (LaneLocation.LaneLength - VehicleSettings.ClosedLaneBrakingDistance))
			{
				TArray<FZoneGraphLinkedLane> LinkedLanes;
				ZoneGraph.GetLinkedLanes(LaneLocation.LaneHandle, EZoneLaneLinkType::Outgoing, EZoneLaneLinkFlags::All, EZoneLaneLinkFlags::OppositeDirection, LinkedLanes);

				TArray<TPair<float, FZoneGraphLinkedLane>> WeightedLanes;
				float BestScore = 0.f;

				// Weigh lanes based on space available and tags
				// This will allow us to pick a random lane with a high score
				for (const FZoneGraphLinkedLane& Lane : LinkedLanes)
				{
					float Weight = 0.f;

					// Lanes with an invalid tag should stay at 0 weight
					auto NextLaneTags = AnnotationSubsystem.GetAnnotationTags(Lane.DestLane);
					if (!VehicleSettings.VehicleLaneFilter.Pass(NextLaneTags))
					{
						continue;
					}
					
					TArray<FZoneGraphLinkedLane> NextLinkedLanes;
					ZoneGraph.GetLinkedLanes(Lane.DestLane, EZoneLaneLinkType::Outgoing, EZoneLaneLinkFlags::All, EZoneLaneLinkFlags::None, NextLinkedLanes);

					for (const FZoneGraphLinkedLane& NextLinkedLane : NextLinkedLanes)
					{
						auto LinkedTags = AnnotationSubsystem.GetAnnotationTags(NextLinkedLane.DestLane);
					
						// check for available space and valid lane
						bool bIsValidNextLane = VehicleSettings.VehicleLaneFilter.Pass(LinkedTags);
						bool bHasEnoughSpace = VehicleSubsystem.GetAvailableSpace(NextLinkedLane.DestLane, true) >= 800.f;

						if (bIsValidNextLane)
						{
							Weight += 1.f;
						}
						if (bHasEnoughSpace)
						{
							Weight += 0.5f;
						}
					}

					BestScore = FMath::Max(BestScore, Weight); // Store the best score, so we can pick a random one
					WeightedLanes.Add({Weight, Lane});
				}

				// get all lanes that are a high score
				auto BestLanes = WeightedLanes.FilterByPredicate([BestScore](const TPair<float, FZoneGraphLinkedLane>& TestLane)
				{
					return TestLane.Key >= BestScore;
				});

				// No valid lanes, continue
				if (BestLanes.IsEmpty())
				{
					return;
				}
				
				int RandomIndex = FMath::RandRange(0, BestLanes.Num()-1);
				auto& LinkedLane = BestLanes[RandomIndex].Value;

				// If we are entering an intersection, claim the destination lane to prevent overfilling
				auto Tags = AnnotationSubsystem.GetAnnotationTags(LinkedLane.DestLane);
				if (Tags.Contains(TrafficLightSettings->IntersectionTag))
				{
					FZoneGraphLinkedLane LaneAfterIntersection;
					ZoneGraph.GetFirstLinkedLane(LinkedLane.DestLane, EZoneLaneLinkType::Outgoing, EZoneLaneLinkFlags::All, EZoneLaneLinkFlags::None, LaneAfterIntersection);

					VehicleSubsystem.ClaimLaneSpot(LaneAfterIntersection.DestLane);
				}
				
				// By this point, we are confident we will choose a new lane to go to

				VehicleLocomotionFragment.NextLane = LinkedLane.DestLane;
			}

			// When we have actually reached the end of the lane, switch to the next lane
			if (VehicleLocomotionFragment.NextLane.IsValid() && LaneLocation.DistanceAlongLane >= LaneLocation.LaneLength)
			{
				auto ZoneGraphStorage = ZoneGraph.GetZoneGraphStorage(VehicleLocomotionFragment.NextLane.DataHandle);

				// if we were in an intersection, remove our 'incoming' spot - we will become the tail vehicle and space will be calculated correctly.
				auto Tags = AnnotationSubsystem.GetAnnotationTags(LaneLocation.LaneHandle);
				if (Tags.Contains(TrafficLightSettings->IntersectionTag))
				{
					VehicleSubsystem.UnclaimLaneSpot(VehicleLocomotionFragment.NextLane);
				}

				// Leave current lane
				VehicleSubsystem.VehicleLeaveLane(LaneLocation.LaneHandle, Context.GetEntity(EntityIndex));
				
				LaneLocation.LaneHandle = VehicleLocomotionFragment.NextLane;
				LaneLocation.DistanceAlongLane = 0;
				UE::ZoneGraph::Query::GetLaneLength(*ZoneGraphStorage, LaneLocation.LaneHandle, LaneLocation.LaneLength);

				VehicleLocomotionFragment.NextLane.Reset();

				// Enter new lane
				VehicleSubsystem.VehicleEnterLane(LaneLocation.LaneHandle, Context.GetEntity(EntityIndex));
			}
		});
	});
	
	MoveVehicleQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		// Steps
		// If we are not currently following a lane, find the nearest lane to follow
		// Use speed to move along lane until the end
		// Once we reach the end, attempt to move to the next lane

		const UZoneGraphSubsystem& ZoneGraph = Context.GetSubsystemChecked<UZoneGraphSubsystem>();
		TConstArrayView<FMassRepresentationFragment> RepresentationFragments = Context.GetFragmentView<FMassRepresentationFragment>();
		TConstArrayView<FVehicleLocomotionFragment> VehicleLocomotionFragments = Context.GetFragmentView<FVehicleLocomotionFragment>();

		TArrayView<FMassZoneGraphLaneLocationFragment> ZoneGraphLaneLocations = Context.GetMutableFragmentView<FMassZoneGraphLaneLocationFragment>();
		TArrayView<FTransformFragment> TransformFragments = Context.GetMutableFragmentView<FTransformFragment>();
		TArrayView<FMassVelocityFragment> VelocityFragments = Context.GetMutableFragmentView<FMassVelocityFragment>();

		for (int i=0;i<Context.GetNumEntities();i++)
		{
			const FMassRepresentationFragment& Representation = RepresentationFragments[i];
			const FVehicleLocomotionFragment& VehicleLocomotion = VehicleLocomotionFragments[i];
			
			FMassZoneGraphLaneLocationFragment& LaneLocation = ZoneGraphLaneLocations[i];
			FTransformFragment& TransformFragment = TransformFragments[i];
			FMassVelocityFragment& VelocityFragment = VelocityFragments[i];

			FTransform& Transform = TransformFragment.GetMutableTransform();

			if (!LaneLocation.LaneHandle.IsValid())
			{
				UE_VLOG_LOCATION(Context.GetWorld(), LogMassVehicle, Error, Transform.GetLocation(), 20.f, FColor::Red, TEXT("Vehicle is unable to find nearby lane!"
													  "Please make sure zonegraph is built and that you are spawning vehicles on valid lanes"));
				continue;
			}

			// Advance vehicle along lane

			// High LOD will update transforms/velocity manually
			if (Representation.CurrentRepresentation != EMassRepresentationType::HighResSpawnedActor)
			{
				LaneLocation.DistanceAlongLane += VehicleLocomotion.DesiredSpeed * Context.GetDeltaTimeSeconds();
				LaneLocation.DistanceAlongLane = FMath::Min(LaneLocation.LaneLength, LaneLocation.DistanceAlongLane); // clamp

				// Update transform to match lane position
				FZoneGraphLaneLocation CurrentLanePosition;
				ZoneGraph.CalculateLocationAlongLane(LaneLocation.LaneHandle, LaneLocation.DistanceAlongLane, CurrentLanePosition);
				auto Direction = FRotationMatrix::MakeFromX(CurrentLanePosition.Tangent).ToQuat();
				
				auto InterpLocation = FMath::VInterpTo(Transform.GetLocation(), CurrentLanePosition.Position, Context.GetDeltaTimeSeconds(), 1.f);
				InterpLocation.Z = CurrentLanePosition.Position.Z;
				Transform.SetLocation(InterpLocation);
				
				auto InterpRotation = FMath::QInterpTo(Transform.GetRotation(), Direction, Context.GetDeltaTimeSeconds(), 1.f);
				
				Transform.SetRotation(InterpRotation);

				// Update velocity fragment
				VelocityFragment.Value = Direction.GetForwardVector() * VehicleLocomotion.DesiredSpeed;
			}
			else
			{
				// If we are using PID, we simply update our distance along the lane based on the closest location to us
				FZoneGraphLaneLocation NearestLocation;
				float DistSq;
				ZoneGraph.FindNearestLocationOnLane(LaneLocation.LaneHandle, FBox::BuildAABB(Transform.GetLocation(), FVector(1000.f)), NearestLocation, DistSq);

				// Since bp vehicle can be slightly behind, we need to let the system know of our nearest location on the zonegraph
				LaneLocation.DistanceAlongLane = NearestLocation.DistanceAlongLane;
			}
		}
	});
}

void UVehicleMovementProcessor::CalculateSpeedFromObstacle(float DistanceToObstacle, float MinDistanceToObstacle,
                                                           float BreakingDistanceToObstacle, float BrakingPower, float& Speed)
{
	float ObstacleAvoidanceBrakingSpeedFactor = FMath::Clamp(FMath::GetRangePct(MinDistanceToObstacle, BreakingDistanceToObstacle, DistanceToObstacle), 0.0f, 1.0f);
	ObstacleAvoidanceBrakingSpeedFactor = FMath::Pow(ObstacleAvoidanceBrakingSpeedFactor, BrakingPower);
			
	const float MaxAvoidanceSpeed = Speed * ObstacleAvoidanceBrakingSpeedFactor;
	Speed = FMath::Min(Speed, MaxAvoidanceSpeed);
}
