// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/VehicleObstacleProcessor.h"

#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "Vehicles/Mass/MassVehicleSubsystem.h"
#include "Vehicles/Mass/VehicleFragments.h"

void UVehicleObstacleProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FVehicleObstacleFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSubsystemRequirement<UMassVehicleSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UVehicleObstacleProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		auto& VehicleSubsystem = Context.GetMutableSubsystemChecked<UMassVehicleSubsystem>();
		auto Transforms = Context.GetFragmentView<FTransformFragment>();
		auto VehicleObstacles = Context.GetMutableFragmentView<FVehicleObstacleFragment>();
		auto RadiusFragments = Context.GetFragmentView<FAgentRadiusFragment>();

		for (int i=0;i<Context.GetNumEntities();i++)
		{
			const auto& Location = Transforms[i].GetTransform().GetLocation();
			auto& VehicleObstacle = VehicleObstacles[i];
			const auto& Radius = RadiusFragments[i].Radius;

			auto Extent = FBox::BuildAABB(Location, FVector(Radius)); 
			VehicleObstacle.CellLocation = VehicleSubsystem.VehicleObstacles.Move(Context.GetEntity(i), VehicleObstacle.CellLocation, Extent);
		}
	});
}

UVehicleObstacleInitializer::UVehicleObstacleInitializer()
{
	ObservedType = FVehicleObstacleFragment::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
}

void UVehicleObstacleInitializer::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FVehicleObstacleFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSubsystemRequirement<UMassVehicleSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UVehicleObstacleInitializer::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		auto& VehicleSubsystem = Context.GetMutableSubsystemChecked<UMassVehicleSubsystem>();
		auto Transforms = Context.GetFragmentView<FTransformFragment>();
		auto VehicleObstacles = Context.GetMutableFragmentView<FVehicleObstacleFragment>();
		auto RadiusFragments = Context.GetFragmentView<FAgentRadiusFragment>();

		for (int i=0;i<Context.GetNumEntities();i++)
		{
			const auto& Location = Transforms[i].GetTransform().GetLocation();
			auto& VehicleObstacle = VehicleObstacles[i];
			const auto& Radius = RadiusFragments[i].Radius;
			
			auto Extent = FBox::BuildAABB(Location, FVector(Radius)); 
			VehicleObstacle.CellLocation = VehicleSubsystem.VehicleObstacles.Add(Context.GetEntity(i), Extent);
		}
	});
}

UVehicleObstacleDestructor::UVehicleObstacleDestructor()
{
	ObservedType = FVehicleObstacleFragment::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
}

void UVehicleObstacleDestructor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FVehicleObstacleFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassVehicleSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UVehicleObstacleDestructor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		auto& VehicleSubsystem = Context.GetMutableSubsystemChecked<UMassVehicleSubsystem>();
		auto VehicleObstacles = Context.GetMutableFragmentView<FVehicleObstacleFragment>();

		for (int i=0;i<Context.GetNumEntities();i++)
		{
			auto& VehicleObstacle = VehicleObstacles[i];
			
			VehicleSubsystem.VehicleObstacles.Remove(Context.GetEntity(i), VehicleObstacle.CellLocation);
		}
	});
}
