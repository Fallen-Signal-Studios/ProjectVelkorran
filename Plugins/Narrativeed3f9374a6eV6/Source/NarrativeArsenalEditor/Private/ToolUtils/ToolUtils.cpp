// Copyright Narrative Tools 2025.

#include "ToolUtils/ToolUtils.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogNarrativeToolUtils, All, All)

#define LOCTEXT_NAMESPACE "NarrativeToolUtils"

void NarrativeToolUtils::OpenMapWithSaveCheck(const FSoftObjectPath& MapSoftObjPath)
{
	// can not load nothing
	if (!ensure(MapSoftObjPath.IsAsset()))
	{
		UE_LOG(LogNarrativeToolUtils, Error, TEXT("OpenMapWithSaveCheck: no map asset provided! Cannot open!"))
		return;
	}

	const FString MapPath = MapSoftObjPath.GetLongPackageName();
	const FString MapToOpen = FPackageName::LongPackageNameToFilename(MapPath, FPackageName::GetMapPackageExtension());
	
	const bool bPromptUserToSave = true;
	const bool bSaveMapPackages = true;
	const bool bSaveContentPackages = false; // don't save general assets, just assets relating to the current open map
	const bool bFastSave = false;
	const bool bNotifyNoPackagesSaved = true;
	const bool bCanBeDeclined = true;
	
	// if we save the assets, then we can open the level
	if (FEditorFileUtils::SaveDirtyPackages( bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined ))
	{
		// open the level ensuring it is not a template
		FEditorFileUtils::LoadMap(MapToOpen, false, true);
	}
}

/* class to wrap a restart popup */
class FRestartRequiredNotification
{
	TWeakPtr<SNotificationItem> NotificationPtr;
		
public:
		
	void OnRestartRequired()
	{
		TSharedPtr<SNotificationItem> NotificationPin = NotificationPtr.Pin();
		if (NotificationPin.IsValid())
		{
			return;
		}
			
		FNotificationInfo Info( LOCTEXT("RestartRequiredTitle", "Restart required to apply new settings") );

		// Add the buttons with text, tooltip and callback
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			LOCTEXT("RestartNow", "Restart Now"), 
			LOCTEXT("RestartNowToolTip", "Restart now to finish applying your new settings."), 
			FSimpleDelegate::CreateRaw(this, &FRestartRequiredNotification::OnRestartClicked))
			);
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			LOCTEXT("RestartLater", "Restart Later"), 
			LOCTEXT("RestartLaterToolTip", "Dismiss this notificaton without restarting. Some new settings will not be applied."), 
			FSimpleDelegate::CreateRaw(this, &FRestartRequiredNotification::OnDismissClicked))
			);

		// We will be keeping track of this ourselves
		Info.bFireAndForget = false;

		// Set the width so that the notification doesn't resize as its text changes
		Info.WidthOverride = 300.0f;

		Info.bUseLargeFont = false;
		Info.bUseThrobber = false;
		Info.bUseSuccessFailIcons = false;

		// Launch notification
		NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
		NotificationPin = NotificationPtr.Pin();

		if (NotificationPin.IsValid())
		{
			NotificationPin->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}
	
	void OnRestartClicked()
	{
		TSharedPtr<SNotificationItem> NotificationPin = NotificationPtr.Pin();
		if (NotificationPin.IsValid())
		{
			NotificationPin->SetText(LOCTEXT("RestartingNow", "Restarting..."));
			NotificationPin->SetCompletionState(SNotificationItem::CS_Success);
			NotificationPin->ExpireAndFadeout();
			NotificationPtr.Reset();
			
			FUnrealEdMisc::Get().RestartEditor(false);
		}
	}
	
	void OnDismissClicked()
	{
		TSharedPtr<SNotificationItem> NotificationPin = NotificationPtr.Pin();
		if (NotificationPin.IsValid())
		{
			NotificationPin->SetText(LOCTEXT("RestartDismissed", "Restart Dismissed..."));
			NotificationPin->SetCompletionState(SNotificationItem::CS_None);
			NotificationPin->ExpireAndFadeout();
			NotificationPtr.Reset();
		}
	}
};

static FRestartRequiredNotification RestartRequiredNotification;

void NarrativeToolUtils::NotifyRequiredRestart()
{
	RestartRequiredNotification.OnRestartRequired();
}

void NarrativeToolUtils::ClearRequiredRestart()
{
	RestartRequiredNotification.OnDismissClicked();
}

#undef LOCTEXT_NAMESPACE
