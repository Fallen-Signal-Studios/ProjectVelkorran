// Copyright Narrative Tools 2024. 


#include "Widgets/NarrativeSpinBox.h"
#include "NarrativeUIDeveloperSettings.h"

TSharedRef<SWidget> UNarrativeSpinBox::RebuildWidget()
{

	if (UNarrativeUIDeveloperSettings* Settings = GetMutableDefault<UNarrativeUIDeveloperSettings>())
	{
		SetForegroundColor(Settings->UIPrimaryColor);

		FSpinBoxStyle NewStyle = GetWidgetStyle();

		NewStyle.BackgroundBrush.TintColor = FSlateColor(Settings->UIInvertColor);
		NewStyle.ActiveBackgroundBrush.TintColor = FSlateColor(Settings->UIInvertAccentColor);
		NewStyle.HoveredBackgroundBrush.TintColor = FSlateColor(Settings->UIInvertColor);
		NewStyle.ActiveFillBrush.TintColor = FSlateColor(Settings->UIInvertAccentColor);
		NewStyle.HoveredFillBrush.TintColor = FSlateColor(Settings->UIInvertColor);
		NewStyle.InactiveFillBrush.TintColor = FSlateColor(Settings->UIInvertAccentColor);

		SetWidgetStyle(NewStyle);
	}

	return Super::RebuildWidget();

}
