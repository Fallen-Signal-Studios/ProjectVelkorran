// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/InitVehicleProcessor.h"

#include "INodeAndChannelMappings.h"
#include "MassExecutionContext.h"
#include "MassZoneGraphNavigationFragments.h"
#include "ZoneGraphQuery.h"
#include "ZoneGraphSubsystem.h"
#include "Vehicles/Mass/InitSeedProcessor.h"
#include "Vehicles/Mass/MassVehicle.h"
#include "Vehicles/Mass/VehicleFragments.h"

UInitVehicleProcessor::UInitVehicleProcessor() : EntityQuery(*this), SetLaneQuery(*this)
{
	ExecutionOrder.ExecuteAfter.Add(UInitSeedProcessor::StaticClass()->GetFName());
	ObservedType = FVehicleLocomotionFragment::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
}

void UInitVehicleProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FSeedFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FPassengerSettingsFragment>();
	
	EntityQuery.AddRequirement<FVehicleLocomotionFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FVehiclePassengersFragment>(EMassFragmentAccess::ReadWrite);

	SetLaneQuery.AddConstSharedRequirement<FVehicleSettingsFragment>();
	SetLaneQuery.AddSubsystemRequirement<UZoneGraphSubsystem>(EMassFragmentAccess::ReadOnly);

	SetLaneQuery.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadWrite);
	SetLaneQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	SetLaneQuery.AddSubsystemRequirement<UMassVehicleSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UInitVehicleProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		auto SeedFragments = Context.GetFragmentView<FSeedFragment>();
		auto VehicleFragments = Context.GetMutableFragmentView<FVehicleLocomotionFragment>();
		auto& PassengerDefinitions = Context.GetConstSharedFragment<FPassengerSettingsFragment>();
		auto VehiclePassengerFragments = Context.GetMutableFragmentView<FVehiclePassengersFragment>();
		
		for (int i=0;i<Context.GetNumEntities();i++)
		{
			auto& SeedFragment = SeedFragments[i];
			auto& VehicleLocomotion = VehicleFragments[i];
			auto& VehiclePassengerFragment = VehiclePassengerFragments[i];

			FRandomStream ColorStream = FRandomStream(SeedFragment.Seed);
			VehicleLocomotion.Color = ColorStream.FRand(); // Mimic same logic on vehicle bp

			// Make sure we have valid seat/npc data before performing npc passenger calculations
			if (PassengerDefinitions.IsValid())
			{
				FVehiclePassengerWeightedSampler PassengerSampler(PassengerDefinitions.PassengerCountProbability);
				PassengerSampler.Initialize();

				int Index = PassengerSampler.GetEntryIndex(SeedFragment.Stream.FRand(), SeedFragment.Stream.FRand());
				
				check(PassengerDefinitions.PassengerCountProbability.IsValidIndex(Index))
				
				int NumPassengers = PassengerDefinitions.PassengerCountProbability[Index].NumPassengers;

				auto GetRandomPassengerDefinition = [&]() -> UNPCDefinition*
				{
					int DefinitionIndex = SeedFragment.Stream.FRandRange(0, PassengerDefinitions.PassengerDefinitions.Num());
					return PassengerDefinitions.PassengerDefinitions[DefinitionIndex];
				};

				// add passengers in sequential order
				for (int P=0;P<NumPassengers;P++)
				{
					VehiclePassengerFragment.Passengers.Add({P, GetRandomPassengerDefinition()});
				}
			}
		}
	});

	SetLaneQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const FVehicleSettingsFragment& VehicleSettings = Context.GetConstSharedFragment<FVehicleSettingsFragment>();
		const UZoneGraphSubsystem& ZoneGraph = Context.GetSubsystemChecked<UZoneGraphSubsystem>();
		
		auto LaneLocationFragments = Context.GetMutableFragmentView<FMassZoneGraphLaneLocationFragment>();
		auto TransformFragments = Context.GetMutableFragmentView<FTransformFragment>();
		UMassVehicleSubsystem& VehicleSubsystem = Context.GetMutableSubsystemChecked<UMassVehicleSubsystem>();
		
		Context.ForEachEntityInChunk([&](FMassExecutionContext& Context, int32 EntityIndex)
		{
			FMassZoneGraphLaneLocationFragment& LaneLocation = LaneLocationFragments[EntityIndex];
			FTransformFragment& TransformFragment = TransformFragments[EntityIndex];

			FTransform& Transform = TransformFragment.GetMutableTransform();
			
			// Find nearest lane if we dont have one
			if (!LaneLocation.LaneHandle.IsValid())
			{
				FBox Bounds = FBox::BuildAABB(Transform.GetLocation(), FVector(VehicleSettings.LaneSearchRadius));

				TFunction<FZoneGraphLaneLocation()> GetLaneClosestDirection = [&]()
				{
					TArray<FZoneGraphLaneHandle> NearbyLanes;
					ZoneGraph.FindOverlappingLanes(Bounds, VehicleSettings.VehicleLaneFilter, NearbyLanes);

					FVector VehicleDirection = Transform.GetRotation().GetForwardVector();
					FZoneGraphLaneLocation BestLaneLocation;
					float BestDot = -1.f;

					// Determine lane based on how similar the directions are
					// We ideally want a lane that is already in the same direction as the vehicle
					// One drawback to this method is we only query the closest location on the lane. This could mean another part of the lane could be better aligned 
					for (const FZoneGraphLaneHandle& NearbyLane : NearbyLanes)
					{
						FZoneGraphLaneLocation NearbyLaneLocation;
						float DistSq;
						ZoneGraph.FindNearestLocationOnLane(NearbyLane, Bounds, NearbyLaneLocation, DistSq);

						float Dot = FVector::DotProduct(VehicleDirection, NearbyLaneLocation.Tangent);
						if (Dot > BestDot) // We want a dot product closest to 1 as that means the directions are similar
						{
							BestLaneLocation = NearbyLaneLocation;
							BestDot = Dot;
						}
					}
					return BestLaneLocation;
				};
				
				TFunction<FZoneGraphLaneLocation()> GetLaneClosestLocation = [&]()
				{
					FZoneGraphLaneLocation BestLaneLocation;
					float DistSq;
					ZoneGraph.FindNearestLane(Bounds, VehicleSettings.VehicleLaneFilter, BestLaneLocation, DistSq);

					return BestLaneLocation;
				};

				// If our rotation has not been set yet, we will assume that the closest lane will be best
				FZoneGraphLaneLocation BestLaneLocation = Transform.GetRotation() == FQuat::Identity ? GetLaneClosestLocation() : GetLaneClosestDirection();

				// Nearest lane not found, could be out of bounds
				if (!BestLaneLocation.IsValid())
				{
					UE_LOG(LogMassVehicle, Verbose, TEXT("Unable to find a lane location for vehicle. Most likely out of bounds, retrying..."))
					return;
				}

				auto ZoneGraphStorage = ZoneGraph.GetZoneGraphStorage(BestLaneLocation.LaneHandle.DataHandle);

				LaneLocation.LaneHandle = BestLaneLocation.LaneHandle;
				LaneLocation.DistanceAlongLane = BestLaneLocation.DistanceAlongLane;
				UE::ZoneGraph::Query::GetLaneLength(*ZoneGraphStorage, LaneLocation.LaneHandle, LaneLocation.LaneLength);

				// Apply transforms immediately the first time
				Transform.SetLocation(BestLaneLocation.Position);
				Transform.SetRotation(BestLaneLocation.Direction.ToOrientationQuat());

				VehicleSubsystem.VehicleEnterLane(BestLaneLocation.LaneHandle, Context.GetEntity(EntityIndex));
			}
		});
	});
}
