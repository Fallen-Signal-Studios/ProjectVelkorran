// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "SovNPCCharacterBase.generated.h"

/** Project-owned NPC base with authoritative combat extensions available by default. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovNPCCharacterBase : public ANarrativeNPCCharacter
{
	GENERATED_BODY()

public:
	ASovNPCCharacterBase(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovDismembermentComponent* GetDismembermentComponent() const
	{
		return DismembermentComponent;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovCombatSustainDropComponent* GetCombatSustainDropComponent() const
	{
		return CombatSustainDropComponent;
	}

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovDismembermentComponent> DismembermentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovCombatSustainDropComponent> CombatSustainDropComponent;
};
