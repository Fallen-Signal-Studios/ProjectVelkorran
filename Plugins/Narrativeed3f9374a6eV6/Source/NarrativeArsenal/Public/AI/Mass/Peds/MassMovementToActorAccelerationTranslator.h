// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassTranslator.h"
#include "MassEntityQuery.h"
#include "MassMovementToActorAccelerationTranslator.generated.h"

/**
 * Translator that sends movement acceleration data to the actor CMC. The functionality is similar to when enabling acceleration for AI paths.
 */
UCLASS()
class NARRATIVEARSENAL_API UMassMovementToActorAccelerationTranslator : public UMassTranslator
{
	GENERATED_BODY()
public:
	UMassMovementToActorAccelerationTranslator();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& Manager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
