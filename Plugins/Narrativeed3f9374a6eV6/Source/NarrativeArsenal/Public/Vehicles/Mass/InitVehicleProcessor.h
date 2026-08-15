// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassEntityQuery.h"
#include "InitVehicleProcessor.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UInitVehicleProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

	UInitVehicleProcessor();
	
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;

	FMassEntityQuery SetLaneQuery;
};
