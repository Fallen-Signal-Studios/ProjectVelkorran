// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UI/SovHolographicHUDSurface.h"
#include "SovHolographicHUDTestFixtures.generated.h"

/** An authored surface without authored art: it records the frames the HUD publishes to it. */
UCLASS(Transient, NotBlueprintable)
class USovHolographicHUDTestSurface : public USovHolographicHUDSurface
{
	GENERATED_BODY()
public:
	int32 Updates = 0;
	/** The production path calls the Blueprint event; a native fixture observes the applied view instead. */
	const FSovHolographicHUDView& Applied() const { return View; }
};
