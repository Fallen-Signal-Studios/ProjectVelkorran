// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "VehicleObstacleProcessor.generated.h"

/**
 * Handles updating the obstacle hash grid for vehicle avoidance
 */
UCLASS()
class NARRATIVEARSENAL_API UVehicleObstacleProcessor : public UMassProcessor
{
	GENERATED_BODY()

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery{*this};
};

/**
 * Initializes the obstacle entity so that we can query and update its location within the hash grid
 */
UCLASS()
class UVehicleObstacleInitializer : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UVehicleObstacleInitializer();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery{*this};
};

/**
 * Cleans up the obstacle entity so that it is no longer updated and available within the hash grid
 */
UCLASS()
class UVehicleObstacleDestructor : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UVehicleObstacleDestructor();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery{*this};
};
