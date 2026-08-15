// Copyright Narrative Tools 2025.


#include "Widgets/NarrativeAnalogSlider.h"
#include "NarrativeUIDeveloperSettings.h"

TSharedRef<SWidget> UNarrativeAnalogSlider::RebuildWidget()
{

	//Pull foreground from settings instead of designer set value 
	if (UNarrativeUIDeveloperSettings* Settings = GetMutableDefault<UNarrativeUIDeveloperSettings>())
	{
		SetSliderHandleColor(Settings->UIPrimaryColor);
	}
	
	return Super::RebuildWidget();
}
