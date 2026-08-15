// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/MassVehicleSpawner.h"

#include "Vehicles/Mass/RoadControlsSpawnPointsGenerator.h"
#include "Vehicles/Mass/TrafficIntersectionAnnotations.h"
#include "MassSpawnerTypes.h"


// Sets default values
AMassVehicleSpawner::AMassVehicleSpawner()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	IntersectionAnnotations = CreateDefaultSubobject<UTrafficIntersectionAnnotations>(TEXT("Intersection Annotations"));
	FMassSpawnDataGenerator Generator = FMassSpawnDataGenerator();
	Generator.GeneratorClass = URoadControlsSpawnPointsGenerator::StaticClass();
	Generator.GeneratorInstance = NewObject<URoadControlsSpawnPointsGenerator>();
	Generator.Proportion = 1.f;
	SpawnDataGenerators = { Generator };
}
 
void AMassVehicleSpawner::SetSpawnCount(int NewCount)
{
	Count = NewCount;
}

