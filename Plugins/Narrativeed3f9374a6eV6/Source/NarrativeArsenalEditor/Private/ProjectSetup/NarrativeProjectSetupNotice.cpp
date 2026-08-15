// Copyright Narrative Tools 2025.

#include "NarrativeProjectSetupNotice.h"
#include "NarrativeArsenalEditorModule.h"
#include "NarrativeProjectSetup.h"

#define LOCTEXT_NAMESPACE "NarrativeProjectSetupNotice"

bool SNarrativeProjectSetupNotice::DoProjectSettingsMatch() const
{
	return NarrativeProjectSetup.IsValid()? NarrativeProjectSetup->DoProjectSettingsMatch() : true;
}

bool SNarrativeProjectSetupNotice::IsRestartRequired() const
{
	return NarrativeProjectSetup.IsValid()? NarrativeProjectSetup->IsRestartRequired() : true;
}

bool SNarrativeProjectSetupNotice::IsDevelopmentProject() const
{
	return NarrativeProjectSetup.IsValid()? NarrativeProjectSetup->IsNarrativeDevelopmentProject() : false;
}

FSlateColor SNarrativeProjectSetupNotice::GetStatusTextColor() const
{
	if (IsRestartRequired())
	{
		return FLinearColor::Red;
	}
	
	return DoProjectSettingsMatch()? FLinearColor::White : FLinearColor::Yellow;
}

FReply SNarrativeProjectSetupNotice::HandleUpdateProjectSettingsButtonClicked()
{
	if (NarrativeProjectSetup.IsValid())
	{
		if (NarrativeProjectSetup->IsNarrativeDevelopmentProject())
		{
			EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgCategory::Success, EAppMsgType::Type::YesNo,
			LOCTEXT("SNarrativeProjectSetupNotice_RestartPopup",
			"It has been determined that this project is the Narrative Pro Development project.\n"
			"Are you sure you want to apply these settings?\n"
			"If no double check that the INI setups in the \"IniSetups\" folder are up to date."),
			LOCTEXT("SNarrativeProjectSetupNotice_ApplyNarrativeProSettings_Title", "Apply Narrative Pro Settings"));

			if (DialogResult == EAppReturnType::Type::No)
			{
				return FReply::Unhandled();
			}
		}
		
		NarrativeProjectSetup->ApplyNarrativeSettings(true, true);
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FText SNarrativeProjectSetupNotice::GetStatusText() const
{
	if (IsRestartRequired())
	{
		return LOCTEXT("SNarrativeProjectSetupNotice_RestartRequired", "The Editor needs to be restarted.");
	}
	
	if (!DoProjectSettingsMatch())
	{
		return IsDevelopmentProject()?
		INVTEXT("INI setups do not match Narrative Pro Development project settings.")
		:
		LOCTEXT("SNarrativeProjectSetupNotice_ProjectSettingsDoNotMatchNotifyText", "Some Project Settings do not match Narrative Pro settings.");
	}
	
	return LOCTEXT("SNarrativeProjectSetupNotice_ProjectSettingsMatchNotifyText", "Project Settings match Narrative Pro settings.");
	
}

FReply SNarrativeProjectSetupNotice::HandleLogProjectSettingDifferencesButtonClicked()
{
	NarrativeProjectSetup->LogAllIniDifferences(true);
	return FReply::Handled();
}

FReply SNarrativeProjectSetupNotice::HandleLogSetupSettingDifferencesButtonClicked()
{
	NarrativeProjectSetup->LogAllIniDifferences(false);
	return FReply::Handled();
}

bool SNarrativeProjectSetupNotice::ShouldActionButtonsBeEnabled() const
{
	return !DoProjectSettingsMatch() && !IsRestartRequired();
}

EVisibility SNarrativeProjectSetupNotice::ShouldActionButtonsBeVisible() const
{
	return DoProjectSettingsMatch() || IsRestartRequired()? EVisibility::Hidden : EVisibility::Visible;
}

void SNarrativeProjectSetupNotice::Construct(const FArguments& InArgs)
{
	if (!NarrativeProjectSetup.IsValid())
	{
		NarrativeProjectSetup = FNarrativeArsenalEditorModule::GetProjectSetup();
	}
	
	constexpr int Padding = 8;
	ChildSlot
	[
		SNew(SBorder)
		.Padding(8)
		.BorderImage(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button").Normal)
		[
			SNew(SHorizontalBox)
			
			// warning icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(Padding)
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Warning"))
				.Visibility_Lambda([this]()
				{
					return DoProjectSettingsMatch() || !IsRestartRequired()? EVisibility::Hidden : EVisibility::Visible;
				})
			]

			// notice text
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(FMargin(0.f, Padding, Padding, Padding))
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Raw(this, &SNarrativeProjectSetupNotice::GetStatusText)
				.ColorAndOpacity_Raw(this, &SNarrativeProjectSetupNotice::GetStatusTextColor)
			]

			// update settings button
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0))
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("NarrativeProjectSetupNotice_UpdateProjectSettings", "Update Project Settings"))
				.ToolTipText(LOCTEXT("NarrativeProjectSetupNotice_UpdateProjectSettings_ToolTip", "updates the project to match the required Narrative Pro project settings, and then restarts."))
				.IsEnabled_Raw(this, &SNarrativeProjectSetupNotice::ShouldActionButtonsBeEnabled)
				.OnClicked(this, &SNarrativeProjectSetupNotice::HandleUpdateProjectSettingsButtonClicked)
				.Visibility_Raw(this, &SNarrativeProjectSetupNotice::ShouldActionButtonsBeVisible)
			]

			// print project settings miss match button
			+ SHorizontalBox::Slot()
			.Padding(Padding, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("NarrativeProjectSetupNotice_LogProjectSettingsDifferences", "Log Project Settings Differences"))
				.ToolTipText(LOCTEXT("NarrativeProjectSetupNotice_LogSettingDifferences_ToolTip", "Prints each setting that does not match to the Message Log for manual correction."))
				.IsEnabled_Raw(this, &SNarrativeProjectSetupNotice::ShouldActionButtonsBeEnabled)
				.OnClicked(this, &SNarrativeProjectSetupNotice::HandleLogProjectSettingDifferencesButtonClicked)
				.Visibility_Raw(this, &SNarrativeProjectSetupNotice::ShouldActionButtonsBeVisible)
			]
			
			// print setup settings miss match button
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, Padding, 0.0f)
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(LOCTEXT("NarrativeProjectSetupNotice_DevLogSetupINIDifferencesToProject Settings", "Dev Log Setup INI Differences To Project Settings"))
				.ToolTipText(LOCTEXT("NarrativeProjectSetupNotice_LogSettingDifferences_ToolTip", "Prints each setting that does not match to the Message Log for manual correction."))
				.IsEnabled_Raw(this, &SNarrativeProjectSetupNotice::IsDevelopmentProject)
				.OnClicked(this, &SNarrativeProjectSetupNotice::HandleLogSetupSettingDifferencesButtonClicked)
				.Visibility_Lambda([this]()
				{
					return ShouldActionButtonsBeVisible() == EVisibility::Visible && IsDevelopmentProject()? EVisibility::Visible : EVisibility::Collapsed;
				})
			]
			
		]
	];
}

#undef LOCTEXT_NAMESPACE