#pragma once

#include "MassEntityHandle.h"
#include "EntityData.generated.h"

/**
 * Holds entities spawned. This can later be used to destroy created entities.
 */
USTRUCT(BlueprintType)
struct FSpawnedEntities
{
	GENERATED_BODY()

	FSpawnedEntities() = default;

	FSpawnedEntities(const TArray<FMassEntityHandle>& InEntitiesSpawned) : SpawnedEntities(InEntitiesSpawned) {};
	
	TArray<FMassEntityHandle> SpawnedEntities;
};
