// Copyright Narrative Tools 2025.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FNarrativeProjectSetupHandler;

/*
 * displays the status of the project settings when compared with the right project settings for a Narrative Pro project
 */
class SNarrativeProjectSetupNotice : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SNarrativeProjectSetupNotice) { }
	SLATE_END_ARGS()

	// current project setup handler object
	TSharedPtr<FNarrativeProjectSetupHandler> NarrativeProjectSetup;

	// wraps NarrativeProjectSetup->DoProjectSettingsMatch()
	bool DoProjectSettingsMatch() const;
	
	// wraps NarrativeProjectSetup->RestartRequired()
	bool IsRestartRequired() const;
	
	// wraps NarrativeProjectSetup->IsNarrativeDevelopmentProject()
	bool IsDevelopmentProject() const;

	FText GetStatusText() const;
	FSlateColor GetStatusTextColor() const;

	// both update project settings and log differences buttons use these
	bool ShouldActionButtonsBeEnabled() const;
	EVisibility ShouldActionButtonsBeVisible() const;

	FReply HandleUpdateProjectSettingsButtonClicked();
	FReply HandleLogProjectSettingDifferencesButtonClicked();
	FReply HandleLogSetupSettingDifferencesButtonClicked();
		
	void Construct( const FArguments& InArgs );
	
};
