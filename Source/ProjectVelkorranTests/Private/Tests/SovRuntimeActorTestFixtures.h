// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "SovRuntimeActorTestFixtures.generated.h"

/** A concrete Narrative controller must supply the stable identity required by its interface. */
UCLASS(Transient, NotBlueprintable)
class ASovRuntimeTestPlayerController : public ANarrativePlayerController
{
	GENERATED_BODY()
public:
	virtual FGuid GetActorGUID_Implementation() const override { return TestActorGuid; }
private:
	FGuid TestActorGuid = FGuid::NewGuid();
};