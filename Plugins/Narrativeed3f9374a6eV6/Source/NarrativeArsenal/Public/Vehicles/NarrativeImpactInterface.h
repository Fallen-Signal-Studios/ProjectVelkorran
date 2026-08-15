// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NarrativeImpactInterface.generated.h"

/**
 * Actors can implement this interface to respond to impacts such as vehicle hits or explosions. 
 */
UINTERFACE(BlueprintType)
class NARRATIVEARSENAL_API UNarrativeImpactInterface : public UInterface
{
	GENERATED_BODY()
	
};


/**
 * Actors can implement this interface to respond to impacts such as vehicle hits or explosions. 
 */
class NARRATIVEARSENAL_API INarrativeImpactInterface
{
	GENERATED_BODY()

public:

	//Handle what should happen when a vehicle collides with us. 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vehicle Impact Interface")
	void HandleVehicleImpact(class ANarrativeVehicleBase* Vehicle, UPrimitiveComponent* OverlappedComponent, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	virtual void HandleVehicleImpact_Implementation(class ANarrativeVehicleBase* Vehicle, UPrimitiveComponent* OverlappedComponent, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	
	//Handle what should happen when a vehicle collides with us. 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Vehicle Impact Interface")
	void HandleExplosionImpact(class UNarrativeAbilitySystemComponent* ExplosionCauser, const FVector& ExplosionLocation, const float IntendedDamage);
	virtual void HandleExplosionImpact_Implementation(class UNarrativeAbilitySystemComponent* ExplosionCauser, const FVector& ExplosionLocation, const float IntendedDamage);
};

