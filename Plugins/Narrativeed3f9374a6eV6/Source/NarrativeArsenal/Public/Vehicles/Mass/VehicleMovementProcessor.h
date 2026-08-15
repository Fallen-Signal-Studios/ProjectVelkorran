// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "VehicleMovementProcessor.generated.h"

/**
 * This processor manages vehicle movement along the zone graph. By taking in local obstacles, speed, etc, the processor will
 * move the vehicle along the lane and pick new lanes when reaching the end of the current one.
 */
UCLASS()
class NARRATIVEARSENAL_API UVehicleMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

	UVehicleMovementProcessor();
	
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	static void CalculateSpeedFromObstacle(float DistanceToObstacle, float MinDistanceToObstacle, float BreakingDistanceToObstacle, float BrakingPower, float
	                                       & Speed);

	FMassEntityQuery MoveVehicleQuery{*this};

	FMassEntityQuery CalculateSpeedQuery{*this};

	FMassEntityQuery CalculateLaneQuery{*this};

	FMassEntityQuery NextVehicleQuery{*this};
};
