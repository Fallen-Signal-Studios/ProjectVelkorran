// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassActorSpawnerSubsystem.h"
#include "MassPedSpawnerSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UMassPedSpawnerSubsystem : public UMassActorSpawnerSubsystem
{
	GENERATED_BODY()
	friend struct FNarrativeMassParticipantBridgeTestAccess;

	virtual ESpawnRequestStatus SpawnActor(FConstStructView SpawnRequestView, TObjectPtr<AActor>& OutSpawnedActor, FActorSpawnParameters& InOutSpawnParameters) const override;
};
