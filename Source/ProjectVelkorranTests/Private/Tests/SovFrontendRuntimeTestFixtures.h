// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Framework/SovPlayerController.h"
#include "UI/SovAccessibilityPresentation.h"
#include "SovFrontendRuntimeTestFixtures.generated.h"

/** Seeds ownership only; frontend producers and presentation consumers use their real implementations. */
UCLASS(Transient, NotBlueprintable)
class ASovFrontendRuntimeController : public ASovPlayerController
{
	GENERATED_BODY()
public:
	ASovFrontendRuntimeController(const FObjectInitializer& Initializer) : Super(Initializer) {}
	void StagePawn(APawn* Pawn) { SetPawn(Pawn); }
};

UCLASS(Transient, NotBlueprintable)
class USovFrontendRuntimePresentation : public USovAccessibilityPresentation
{
	GENERATED_BODY()
public:
	USovFrontendRuntimePresentation(const FObjectInitializer& Initializer) : Super(Initializer) {}
	void Advance(float Delta) { NativeTick(FGeometry(), Delta); }
};
