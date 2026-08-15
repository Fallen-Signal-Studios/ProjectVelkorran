// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeWeaponFX.generated.h"

//Allows blueprints to create instanced weapon FX and use them from a Weapon, Projectile, Gameplay Cue, etc. 
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FInstancedWeaponFX
{
	GENERATED_BODY()

	FInstancedWeaponFX(){};

	//The instanced FX
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadOnly, Category = "Weapon FX")
	TObjectPtr<UNarrativeWeaponFX> WeaponFX;

};

/**
 * Handles spawning some FX, could be for a Weapon, Projectile, Gameplay Cue, etc. 
 * This is a standalone object because we want to decouple FX from any particular class, as many different classes all need to spawn VFX into the world. 
 * 
 * Currently we roll the actual implementation in BP, but have it defined in C++ for flexibility. 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Weapon FX"))
class NARRATIVEARSENAL_API UNarrativeWeaponFX : public UObject
{
	GENERATED_BODY()
	


	// Allows the Object to get a valid UWorld from it's outer - required for spawning VFX into the world. 
	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
			return nullptr;
		}

		UObject* Outer = GetOuter();

		while (Outer)
		{
			UWorld* World = Outer->GetWorld();
			if (World)
			{
				return World;
			}

			Outer = Outer->GetOuter();
		}

		return nullptr;
	}

};
