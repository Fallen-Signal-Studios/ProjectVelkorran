#pragma once
#include "MassObserverProcessor.h"
#include "MassEntityQuery.h"
#include "NarrativePedInitializerProcessor.generated.h"

UCLASS()
class UNarrativePedInitializerProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

	UNarrativePedInitializerProcessor();
	
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& Manager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
