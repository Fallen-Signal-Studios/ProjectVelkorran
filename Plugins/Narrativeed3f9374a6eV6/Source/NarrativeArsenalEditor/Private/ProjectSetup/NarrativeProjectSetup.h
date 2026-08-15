// Copyright Narrative Tools 2025.

#pragma once

#include "TickableEditorObject.h"
#include "Misc/DataValidation/Fixer.h"
#include "Widgets/Notifications/SNotificationList.h"

/**
 * a specialised class for narrative projects and their setup
 */
class FNarrativeProjectSetupHandler
{
	// type of difference found
	enum class EDiffType : uint8
	{
		None,
		ValueDifference,
		MissingKey,
		MissingSection,
		MissingINI
	};

	// a container that holds relevant info about a INI
	struct FIniContainer
	{
		// direct path to the INI
		FString SetupPath;

		// target ini path, Project/Config/Config.ini
		FString TargetPath;

		// name of the INI without the .ini extension
		FString ExtStrippedName;

		// the INI as a FConfigFile
		FConfigFile ConfigFile;
	
		// parsed messages from each line per key
		TMap<FName /* key */, FString /* message */> KeyMessages;
		
	};

	// cemented definition for revision collection
	enum class EBackupRevision : int32
	{
		First = -2,
		MostRecent = -1,
	};
	static int32 FirstRevision;
	static int32 MostRecentRevision;

	// handles individual fixes to ini files from the message log 
	struct FIniSingleKeyFixer : UE::DataValidation::IFixer
	{
		EFixApplicability FixerApplied = EFixApplicability::CanBeApplied;
		FString IniPath;
		FString Section;
		FString Key;
		FString NewValue;

		FIniSingleKeyFixer() = default;
		FIniSingleKeyFixer(const FString& InIniPath, const FString& InSection, const FString& InKey, const FString& InNewValue)
			: IniPath(InIniPath), Section(InSection), Key(InKey), NewValue(InNewValue)
		{}

		virtual EFixApplicability GetApplicability(int32 FixIndex) const override { return FixerApplied; }
		virtual FFixResult ApplyFix(int32 FixIndex) override;
	};

	// handles un doing single key changes
	struct FIniSingleKeyUndoFixer final : FIniSingleKeyFixer
	{
		FString OldValue;

		// if supplied, then this undo can only activate when the fixer has been applied
		TSharedPtr<FIniSingleKeyFixer> ConditionalFixer;

		FIniSingleKeyUndoFixer(const FString& InIniPath, const FString& InSection, const FString& InKey,
			const FString& InOldValue, const FString& InNewValue, TSharedPtr<FIniSingleKeyFixer> InConditionalFixer = nullptr)
		: FIniSingleKeyFixer(InIniPath, InSection, InKey, InNewValue),
		OldValue(InOldValue),
		ConditionalFixer(InConditionalFixer)
		{}

		virtual EFixApplicability GetApplicability(int32 FixIndex) const override;
		virtual FFixResult ApplyFix(int32 FixIndex) override;
	};

	
	
	// // Loaded INIs for Narrative Pro - NarrativePro/IniSetups/Default*.ini
	TMap<FString, FIniContainer> IniSetups_AddOrUpdate, IniSetups_Ignore;

	// Loaded INIs for Narrative Pro add-ons - NarrativePro/IniSetups/Default*.ini
	TMap<FString, TMap<FString, FIniContainer>> IniSetups_NarrativeAddOns;
	
	// <project>\Plugins\NarrativePro\Resources\IniSetups\...
	FString IniSetupsPath;

	// any keys in the ignored INIs will be skipped over and ignored.
	FString IniSetupsIngorePath;

	// anything in the add dir, is used for adding or updating sections and keys
	FString IniSetupsToAddPath;

	// INIs currently listed to be updated
	TArray<FString> InisToUpdate;

	// popup notification ptr
	TWeakPtr<SNotificationItem> SettingsNotification;
	
	// popup redirectors updated notification
	TSharedPtr<SNotificationItem> RedirectorsUpdatedNotification;

	// handle for when the check for ini differences should run
	FDelegateHandle OnMapOpenHandle;

	// array of fixers that are currently active
	TArray<TSharedPtr<FIniSingleKeyFixer>> QueuedFixers;

	// cached to prevent needing to check the differences all the time
	bool bProjectInisDiffer = false;

	// when true, the user must restart
	bool bRestartRequired = false;
	
	// Shutdown delegate handle
	FDelegateHandle OnShutdownHandle;

private:

	// resets each queued fixers 
	void ClearFixers();
	
	// Update redirectors for addons when disabled/re-enabled
	void UpdateRedirectorsForAddons();

	// get each INI and add it to the IniSetups map.
	// runs at module start up
	void LoadIniSetups();
	
	// parse all messages for the ini
	static bool LoadIniKeyMessages(FIniContainer& IniContainer);

	// does any difference exist between the loaded INI and the target for the INI
	// if CompareToSetupIni is true, then the setup ini will be used as a filter for the diff
	[[nodiscard]] EDiffType AnyDifference(const FIniContainer& IniContainer, bool CompareToSetupIni = true) const;
	
	// copies the current existing project inis into the project saved dir
	static void CreateIniBackups(const TArray<FString>& IniNames);
	
	// copies the current existing project inis into the project saved dir, from a backup revision
	static bool RestoreIniFromBackup(const FString& SourceIniFileName, int32 Revision = MostRecentRevision);

	// will attempt to check out all target inis supplied by path
	static bool AttemptCheckoutInis(const TArray<FString>& TargetPaths);
	
	// each value in the target INI will be updated to match the source INI
	static bool UpdateIniValuesFromSource(FConfigFile* TargetIni, const FString& SourceSectionName, const FString& SourceKeyName, const FString& SourceValueString, FString& OldValue);

	// take a loaded setup INI and update the desired target INI
	void UpdateOrAddIniFromSetupIni(const FIniContainer& IniContainer, bool GenerateUndoFixers);
	
	// diffs against source and target INIs and supply a fixer for each different value / key
	void LogDifferences(const FConfigFile& SourceIni, const FConfigFile& TargetIni, FMessageLog& MessageLog, const FText& MessagePrefix);

	// callback to try start the INI check process
	void OnMapOpened(const FString& MapName, bool AsTemplate);
	
	// Text that is displayed when we want to update ini files
	FText GetSettingsMismatchText();

	/* popup notification */
	// when called, a popup notification is displayed to the user
	void ProjectSettingsDiffer();
	
	// takes the user to the narrative pro project settings where more options are provided
	void OnOpenProjectSettingsClicked() const;
	
	// dismisses the project settings notification
	void OnSkipClicked();

	// dismisses the project settings notification and adds an editor project setting to hide disable the popup. 
	void OnDoNotRemindMeClicked();
	/* popup notification */

	// display a popup window telling the user a restart is required
	void DisplayOnApplyCompleteRestartPopup();
	
	// Combines pro plugin Ini Add/Update map with addons. Addons take precedence and are overriden based on order
	void CombineIniUpdates(TMap<FString, FIniContainer>& OutCombinedIniMap);
	
	// Returns enabled addons - this also accounts for addons that are not mounted, but may be enabled
	void GetEnabledAddons(TMap<FString, FString>& OutAddonPaths);
	
public:

	// explicitly used for Narrative Pro development
	static bool IsNarrativeDevelopmentProject();
	
	bool DoProjectSettingsMatch() const { return !bProjectInisDiffer; }
	bool IsRestartRequired() const { return bRestartRequired; }
	
	// if NotifyUserOnComplete is false, the editor must be manually restarted after this
	void ApplyNarrativeSettings(const bool NotifyUserOnComplete = true, bool GenerateUndoFixers = false);

	// prints to the message log all the differences between the setup and project INIs with a quick fix option
	// if UseSetupAsSource is false then setup INI differences will be logged
	void LogAllIniDifferences(const bool UseSetupAsSource = true);
	
	void OnShutdown();
	void Startup();
	void ShutDown();
};
