// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CrosshairWidget.generated.h"

/**
 * Base class for creating a custom crosshair for equipped weapons
 */
UCLASS(Abstract, Blueprintable)
class NARRATIVEARSENAL_API UCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()
};
