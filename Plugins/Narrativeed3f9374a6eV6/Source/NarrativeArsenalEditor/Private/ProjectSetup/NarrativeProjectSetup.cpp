// Copyright Narrative Tools 2025.

#include "NarrativeProjectSetup.h"
#include "ArsenalSettings.h"
#include "ISettingsModule.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "NarrativeArsenalStyle.h"
#include "ProjectDescriptor.h"
#include "SourceControlOperations.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "ToolUtils/ToolUtils.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogNarrativeProjectSetupHandler, All, All);

static TAutoConsoleVariable<bool> CVarDisableProjectSettingsCheckOnStartup(
		TEXT("NarrativePro.DisableProjectSettingsCheckOnStartup"),
		false,
		TEXT("DEPRECATED, REPLACED WITH NARRATIVE PRO SETTINGS OPTION. If true, then Narrative Pro will not check project settings on editor launch."));

static TAutoConsoleVariable<bool> CVarDisableProjectSettingsSetupNotification(
		TEXT("NarrativePro.DisableProjectSettingsSetupNotification"),
		false,
		TEXT("DEPRECATED, REPLACED WITH NARRATIVE PRO SETTINGS OPTION. If true, then the Narrative Pro update project settings notification will not display on editor launch."));

int32 FNarrativeProjectSetupHandler::FirstRevision = static_cast<int32>(EBackupRevision::First);
int32 FNarrativeProjectSetupHandler::MostRecentRevision = static_cast<int32>(EBackupRevision::MostRecent);

// produces "MyConfig_(<Rev Number>).ini"
#define BACKUP_NAME_FORMAT_IMPL(IniName, RevNumber) IniName + "_(" + FString::FromInt(RevNumber) + ")" + ".ini"


// when true, the inis in the add dir are to be updated from the projects ini
// explicitly used for Narrative Pro development
static bool bIsNarrativeDevelopmentProject = false;

#define LOCTEXT_NAMESPACE "NarrativeProjectSetup"

FFixResult FNarrativeProjectSetupHandler::FIniSingleKeyFixer::ApplyFix(int32 FixIndex)
{	
	FConfigFile TargetIni;
	// create INI if need be
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.FileExists(*IniPath))
	{
		FFileHelper::SaveStringToFile(TEXT(""), *IniPath);
	}

	// try check out the INI about to be fixed
	if (!AttemptCheckoutInis({IniPath}))
	{
		const FText CheckoutFailureText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_FixerFailedCheckout_Format",
			"Could not check out {0}"),
			FText::FromString(FPaths::GetCleanFilename(IniPath))
			);

		UE_LOG(LogNarrativeProjectSetupHandler, Error, TEXT("%s"), *CheckoutFailureText.ToString());
		
		return FFixResult::Failure(CheckoutFailureText);
	}

	// fix INI
	TargetIni.Read(*IniPath);	
	if (FString OldValue; UpdateIniValuesFromSource(&TargetIni, Section, Key, NewValue, OldValue))
	{
		// try to write the changes to disk
		if (TargetIni.Write(IniPath))
		{
			FixerApplied = EFixApplicability::Applied;
		}
		else
		{
			// could not write for some reason
			FixerApplied = EFixApplicability::DidNotApply;
			
			const FText FailureToWriteText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_FixerWriteFailed_Format",
			"Could Write \"{0}\" to disk!"),
			FText::FromString(FPaths::GetCleanFilename(IniPath))
			);

			UE_LOG(LogNarrativeProjectSetupHandler, Error, TEXT("%s"), *FailureToWriteText.ToString());
			
			return FFixResult::Failure(FailureToWriteText);
		}
	}
	else
	{
		// could no update for some reason
		FixerApplied = EFixApplicability::DidNotApply;
		
		const FText FailureText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_FixerApplyFailed_Format",
			"Could not apply update to key \"{0}\" for \"{1}\""),
			FText::FromString(Key),
			FText::FromString(FPaths::GetCleanFilename(IniPath))
			);

		UE_LOG(LogNarrativeProjectSetupHandler, Error, TEXT("%s"), *FailureText.ToString());
		return FFixResult::Failure(FailureText);
	}
	
	const FText SuccessText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_FixerApplySuccess_Format",
		"Applied update to key \"{0}\" for \"{1}\""),
		FText::FromString(Key),
		FText::FromString(FPaths::GetCleanFilename(IniPath))
		);

	UE_LOG(LogNarrativeProjectSetupHandler, Log, TEXT("%s"), *SuccessText.ToString());
	return FFixResult::Success(SuccessText);
}

EFixApplicability FNarrativeProjectSetupHandler::FIniSingleKeyUndoFixer::GetApplicability(int32 FixIndex) const
{
	if (ConditionalFixer.IsValid())
	{
		// index does not matter here
		return ConditionalFixer->GetApplicability(INDEX_NONE) == EFixApplicability::Applied? EFixApplicability::CanBeApplied : EFixApplicability::DidNotApply;
	}
	
	return FIniSingleKeyFixer::GetApplicability(FixIndex);
}

FFixResult FNarrativeProjectSetupHandler::FIniSingleKeyUndoFixer::ApplyFix(int32 FixIndex)
{
	FString LastValue;
	FConfigFile TargetIni;
	// create INI if need be
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.FileExists(*IniPath))
	{
		FFileHelper::SaveStringToFile(TEXT(""), *IniPath);
	}

	// fix INI
	TargetIni.Read(*IniPath);

	bool UndoFailed = false;
	// when there is a conditional fixer, check if the key needs to be removed
	if (ConditionalFixer.IsValid())
	{
		if (OldValue.IsEmpty() || OldValue == TEXT("None"))
		{
			TargetIni.RemoveKeyFromSection(*Section, *Key);
		}
		else
		{
			UndoFailed = !TargetIni.RemoveFromSection(*Section, *Key, OldValue);
		}
	}
	else
	{		
		// instead of the new value being used, we revert by using the old value
		UndoFailed = !UpdateIniValuesFromSource(&TargetIni, Section, Key, OldValue, LastValue);
	}
	
	if (UndoFailed)
	{
		// could not update for some reason
		FixerApplied = EFixApplicability::DidNotApply;
		
		const FText FailureText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_UndoFixerApplyFailed_Format",
			"Could not undo update of key \"{0}\" for \"{1}\""),
			FText::FromString(Key),
			FText::FromString(FPaths::GetCleanFilename(IniPath))
			);

		UE_LOG(LogNarrativeProjectSetupHandler, Error, TEXT("%s"), *FailureText.ToString());
		return FFixResult::Failure(FailureText);
	}

	// try to write the changes to disk, error if not able to
	if (!TargetIni.Write(IniPath))
	{
		// could not write for some reason
		FixerApplied = EFixApplicability::DidNotApply;
			
		const FText FailureToWriteText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_UndoFixerWriteFailed_Format",
		"Could Write \"{0}\" to disk!"),
		FText::FromString(FPaths::GetCleanFilename(IniPath))
		);

		UE_LOG(LogNarrativeProjectSetupHandler, Error, TEXT("%s"), *FailureToWriteText.ToString());
			
		return FFixResult::Failure(FailureToWriteText);
	}
	
	const FText SuccessText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_FixerApplySuccess_Format",
		"update to key \"{0}\" for \"{1}\" reset back to \"{2}\""),
		FText::FromString(Key),
		FText::FromString(FPaths::GetCleanFilename(IniPath)),
		FText::FromString(OldValue)
		);

	UE_LOG(LogNarrativeProjectSetupHandler, Log, TEXT("%s"), *SuccessText.ToString());

	// when there is a condition fixer, update it to allow the user to apply the fix again
	if (ConditionalFixer.IsValid())
	{
		ConditionalFixer->FixerApplied = EFixApplicability::CanBeApplied;
	}

	FixerApplied = EFixApplicability::Applied;
	return FFixResult::Success(SuccessText);
}

void FNarrativeProjectSetupHandler::ClearFixers()
{
	for (TSharedPtr<FIniSingleKeyFixer>& Fixer : QueuedFixers)
	{
		Fixer.Reset();
	}
}

void FNarrativeProjectSetupHandler::UpdateRedirectorsForAddons()
{
	// We cannot use GConfig in this case as CoreRedirects gets stripped out
	TArray<FString> Redirects;
	FString EngineIniPath = FPaths::ProjectConfigDir() + TEXT("DefaultEngine.ini");
	
	FConfigFile EngineIni = FConfigFile();
	EngineIni.Read(EngineIniPath);
	
	// Get Package Redirects - We need to include the + sign or the array returns empty
	EngineIni.GetArray(TEXT("CoreRedirects"), TEXT("+PackageRedirects"), Redirects);
	
	bool bUpdatedRedirectors = false;
	for (auto It = Redirects.CreateIterator(); It; ++It)
	{
		FString& Redirect = *It;
		
		auto GetPathArray = [&](const FString& Identifier, FString& OutPath) -> TArray<FString>
		{
			TArray<FString> OutArray;
			FParse::Value(*Redirect, *Identifier, OutPath);
			OutPath.ParseIntoArray(OutArray, TEXT("/"));
			
			return OutArray;
		};
		
		FString OldNamePath, NewNamePath;
		TArray<FString> OldNameArray = GetPathArray(TEXT("OldName="), OldNamePath);
		TArray<FString> NewNameArray = GetPathArray(TEXT("NewName="), NewNamePath);
		
		if (OldNameArray.IsEmpty() || NewNameArray.IsEmpty())
		{
			UE_LOG(LogNarrativeProjectSetupHandler, Error, TEXT("Malformed redirector (%s) - invalid OldName/NewName values"), *Redirect);
			continue;
		}
		
		FString& SourceName = OldNameArray[0];
		FString& TargetName = NewNameArray[0];
		
		// Since this can be run on shutdown - we cannot rely on the plugin manager to give us reliable information
		// However, if the plugin can be enabled by default, the Plugins array will not be reliable either, we need to use a hybrid method
		
		TMap<FString, FString> Addons;
		GetEnabledAddons(Addons);
		
		bool bPluginEnabled = Addons.Contains(SourceName) || Addons.Contains(TargetName);
		
		if (SourceName.Contains("NarrativePro") && TargetName.Contains("NP_"))
		{
			// Check if we are pointing NPro assets to an addon
			UE_LOG(LogNarrativeProjectSetupHandler, Verbose, TEXT("Found redirector for plugin %s"), *TargetName);
			
			// See if the plugin that this redirector is using is disabled/not found
			
			if (!bPluginEnabled)
			{
				// We found a redirector to an invalid plugin - we should fix this for the user now
				FString TempHolder = "_Temp_";
				Redirect = Redirect.Replace(*OldNamePath, *TempHolder);
				Redirect = Redirect.Replace(*NewNamePath, *OldNamePath);
				Redirect = Redirect.Replace(*TempHolder, *NewNamePath);
				
				UE_LOG(LogNarrativeProjectSetupHandler, Log, TEXT("Found redirector for disabled/missing plugin %s. Fixing redirector to %s"), *TargetName, *Redirect)
				bUpdatedRedirectors = true;
			}
		}
		else if (SourceName.Contains("NP_") && TargetName.Contains("NarrativePro"))
		{
			// Check if we are pointing an addon to NPro - this can happen if we previously disabled an addon
			
			// If the plugin is enabled, we want to remove this redirect - as this will break redirects that this addon adds
			if (bPluginEnabled)
			{
				UE_LOG(LogNarrativeProjectSetupHandler, Log, TEXT("Found old redirector for previously disabled addon: %s. Removing redirector: %s"), *SourceName, *Redirect)
				It.RemoveCurrentSwap();
				bUpdatedRedirectors = true;
			}
		}
	}
	
	//@todo try to checkout file - also should show a notification rather than doing this automatically
	// Update ini file
	
	FString DefaultEngineIniPath = FPaths::SourceConfigDir() + TEXT("DefaultEngine.ini");
	if (bUpdatedRedirectors && AttemptCheckoutInis({ DefaultEngineIniPath }))
	{
		EngineIni.SetArray(TEXT("CoreRedirects"), TEXT("+PackageRedirects"), Redirects);
		EngineIni.Write(EngineIniPath);
		
		FNotificationInfo Info(FText(LOCTEXT("NarrativeProjectSetupHandler_RedirectorsUpdated", "Updated Redirectors in DefaultEngine.ini due to missing/added add-ons")));
		Info.bFireAndForget = false;
		Info.bUseLargeFont = false;
		Info.bUseThrobber = false;
		Info.bUseSuccessFailIcons = false;
		Info.Image = FSlateIcon(FNarrativeArsenalStyle::GetStyleSetName(), "NarrativeToolbar.Icon").GetOptionalIcon();
		
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			LOCTEXT("NarrativeProjectSetupHandler_AckRedirectorsUpdated", "Acknowledge"),
			LOCTEXT("NarrativeProjectSetupHandler_AckRedirectorsUpdated_ToolTip", "Acknowledge that redirectors have been updated"), 
			FSimpleDelegate::CreateLambda([&]()
				{
					if (RedirectorsUpdatedNotification.IsValid())
					{
						RedirectorsUpdatedNotification->ExpireAndFadeout();
						RedirectorsUpdatedNotification.Reset();
					}
				}))
			);
	
		RedirectorsUpdatedNotification = FSlateNotificationManager::Get().AddNotification(Info);
		if (RedirectorsUpdatedNotification.IsValid())
		{
			RedirectorsUpdatedNotification->SetCompletionState(SNotificationItem::CS_Pending);
		}
		
		NarrativeToolUtils::NotifyRequiredRestart();
	}
}

void FNarrativeProjectSetupHandler::LoadIniSetups()
{
	// Clear existing ini setups as we may have enabled/disabled addons
	IniSetups_NarrativeAddOns.Empty();
	IniSetups_AddOrUpdate.Empty();
	
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("NarrativePro"));
	IniSetupsPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("IniSetups"));
	IniSetupsToAddPath = IniSetupsPath / TEXT("Add");
	IniSetupsIngorePath = IniSetupsPath / TEXT("Ignore");
	
	// helper lambda
	auto LoadIniContainerFromPath = [](const FString& Dir, TMap<FString, FIniContainer>& IniSetups){
		TArray<FString> FoundIniFiles;
		IFileManager::Get().FindFiles(FoundIniFiles, *Dir, TEXT("*.ini"));
		for (const FString& IniFile : FoundIniFiles)
		{
			UE_LOG(LogNarrativeProjectSetupHandler, Verbose, TEXT("Found ini %s in %s"), *IniFile, *Dir);
			//Find or add ensures that narrative add-ons that want to override anything from the base ini can do so, and will simply overwrite 
			IniSetups.FindOrAdd(IniFile, {});
			IniSetups[IniFile].SetupPath = Dir / IniFile;
			IniSetups[IniFile].ExtStrippedName = FPaths::GetBaseFilename(IniFile);
			IniSetups[IniFile].TargetPath = FPaths::ProjectConfigDir() / IniFile;
			IniSetups[IniFile].ConfigFile.Read(*IniSetups[IniFile].SetupPath);
		
			// collect all messages for keys in this file
			LoadIniKeyMessages(IniSetups[IniFile]);
		}
	};

	LoadIniContainerFromPath(IniSetupsToAddPath, IniSetups_AddOrUpdate);
	LoadIniContainerFromPath(IniSetupsIngorePath, IniSetups_Ignore);
	LoadIniContainerFromPath(IniSetupsIngorePath, IniSetups_Ignore);

	//Find any narrative pro add-ons, we can easily identify these using the NP_ name.
	TMap<FString, FString> Addons;
	GetEnabledAddons(Addons);
	for (auto& Addon  : Addons)
	{
		FString PluginName = Addon.Key;
		UE_LOG(LogNarrativeProjectSetupHandler, Log, TEXT("Found NPro add-on: %s"), *PluginName);

		auto& AddOnSetup = IniSetups_NarrativeAddOns.FindOrAdd(PluginName);
		LoadIniContainerFromPath(FPaths::Combine(Addon.Value, TEXT("Resources"), TEXT("IniSetups"), TEXT("Add")), AddOnSetup);
	}
}

bool FNarrativeProjectSetupHandler::LoadIniKeyMessages(FIniContainer& IniContainer)
{
	TArray<FString> FileLines;
	const bool LoadedLines = FFileHelper::LoadFileToStringArray(FileLines, *IniContainer.SetupPath);
	
	for (const FString& Line : FileLines)
	{
		FString Key, ValueAndMessage, ValueUnused, Message;
		
		// get key
		Line.Split(TEXT("="), &Key, &ValueAndMessage);
			
		// get message, we don't care for the value here
		ValueAndMessage.Split(";;;", &ValueUnused, &Message);
		
		IniContainer.KeyMessages.Add(*Key, Message.TrimStartAndEnd());
	}
	return LoadedLines;
}

FNarrativeProjectSetupHandler::EDiffType FNarrativeProjectSetupHandler::AnyDifference(const FIniContainer& IniContainer, bool CompareToSetupIni) const
{
	// if CompareToSetupIni is true, then our target path is the project config dir
	const FString TargetPath = CompareToSetupIni? FPaths::ProjectConfigDir() : IniSetupsPath;
	
	// check if target exists already
	const FString TargetIniFileName = IniContainer.ExtStrippedName + TEXT(".ini");
	const FString TargetIniPath = TargetPath / *TargetIniFileName;
	if (!IFileManager::Get().FileExists(*TargetIniPath))
	{
		// the difference is that the INI does not already exist, it will be created
		return EDiffType::MissingINI;
	}

	// parse target INI
	FConfigFile TargetIni;
	TargetIni.Read(TargetIniPath);

	// find difference
	for(const auto&[SourceSectionName, SourceSection] : (CompareToSetupIni? IniContainer.ConfigFile : TargetIni))
	{
		// collect ignore INI if it exists
		const FConfigFile* OptionalIgnoreIni = nullptr;
		if (IniSetups_Ignore.Contains(SourceSectionName))
		{
			OptionalIgnoreIni = &IniSetups_Ignore[SourceSectionName].ConfigFile;
		}
		
		for(FConfigSection::TConstIterator SourcePropertyIt(SourceSection); SourcePropertyIt; ++SourcePropertyIt )
		{
			// get source value
			FString SourceKeyName = SourcePropertyIt.Key().ToString();
			const FString SourceValueString = SourcePropertyIt.Value().GetValue();

			// check ignore INI for section
			const FConfigSection* Section = OptionalIgnoreIni? OptionalIgnoreIni->FindSection(SourceSectionName) : nullptr;
			if (Section && Section->Contains(*SourceKeyName))
			{
				continue;
			}

			// get the right target section
			const FConfigSection* TargetSection;
			if (CompareToSetupIni)
			{
				TargetSection = TargetIni.FindSection(SourceSectionName);
			}
			else
			{
				TargetSection = IniContainer.ConfigFile.FindSection(SourceSectionName);
			}
			
			if (!TargetSection)
			{
				// section does not exist, will be added
				return EDiffType::MissingSection;
			}

			// check if there is any matching value already
			TArray<FConfigValue> TargetValues;
			TargetSection->MultiFind(*SourceKeyName, TargetValues);
			
			// update only when there is a difference
			if (TargetValues.Find(SourceValueString) == INDEX_NONE)
			{
				return EDiffType::ValueDifference;
			}
		}
	}
	
	return EDiffType::None;
}

void FNarrativeProjectSetupHandler::CreateIniBackups(const TArray<FString>& IniNames)
{
	const FString ProjectConfigDir = FPaths::ProjectConfigDir();
	const FString ProjectSavedDir = FPaths::ProjectSavedDir();
	IPlatformFile& PlatformFileIO = FPlatformFileManager::Get().GetPlatformFile();
	IFileManager& FileManager = IFileManager::Get();

	const FString BackupsDir = ProjectSavedDir / TEXT("NarrativePro/IniBackup");
	if (!PlatformFileIO.DirectoryExists(*BackupsDir))
	{
		PlatformFileIO.CreateDirectoryTree(*BackupsDir);
	}
	
	// only copy existing INI files
	TArray<FString> FoundIniFiles;
	IFileManager::Get().FindFiles(FoundIniFiles, *ProjectConfigDir, TEXT("*.ini"));
	for (const FString& IniFileName : IniNames)
	{
		if (FoundIniFiles.Contains(IniFileName))
		{
			const FString SourceFile = ProjectConfigDir / IniFileName;
			const FString IniName = FPaths::GetBaseFilename(IniFileName);
			
			// store the backup as a simple number revision "MyConfig_(<Rev>).ini", ensuring that none matches already
			int32 RevNumber = 1;
			FString RevisionFileName = BACKUP_NAME_FORMAT_IMPL(IniName, RevNumber);
			FString TargetPath = BackupsDir / RevisionFileName;
			while (FileManager.FileExists(*TargetPath))
			{
				RevNumber++;
				RevisionFileName = BACKUP_NAME_FORMAT_IMPL(IniName, RevNumber);
				TargetPath = BackupsDir / RevisionFileName;
			}
			
			// create backup file
			FFileHelper::SaveStringToFile(TEXT(""), *TargetPath);
			PlatformFileIO.CopyFile(*TargetPath, *SourceFile);
		}
	}
}

bool FNarrativeProjectSetupHandler::RestoreIniFromBackup(const FString& SourceIniFileName, int32 Revision)
{
	const FString IniName = FPaths::GetBaseFilename(SourceIniFileName);

	const FString ProjectConfigDir = FPaths::ProjectConfigDir();
	const FString ProjectSavedDir = FPaths::ProjectSavedDir();
	IPlatformFile& PlatformFileIO = FPlatformFileManager::Get().GetPlatformFile();

	const FString BackupsDir = ProjectSavedDir / TEXT("NarrativePro/IniBackup");
	if (!PlatformFileIO.DirectoryExists(*(BackupsDir)))
	{
		UE_LOG(LogNarrativeProjectSetupHandler, Error, TEXT("No backup to restore from"));
		return false;
	}

	TArray<FString> FoundIniBackupFiles;
	IFileManager::Get().FindFiles(FoundIniBackupFiles, *BackupsDir, TEXT("*.ini"));

	// build an array of revisions
	TArray<FString> FileRevisions;
	for (const FString& BackupIniFileName : FoundIniBackupFiles)
	{
		if (BackupIniFileName.Contains(IniName))
		{
			FileRevisions.Add(BackupIniFileName);
		}
	}

	FString TargetBackupFileName;
	if (Revision == MostRecentRevision)
	{
		TargetBackupFileName = FileRevisions.Last();
	}
	else
	{
		int32 RevisionIndex = Revision == FirstRevision? 0 : Revision;
		TargetBackupFileName = FileRevisions.IsValidIndex(RevisionIndex)? FileRevisions[RevisionIndex] : "";
	}

	if (TargetBackupFileName.IsEmpty())
	{
		UE_LOG(LogNarrativeProjectSetupHandler, Error, TEXT("No backup to restore from"));
		return false;
	}

	const FString SourceIniFilePath = ProjectConfigDir / SourceIniFileName;
	// delete source file
	PlatformFileIO.DeleteFile(*SourceIniFilePath);
	// copy to where the source file was, from the target backup
	PlatformFileIO.CopyFile(*SourceIniFilePath, *(BackupsDir/ TargetBackupFileName));

	UE_LOG(LogNarrativeProjectSetupHandler, Log, TEXT("INI backup to restored %s"), *SourceIniFilePath);
	return true;
}

bool FNarrativeProjectSetupHandler::AttemptCheckoutInis(const TArray<FString>& TargetPaths)
{
	// even without source control, nothing can be done with no target paths 
	if (TargetPaths.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("AttemptCheckoutInis: no ini paths supplied"));
		return false;
	}
	
	// true by default. if source control is disabled then there is no need to try checking out
	bool CouldCheckout = true;
	
	// if source control is enabled, then make sure that the INIs that are targeted for update can be modified.
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if (SourceControlProvider.IsEnabled())
	{
		CouldCheckout = false;
		
		TArray<FString> TargetIniFilePaths;
		TargetIniFilePaths.Reserve(TargetPaths.Num());
		for (const FString& TargetPath : TargetPaths)
		{
			ISourceControlModule::Get().QueueStatusUpdate(TargetIniFilePaths);
			
			// only try checkout files that are not checked out currently
			const FSourceControlStatePtr State = SourceControlProvider.GetState(TargetPath, EStateCacheUsage::Use);
			if (!State->IsCheckedOut())
			{
				TargetIniFilePaths.Add(TargetPath);
			}
		}
		
		FMessageLog MessageLog("Narrative Pro");

		// try checkout
		ECommandResult::Type CheckoutResult;
		if (TargetIniFilePaths.IsEmpty())
		{
			// files are probably already checked out
			CheckoutResult = ECommandResult::Type::Succeeded;
		}
		else
		{
			CheckoutResult = SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), TargetIniFilePaths);
		}
		
		switch (CheckoutResult)
		{
			// either the INIs are checked out, or checkout was successful
		case ECommandResult::Type::Succeeded: CouldCheckout = true; break;

			// could not check out
		case ECommandResult::Type::Failed:
		case ECommandResult::Type::Cancelled:
			for (const FString& IniFilePath : TargetIniFilePaths)
			{
				const FText ErrorText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_CouldNotCheckout_Format", "Could not check out: {0}"), FText::FromString(IniFilePath));
				MessageLog.Error(ErrorText);
				UE_LOG(LogTemp, Error, TEXT("Could not check out: %s"), *ErrorText.ToString());
				MessageLog.Open();
			}
			CouldCheckout = false;
			break;
		}
	}

	return CouldCheckout;
}

bool FNarrativeProjectSetupHandler::UpdateIniValuesFromSource(FConfigFile* TargetIni, const FString& SourceSectionName, const FString& SourceKeyName, const FString& SourceValueString, FString& OldValue)
{
	OldValue = "None";
	if (!TargetIni)
	{
		UE_LOG(LogNarrativeProjectSetupHandler, Error, TEXT("UpdateIniValuesFromSource: No Target INI"));
		return false;
	}
	
	// update sections
	// get source value
	const bool IsAddRemoveCommand = SourceKeyName.StartsWith("+") || SourceKeyName.StartsWith("-");

	const FConfigSection* TargetSection = TargetIni->FindSection(SourceSectionName);

	TArray<FConfigValue> TargetValues;
	if (TargetSection)
	{
		TargetSection->MultiFind(*SourceKeyName, TargetValues);
	}
	// check if there is any matching value already
	const int32 ValueIndex = TargetValues.Find(SourceValueString);

	// if the value is missing, add it
	if (ValueIndex == INDEX_NONE)
	{
		// for missing values and or add/remove commands, simply add them
		if (TargetValues.IsEmpty() || IsAddRemoveCommand)
		{
			TargetIni->AddUniqueToSection(*SourceSectionName, *SourceKeyName, SourceValueString);

			UE_LOG(LogNarrativeProjectSetupHandler, Log, TEXT("%s key %s added value %s"), *TargetIni->Name.ToString(), *SourceKeyName, *SourceValueString);
		}
		// if the value exists and there is only 1 key, then it is safe to replace it.
		// as long as it is not an add or remove
		else if (!IsAddRemoveCommand && TargetValues.Num() == 1)
		{
			OldValue = TargetValues[0].GetValue();

			// remove the section to possibly add it later
			TargetIni->RemoveKeyFromSection(*SourceSectionName, *SourceKeyName);
			TargetIni->AddUniqueToSection(*SourceSectionName, *SourceKeyName, SourceValueString);
			
			UE_LOG(LogNarrativeProjectSetupHandler, Log, TEXT("%s key %s added value updated from %s to %s"), *TargetIni->Name.ToString(), *SourceKeyName, *TargetValues[0].GetValue(), *SourceValueString);
		}
		return true;
	}

	return false;
}

void FNarrativeProjectSetupHandler::UpdateOrAddIniFromSetupIni(const FIniContainer& IniContainer, bool GenerateUndoFixers)
{
	const FString TargetIniFileName = IniContainer.ExtStrippedName + TEXT(".ini");
	const FString TargetIniPath = FPaths::ProjectConfigDir() / *TargetIniFileName;

	// create INI if one does not exist
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.FileExists(*TargetIniPath))
	{
		FFileHelper::SaveStringToFile(TEXT(""), *TargetIniPath);
	}

	// get the most recent version of the INI
	FConfigFile TargetIni;
	TargetIni.Read(*TargetIniPath);
	// add anything that is missing
	TargetIni.AddMissingProperties(IniContainer.ConfigFile);

	FMessageLog MessageLog("NarrativePro");
	
	// roll our own INI update as FConfigFile::UpdateSections() will remove sections.
	// if any sections are to be removed, a remove list step will be created
	for(const auto&[SourceSectionName, SourceSection] : IniContainer.ConfigFile)
	{
		for(FConfigSection::TConstIterator SourcePropertyIt(SourceSection); SourcePropertyIt; ++SourcePropertyIt )
		{
			const FString Key = SourcePropertyIt.Key().ToString();	
			const FString Value = SourcePropertyIt.Value().GetValue();

			FString OldValue;
			if (UpdateIniValuesFromSource(&TargetIni, SourceSectionName, Key, Value, OldValue) && GenerateUndoFixers)
			{
				const int32 QueueIndex = QueuedFixers.Add(MakeShared<FIniSingleKeyUndoFixer>(TargetIniPath, SourceSectionName, Key, OldValue, Value));
			
				FFormatNamedArguments Args;
				Args.Add(TEXT("TargetIni"), FText::FromString(IniContainer.ExtStrippedName + TEXT(".ini")));
				Args.Add(TEXT("Section"), FText::FromString(SourceSectionName));
				Args.Add(TEXT("SectionKey"), FText::FromString(Key));
				Args.Add(TEXT("SourceIniValue"), FText::FromString(Value));
				Args.Add(TEXT("OldTargetIniValue"), FText::FromString(OldValue));
			
				MessageLog.AddMessage(FTokenizedMessage::Create(EMessageSeverity::Info))
				->AddText(FText::Format(LOCTEXT("NarrativeProjectSetupHandler_FixerUndoMessage_Format","Project Config \"{TargetIni}\" section: \"{Section}\" key: \"{SectionKey}\" value Set to \"{SourceIniValue}\" from \"{OldTargetIniValue}\""), Args))
				->AddToken(FFixToken::Create(LOCTEXT("NarrativeProjectSetupHandler_Undo", "Undo"), QueuedFixers[QueueIndex]->AsShared(), QueueIndex));
			}
		}
	}
	
	// update the INI file
	TargetIni.Write(TargetIniPath);	
}

void FNarrativeProjectSetupHandler::LogDifferences(const FConfigFile& SourceIni, const FConfigFile& TargetIni, FMessageLog& MessageLog, const FText& MessagePrefix)
{
	// check if target exists already
	const FString TargetIniPath = TargetIni.Name.ToString();
	const bool IniMissing = !IFileManager::Get().FileExists(*TargetIniPath);
	
	// find difference
	for(const auto&[SourceSectionName, SourceSection] : SourceIni)
	{
		const FConfigFile* OptionalIgnoreIni = nullptr;
		if (IniSetups_Ignore.Contains(SourceSectionName))
		{
			OptionalIgnoreIni = &IniSetups_Ignore[SourceSectionName].ConfigFile;
		}
		
		for(FConfigSection::TConstIterator SourcePropertyIt(SourceSection); SourcePropertyIt; ++SourcePropertyIt )
		{
			// get source value
			const FString SourceKeyName = SourcePropertyIt.Key().ToString();
			const FString SourceValueString = SourcePropertyIt.Value().GetValue();

			TArray<FConfigValue> TargetValues;
			if (!IniMissing)
			{
				// skip any ignored stuff
				const FConfigSection* Section = OptionalIgnoreIni? OptionalIgnoreIni->FindSection(SourceSectionName) : nullptr;
				if (Section && Section->Contains(*SourceKeyName))
				{
					continue;
				}
				
				// if there is no section then it is treated as a missing key and value
				if (const FConfigSection* TargetSection = TargetIni.FindSection(SourceSectionName))
				{
					TargetSection->MultiFind(*SourceKeyName, TargetValues);
				}
				// check that the value is not the same already
				if (TargetValues.Find(SourceValueString) != INDEX_NONE)
				{
					continue;
				}
			}
			
			FFormatNamedArguments Args;
			Args.Add(TEXT("MessagePrefix"), MessagePrefix);
			Args.Add(TEXT("TargetIni"), FText::FromString(FPaths::GetCleanFilename(TargetIni.Name.ToString())));
			Args.Add(TEXT("Section"), FText::FromString(SourceSectionName));
			Args.Add(TEXT("SectionKey"), FText::FromString(SourceKeyName));
			Args.Add(TEXT("SourceIniValue"), FText::FromString(SourceValueString));

			FString TargetValue = "None";
			
			FText LogText;
			if (IniMissing)
			{
				LogText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_MissingIni_Format",
					"\"{TargetIni}\" is missing. Fix will create \"{TargetIni}\" "), Args);
			}
			else if (SourceKeyName.StartsWith("+") || SourceKeyName.StartsWith("-"))
			{
				LogText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_ArrayKeyMissing_Format",
					"{MessagePrefix}\"{TargetIni}\" section: \"{Section}\" key: \"{SectionKey}\" values are missing \"{SourceIniValue}\" "), Args);
			}
			else
			{
				// non array items will either be empty or only have 1 elem
				TargetValue = TargetValues.IsValidIndex(0)? TargetValues[0].GetValue() : "None";
				Args.Add(TEXT("TargetIniValue"), FText::FromString(TargetValue));
				
				LogText = FText::Format(LOCTEXT("NarrativeProjectSetupHandler_KeyValueDiff_Format",
					"{MessagePrefix}\"{TargetIni}\" section: \"{Section}\" key: \"{SectionKey}\" value is \"{TargetIniValue}\" instead of \"{SourceIniValue}\" "), Args);
			}

			// add fixer to queue and get its index
			const int32 QueueFixIndex = QueuedFixers.Add(MakeShared<FIniSingleKeyFixer>(TargetIniPath, SourceSectionName, SourceKeyName, SourceValueString));
			const int32 QueueUndoIndex = QueuedFixers.Add(MakeShared<FIniSingleKeyUndoFixer>(TargetIniPath, SourceSectionName, SourceKeyName, SourceValueString, TargetValue, QueuedFixers[QueueFixIndex]));
			
			// add a message with fix token to the message log
			MessageLog.AddMessage(FTokenizedMessage::Create(EMessageSeverity::Warning))
			->AddText(LogText)
			->AddToken(FFixToken::Create(LOCTEXT("NarrativeProjectSetupHandler_Fix", "Fix"), QueuedFixers[QueueFixIndex]->AsShared(), QueueFixIndex))
			->AddToken(FFixToken::Create(LOCTEXT("NarrativeProjectSetupHandler_Undo", "Undo"), QueuedFixers[QueueUndoIndex]->AsShared(), QueueUndoIndex));

			// log to console too
			UE_LOG(LogTemp, Warning, TEXT("%s"), *LogText.ToString());
		}
	}
}

void FNarrativeProjectSetupHandler::OnMapOpened(const FString& MapName, bool AsTemplate)
{

	if (const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>())
	{
		if (!ArsenalSettings->bCheckProjectSettingsOnStartup && !ArsenalSettings->bCheckAddOnSettingsOnStartup)
		{
			return;
		}
	}
	
	if (OnMapOpenHandle.IsValid())
	{
		// only ever want to run the check once
		FEditorDelegates::OnMapOpened.Remove(OnMapOpenHandle);
		OnMapOpenHandle.Reset();
	}
	
	// Combine the add/update to prevent false positives
	TMap<FString, FIniContainer> CombinedAddOrUpdate;
	CombineIniUpdates(CombinedAddOrUpdate);
	
	bProjectInisDiffer = false;
	for (auto&[IniFileName, IniContainer] : CombinedAddOrUpdate)
	{
		EDiffType DiffResult = AnyDifference(IniContainer);
		if (DiffResult != EDiffType::None)
		{
			InisToUpdate.Add(IniFileName);
		}
	}
	
	if (!InisToUpdate.IsEmpty())
	{
		bProjectInisDiffer = true;

		if (const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>(); ArsenalSettings->bDisplayProjectSettingsNotification)
		{
			FTimerHandle Handle;
			GEditor->GetTimerManager()->SetTimer(Handle, FTimerDelegate::CreateRaw(this, &FNarrativeProjectSetupHandler::ProjectSettingsDiffer), 5.0f, false);
		}
	}
	
	UpdateRedirectorsForAddons();
}

FText FNarrativeProjectSetupHandler::GetSettingsMismatchText()
{
	// @todo we current will have false positives with this method.
	// This is because we are individually checking if a plugin/addon has a diff between the source. Ideally we would combine all files (taking into consideration overriding)
	// and detect which addon wants to change which file.

	if (const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>())
	{
		FString Plugins = !IniSetups_AddOrUpdate.IsEmpty() && ArsenalSettings->bCheckProjectSettingsOnStartup ?  "Narrative Pro, " : ""; // Add Narrative Pro if we have ini mismatch
		
		for (auto& Setups_NarrativeAddOn : IniSetups_NarrativeAddOns) 
		{
			if (Setups_NarrativeAddOn.Value.Num() > 0)
			{
				for (auto& AddOnKeys : Setups_NarrativeAddOn.Value)
				{
					if (AnyDifference(AddOnKeys.Value) != EDiffType::None)
					{
						Plugins += Setups_NarrativeAddOn.Key + ", ";
						break;
					}
				}
			}
		}
	
		Plugins.RemoveFromEnd(", "); // Remove last comma
	
		return FText::FromString(FString::Printf(TEXT("%s want to modify ini files"), *Plugins));
	}
	
	return FText::GetEmpty();

}

void FNarrativeProjectSetupHandler::ProjectSettingsDiffer()
{
	if (const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>(); !ArsenalSettings->bDisplayProjectSettingsNotification)
	{
		return;
	}
	
	FNotificationInfo Info(GetSettingsMismatchText());
	
	// Add the buttons with text, tooltip and callback
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("NarrativeProjectSetupHandler_OpenProjectSettings", "Open Project Settings"),
		LOCTEXT("NarrativeProjectSetupHandler_OpenProjectSettings_ToolTip", "Opens Narrative Pro project settings"), 
		FSimpleDelegate::CreateRaw(this, &FNarrativeProjectSetupHandler::OnOpenProjectSettingsClicked))
		);
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("NarrativeProjectSetupHandler_Skip", "Skip"),
		LOCTEXT("NarrativeProjectSetupHandler_Skip_ToolTip", "Skip for now"), 
		FSimpleDelegate::CreateRaw(this, &FNarrativeProjectSetupHandler::OnSkipClicked))
		);
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("NarrativeProjectSetupHandler_DoNotRemindMe", "Don't Remind Me"),
		LOCTEXT("NarrativeProjectSetupHandler_DoNotRemindMe_ToolTip", "do not remind about project settings mismatch"), 
		FSimpleDelegate::CreateRaw(this, &FNarrativeProjectSetupHandler::OnDoNotRemindMeClicked))
		);
	
	Info.bFireAndForget = false;
	Info.WidthOverride = 400.0f;
	Info.bUseLargeFont = false;
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = false;
	Info.Image = FSlateIcon(FNarrativeArsenalStyle::GetStyleSetName(), "NarrativeToolbar.Icon").GetOptionalIcon();
	Info.ExpireDuration = 10.0f;

	// Launch notification
	SettingsNotification = FSlateNotificationManager::Get().AddNotification(Info);
	TSharedPtr<SNotificationItem> NotificationPin = SettingsNotification.Pin();
	if (NotificationPin.IsValid())
	{
		NotificationPin->SetCompletionState(SNotificationItem::CS_Pending);
	}
}

void FNarrativeProjectSetupHandler::OnOpenProjectSettingsClicked() const
{
	TSharedPtr<SNotificationItem> NotificationPin = SettingsNotification.Pin();
	if (NotificationPin.IsValid())
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->ShowViewer("Project", "Plugins", "Narrative Pro");

			NotificationPin->SetText(LOCTEXT("NarrativeProjectSetupHandler_OpeningProjectSettings", "Opening Project Settings..."));
			NotificationPin->SetCompletionState(SNotificationItem::CS_Success);
			NotificationPin->ExpireAndFadeout();
			NotificationPin.Reset();
		}
	}
}

void FNarrativeProjectSetupHandler::OnSkipClicked()
{
	TSharedPtr<SNotificationItem> NotificationPin = SettingsNotification.Pin();
	if (NotificationPin.IsValid())
	{
		NotificationPin->SetText(LOCTEXT("NarrativeProjectSetupHandler_Skipped", "Skipped"));
		NotificationPin->SetCompletionState(SNotificationItem::CS_Success);
		NotificationPin->ExpireAndFadeout();
		NotificationPin.Reset();
	}
}

void FNarrativeProjectSetupHandler::OnDoNotRemindMeClicked()
{
	const FString DefaultEdPath = FPaths::ProjectConfigDir() / TEXT("DefaultEditor.ini");
	if (AttemptCheckoutInis({DefaultEdPath}))
	{
		FConfigFile EditorIni;
		EditorIni.Read(DefaultEdPath);
		EditorIni.AddToSection(TEXT("ConsoleVariables"), TEXT("NarrativePro.DisableProjectSettingsSetupNotification"), TEXT("1"));
		EditorIni.Write(DefaultEdPath);
		
		TSharedPtr<SNotificationItem> NotificationPin = SettingsNotification.Pin();
		if (NotificationPin.IsValid())
		{
			NotificationPin->SetCompletionState(SNotificationItem::CS_Success);
			NotificationPin->ExpireAndFadeout();
			NotificationPin.Reset();
		}
	}	
}

bool FNarrativeProjectSetupHandler::IsNarrativeDevelopmentProject()
{
	return bIsNarrativeDevelopmentProject;
}

void FNarrativeProjectSetupHandler::DisplayOnApplyCompleteRestartPopup()
{
	// let the user know the changes are done and that an editor restart is needed
	EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgCategory::Success, EAppMsgType::Type::YesNo,
		LOCTEXT("NarrativeProjectSetupHandler_RestartPopup",
		"Project Settings Updated!.\n"
		"The editor must be restarted. Restart now?"),
		LOCTEXT("NarrativeProjectSetupHandler_RestartPopupTitle", "Settings Applied!"));

	if (DialogResult == EAppReturnType::Type::Yes)
	{
		FUnrealEdMisc::Get().RestartEditor(false);
	}
	else
	{
		NarrativeToolUtils::NotifyRequiredRestart();
	}
}

void FNarrativeProjectSetupHandler::CombineIniUpdates(TMap<FString, FIniContainer>& OutCombinedIniMap)
{
	// NP now lets us check whether to apply all project settings, or just add on ones 
	if (const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>())
	{
		if (ArsenalSettings->bCheckProjectSettingsOnStartup)
		{
			OutCombinedIniMap = IniSetups_AddOrUpdate;
		}
		
		if (ArsenalSettings->bCheckAddOnSettingsOnStartup)
		{
			for (auto& AddonIniSetups : IniSetups_NarrativeAddOns)
			{
				auto& IniFiles = AddonIniSetups.Value;
				for (auto& IniFile : IniFiles)
				{
					// Add-ons will try to override any previous value within the ini container
					auto& IniContainer = OutCombinedIniMap.FindOrAdd(IniFile.Key, IniFile.Value);
					
					for (const TTuple<FString, FConfigSection>& ConfigFile : IniFile.Value.ConfigFile)
					{
						TArray<FName> Keys;
						ConfigFile.Value.GenerateKeyArray(Keys);
						for (auto It = Keys.CreateConstIterator(); It; ++It) 
						{
							TArray<FConfigValue> ValueArray;
							ConfigFile.Value.GenerateValueArray(ValueArray);
							FString Value = ValueArray[It.GetIndex()].GetValue();
					
							FString OldValue;
							UpdateIniValuesFromSource(&IniContainer.ConfigFile, ConfigFile.Key, *It->ToString(), Value, OldValue);
						}
					}
				}
			}
		}
	}
}

void FNarrativeProjectSetupHandler::GetEnabledAddons(TMap<FString, FString>& OutAddonPaths)
{
	const FProjectDescriptor* Project = IProjectManager::Get().GetCurrentProject();
	TArray<FPluginReferenceDescriptor> Addons = Project->Plugins.FilterByPredicate(
		[&](const FPluginReferenceDescriptor& PluginReferenceDescriptor)
		{
			return PluginReferenceDescriptor.Name.Contains("NP_");
		});
	
	// This will return us addons that the project explicitly has enabled - this will miss any plugins that are enabled by default
	for (const FPluginReferenceDescriptor& Addon : Addons)
	{
		if (Addon.bEnabled)
		{
			OutAddonPaths.Add({Addon.Name, FPaths::ProjectPluginsDir() + Addon.Name});
		}
	}
	
	// We will now add any addons that are enabled by default (that are not explicitly disabled)
	auto DiscoveredPlugins = IPluginManager::Get().GetDiscoveredPlugins();
	for (const TSharedRef<IPlugin>& DiscoveredPlugin : DiscoveredPlugins)
	{
		auto AddonDesc = Addons.FindByPredicate([&](const FPluginReferenceDescriptor& PluginDesc)
		{
			return PluginDesc.Name == DiscoveredPlugin->GetName();
		});
		
		bool bAddonExplicitlyDisabled = AddonDesc ? !AddonDesc->bEnabled : false;
		if (DiscoveredPlugin->GetName().Contains("NP_") && !bAddonExplicitlyDisabled && DiscoveredPlugin->IsEnabledByDefault(!Project->bDisableEnginePluginsByDefault))
		{
			OutAddonPaths.Add({DiscoveredPlugin->GetName(), DiscoveredPlugin->GetBaseDir()});
		}
	}
}

void FNarrativeProjectSetupHandler::ApplyNarrativeSettings(const bool NotifyUserOnComplete, bool GenerateUndoFixers)
{
	if (bRestartRequired)
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyNarrativeSettings: can not apply setting when restart pending"));
		return;
	}

	FMessageLog MessageLog("NarrativePro");
	
	// generating fixers, clear the message log to make it easy to identify the changes
	if (GenerateUndoFixers)
	{
		MessageLog.Flush();
	}
	
	TArray<FString> TargetPaths;
	TargetPaths.Reserve(InisToUpdate.Num());
	for (const FString& IniFileName : InisToUpdate)
	{
		 TargetPaths.Add(IniSetups_AddOrUpdate[IniFileName].TargetPath);
	}
	
	if (!AttemptCheckoutInis(TargetPaths))
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyNarrativeSettings: Could not check out"));
		return;
	}
	
	bRestartRequired = true;
	
	CreateIniBackups(InisToUpdate);
	
	// Combine ini files from pro plugin and addons
	TMap<FString, FIniContainer> CombinedAddOrUpdate;
	CombineIniUpdates(CombinedAddOrUpdate);
	
	// update each INI
	for (const FString& Ini : InisToUpdate)
	{
		UpdateOrAddIniFromSetupIni(CombinedAddOrUpdate[Ini], GenerateUndoFixers);
	}

	// popup delay used to avoid the user missing the applied changes if any when logged to the message log.
	float PopupDelay = 0.1f;
	
	if (GenerateUndoFixers)
	{
		MessageLog.Open();
		PopupDelay = 1.0f;
	}
	
	if (NotifyUserOnComplete)
	{
		FTimerHandle Handle;
		GEditor->GetTimerManager()->SetTimer(Handle, FTimerDelegate::CreateRaw(this, &FNarrativeProjectSetupHandler::DisplayOnApplyCompleteRestartPopup), PopupDelay, false);
	}
}

void FNarrativeProjectSetupHandler::LogAllIniDifferences(const bool UseSetupAsSource)
{
	FMessageLog MessageLog("Narrative Pro");

	// clean up before doing anything
	MessageLog.Flush();
	ClearFixers();
	
	// Combine addon and pro plugin add/update
	TMap<FString, FIniContainer> CombinedAddOrUpdate;
	CombineIniUpdates(CombinedAddOrUpdate);
	
	for (auto&[IniFileName, IniContainer] : CombinedAddOrUpdate)
	{
		FConfigFile SourceIni, TargetIni;
		FText MessagePrefix;
		if (UseSetupAsSource)
		{
			SourceIni = IniContainer.ConfigFile;
			TargetIni.Read(IniContainer.TargetPath);
			MessagePrefix = LOCTEXT("NarrativeProjectSetupHandler_ProjectConfig_MessagePrefix", "Project Config ");
		}
		else
		{
			SourceIni.Read(IniContainer.TargetPath);
			TargetIni = IniContainer.ConfigFile;
			MessagePrefix = LOCTEXT("NarrativeProjectSetupHandler_LogIniDiffSetupConfig_MessagePrefix", "Setup Config ");
		}
		LogDifferences(SourceIni, TargetIni, MessageLog, MessagePrefix);
	}
	
	if (MessageLog.NumMessages() > 0)
	{
		MessageLog.Open();
	}
}

void FNarrativeProjectSetupHandler::OnShutdown()
{
	/*UpdateRedirectorsForAddons();
	LoadIniSetups();
	
	TMap<FString, FIniContainer> CombinedAddOrUpdate;
	CombineIniUpdates(CombinedAddOrUpdate);
	
	for (auto&[IniFileName, IniContainer] : CombinedAddOrUpdate)
	{
		EDiffType DiffResult = AnyDifference(IniContainer);
		if (DiffResult != EDiffType::None)
		{
			InisToUpdate.Add(IniFileName);
		}
	}
	
	if (!InisToUpdate.IsEmpty())
	{
		ApplyNarrativeSettings(false, false);
	}*/
}

void FNarrativeProjectSetupHandler::Startup()
{
	LoadIniSetups();
	
	// check if this is the narrative pro development project
	FString ProjectID;
	GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectID"),ProjectID, GGameIni);
	const FString ProjectName = FApp::GetProjectName();
	if (ProjectName == TEXT("NarrativeDevelopment") && ProjectID == TEXT("2B45C3F5441403AED89DCB88AB29A743"))
	{
		// skips any kind of narrative dev checking stuff 
#ifndef NARRATIVE_DEV_OVERRIDE
		bIsNarrativeDevelopmentProject = true;
#endif
	}
	
	// look for settings override to skip startup check 
	if (const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>(); (!ArsenalSettings->bCheckProjectSettingsOnStartup && !ArsenalSettings->bCheckAddOnSettingsOnStartup))
	{
		UE_LOG(LogTemp, Log, TEXT("Startup check skipped. UArsenalSettings::bCheckProjectSettingsOnStartup =0"));
		return;
	}
	
	// only run the check after the project is up and loaded
	OnMapOpenHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &FNarrativeProjectSetupHandler::OnMapOpened);
	OnShutdownHandle = FEditorDelegates::OnShutdownPostPackagesSaved.AddRaw(this, &FNarrativeProjectSetupHandler::OnShutdown);
}

void FNarrativeProjectSetupHandler::ShutDown()
{
	FEditorDelegates::OnShutdownPostPackagesSaved.Remove(OnShutdownHandle);
	SettingsNotification.Reset();
	ClearFixers();
}

#undef LOCTEXT_NAMESPACE
