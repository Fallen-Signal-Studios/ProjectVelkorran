// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "SovPassiveResourceOwnershipTestFixtures.generated.h"

/** Makes a real same-avatar canonical ASC handoff available to ownership tests. */
UCLASS(Transient, NotBlueprintable)
class ASovPassiveResourceOwnershipTestCharacter : public ASovAxiomRuntimeTestCharacter
{
	GENERATED_BODY()
public:
	void AdoptAbilitySystemForTest(UNarrativeAbilitySystemComponent* NewAbilitySystem)
	{
		AbilitySystemComponent = NewAbilitySystem;
		AttributeSetBase = const_cast<UNarrativeAttributeSetBase*>(NewAbilitySystem->GetSet<UNarrativeAttributeSetBase>());
	}
};