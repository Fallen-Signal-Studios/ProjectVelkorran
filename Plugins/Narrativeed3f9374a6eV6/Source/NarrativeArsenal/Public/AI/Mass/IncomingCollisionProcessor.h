// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "IncomingCollisionProcessor.generated.h"

/**
 * Keeps track of incoming collisions to the entity
 */
UCLASS()
class NARRATIVEARSENAL_API UIncomingCollisionProcessor : public UMassProcessor
{
	GENERATED_BODY()

	UIncomingCollisionProcessor();
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
