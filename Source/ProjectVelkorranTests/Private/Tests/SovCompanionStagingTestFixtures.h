// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "SovCompanionStagingTestFixtures.generated.h"

/** Real character/component startup and Narrative movement; the asset-loading delay is controlled. */
UCLASS()
class ASovCompanionStagingTestCharacter : public ASovProtagonistCompanionCharacter
{
	GENERATED_BODY()
public:
	ASovCompanionStagingTestCharacter(const FObjectInitializer& Initializer);
	virtual void BeginPlay() override;
	virtual void OnDefinitionSet_Implementation(UCharacterDefinition* NewDefinition) override {}
	virtual void PossessedBy(AController* NewController) override { APawn::PossessedBy(NewController); }
	virtual void SetActorHiddenInGame(bool bNewHidden) override;
	TFunction<void(bool)> DuringVisibilityChange;
};
