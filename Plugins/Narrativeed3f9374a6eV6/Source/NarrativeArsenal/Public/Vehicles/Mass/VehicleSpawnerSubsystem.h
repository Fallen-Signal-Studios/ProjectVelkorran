// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassActorSpawnerSubsystem.h"
#include "VehicleSpawnerSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UVehicleSpawnerSubsystem : public UMassActorSpawnerSubsystem
{
	GENERATED_BODY()

	virtual ESpawnRequestStatus SpawnActor(FConstStructView SpawnRequestView, TObjectPtr<AActor>& OutSpawnedActor, FActorSpawnParameters& InOutSpawnParameters) const override;

	//UFUNCTION()
	//virtual void SpawnedOccupantAppearanceReady(class ANarrativeCharacter* Character);

	UPROPERTY()
	mutable TMap<AActor*, int> ActorToSeatIndexMap;
};
