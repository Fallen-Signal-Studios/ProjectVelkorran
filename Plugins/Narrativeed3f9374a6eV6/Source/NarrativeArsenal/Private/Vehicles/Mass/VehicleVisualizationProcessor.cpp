// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/VehicleVisualizationProcessor.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "MassMovementFragments.h"
#include "MassZoneGraphNavigationFragments.h"
#include "ZoneGraphSubsystem.h"
#include "MassExternalSubsystemTraits.h"
#include "MassLODFragments.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationSubsystem.h"
#include "MassUpdateISMProcessor.h"
#include "Vehicles/Mass/VehicleFragments.h"
#include "Vehicles/Mass/VehicleRepresentationSubsystem.h"

UVehicleVisualizationProcessor::UVehicleVisualizationProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SyncWorldToMass;
	RequiredTags.Add<FMassVehicleMovementToActorTag>();
	bRequiresGameThreadExecution = true;
}

void UVehicleVisualizationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	AddRequiredTagsToQuery(EntityQuery);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FVehicleComponentWrapperFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FVehiclePIDControllerFragment>();
	EntityQuery.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FVehicleLocomotionFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UZoneGraphSubsystem>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.SetChunkFilter(&FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);

	AddRequiredTagsToQuery(SyncVelocityQuery);
	SyncVelocityQuery.AddRequirement<FVehicleComponentWrapperFragment>(EMassFragmentAccess::ReadOnly);
	SyncVelocityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	SyncVelocityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
}

void UVehicleVisualizationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	SyncVelocityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		TArrayView<FMassVelocityFragment> VelocityFragments = Context.GetMutableFragmentView<FMassVelocityFragment>();
		TArrayView<FTransformFragment> TransformFragments = Context.GetMutableFragmentView<FTransformFragment>();
		TConstArrayView<FVehicleComponentWrapperFragment> VehicleComponentFragments = Context.GetFragmentView<FVehicleComponentWrapperFragment>();
		
		for (int i=0;i<Context.GetNumEntities();i++)
		{
			FMassVelocityFragment& VelocityFragment = VelocityFragments[i];
			FTransformFragment& TransformFragment = TransformFragments[i];
			const FVehicleComponentWrapperFragment& VehicleComponentFragment = VehicleComponentFragments[i];

			if (VehicleComponentFragment.Component.IsValid())
			{
				AActor* Owner = VehicleComponentFragment.Component->GetOwner();
				
				// Sync BP with Mass
				UPrimitiveComponent* RootComponent = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
				VelocityFragment.Value = RootComponent->GetPhysicsLinearVelocity();
				TransformFragment.SetTransform(Owner->GetTransform());
				//UE_LOG(LogTemp, Log, TEXT("Velocity: %f"), VelocityFragment.Value.Length());
			}
		}
	});
	
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		auto VelocityFragments = Context.GetMutableFragmentView<FMassVelocityFragment>();
		auto VehicleComponentFragments = Context.GetMutableFragmentView<FVehicleComponentWrapperFragment>();
		auto& PIDControllerFragment = Context.GetConstSharedFragment<FVehiclePIDControllerFragment>();
		auto LaneLocationFragments = Context.GetMutableFragmentView<FMassZoneGraphLaneLocationFragment>();
		auto& ZoneGraph = Context.GetSubsystemChecked<UZoneGraphSubsystem>();
		auto VehicleLocomotionFragments = Context.GetMutableFragmentView<FVehicleLocomotionFragment>();
		auto TransformFragments = Context.GetMutableFragmentView<FTransformFragment>();
		auto ActorFragments = Context.GetFragmentView<FMassActorFragment>();
		auto RepresentationFragments = Context.GetFragmentView<FMassRepresentationFragment>();

		for (int i=0;i<Context.GetNumEntities();i++)
		{
			auto& VelocityFragment = VelocityFragments[i];
			auto& VehicleComponentFragment = VehicleComponentFragments[i];
			auto& LaneLocationFragment = LaneLocationFragments[i];
			auto& VehicleLocomotion = VehicleLocomotionFragments[i];
			auto& TransformFragment = TransformFragments[i];
			auto Actor = ActorFragments[i].Get();
			auto& Representation = RepresentationFragments[i];
			
			
			switch (Representation.CurrentRepresentation)
			{
			case EMassRepresentationType::HighResSpawnedActor:
			case EMassRepresentationType::LowResSpawnedActor:
				// Only perform logic when we have a BP vehicle
				if (Actor && VehicleComponentFragment.Component.IsValid())
				{
					// PID Controller

					// Calculate error
					FZoneGraphLaneLocation CurrentLanePosition;
					ZoneGraph.CalculateLocationAlongLane(LaneLocationFragment.LaneHandle, LaneLocationFragment.DistanceAlongLane, CurrentLanePosition);

					float LookAheadDistance = PIDControllerFragment.LookAheadDistance;
					FZoneGraphLaneHandle FutureLaneHandle = LaneLocationFragment.LaneHandle;
					float FuturePosition = FMath::Clamp(LaneLocationFragment.DistanceAlongLane + LookAheadDistance, 0, LaneLocationFragment.LaneLength);

					// If our future position is beyond the end of the current lane, look at the next lane if possible
					if (VehicleLocomotion.NextLane.IsValid() && FuturePosition >= LaneLocationFragment.LaneLength)
					{
						FutureLaneHandle = VehicleLocomotion.NextLane;
						FuturePosition = LookAheadDistance + (LaneLocationFragment.LaneLength - LaneLocationFragment.DistanceAlongLane);
					}

					FZoneGraphLaneLocation FutureLanePosition;
					ZoneGraph.CalculateLocationAlongLane(FutureLaneHandle, FuturePosition, FutureLanePosition);

					VehicleLocomotion.Throttle = VehicleLocomotion.ThrottlePIDController.Tick(VehicleLocomotion.DesiredSpeed, VelocityFragment.Value.Length(), PIDControllerFragment.ThrottleSettings);
					VehicleLocomotion.Throttle = FMath::Clamp(VehicleLocomotion.Throttle, -1.0f, 1.0f);
					
					const FVector ToSteeringControlChaseTargetLocal = TransformFragment.GetTransform().InverseTransformPositionNoScale(FutureLanePosition.Position);
					const float NormalizedDeltaAngle = ToSteeringControlChaseTargetLocal.HeadingAngle() / 0.524; // MaxSteeringAngle in radians
					VehicleLocomotion.Steering = FMath::Clamp(VehicleLocomotion.SteeringPIDController.Tick(0.0f, -NormalizedDeltaAngle, PIDControllerFragment.SteeringSettings), -1.0f, 1.0f);
					
					// output
					float Throttle = VehicleLocomotion.Throttle > 0 ? VehicleLocomotion.Throttle : 0;
					float Brake = VehicleLocomotion.Throttle < 0 ? FMath::Abs(VehicleLocomotion.Throttle) : 0;
					
					VehicleComponentFragment.Component->SetSteeringInput(VehicleLocomotion.Steering);
					VehicleComponentFragment.Component->SetThrottleInput(Throttle);
					VehicleComponentFragment.Component->SetBrakeInput(Brake);

					//UE_LOG(LogTemp, Log, TEXT("Throttle: %f | Steering: %f | Speed: %f"), VehicleLocomotion.Throttle, VehicleLocomotion.Steering, VelocityFragment.Value.Length());
				}
				break;
			default:
				break;
			}
		}
	});
}

UVehicleISMVisualizationProcessor::UVehicleISMVisualizationProcessor()
{
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::Representation);
	bRequiresGameThreadExecution = true;
}

void UVehicleISMVisualizationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSubsystemRequirement<UVehicleRepresentationSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FVehicleLocomotionFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddChunkRequirement<FMassVisualizationChunkFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.SetChunkFilter(&FMassVisualizationChunkFragment::AreAnyEntitiesVisibleInChunk);
}

void UVehicleISMVisualizationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		auto RepresentationSubsystem = Context.GetMutableSubsystem<UVehicleRepresentationSubsystem>();
		auto RepresentationLODFragments = Context.GetFragmentView<FMassRepresentationLODFragment>();
		auto RepresentationFragments = Context.GetFragmentView<FMassRepresentationFragment>();
		auto VehicleLocomotionFragments = Context.GetFragmentView<FVehicleLocomotionFragment>();

		FMassInstancedStaticMeshInfoArrayView ISMInfo = RepresentationSubsystem->GetMutableInstancedStaticMeshInfos();
		
		for (int i=0;i<Context.GetNumEntities();i++)
		{
			auto& RepresentationLODFragment = RepresentationLODFragments[i];
			auto& Representation = RepresentationFragments[i];
			auto& VehicleLocomotion = VehicleLocomotionFragments[i];

			if (Representation.CurrentRepresentation == EMassRepresentationType::StaticMeshInstance)
			{
				if (Representation.StaticMeshDescHandle.IsValid())
				{
					FVehicleVisualPackedData PackedData = FVehicleVisualPackedData();
					PackedData.Color = VehicleLocomotion.Color;
						
					ISMInfo[Representation.StaticMeshDescHandle.ToIndex()].AddBatchedCustomData(PackedData, RepresentationLODFragment.LODSignificance);
				}
			}
		}
	});
}
