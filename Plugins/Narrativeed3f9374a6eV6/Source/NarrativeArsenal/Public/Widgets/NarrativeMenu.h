// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "NarrativeActivatableWidget.h"
#include "NarrativeMenu.generated.h"

/**
 * Base class for menus - screens that can be activated and show some content, and closed later. Added to the gameplay HUD. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeMenu : public UNarrativeActivatableWidget
{
	GENERATED_BODY()
	
	UNarrativeMenu();

};
