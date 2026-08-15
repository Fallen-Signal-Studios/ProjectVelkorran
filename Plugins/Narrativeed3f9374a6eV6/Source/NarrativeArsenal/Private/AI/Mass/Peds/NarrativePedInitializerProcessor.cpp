// Copyright Narrative Tools 2025.


#include "AI/Mass/Peds/NarrativePedInitializerProcessor.h"

#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationSubsystem.h"
#include "AI/Mass/Peds/NarrativePedFragments.h"

UNarrativePedInitializerProcessor::UNarrativePedInitializerProcessor()
	: EntityQuery(*this)
{
	ObservedType = FNarrativePedFragment::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
}

void UNarrativePedInitializerProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& Manager)
{
	EntityQuery.AddConstSharedRequirement<FNarrativePedProperties>();
	EntityQuery.AddRequirement<FNarrativePedFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UMassRepresentationSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UNarrativePedInitializerProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& Context)
	{
		const auto& PedProperties = Context.GetConstSharedFragment<FNarrativePedProperties>();
		const auto& NarrativePedFragments = Context.GetMutableFragmentView<FNarrativePedFragment>();
		const auto& RepresentationFragments = Context.GetMutableFragmentView<FMassRepresentationFragment>();
		auto RepresentationSubsystem = Context.GetMutableSubsystem<UMassRepresentationSubsystem>();

		const int32 NumEntities = Context.GetNumEntities();
		for (int32 i = 0; i < NumEntities; ++i)
		{
			auto& NarrativePedFragment = NarrativePedFragments[i];
			auto& RepresentationFragment = RepresentationFragments[i];

			// Applies a random pedestrian asset
			NarrativePedFragment.NarrativePedIndex = FMath::RandRange(0, PedProperties.NarrativePeds.Num() - 1);
			NarrativePedFragment.NarrativePedSeed = FMath::Rand32();
			RepresentationFragment.StaticMeshDescHandle = PedProperties.VisualizationHandles[NarrativePedFragment.NarrativePedIndex];
		}
	});
}
