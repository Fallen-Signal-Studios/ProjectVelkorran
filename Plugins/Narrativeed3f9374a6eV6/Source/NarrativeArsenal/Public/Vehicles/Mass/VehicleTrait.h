// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassCommonFragments.h"
#include "MassEntityTraitBase.h"
#include "VehicleFragments.h"
#include "VehicleTrait.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UVehicleTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Vehicle Trait")
	FAgentRadiusFragment Radius;

	UPROPERTY(EditAnywhere, Category = "Vehicle Trait")
	FVehiclePIDControllerFragment PIDControllerSettings;

	UPROPERTY(EditAnywhere, Category = "Vehicle Trait")
	FVehicleSettingsFragment VehicleSettingsFragment;

	// Optionally define a seed for the spawned entities. If left at 0, a random number will be chosen for each entity.
	UPROPERTY(EditAnywhere, Category = "Vehicle Trait")
	FSeedFragment SeedFragment = FSeedFragment();

	// Defines the passengers that will be within the vehicle
	UPROPERTY(EditAnywhere, Category = "Vehicle Trait")
	FPassengerSettingsFragment Passengers;
};
