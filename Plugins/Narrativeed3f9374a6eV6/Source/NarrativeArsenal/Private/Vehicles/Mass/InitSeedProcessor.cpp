// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/InitSeedProcessor.h"

#include "MassExecutionContext.h"
#include "Vehicles/Mass/VehicleFragments.h"

UInitSeedProcessor::UInitSeedProcessor()
{
	ObservedType = FSeedFragment::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
}

void UInitSeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FSeedFragment>(EMassFragmentAccess::ReadWrite);
}

void UInitSeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		auto SeedFragments = Context.GetMutableFragmentView<FSeedFragment>();

		for (int i=0;i<Context.GetNumEntities();i++)
		{
			auto& SeedFragment = SeedFragments[i];

			// Only randomize if the value is left as default
			if (SeedFragment.Seed == 0)
			{
				SeedFragment.Seed = FMath::Rand32();
				SeedFragment.Stream = FRandomStream(SeedFragment.Seed);
			}
		}
	});
}
