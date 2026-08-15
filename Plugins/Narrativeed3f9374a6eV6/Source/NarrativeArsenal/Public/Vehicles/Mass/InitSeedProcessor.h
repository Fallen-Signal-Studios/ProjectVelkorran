// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassEntityQuery.h"
#include "InitSeedProcessor.generated.h"

/**
 * Initializes a FSeedFragment with a random value per entity
 */
UCLASS()
class NARRATIVEARSENAL_API UInitSeedProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

	UInitSeedProcessor();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery{*this};
};
