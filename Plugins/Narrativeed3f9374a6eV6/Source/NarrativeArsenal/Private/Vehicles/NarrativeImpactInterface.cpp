// Copyright Narrative Tools 2025.


#include "Vehicles/NarrativeImpactInterface.h"

void INarrativeImpactInterface::HandleVehicleImpact_Implementation(class ANarrativeVehicleBase* Vehicle, UPrimitiveComponent* OverlappedComponent, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
}

void INarrativeImpactInterface::HandleExplosionImpact_Implementation(class UNarrativeAbilitySystemComponent* ExplosionCauser, const FVector& ExplosionLocation, const float IntendedDamage)
{
	
}