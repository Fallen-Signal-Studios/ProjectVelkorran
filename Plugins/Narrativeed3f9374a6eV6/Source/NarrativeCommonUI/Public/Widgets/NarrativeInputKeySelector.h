// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Components/InputKeySelector.h"
#include "NarrativeInputKeySelector.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVECOMMONUI_API UNarrativeInputKeySelector : public UInputKeySelector
{
	GENERATED_BODY()
	
	protected:
    
    	//~ Begin UWidget Interface
    	virtual TSharedRef<SWidget> RebuildWidget() override;
    	//~ End UWidget Interface
    	
};
