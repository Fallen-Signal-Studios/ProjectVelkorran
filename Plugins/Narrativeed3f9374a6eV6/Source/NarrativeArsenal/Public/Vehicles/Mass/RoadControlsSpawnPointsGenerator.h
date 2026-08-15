// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityZoneGraphSpawnPointsGenerator.h"
#include "RoadControlsSpawnPointsGenerator.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API URoadControlsSpawnPointsGenerator : public UMassEntityZoneGraphSpawnPointsGenerator
{
	GENERATED_BODY()

	virtual void Generate(UObject& QueryOwner, TConstArrayView<FMassSpawnedEntityType> EntityTypes, int32 Count, FFinishedGeneratingSpawnDataSignature& FinishedGeneratingSpawnPointsDelegate) const override;
};
