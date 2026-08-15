// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "AnalogSlider.h"
#include "NarrativeAnalogSlider.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVECOMMONUI_API UNarrativeAnalogSlider : public UAnalogSlider
{
	GENERATED_BODY()

public:

	
	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface
	
};
