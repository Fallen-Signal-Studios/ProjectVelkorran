// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/AutoDestroyProcessor.h"

#include "MassLODFragments.h"
#include "MassSimulationLOD.h"
#include "Vehicles/Mass/VehicleFragments.h"

UAutoDestroyProcessor::UAutoDestroyProcessor() : EntityQuery(FMassEntityQuery(*this))
{
	ObservedType = FMassLowLODTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
}

void UAutoDestroyProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FMassLowLODTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FAutoDestroyTag>(EMassFragmentPresence::All);
}

void UAutoDestroyProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		Context.Defer().DestroyEntities(Context.GetEntities());
	});
}
