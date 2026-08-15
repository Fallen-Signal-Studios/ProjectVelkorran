// Copyright Narrative Tools 2025.

#include "NarrativeEditorSaveMenus.h"
#include "ArsenalSettings.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "LevelEditor.h"
#include "NarrativeArsenalStyle.h"
#include "SaveSystemDeveloperSettings.h"
#include "SourceControlOperations.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/DebuggerCommands.h"
#include "NarrativeToolbar/NarrativeToolbar.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "NarrativeEditorSaveMenus"

// always 0, macro used for readability
#define NONE_OPTION 0

FNarrativeEditorSaveMenus::FNarrativeEditorSaveMenus()
	: CurrentLabelBeingEdited(0),
	CurrentSharedSave(MakeShared<FName>(NAME_None))
{
}

void FNarrativeEditorSaveMenus::Startup()
{	

#if WITH_EDITOR

	// register narrative play menu options
	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* PlayCommandsMenu = ToolMenus? ToolMenus->ExtendMenu("UnrealEd.PlayWorldCommands.PlayMenu") : nullptr;
	if (!PlayCommandsMenu)
	{
		return;
	}
	
	FToolMenuSection& NarrativeSaveSlotSection = PlayCommandsMenu->AddSection("NarrativeSaveSlot",
		LOCTEXT("NarrativeEditorSaveMenus_PlaySectionTitle", "Narrative Save"),
		FToolMenuInsert("LevelEditorPlayModes", EToolMenuInsertType::After));
	
	// HACK: adding an empty section like this, prevents constantly calls each slate frame.
	PlayCommandsMenu->AddDynamicSection("CheckSaveGames", FNewToolMenuDelegate::CreateRaw(this, &FNarrativeEditorSaveMenus::UpdateOnMenuOpen));
	
	/* save slots */
	// generate label array ahead of time
	UpdateSaveSlotLabels();
	
	// create and add save slot options
	const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();
	for (int32 OptionIndex = NONE_OPTION; OptionIndex < ArsenalSettings->NumSaveSlots+1; ++OptionIndex)
	{
		const FString ItemName = "Item" + FString::FromInt(OptionIndex);
		NarrativeSaveSlotSection.AddEntry(
			FToolMenuEntry::InitWidget(*ItemName,
				CreateSaveSlotMenuItemWidget(OptionIndex).ToSharedRef(),
				FText::GetEmpty())
		);
	}
	/* save slots */

	/* shared save slots */
	// look for all shared saves
	FindSharedSaves();

	// get the current set shared save. called before creating the combo to have the initial selected item be set right away
	SetSelectedSaveFromConfig();
	
	// add combo to menu
	NarrativeSaveSlotSection.AddEntry(
		FToolMenuEntry::InitWidget(TEXT("SharedSaves"),
		CreateSharedSaveSlotsMenuItemWidget().ToSharedRef(),
		FText::GetEmpty())
	);
	/* shared save slots */

	InsertPlayUsingSaveButton();

#endif // WITH_EDITOR

}

void FNarrativeEditorSaveMenus::Shutdown()
{
}

void FNarrativeEditorSaveMenus::UpdateAvailableSaveGames()
{
	AvailableSaveSlots.Empty();
	AvailableSaveSlots.Add(true);
	if (const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>())
	{
		for (int32 SlotIndex = 0; SlotIndex < ArsenalSettings->NumSaveSlots; ++SlotIndex)
		{
			const FString SaveOptionName = ArsenalSettings->DefaultSaveName + FString::FromInt(SlotIndex);
			AvailableSaveSlots.Add(UGameplayStatics::DoesSaveGameExist(SaveOptionName, 0));
		}
	}
}

void FNarrativeEditorSaveMenus::UpdateSaveSlotLabels()
{
	const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();
	const int32 TotalOptions = ArsenalSettings->NumSaveSlots+1;
	
	// fill array with generic label names
	SaveSlotLabels.Empty();
	for (int32 LabelIndex = NONE_OPTION; LabelIndex < TotalOptions; ++LabelIndex)
	{
		FText DefaultLabel;
		if (LabelIndex == NONE_OPTION)
		{
			DefaultLabel = LOCTEXT("NarrativeEditorSaveMenus_NoSaveSlot", "No Save Slot");
		}
		else
		{
			DefaultLabel = FText::Format(LOCTEXT("NarrativeEditorSaveMenus_DefaultSlotLabel_Format", "Save Slot {0}"), FText::AsNumber(LabelIndex));
		}
		
		SaveSlotLabels.Add(DefaultLabel);
	}
	
	// look for user set labels 
	const FString PerUserSettingsPath = FPaths::GeneratedConfigDir() / TEXT("EditorPerUserSettings.ini");
	if (!FPaths::FileExists(PerUserSettingsPath))
	{
		// no per user settings, use default
		return;
	}
	
	FString ConfigFilePath;
	FConfigFile PerUserSettings;
	if (!ReadEditorPerUserSettings(PerUserSettings, ConfigFilePath))
	{
		return;
	}
			
	if (!PerUserSettings.Contains("Narrative.Editor.SaveSlots"))
	{
		// no save slot labels, use default
		return;
	}

	// read values
	for (int32 OptionIndex = NONE_OPTION; OptionIndex < TotalOptions; ++OptionIndex)
	{
		const FString SectionKey = "SaveSlotLabel" + FString::FromInt(OptionIndex);
		FString Value;
		PerUserSettings.GetValue(TEXT("Narrative.Editor.SaveSlots"), *SectionKey, Value);
		if (!Value.IsEmpty())
		{
			SaveSlotLabels[OptionIndex] = FText::FromString(Value);
		}
	}
}

void FNarrativeEditorSaveMenus::FindSharedSaves()
{
	SharedSaves.Empty(SharedSaves.Num());

	// add none option
	SharedSaves.Add(NAME_None);

	const USaveSystemDeveloperSettings* SaveSystemSettings = GetDefault<USaveSystemDeveloperSettings>();
	const FString SharedSavesDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir(), SaveSystemSettings->SharedSavesDirectory.Path);

	IFileManager::Get().IterateDirectory(*SharedSavesDir, [this](const TCHAR* Pathname, bool bIsDirectory)
	{
		// when a directory is found, look inside for save files
		if (bIsDirectory)
		{
			FString Path = Pathname, Lhs, FolderName;
			Path.Split("/", &Lhs, &FolderName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

			// iterate files in dir
			TArray<FString> FoundIniFiles;
			IFileManager::Get().FindFiles(FoundIniFiles, Pathname, TEXT("*.sav"));
			for (const FString& File : FoundIniFiles)
			{
				const FString FilePath = Pathname / File;
				if (IFileManager::Get().FileSize(*FilePath) >= 0)
				{
					TArray<FString>& Saves = SharedSaves.FindOrAdd(FName(FolderName));
					Saves.Add(FPaths::GetBaseFilename(File));
				}
			}
		}
		else
		{
			// when a file is encountered and it is a save, check it for add 
			if (FPaths::GetExtension(FString(Pathname), true) == TEXT(".sav"))
			{
				if (IFileManager::Get().FileSize(Pathname) >= 0)
				{
					SharedSaves.FindOrAdd(FName(FPaths::GetBaseFilename(Pathname)));
				}
			}
		}
		return true;
	});
}

void FNarrativeEditorSaveMenus::SetSelectedSaveFromConfig()
{
	FString ConfigFilePath;
	FConfigFile PerUserSettings;
	if (!ReadEditorPerUserSettings(PerUserSettings, ConfigFilePath))
	{
		return;
	}

	FString Value;
	PerUserSettings.GetValue(TEXT("Narrative.Editor.SaveSlots"), TEXT("SelectedSaveSlotName"), Value);

	if (!Value.IsEmpty() && Value != "None")
	{
		SetSaveUrl(Value, false, false);

		if (Value.Contains("SharedSaves"))
		{
			Value.RemoveFromStart(TEXT("SharedSaves/"));
			CurrentSharedSave = MakeShared<FName>(Value);
		}
	}
}


bool FNarrativeEditorSaveMenus::ReadEditorPerUserSettings(FConfigFile& ConfigOut, FString& FilePath)
{	
	// create EditorPerUserSettings, if it does not exist
	FilePath = FPaths::GeneratedConfigDir() / TEXT("EditorPerUserSettings.ini");
	if (!FPaths::FileExists(FilePath))
	{
		if (!FFileHelper::SaveStringToFile(TEXT(""), *FilePath))
		{			
			return false;
		}
	}
	
	ConfigOut.Read(FilePath);
	return true;
}

void FNarrativeEditorSaveMenus::SetSaveUrl(const FString& SaveName, const bool bSaveToConfig, bool bApplyToWorldURL)
{
	if (GEditor)
	{
		const FString SaveOptionName = "?SaveGameName=" + SaveName;
		FString& URL = bApplyToWorldURL? GEditor->UserEditedPlayWorldURL : PlayWithSaveURLOptions;
		if (UGameplayStatics::HasOption(URL, "SaveGameName"))
		{
			// when an option is selected, replace it with the new one, this could mean replacing it with nothing
			const FString CurrentSaveGameName = UGameplayStatics::ParseOption(URL, "SaveGameName");
			const FString CurrentSlotOption = "?SaveGameName=" + CurrentSaveGameName;
			URL = URL.Replace(*CurrentSlotOption, SaveName.IsEmpty()? TEXT("") : *SaveOptionName);
		}
		else if (!SaveName.IsEmpty())
		{
			// add option
			URL.Append(SaveOptionName);
		}

		if (bSaveToConfig)
		{
			FString Path;
			FConfigFile ConfigFile;
			if (!ReadEditorPerUserSettings(ConfigFile, Path))
			{
				return;
			}

			ConfigFile.SetString(TEXT("Narrative.Editor.SaveSlots"), TEXT("SelectedSaveSlotName"), *SaveName);
			
			ConfigFile.Write(Path);
		}				
	}
}

TSharedPtr<SWidget> FNarrativeEditorSaveMenus::CreateSaveSlotMenuItemWidget(const int32 OptionIndex)
{
	constexpr float HorizontalPad = 5.0f;
	constexpr float VerticalPad = 0.0f;
	const bool IsNoneIndex = OptionIndex == NONE_OPTION;

	FText OptionToolTip;
	if (IsNoneIndex)
	{
		OptionToolTip = LOCTEXT("NarrativeEditorSaveMenus_NoneOptionTooltip", "Do not auto load any save");
	}
	else
	{
		OptionToolTip = FText::Format(LOCTEXT("NarrativeEditorSaveMenus_DisplayNameTooltip_Format",
			"Auto load Save Slot {0} on play."),
			FText::AsNumber(OptionIndex)
			);
	}
	
	return SAssignNew(RootBox, SHorizontalBox)
			
		// "radio button"
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "Menu.Button")
			.IsEnabled_Raw(this, &FNarrativeEditorSaveMenus::IsExistingSaveSlot, OptionIndex)
			.OnClicked_Raw(this, &FNarrativeEditorSaveMenus::OptionSelected, OptionIndex)
			.ToolTipText(OptionToolTip)
			[
				SNew(SHorizontalBox)
				
				// "radio button"
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(HorizontalPad, VerticalPad)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "Menu.RadioButton")
					.IsChecked_Raw(this, &FNarrativeEditorSaveMenus::IsSaveSlotOptionChecked, OptionIndex)
					.Visibility(EVisibility::HitTestInvisible)
				]
		
				// save icon
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(HorizontalPad, VerticalPad)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("LevelEditor.Save"))
					.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
					.Visibility(IsNoneIndex? EVisibility::Collapsed : EVisibility::HitTestInvisible)
				]
				
				// label
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(HorizontalPad, VerticalPad)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Raw(this, &FNarrativeEditorSaveMenus::GetSaveSlotLabel, OptionIndex)
					.Visibility(EVisibility::HitTestInvisible)
				]
			]
		]

		// label change button
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(HorizontalPad, VerticalPad)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ContentPadding(0.0f)
			.ButtonStyle(FNarrativeArsenalStyle::Get(), "NarrativeMenu.Button.LabelEdit")
			.Visibility(IsNoneIndex? EVisibility::Collapsed : EVisibility::Visible)
			.ToolTipText(LOCTEXT("NarrativeEditorSaveMenus_EditLabel_ToolTip", "Edit Save Slot Label"))
			.OnReleased_Raw(this, &FNarrativeEditorSaveMenus::BeginEditLabelText, OptionIndex)
		]
		
		// shared save button
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(HorizontalPad, VerticalPad)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ContentPadding(0.0f)
			.ButtonStyle(FNarrativeArsenalStyle::Get(), "NarrativeMenu.Button.Share")
			.Visibility(IsNoneIndex? EVisibility::Collapsed : EVisibility::Visible)
			.ToolTipText(LOCTEXT("NarrativeEditorSaveMenus_ShareSave_ToolTip", "add this save to the shared saves, and set a friendly name for it."))
			.OnReleased_Raw(this, &FNarrativeEditorSaveMenus::BeginShareSave, OptionIndex)
		];
}

FText FNarrativeEditorSaveMenus::GetSaveSlotLabel(const int32 OptionIndex) const
{
	if (OptionIndex == NONE_OPTION)
	{
		return LOCTEXT("NarrativeEditorSaveMenus_NoSaveSlot", "No Save Slot");
	}
	
	const FText FallbackText = FText::Format(LOCTEXT("NarrativeEditorSaveMenus_DefaultSlotLabel_Format", "Save Slot {0}"), FText::AsNumber(OptionIndex));
	return SaveSlotLabels.IsValidIndex(OptionIndex)? SaveSlotLabels[OptionIndex] : FallbackText;
}

bool FNarrativeEditorSaveMenus::IsExistingSaveSlot(const int32 OptionIndex) const
{
	return AvailableSaveSlots.IsValidIndex(OptionIndex)? AvailableSaveSlots[OptionIndex] : false;
}

void FNarrativeEditorSaveMenus::BeginEditLabelText(const int32 OptionIndex)
{
	CurrentLabelBeingEdited = OptionIndex;

	TSharedRef<SWidget> PopupWidget = SNew(SVerticalBox)
		// label entry box
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextEntryPopup)
			.Label(LOCTEXT("NarrativeEditorSaveMenus_NewLabelName", "New Label Name:"))
			.DefaultText(SaveSlotLabels[OptionIndex])
			.OnTextCommitted_Raw(this, &FNarrativeEditorSaveMenus::LabelChanged)
			.SelectAllTextWhenFocused(true)
			.ClearKeyboardFocusOnCommit(false)
		]

		// reset label name
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("NarrativeEditorSaveMenus_ResetToDefault", "Reset To Default"))
			.OnClicked_Raw(this, &FNarrativeEditorSaveMenus::ResetLabelToDefault, OptionIndex)
			.Visibility_Lambda([this, OptionIndex]()
			{
				return HasDefaultLabel(OptionIndex)? EVisibility::Collapsed : EVisibility::Visible;
			})
		];

	// add edit popup
	FSlateApplication& SlateApplication = FSlateApplication::Get();
	LabelEntryMenu = SlateApplication.PushMenu(
		RootBox.ToSharedRef(),
		FWidgetPath(),
		PopupWidget,
		SlateApplication.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
	
}

FReply FNarrativeEditorSaveMenus::OptionSelected(const int32 OptionIndex)
{
	// SharedSavesCombo
	const FString SlotIndex = FString::FromInt(OptionIndex-1);
	const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();
	const FString SaveOptionName = ArsenalSettings->DefaultSaveName + SlotIndex;
	
	SetSaveUrl(OptionIndex == NONE_OPTION? "" : SaveOptionName, true, false);

	// when a labeled save is selected, clear the selected shared save display name
	if (SharedSaveComboButtonText.IsValid())
	{
		SharedSaveComboButtonText->SetText(FText::FromName(NAME_None));
	}
	
	CurrentSharedSave = MakeShared<FName>(NAME_None);
	
	return FReply::Handled();
}

ECheckBoxState FNarrativeEditorSaveMenus::IsSaveSlotOptionChecked(const int32 OptionIndex) const
{
	// if a shared save is in use, then no slot is selected
	if (CurrentSharedSave.IsValid() && !CurrentSharedSave->IsNone())
	{
		return ECheckBoxState::Unchecked;
	}
	
	const int32 SlotIndex = OptionIndex-1;
	if (GEditor)
	{
		const FString& URL = PlayWithSaveURLOptions.IsEmpty()? GEditor->UserEditedPlayWorldURL : PlayWithSaveURLOptions;
		if (OptionIndex == NONE_OPTION)
		{
			// there is a save set, none command cannot be selected option
			return !UGameplayStatics::HasOption(URL, "SaveGameName")? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		const FString SaveNameOption = UGameplayStatics::ParseOption(URL, "SaveGameName");

		// no labeled option should be checked while using a shared save
		if (SaveNameOption.Contains("SharedSaves/"))
		{
			return ECheckBoxState::Unchecked;
		}		
		
		return SaveNameOption.Contains(FString::FromInt(SlotIndex))? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

bool FNarrativeEditorSaveMenus::HasDefaultLabel(const int32 OptionIndex) const
{
	if (OptionIndex == NONE_OPTION)
	{
		// none can not have custom label, so it always has the default one
		return true;
	}
	
	const FString CurrentLabel = GetSaveSlotLabel(OptionIndex).ToString();
	return CurrentLabel == "Save Slot " + FString::FromInt(OptionIndex);
}

void FNarrativeEditorSaveMenus::LabelChanged(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::Type::OnEnter)
	{
		return;
	}
	
	// create EditorPerUserSettings, if it does not exist
	const FString PerUserSettingsPath = FPaths::GeneratedConfigDir() / TEXT("EditorPerUserSettings.ini");
	if (!FPaths::FileExists(PerUserSettingsPath))
	{
		if (!FFileHelper::SaveStringToFile(TEXT(""), *PerUserSettingsPath))
		{
			if (LabelEntryMenu.Pin())
			{
				LabelEntryMenu.Pin()->Dismiss();
			}
			
			return;
		}
	}
	
	FConfigFile PerUserSettings;
	
	PerUserSettings.Read(PerUserSettingsPath);
	PerUserSettings.FindOrAddConfigSection(TEXT("Narrative.Editor.SaveSlots"));

	// update value if there is any change
	FString CurrentValue;
	const FString SectionKey = "SaveSlotLabel" + FString::FromInt(CurrentLabelBeingEdited);
	PerUserSettings.GetValue(TEXT("Narrative.Editor.SaveSlots"), *SectionKey, CurrentValue);
	if (CurrentValue != Text.ToString())
	{
		PerUserSettings.RemoveKeyFromSection(TEXT("Narrative.Editor.SaveSlots"), *SectionKey);
		PerUserSettings.AddToSection(TEXT("Narrative.Editor.SaveSlots"), *SectionKey, *Text.ToString());
		PerUserSettings.Write(PerUserSettingsPath);
	}
	
	if (LabelEntryMenu.Pin())
	{
		LabelEntryMenu.Pin()->Dismiss();
	}
	
	UpdateSaveSlotLabels();
}

FReply FNarrativeEditorSaveMenus::ResetLabelToDefault(const int32 OptionIndex)
{
	FString ConfigFilePath;
	FConfigFile PerUserSettings;
	if (!ReadEditorPerUserSettings(PerUserSettings, ConfigFilePath))
	{
		return FReply::Handled();
	}

	// remove custom label if it exists
	if (PerUserSettings.FindSection(TEXT("Narrative.Editor.SaveSlots")))
	{
		const FString SectionKey = "SaveSlotLabel" + FString::FromInt(OptionIndex);
		if (PerUserSettings.RemoveKeyFromSection(TEXT("Narrative.Editor.SaveSlots"), *SectionKey))
		{
			PerUserSettings.Write(ConfigFilePath);
		}
	}

	if (LabelEntryMenu.Pin())
	{
		LabelEntryMenu.Pin()->Dismiss();
	}

	// now update all slot labels
	UpdateSaveSlotLabels();
	
	return FReply::Handled();
}

TSharedPtr<SWidget> FNarrativeEditorSaveMenus::CreateSharedSaveSlotsMenuItemWidget()
{
	return SNew(SBox)
		.Padding(5.0f, 0.0f)
		[
			SNew(SVerticalBox)

			// menu text
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "Menu.Label")
				.Text(LOCTEXT("NarrativeEditorSaveMenus_SharedSaves", "Shared Saves"))
			]

			// selector
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.5f)
			[
				SNew(SComboButton)
				.OnGetMenuContent_Raw(this, &FNarrativeEditorSaveMenus::ConstructSharedSaveOptions)
				.ButtonContent()
				[
					SAssignNew(SharedSaveComboButtonText, STextBlock)
					.Text(GetSelectedSharedSaveText())
				]
			]
		];
}

TSharedRef<SWidget> FNarrativeEditorSaveMenus::ConstructSharedSaveOptions()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	for (const auto&[SaveOrDir, SavesInDir] : SharedSaves)
	{
		// create combo option
		if (!SavesInDir.IsEmpty())
		{
			MenuBuilder.AddSubMenu(
				FText::FromString(SaveOrDir.ToString()),
				INVTEXT(""),
				FNewMenuDelegate::CreateRaw(this, &FNarrativeEditorSaveMenus::ConstructSharedSaveOptionSubMenu, SaveOrDir));
		}
		// normal option
		else
		{
			MenuBuilder.AddMenuEntry(
				FText::FromName(SaveOrDir),
				INVTEXT(""),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FNarrativeEditorSaveMenus::OnSaveSelected, SaveOrDir.ToString())));
		}
	}
		
	return MenuBuilder.MakeWidget();
}

void FNarrativeEditorSaveMenus::ConstructSharedSaveOptionSubMenu(FMenuBuilder& MenuBuilder, FName DirName)
{
	TArray<FString>& Saves = SharedSaves[DirName];
	for (const FString& SaveName : Saves)
	{
		const FString SaveFile = SaveName;
		MenuBuilder.AddMenuEntry(
			FText::FromString(SaveName),
			INVTEXT(""),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FNarrativeEditorSaveMenus::OnSaveSelected, FString{DirName.ToString() / SaveFile})));
	}
}

FText FNarrativeEditorSaveMenus::GetSelectedSharedSaveText() const
{
	return FText::FromName(CurrentSharedSave.IsValid()? *CurrentSharedSave.Get() : NAME_None);
}

void FNarrativeEditorSaveMenus::OnSaveSelected(FString SaveName)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	// ensure that the default shared saves path exists
	FString DefaultSaveDir = FPaths::ProjectSavedDir() / "SaveGames/SharedSaves";

	// if there is a subfolder in the save name, it is assumed the dir needs to be created
	FString FolderName, FileName;
	if (SaveName.Split("/", &FolderName, &FileName))
	{
		DefaultSaveDir = DefaultSaveDir / FolderName;
	}
	else
	{
		FileName = SaveName;
	}
	
	if (!PlatformFile.DirectoryExists(*DefaultSaveDir))
	{
		PlatformFile.CreateDirectoryTree(*DefaultSaveDir);
	}

	if (SharedSaveComboButtonText.IsValid())
	{
		SharedSaveComboButtonText->SetText(FText::FromString(SaveName));
	}
	
	const USaveSystemDeveloperSettings* SaveSystemSettings = GetDefault<USaveSystemDeveloperSettings>();
	const FString SharedSavesDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir(), SaveSystemSettings->SharedSavesDirectory.Path);
		
	// copy save over to "SharedSaves/" if the project set path is not the default SharedSaves one
	if (SharedSavesDir != DefaultSaveDir)
	{
		const FString SelectedFile = FileName + ".sav";
		const FString SourceSavePath = SharedSavesDir / FolderName / SelectedFile;
		const FString TargetSavePath = DefaultSaveDir / SelectedFile;
		// try copy and set target to read & write
		if (!PlatformFile.CopyFile(*TargetSavePath, *SourceSavePath))
		{
			// could not copy, clear selected save. 9 time out of 10 this is because the user selected none option
			SetSaveUrl("", true, false);
			return;
		}
		PlatformFile.SetReadOnly(*TargetSavePath, false);
	}

	// to keep inline with using existing save functionality, we insert "SharedSaves/" to separate it from other saves 
	SetSaveUrl("SharedSaves" / SaveName, true, false);
}

void FNarrativeEditorSaveMenus::BeginShareSave(const int32 OptionIndex)
{
	TSharedRef<SWidget> PopupWidget = SNew(SVerticalBox)
		// shared save label entry box
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextEntryPopup)
			.Label(LOCTEXT("NarrativeEditorSaveMenus_NewLabelName", "Shared Save Label Name:"))
			.DefaultText(SaveSlotLabels[OptionIndex])
			.OnTextCommitted_Raw(this, &FNarrativeEditorSaveMenus::SharedSaveLabelChanged)
			.SelectAllTextWhenFocused(true)
			.ClearKeyboardFocusOnCommit(false)
		]

		// action buttons
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5.0f)
		[
			SNew(SHorizontalBox)
			// cancel button
			+SHorizontalBox::Slot()
			[
				SNew(SButton)
				.Text(LOCTEXT("NarrativeEditorSaveMenus_Cancel", "Cancel"))
				.OnClicked_Raw(this, &FNarrativeEditorSaveMenus::CancelShareSaveWithLabel, OptionIndex)
			]

			// share button
			+SHorizontalBox::Slot()
			[
				SNew(SButton)
				.Text(LOCTEXT("NarrativeEditorSaveMenus_Share", "Share"))
				.OnClicked_Raw(this, &FNarrativeEditorSaveMenus::ShareSaveWithLabel, OptionIndex)
			]
			
		];

	// add edit popup
	FSlateApplication& SlateApplication = FSlateApplication::Get();
	LabelEntryMenu = SlateApplication.PushMenu(
		RootBox.ToSharedRef(),
		FWidgetPath(),
		PopupWidget,
		SlateApplication.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}

FReply FNarrativeEditorSaveMenus::CancelShareSaveWithLabel(const int32 OptionIndex)
{
	// remove popup and do nothing...
	ClearSavePopup();
	SharedSaveLabelName = NAME_None;
	return FReply::Handled();
}

FReply FNarrativeEditorSaveMenus::ShareSaveWithLabel(const int32 OptionIndex)
{

	const USaveSystemDeveloperSettings* SaveSystemSettings = GetDefault<USaveSystemDeveloperSettings>();
	const FString SharedSavesDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir(), SaveSystemSettings->SharedSavesDirectory.Path);
	const FString DefaultSaveDir = FPaths::ProjectSavedDir() / "SaveGames";
	const FString IntendedSavePath = SharedSavesDir / SharedSaveLabelName.ToString() + ".sav";
	const FString IntendedSaveDir = FPaths::GetPath(IntendedSavePath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// prompt about save with name existing already, and ask if the user would like to overwrite it
	if (FPaths::FileExists(IntendedSavePath))
	{
		const EAppReturnType::Type Response = FMessageDialog::Open(
			EAppMsgType::OkCancel,
			EAppReturnType::Cancel,
			FText::Format(LOCTEXT("NarrativeEditorSaveMenus_ExistingSharedSave", "a Shared Save with the name '{0}' exists already. overwrite?"), FText::FromName(SharedSaveLabelName)),
			LOCTEXT("NarrativeEditorSaveMenus_ExportError_Title", "Shared Save exists already"));
		
		if (Response == EAppReturnType::Type::Ok)
		{
			PlatformFile.DeleteFile(*IntendedSavePath);
		}
		else
		{
			ClearSavePopup();
			// shared save label no longer needed
			SharedSaveLabelName = NAME_None;
			return FReply::Handled();
		}
	}
	
	// check for a dir in the name and create it in the shared saves folder if it does not exist, this will be the category
	if (!PlatformFile.DirectoryExists(*IntendedSaveDir))
	{
		PlatformFile.CreateDirectoryTree(*IntendedSaveDir);
	}

	const FString SlotIndex = FString::FromInt(OptionIndex-1);
	const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();
	const FString SaveOptionName = ArsenalSettings->DefaultSaveName + SlotIndex;

	const FString SaveToShareFilePath = DefaultSaveDir / SaveOptionName + ".sav";
	
	// copy save to the dir
	if (PlatformFile.CopyFile(*IntendedSavePath, *SaveToShareFilePath))
	{
		if (TryCheckOutOrAddFile(IntendedSavePath))
		{
			// display little notification to tell the user the shared save is ready
			FNotificationInfo Info(FText::Format(LOCTEXT("NarrativeEditorSaveMenus_CompleteNotification_Format", "Save '{0}' ready for submit to VCS."), FText::FromName(SharedSaveLabelName)));
			Info.bFireAndForget = true;
			Info.bUseLargeFont = false;
			Info.bUseThrobber = false;
			Info.bUseSuccessFailIcons = true;
			Info.ExpireDuration = 5.0f;

			// Launch notification
			TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
			if (NotificationItem.IsValid())
			{
				NotificationItem->SetCompletionState(SNotificationItem::CS_Success);
			}
		}
	}
	else
	{
		FMessageLog MessageLog("Narrative Pro");
		const FText ErrorText = FText::Format(LOCTEXT("NarrativeEditorSaveMenus_CouldNotCopy_Format", "Could not copy: {0}"), FText::FromString(SaveToShareFilePath));
		MessageLog.Error(ErrorText);
		UE_LOG(LogTemp, Error, TEXT("Shared Save Could Not Copy: %s"), *ErrorText.ToString());
		MessageLog.Open();
	}
		
	ClearSavePopup();
	// shared save label no longer needed
	SharedSaveLabelName = NAME_None;
	return FReply::Handled(); 
}

void FNarrativeEditorSaveMenus::SharedSaveLabelChanged(const FText& Text, ETextCommit::Type CommitMethod)
{

	SharedSaveLabelName = FName{Text.ToString()};
}

void FNarrativeEditorSaveMenus::ClearSavePopup()
{
	if (LabelEntryMenu.Pin())
	{
		LabelEntryMenu.Pin()->Dismiss();
	}
}

void FNarrativeEditorSaveMenus::InsertPlayUsingSaveButton()
{
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		UToolMenu* PlayToolBar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		if (!PlayToolBar)
		{
			return;
		}
		
		// this is also part of the trick to display the save slot name
		FToolMenuSection& PlaySection = PlayToolBar->FindOrAddSection("Play");
		FToolMenuEntry PlayWithSaveMenuEntry = FToolMenuEntry::InitToolBarButton(
			"PlaySave",
			FUIAction(
				FExecuteAction::CreateRaw(this, &FNarrativeEditorSaveMenus::TryPlayUsingSave),
				FCanExecuteAction::CreateRaw(this, &FNarrativeEditorSaveMenus::CanPlayUsingSave),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateLambda([](){ return NarrativeToolbar::HasNoPlayWorld(); })
			),
			FText::GetEmpty(),
			// small missed opportunity here, there isn't a super easy way to display the selected save in real time
			LOCTEXT("NarrativeEditorSaveMenus_PlayUsingSave_ToolTip", "Play this level in the active level editor viewport and auto load the selected save slot on play."),  
			FSlateIcon(FNarrativeArsenalStyle::GetStyleSetName(), "NarrativeMenu.Button.PlayUsingSave")
		);
			
		// insert the button into the play section
		PlayWithSaveMenuEntry.StyleNameOverride = FName("Toolbar.BackplateCenter");
		PlayWithSaveMenuEntry.InsertPosition = FToolMenuInsert("PlaySimulate", EToolMenuInsertType::After);
		PlaySection.AddEntry(PlayWithSaveMenuEntry);
	}
}

void FNarrativeEditorSaveMenus::TryPlayUsingSave()
{
	// apply option to world template URL
	const FString CurrentSaveGameName = UGameplayStatics::ParseOption(PlayWithSaveURLOptions, "SaveGameName");
	SetSaveUrl(CurrentSaveGameName, true, true);

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.StartPlayInEditorSession();

	// listen for exit PIE
	FEditorDelegates::EndPIE.AddRaw(this, &FNarrativeEditorSaveMenus::PieEnd);
	FEditorDelegates::CancelPIE.AddRaw(this, &FNarrativeEditorSaveMenus::PieCancel);
}

bool FNarrativeEditorSaveMenus::CanPlayUsingSave()
{
	return NarrativeToolbar::HasNoPlayWorld() && !PlayWithSaveURLOptions.IsEmpty();
}

bool FNarrativeEditorSaveMenus::PlayUsingSaveVisible()
{
	return NarrativeToolbar::HasNoPlayWorld();
}

void FNarrativeEditorSaveMenus::PieEnd(bool bSimulated)
{
	PieCancel();
}

void FNarrativeEditorSaveMenus::PieCancel()
{
	// remove selected save from world URL template and remove binding
	SetSaveUrl("", false, true);
	FEditorDelegates::EndPIE.RemoveAll(this);
	FEditorDelegates::CancelPIE.RemoveAll(this);
}

bool FNarrativeEditorSaveMenus::TryCheckOutOrAddFile(const FString& FilePath)
{
	// even without source control, nothing can be done with no target paths 
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("AttemptCheckoutInis: no path supplied"));
		return false;
	}
	
	// true by default. if source control is disabled then there is no need to try checking out
	bool CouldCheckout = true;
	
	// if source control is enabled, then make sure that the INIs that are targeted for update can be modified.
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if (SourceControlProvider.IsEnabled())
	{
		CouldCheckout = false;
		
		//ISourceControlModule::Get().QueueStatusUpdate(FilePath);
			
		// only try checkout files that are not checked out currently
		const FSourceControlStatePtr State = SourceControlProvider.GetState(FilePath, EStateCacheUsage::ForceUpdate);
		if (State->IsCheckedOut() || State->IsAdded())
		{
			return true;
		}

		FMessageLog MessageLog("Narrative Pro");
		MessageLog.Flush();

		// try add
		const bool a = State->CanAdd();
		const bool b = State->CanCheckout();
		
		if (a)
		{
			const ECommandResult::Type Result = SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilePath);
			switch (Result)
			{
				// either the file is added, or add was successful
			case ECommandResult::Type::Succeeded: CouldCheckout = true; break;

				// did not add
			case ECommandResult::Type::Failed:
			case ECommandResult::Type::Cancelled:
				const FText ErrorText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_CouldNotMarkForAdd_Format", "Could not mark for add: {0}"), FText::FromString(FilePath));
				MessageLog.Error(ErrorText);
				UE_LOG(LogTemp, Error, TEXT("Could not mark for add: %s"), *ErrorText.ToString());
				MessageLog.Open();
				CouldCheckout = false;
				break;
			}
		}
		else if (b)
		{
			ECommandResult::Type Result;
			
			// try checkout
			if (FilePath.IsEmpty())
			{
				// files are probably already checked out
				Result = ECommandResult::Type::Succeeded;
			}
			else
			{
				Result = SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilePath);
			}
		
			switch (Result)
			{
				// either the file is checked out, or checkout was successful
			case ECommandResult::Type::Succeeded: CouldCheckout = true; break;

				// did not check out
			case ECommandResult::Type::Failed:
			case ECommandResult::Type::Cancelled:
				const FText ErrorText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_CouldNotCheckout_Format", "Could not check out: {0}"), FText::FromString(FilePath));
				MessageLog.Error(ErrorText);
				UE_LOG(LogTemp, Error, TEXT("Could not check out: %s"), *ErrorText.ToString());
				MessageLog.Open();
				CouldCheckout = false;
				break;
			}
		}
		else
		{
			const FText ErrorText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_CouldNotCheckoutOrAdd_Format", "Could not check out or add: {0}"), FText::FromString(FilePath));
			MessageLog.Error(ErrorText);
			UE_LOG(LogTemp, Error, TEXT("Could not check out or add: %s"), *ErrorText.ToString());
			MessageLog.Open();
			CouldCheckout = false;
		}
	}
	
	return CouldCheckout;
}

#undef LOCTEXT_NAMESPACE
