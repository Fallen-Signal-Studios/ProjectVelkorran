// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Characters/SovNPCCharacterBase.h"
#include "SovPlacedNPCDefinitionTestFixtures.generated.h"

/** Suppresses content loading, retaining the real SetNPCDefinition/OnRep dispatch. */
UCLASS()
class ASovPlacedNPCDefinitionTestCharacter : public ASovNPCCharacterBase
{
	GENERATED_BODY()
public:
	int32 DefinitionDispatches = 0;
	bool InitializePlaced(FString& Error) { return InitializeAuthoredPlacedDefinition(Error); }
	virtual void OnDefinitionSet_Implementation(UCharacterDefinition* NewDefinition) override { ++DefinitionDispatches; }
};
