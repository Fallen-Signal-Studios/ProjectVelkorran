// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SovRuntimeObjectTestFixtures.generated.h"

/** Concrete identity for weak ownership and receipt tests; UObject itself is abstract. */
UCLASS(Transient, NotBlueprintable)
class USovRuntimeTestIdentity : public UObject
{
	GENERATED_BODY()
};