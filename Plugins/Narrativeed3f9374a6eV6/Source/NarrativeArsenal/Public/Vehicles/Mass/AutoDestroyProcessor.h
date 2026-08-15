// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassEntityQuery.h"
#include "AutoDestroyProcessor.generated.h"

/**
 * Observer which automatically destroys entities when they are at Low LOD and have the Auto Destroy tag
 */
UCLASS()
class NARRATIVEARSENAL_API UAutoDestroyProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

	UAutoDestroyProcessor();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
