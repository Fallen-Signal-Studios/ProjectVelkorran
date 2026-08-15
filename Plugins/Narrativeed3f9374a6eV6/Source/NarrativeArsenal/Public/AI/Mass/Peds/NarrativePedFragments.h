#pragma once
#include "MassEntityTypes.h"
#include "AI/PedNPCDefinition.h"
#include "MassRepresentationTypes.h"

#include "NarrativePedFragments.generated.h"

// Stores shared information about pedestrians
USTRUCT()
struct FNarrativePedProperties : public FMassConstSharedFragment
{
	GENERATED_BODY()

	// The different types of pedestrians that this entity will become
	UPROPERTY(EditAnywhere, Category = "NarrativePedProperties")
	TArray<TSoftObjectPtr<UPedNPCDefinition>> NarrativePeds;

	TArray<FStaticMeshInstanceVisualizationDescHandle> VisualizationHandles;
};

// Defines entity-specific information for pedestrians
USTRUCT()
struct FNarrativePedFragment : public FMassFragment
{
	GENERATED_BODY()

	int32 NarrativePedIndex;

	int32 NarrativePedSeed;
};
