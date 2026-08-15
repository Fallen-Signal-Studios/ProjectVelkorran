// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "NarrativeCommonHUD.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVECOMMONUI_API UNarrativeCommonHUD : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:

	virtual void NativeConstruct() override;


};
