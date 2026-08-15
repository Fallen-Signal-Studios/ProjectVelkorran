// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/WeaponVisual.h"
#include "FirearmWeaponVisual.generated.h"

/**
 * Base class for firearm weapon visual that need aim down sights 
 */
UCLASS()
class NARRATIVEARSENAL_API AFirearmWeaponVisual : public AWeaponVisual
{
	GENERATED_BODY()
	
public: 

	AFirearmWeaponVisual(const FObjectInitializer& ObjectInitializer);

	//Return the FVector we need to camera to line up with when we're aiming
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon Visual")
	FVector GetADSLocation() const; 
	virtual FVector GetADSLocation_Implementation() const;

	//Return the location our ADS needs to go to, relative to the root bone 
	UFUNCTION(BlueprintPure, Category = "Weapon Visual")
	FVector GetADSRelativeLocation() const;

	/** We'll look on the weapon mesh for a socket with this name to provide the ADS location */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character Visual")
	FName ADSSightSocketName;

};
