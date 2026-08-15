// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassRepresentationSubsystem.h"
#include "VehicleRepresentationSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UVehicleRepresentationSubsystem : public UMassRepresentationSubsystem
{
	GENERATED_BODY()

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

public:
	virtual void AddManagedEntity(const FMassEntityHandle& Entity);
};
