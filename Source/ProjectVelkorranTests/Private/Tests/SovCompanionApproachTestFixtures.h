// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "SovCompanionApproachTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class ASovCompanionApproachTestSelene : public ASovHandoffRuntimeTestPawn
{
	GENERATED_BODY()
public:
	ASovCompanionApproachTestSelene(const FObjectInitializer& Initializer) : Super(Initializer) {}
	virtual FGameplayTag GetProtagonistIdentityTag() const override;
};

/** Supplies the missing content/visual-ready boundary only. Native proxy preparation,
 * kit copying, resources, AI controller, activity owner and commit run unchanged. */
UCLASS(Transient, NotBlueprintable)
class ASovCompanionApproachTestProxy : public ASovProtagonistCompanionCharacter
{
	GENERATED_BODY()
public:
	ASovCompanionApproachTestProxy(const FObjectInitializer& Initializer) : Super(Initializer) {}
protected:
	virtual void OnDefinitionSet_Implementation(class UCharacterDefinition* NewDefinition) override;
};
