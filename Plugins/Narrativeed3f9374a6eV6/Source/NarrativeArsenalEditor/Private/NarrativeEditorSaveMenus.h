// Copyright Narrative Tools 2025.

#pragma once

// class that handles the editor save slot options
class FNarrativeEditorSaveMenus
{
	
	// result of UpdateAvailableSaveGames(), if command is index 0 or "none command" it is always true
	TArray<bool> AvailableSaveSlots;
	// this is used as a placeholder for the real world template URL.
	FString PlayWithSaveURLOptions;
	
	// root custom menu widget
	TSharedPtr<SHorizontalBox> RootBox;

	// popup menu
	TWeakPtr<class IMenu> LabelEntryMenu;
	// cached labels
	TArray<FText> SaveSlotLabels;
	// what label is to be edited
	int32 CurrentLabelBeingEdited;

	// combo button selected text
	TSharedPtr<STextBlock> SharedSaveComboButtonText;
	// list of all possible shared saves
	TMap<FName, TArray<FString>> SharedSaves;
	// current selected shared save name
	TSharedPtr<FName> CurrentSharedSave;

	FName SharedSaveLabelName;

public:

	FNarrativeEditorSaveMenus();
	
	void Startup();
	void Shutdown();

private:

	void UpdateOnMenuOpen(UToolMenu* ToolMenu)
	{
		UpdateAvailableSaveGames();
		FindSharedSaves();
	}
	
	void UpdateAvailableSaveGames();
	void UpdateSaveSlotLabels();
	void FindSharedSaves();

	void SetSelectedSaveFromConfig();
	
	bool ReadEditorPerUserSettings(FConfigFile& ConfigOut, FString& FilePath);
	
	// sets the "?SaveName=" option in the editor world url
	void SetSaveUrl(const FString& SaveName, const bool bSaveToConfig, bool bApplyToWorldURL = true);
	
	/* save slots */
	TSharedPtr<SWidget> CreateSaveSlotMenuItemWidget(const int32 OptionIndex);
	FText GetSaveSlotLabel(const int32 OptionIndex) const;
	bool IsExistingSaveSlot(const int32 OptionIndex) const;
	FReply OptionSelected(const int32 OptionIndex);
	ECheckBoxState IsSaveSlotOptionChecked(const int32 OptionIndex) const;
	
	bool HasDefaultLabel(const int32 OptionIndex) const;
	void BeginEditLabelText(const int32 OptionIndex);
	void LabelChanged(const FText& Text, ETextCommit::Type CommitMethod);
	FReply ResetLabelToDefault(const int32 OptionIndex);
	/* save slots */
	
	/* shared saves */
	TSharedPtr<SWidget> CreateSharedSaveSlotsMenuItemWidget();
	TSharedRef<SWidget> ConstructSharedSaveOptions();
	void ConstructSharedSaveOptionSubMenu(FMenuBuilder& MenuBuilder, FName DirName);
	FText GetSelectedSharedSaveText() const;
	void OnSaveSelected(FString SaveName);
	void BeginShareSave(const int32 OptionIndex);
	FReply CancelShareSaveWithLabel(const int32 OptionIndex);
	FReply ShareSaveWithLabel(const int32 OptionIndex);
	void SharedSaveLabelChanged(const FText& Text, ETextCommit::Type CommitMethod);
	void ClearSavePopup();
	/* shared saves */

	/* play using save button */
	void InsertPlayUsingSaveButton();
	void TryPlayUsingSave();
	bool CanPlayUsingSave();
	bool PlayUsingSaveVisible();
	void PieEnd(bool bSimulated);
	void PieCancel();
	/* play using save button */

	bool TryCheckOutOrAddFile(const FString& FilePath);
};
