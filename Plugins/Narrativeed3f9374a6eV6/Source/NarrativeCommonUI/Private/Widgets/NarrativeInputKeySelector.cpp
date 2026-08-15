// Copyright Narrative Tools 2025.


#include "Widgets/NarrativeInputKeySelector.h"
#include "NarrativeUIDeveloperSettings.h"

TSharedRef<SWidget> UNarrativeInputKeySelector::RebuildWidget()
{

	if (UNarrativeUIDeveloperSettings* Settings = GetMutableDefault<UNarrativeUIDeveloperSettings>())
	{
		FButtonStyle NewStyle = GetButtonStyle();
		
		NewStyle.Normal.TintColor = FSlateColor(Settings->UIPrimaryColor);
		NewStyle.Hovered.TintColor = FSlateColor(Settings->UIInvertColor);
		NewStyle.Pressed.TintColor = FSlateColor(Settings->UIPrimaryColor);
		NewStyle.NormalForeground = FSlateColor(Settings->UIPrimaryColor);
		NewStyle.HoveredForeground = FSlateColor(Settings->UIPrimaryColor);
		NewStyle.PressedForeground = FSlateColor(Settings->UIPrimaryColor);

		SetButtonStyle(NewStyle);
	}

	return Super::RebuildWidget();

}
