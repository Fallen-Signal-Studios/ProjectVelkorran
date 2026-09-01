// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "SovPlayerController.generated.h"

/** Project ownership seam for campaign input, HUD, possession, and handoffs. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovPlayerController : public ANarrativePlayerController
{
	GENERATED_BODY()

public:
	ASovPlayerController(const FObjectInitializer& ObjectInitializer);
};
