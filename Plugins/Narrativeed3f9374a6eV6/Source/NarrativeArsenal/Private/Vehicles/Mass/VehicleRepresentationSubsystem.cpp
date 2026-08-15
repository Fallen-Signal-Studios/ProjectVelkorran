// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/VehicleRepresentationSubsystem.h"

#include "Vehicles/Mass/VehicleSpawnerSubsystem.h"
#include "Engine/World.h"

void UVehicleRepresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UVehicleSpawnerSubsystem::StaticClass());

	Super::Initialize(Collection);

	ActorSpawnerSubsystem = UWorld::GetSubsystem<UVehicleSpawnerSubsystem>(GetWorld());
}

void UVehicleRepresentationSubsystem::AddManagedEntity(const FMassEntityHandle& Entity)
{
	++(HandledMassAgents.FindOrAdd(Entity));
}
