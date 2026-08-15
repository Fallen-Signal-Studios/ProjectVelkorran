// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassTranslator.h"
#include "MassEntityQuery.h"
#include "VehicleVisualizationProcessor.generated.h"

// Packed data that will be sent to the material for calculating color and other effects of the vehicle
USTRUCT()
struct FVehicleVisualPackedData
{
	GENERATED_BODY()

	FVehicleVisualPackedData() = default;

	// Vehicle material is already using index 0 for a dirt-like effect, this simply sets that value to 0
	UPROPERTY()
	float Dirt = 0.f;
	
	UPROPERTY()
	float Color = 0.f;
};

/**
 * Manages visualization of vehicles within the world. Mainly for high-res vehicle bp and synchronization between mass and BP.
 */
UCLASS()
class NARRATIVEARSENAL_API UVehicleVisualizationProcessor : public UMassTranslator
{
	GENERATED_BODY()

	UVehicleVisualizationProcessor();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery{*this};
	FMassEntityQuery SyncVelocityQuery{*this};
	
};

UCLASS()
class UVehicleISMVisualizationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UVehicleISMVisualizationProcessor();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery{*this};
};
