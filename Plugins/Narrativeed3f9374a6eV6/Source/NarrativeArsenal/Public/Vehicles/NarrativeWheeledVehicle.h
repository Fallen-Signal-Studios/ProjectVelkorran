// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "NarrativeWheeledVehicle.generated.h"

/**
 * Base class for wheeled vehicles in Narrative Pro. 
 */
UCLASS(abstract, BlueprintType)
class NARRATIVEARSENAL_API ANarrativeWheeledVehicle : public ANarrativeVehicleBase
{
	GENERATED_BODY()
	
public: 

	ANarrativeWheeledVehicle(const FObjectInitializer& ObjectInitializer);

	/** Name of the VehicleMovement. Use this name if you want to use a different class (with ObjectInitializer.SetDefaultSubobjectClass). */
	static FName VehicleMovementComponentName;

	/** Util to get the wheeled vehicle movement component */
	class UChaosVehicleMovementComponent* GetVehicleMovementComponent() const;

protected:


	virtual void PossessedBy(AController* NewController) override;

	/** vehicle simulation component */
	UPROPERTY(Category = Vehicle, VisibleDefaultsOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UChaosVehicleMovementComponent> VehicleMovementComponent;
};
