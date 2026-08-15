// Copyright Narrative Tools 2025.

#include "NarrativeToolbar.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "ISettingsModule.h"
#include "NarrativeArsenalStyle.h"
#include "ToolUtils/ToolUtils.h"

#define LOCTEXT_NAMESPACE "NarrativeToolbar"

TSharedRef<SWidget> NarrativeToolbar::GetToolbarMenuDropDown()
{
	// generic new URL UI action
	auto NewOpenURLAction = [](const FString& InURL) -> FUIAction
	{
		return { FExecuteAction::CreateLambda([InURL]()
			{
				FPlatformProcess::LaunchURL(*InURL, nullptr, nullptr);
			}) };
	};

	const FText OpensInBrowserTestFormat = LOCTEXT("NarrativeToolbar_Website_ToolTip", "Opens {0} in browser");
	
	FMenuBuilder MenuBuilder(true, nullptr);

	/* shortcuts */
	// website URL
	MenuBuilder.AddMenuEntry(
	LOCTEXT("NarrativeToolbar_Website", "Website"),
	FText::Format(OpensInBrowserTestFormat, FText::FromString(WebsiteURL)),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "TranslationEditor.ImportLatestFromLocalizationService"),
		NewOpenURLAction(WebsiteURL)
		);
	
	// docs URL
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NarrativeToolbar_Documentation", "Documentation"),
		FText::Format(OpensInBrowserTestFormat, FText::FromString(DocsURL)),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "TranslationEditor.ImportLatestFromLocalizationService"),
		NewOpenURLAction(DocsURL)
		);
	
	// project settings
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NarrativeToolbar_NarrativeProjectSettings", "Narrative Project Settings"),
		LOCTEXT("NarrativeToolbar_NarrativeProjectSettings_ToolTip", "Opens Narrative Pro plugin Project Settings"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ProjectSettings.TabIcon"),
		FExecuteAction::CreateStatic(&NarrativeToolbar::OpenNarrativeProjectPluginSettings)
		);

	// demo map
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NarrativeToolbar_DemoMap", "Demo Map"),
		LOCTEXT("NarrativeToolbar_DemoMap_ToolTip", "Opens the Narrative Pro demo map"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Level"),
		FExecuteAction::CreateLambda([]()
			{
				FSoftObjectPath DemoMapPath;
				DemoMapPath.SetPath("/NarrativePro/Pro/Demo/Maps/OpenWorld/L_DemoMap_OpenWorld.L_DemoMap_OpenWorld'");
				NarrativeToolUtils::OpenMapWithSaveCheck(DemoMapPath);
			})
		);
	/* shortcuts */

	/* utility */
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("NarrativeToolbar_Utility_MenuCategory", "Utility"));
	// generate
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NarrativeToolbar_MapGeneration", "Map Generation Utility"),
		LOCTEXT("NarrativeToolbar_MapGeneration_ToolTip", "Opens Map Generation Editor Utility"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(),"LevelEditor.Build"),
		FExecuteAction::CreateStatic(&NarrativeToolbar::OpenNavigationGenerateTool)
		);
	MenuBuilder.EndSection();
	/* utility */

	return MenuBuilder.MakeWidget();
}

void NarrativeToolbar::CreateSection()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	FToolMenuSection& Section = Menu->AddSection(
		"NarrativeToolbar",
		TAttribute<FText>(),
		FToolMenuInsert("Play", EToolMenuInsertType::After)
		);

	// the combo button
	FToolMenuEntry NarrativeToolbarEntry = FToolMenuEntry::InitComboButton(
	"NarrativeToolbar",
	FUIAction(
		FExecuteAction(),
			FCanExecuteAction::CreateStatic(&NarrativeToolbar::HasNoPlayWorld),
			FIsActionChecked(),
			FIsActionButtonVisible()),
		FOnGetContent::CreateStatic(&NarrativeToolbar::GetToolbarMenuDropDown),
	LOCTEXT("NarrativeToolbar_Label", "Narrative Pro"),
	LOCTEXT("NarrativeToolbar_ToolTip", "Toolbar shortcut for Narrative Pro tools"),
		FSlateIcon(FNarrativeArsenalStyle::GetStyleSetName(), "NarrativeToolbar.Icon")
	);

	// use the same toolbar styling for text and anything that is not explicitly some other style.
	// icons are _not_ effected by this for example.
	NarrativeToolbarEntry.StyleNameOverride = "CalloutToolbar";
	Section.AddEntry(NarrativeToolbarEntry);
}

void NarrativeToolbar::OpenNavigationGenerateTool()
{
	// load generate widget utility and get utility sub system
	UEditorUtilityWidgetBlueprint* GenerateUtilityWidget = LoadObject<UEditorUtilityWidgetBlueprint>(nullptr,
		TEXT("/NarrativePro/Pro/Editor/Navigator/Utility_Navigator.Utility_Navigator"));
	UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	
	if (GenerateUtilityWidget && EditorUtilitySubsystem)
	{
		// try open the window
		EditorUtilitySubsystem->SpawnAndRegisterTab(GenerateUtilityWidget);
	}
}

void NarrativeToolbar::OpenNarrativeProjectPluginSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->ShowViewer("Project", "Plugins", "Narrative Pro");
	}
}

#undef LOCTEXT_NAMESPACE
