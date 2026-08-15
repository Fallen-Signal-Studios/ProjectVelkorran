// Copyright Narrative Tools 2025.


#include "Vehicles/NarrativeWheeledVehicle.h"
#include "ChaosVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "UnrealFramework/NarrativePlayerController.h"

FName ANarrativeWheeledVehicle::VehicleMovementComponentName(TEXT("VehicleMovementComp"));

class UChaosVehicleMovementComponent* ANarrativeWheeledVehicle::GetVehicleMovementComponent() const
{
	return VehicleMovementComponent;
}

void ANarrativeWheeledVehicle::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	//If we're a player controller, force reverse as brake since mass turns this off
	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(NewController))
	{
		if (VehicleMovementComponent)
		{
			VehicleMovementComponent->bReverseAsBrake = true;
		}
	}
}

ANarrativeWheeledVehicle::ANarrativeWheeledVehicle(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	VehicleMovementComponent = CreateDefaultSubobject<UChaosVehicleMovementComponent, UChaosWheeledVehicleMovementComponent>(VehicleMovementComponentName);
	VehicleMovementComponent->SetIsReplicated(true); // Enable replication by default
	VehicleMovementComponent->UpdatedComponent = Mesh;
}
