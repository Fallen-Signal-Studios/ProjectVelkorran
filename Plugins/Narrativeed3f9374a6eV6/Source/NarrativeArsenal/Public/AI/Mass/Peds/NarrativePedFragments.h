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
struct NARRATIVEARSENAL_API FNarrativePedFragment : public FMassFragment
{
	GENERATED_BODY()

	int32 NarrativePedIndex;

	int32 NarrativePedSeed;
};

/**
 * Identity of a project-owned representation. These entities deliberately do not
 * contain FNarrativePedFragment/FNarrativePedProperties: they must never choose a
 * random NPC definition or create a second gameplay character.
 */
USTRUCT()
struct NARRATIVEARSENAL_API FNarrativeMassParticipantFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> Owner;

	UPROPERTY(Transient)
	FGuid ActorIdentity;

	UPROPERTY(Transient)
	FName ParticipantId;

	UPROPERTY(Transient)
	FName EncounterId;

	UPROPERTY(Transient)
	uint64 Generation = 0;

	// Presentation-only proxies cannot acquire collision or actor ticking from LOD changes.
	UPROPERTY(Transient)
	bool bPresentationOnly = true;

	bool HasValidIdentity() const
	{
		return Owner.IsValid() && ActorIdentity.IsValid() && !ParticipantId.IsNone()
			&& !EncounterId.IsNone() && Generation != 0;
	}

	bool HasSameIdentity(const FNarrativeMassParticipantFragment& Other) const
	{
		return Owner == Other.Owner && ActorIdentity == Other.ActorIdentity
			&& ParticipantId == Other.ParticipantId && EncounterId == Other.EncounterId
			&& Generation == Other.Generation && bPresentationOnly == Other.bPresentationOnly;
	}
};
