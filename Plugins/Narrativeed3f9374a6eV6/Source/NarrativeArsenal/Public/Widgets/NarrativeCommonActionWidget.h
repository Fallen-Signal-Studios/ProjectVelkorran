// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "CommonActionWidget.h"
#include "NarrativeCommonActionWidget.generated.h"

/**
 * Overriden version of common action widget, need this so we can fix ue5 assert 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeCommonActionWidget : public UCommonActionWidget
{
	GENERATED_BODY()
	
protected: 

	virtual void UpdateActionWidget() override;

};
