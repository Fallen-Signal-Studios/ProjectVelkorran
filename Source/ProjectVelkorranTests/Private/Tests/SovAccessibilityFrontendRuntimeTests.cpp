// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovAccessibilityPresentation.h"
#include "UI/SovAccessibilitySettingsMenu.h"
#include "UI/SovAccessibleRecordMenu.h"
#include "Campaign/SovEvidenceDefinition.h"
#include "Tests/SovSettingsTestFixtures.h"
#include "Tests/SovPlatformOutputTestFixtures.h"
#include "Misc/AutomationTest.h"
#include "ICommonInputModule.h"
#include "UObject/UnrealType.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetNavigation.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Widgets/SWindow.h"
#include <limits>

struct FSovAccessibilityFrontendTestAccess
{
	static TArray<UTextBlock*> DialogueText(USovAccessibilityPresentation* Presentation)
	{ return {Presentation->SubtitleText, Presentation->CaptionText}; }
	static bool Back(USovAccessibilitySettingsMenu* Menu) { return Menu->NativeOnHandleBackAction(); }
	static UWidget* Navigate(USovAccessibilitySettingRow* Row, EUINavigation Direction) { return Row->NavigateValue(Direction); }
	static UScrollBox* RecordScroll(USovAccessibleRecordMenu* Menu) { return Menu->RecordScroll; }
	static bool HasSafeTextRoot(USovAccessibilityPresentation* Presentation)
	{ return Presentation->WidgetTree && Cast<USafeZone>(Presentation->WidgetTree->RootWidget) && Presentation->SafeTextCanvas; }
	static void SetRecords(USovAccessibleRecordMenu* Menu,const FText& Value) { Menu->Records={Value}; }
	static FText FirstRecord(USovAccessibleRecordMenu* Menu) { return Menu->Records.IsEmpty() ? FText::GetEmpty() : Menu->Records[0]; }
	static USovAccessibilitySettingRow* Row(USovAccessibilitySettingsMenu* Menu,USovGameUserSettings* Settings,FName Key,float Min,float Max,float Step)
	{
		Menu->BoundSettings = Settings;
		auto* Row = NewObject<USovAccessibilitySettingRow>(Menu); Row->Configure(Menu,Key,FText::FromName(Key),Min,Max,Step); Menu->Rows.Add(Row); return Row;
	}
};
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueWidthRecoveryTest,"ProjectVelkorran.UI.Accessibility.DialogueWidthRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovDialogueWidthRecoveryTest::RunTest(const FString& Parameters)
{
	auto* Presentation = NewObject<USovAccessibilityPresentation>();
	Presentation->Initialize(); Presentation->TakeWidget();
	const auto Window = SNew(SWindow);
	FHittestGrid Grid;
	const FPaintArgs Args(&Window.Get(), Grid, FVector2D::ZeroVector, 0., 0.f);
	for (auto* Text : FSovAccessibilityFrontendTestAccess::DialogueText(Presentation))
	{
		// Exercise the actual HUD text widgets after a short line has been painted
		// into an auto-sized panel. The next line must recover its viewport budget.
		Text->SetWrapTextAt(600.f);
		Text->SetText(FText::FromString(TEXT("Yes.")));
		const auto Slate = Text->TakeWidget();
		Slate->SlatePrepass();
		const FVector2D ShortSize = Slate->GetDesiredSize();
		FSlateWindowElementList Elements(Window);
		Slate->Paint(Args, FGeometry::MakeRoot(ShortSize, FSlateLayoutTransform()),
			FSlateRect(0, 0, 800, 600), Elements, 0, FWidgetStyle(), true);
		Text->SetText(FText::FromString(TEXT("Keep moving toward the evacuation point and protect the wounded.")));
		Slate->SlatePrepass();
		const FVector2D LongSize = Slate->GetDesiredSize();
		TestTrue(TEXT("A longer line expands beyond the previously painted short line"), LongSize.X > ShortSize.X * 3.f);
		TestTrue(TEXT("Dialogue remains inside its explicit safe-area width"), LongSize.X <= 601.f);
		TestTrue(TEXT("Ordinary dialogue does not become a tall column"), LongSize.Y < 200.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovConsoleMenuBackTest,"ProjectVelkorran.UI.Console.BackAndFirstBoot",EAutomationTestFlags_ApplicationContextMask|EAutomationTestFlags::EngineFilter)
bool FSovConsoleMenuBackTest::RunTest(const FString& Parameters)
{
	auto* Menu=NewObject<USovAccessibilitySettingsMenu>(); Menu->SetFirstBoot(true); Menu->ActivateWidget();
	TestTrue(TEXT("First boot consumes platform Back"),FSovAccessibilityFrontendTestAccess::Back(Menu));
	TestTrue(TEXT("Back cannot bypass explicit setup completion"),Menu->IsActivated());
	Menu->SetFirstBoot(false);
	TestTrue(TEXT("Normal settings handle platform Back"),FSovAccessibilityFrontendTestAccess::Back(Menu));
	TestFalse(TEXT("Normal Back retires settings and their owned preview"),Menu->IsActivated());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovConsoleNativeLayoutTest,"ProjectVelkorran.UI.Console.NavigationAndSafeArea",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSovConsoleNativeLayoutTest::RunTest(const FString& Parameters)
{
	ICommonInputModule::GetSettings().LoadData();
	auto* Presentation=NewObject<USovAccessibilityPresentation>(); Presentation->Initialize(); Presentation->TakeWidget();
	TestTrue(TEXT("Actual subtitle/caption tree uses engine SafeZone"),FSovAccessibilityFrontendTestAccess::HasSafeTextRoot(Presentation));
	auto* Review=NewObject<USovAccessibleRecordMenu>(); Review->Initialize(); Review->TakeWidget();
	TestNotNull(TEXT("Evidence text has an independently scrollable viewport"),FSovAccessibilityFrontendTestAccess::RecordScroll(Review));
	auto* Settings=NewObject<USovSettingsTestSettings>(); auto* Menu=NewObject<USovAccessibilitySettingsMenu>(); Menu->ActivateWidget();
	auto* Row=FSovAccessibilityFrontendTestAccess::Row(Menu,Settings,"UIScale",1,2,.25f); Row->Initialize(); Row->TakeWidget();
	TestEqual(TEXT("Directional rule is installed on the actual focused button"),Row->GetFocusTarget()->Navigation->Right.Rule,EUINavigationRule::Custom);
	TestNull(TEXT("Mapped right direction cannot steal focus from a callback's modal"),FSovAccessibilityFrontendTestAccess::Navigate(Row,EUINavigation::Right));
	TestEqual(TEXT("Mapped direction commits the settings transaction"),Settings->GetSettingsSnapshot().UIScale,1.25f);
	auto* Continue=FSovAccessibilityFrontendTestAccess::Row(Menu,Settings,"Continue",0,1,1);
	FSovAccessibilityFrontendTestAccess::Navigate(Continue,EUINavigation::Left);
	TestTrue(TEXT("Sideways navigation cannot activate Continue"),Menu->IsActivated());
	Menu->DeactivateWidget(); FSovAccessibilityFrontendTestAccess::Navigate(Row,EUINavigation::Right);
	TestEqual(TEXT("Retired menu ignores stale navigation"),Settings->GetSettingsSnapshot().UIScale,1.25f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovConsoleHDRRowsTest,"ProjectVelkorran.UI.Console.SystemManagedHDR",EAutomationTestFlags_ApplicationContextMask|EAutomationTestFlags::EngineFilter)
bool FSovConsoleHDRRowsTest::RunTest(const FString& Parameters)
{
	auto* Settings=NewObject<USovPlatformOutputTestSettings>(); Settings->bSystemManaged=true;
	auto* Menu=NewObject<USovAccessibilitySettingsMenu>();
	for (FName Key : {FName("HDR.Enabled"),FName("HDR.Peak"),FName("HDR.Black"),FName("HDR.Paper"),FName("HDR.UI"),FName("HDR.Preview"),FName("HDR.Confirm"),FName("HDR.Revert")})
	{
		auto* Row=FSovAccessibilityFrontendTestAccess::Row(Menu,Settings,Key,0,1,1);
		TestFalse(TEXT("Console display setting cannot be edited through desktop controls"),Menu->IsRowEnabled(Row));
		TestTrue(TEXT("Unavailable controls explain system ownership"),Menu->ValueText(Row).ToString().Contains(TEXT("console display settings")));
		Menu->Adjust(Row,1);
	}
	TestEqual(TEXT("Disabled controls cannot write console HDR output"),Settings->Writes,0);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRecordViewReentry,"ProjectVelkorran.UI.Accessibility.RecordViewReentry",EAutomationTestFlags_ApplicationContextMask|EAutomationTestFlags::EngineFilter)
bool FSovRecordViewReentry::RunTest(const FString& Parameters)
{
	auto* Menu=NewObject<USovAccessibleRecordMenu>(); bool bReentered=false;
	Menu->OnRequestRefreshFocus().AddLambda([&]()
	{
		if(bReentered) { return; } bReentered=true; Menu->DeactivateWidget(); Menu->ActivateWidget();
		FSovAccessibilityFrontendTestAccess::SetRecords(Menu,FText::FromString(TEXT("Successor view")));
	});
	Menu->ActivateWidget();
	TestTrue(TEXT("Synchronous same-instance replacement remains active"),Menu->IsActivated());
	TestEqual(TEXT("Retired activation cannot rebuild or clear successor record view"),FSovAccessibilityFrontendTestAccess::FirstRecord(Menu).ToString(),FString(TEXT("Successor view")));
	Menu->OnRequestRefreshFocus().Clear(); Menu->DeactivateWidget();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAccessibilityTransaction,"ProjectVelkorran.UI.Accessibility.LocalSettingsAtomicPrivacy",EAutomationTestFlags_ApplicationContextMask|EAutomationTestFlags::EngineFilter)
bool FSovAccessibilityTransaction::RunTest(const FString& Parameters)
{
	auto* Settings = NewObject<USovSettingsTestSettings>(); FString Error; TArray<uint8> Bytes;
	// Config-backed instances inherit the user's first-boot completion on this PC.
	// Stage an uncompleted test instance without touching the real settings owner.
	auto* Completed = FindFProperty<FBoolProperty>(Settings->GetClass(), TEXT("bAccessibilitySetupCompleted"));
	if (!TestNotNull(TEXT("Reflected first-boot fixture field"), Completed)) { return false; }
	Completed->SetPropertyValue_InContainer(Settings, false);
	TestFalse(TEXT("Fixture begins before explicit setup completion"), Settings->HasCompletedAccessibilitySetup());
	TestTrue(TEXT("Capture legacy eleven-byte gameplay payload"),Settings->CapturePortableSettings(Bytes)); TestEqual(TEXT("Schema unchanged"),Bytes.Num(),11);
	auto Value = Settings->GetSettingsSnapshot(); Value.UIScale=2; Value.SubtitleScale=2.5f; Value.bHighContrastHUD=true; Value.bMenuNarration=true;
	Value.DialoguePressureMode=ESovDialoguePressureMode::Disabled; Value.bOverrideTeamColor=true; Value.TeamColor=FLinearColor::Green;
	Value.ControllerAudioVolume=.35f;
	TestTrue(TEXT("Complete accessibility transaction"),Settings->ApplySettingsSnapshot(Value,Error));
	TestFalse(TEXT("Setup not inferred from settings edit"),Settings->HasCompletedAccessibilitySetup());
	TestTrue(TEXT("Explicit continue completes setup"),Settings->CompleteAccessibilitySetup());
	TestTrue(TEXT("Gameplay import remains available"),Settings->RestorePortableSettings(Bytes,Error));
	const auto Actual=Settings->GetSettingsSnapshot();
	TestEqual(TEXT("UI scale retained locally"),Actual.UIScale,2.f); TestEqual(TEXT("Independent subtitle scale retained"),Actual.SubtitleScale,2.5f);
	TestTrue(TEXT("First-boot completion retained locally"),Settings->HasCompletedAccessibilitySetup());
	TestTrue(TEXT("Independent team color retained"),Actual.TeamColor.Equals(FLinearColor::Green));
	TestEqual(TEXT("Pressure accessibility retained"),Actual.DialoguePressureMode,ESovDialoguePressureMode::Disabled);
	TestEqual(TEXT("Controller-audio preference retained"),Actual.ControllerAudioVolume,.35f);
	const int32 Saves=Settings->Saves; Value.TeamColor.R=std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Nonfinite independent color rejected"),Settings->ApplySettingsSnapshot(Value,Error)); TestEqual(TEXT("Rejected snapshot did not persist"),Settings->Saves,Saves);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAccessibilityNativeControl,"ProjectVelkorran.UI.Accessibility.NativeControlTransactions",EAutomationTestFlags_ApplicationContextMask|EAutomationTestFlags::EngineFilter)
bool FSovAccessibilityNativeControl::RunTest(const FString& Parameters)
{
	auto* Settings=NewObject<USovSettingsTestSettings>(); auto* Menu=NewObject<USovAccessibilitySettingsMenu>();
	auto* Row=FSovAccessibilityFrontendTestAccess::Row(Menu,Settings,"UIScale",1,2,.25f);
	Menu->Adjust(Row,1); TestEqual(TEXT("Native control commits real settings owner"),Settings->GetSettingsSnapshot().UIScale,1.25f);
	Menu->Adjust(Row,-1); TestEqual(TEXT("Left decreases independently"),Settings->GetSettingsSnapshot().UIScale,1.f);
	Menu->Adjust(Row,-1); TestEqual(TEXT("Control wraps bounded range"),Settings->GetSettingsSnapshot().UIScale,2.f);
	Row=FSovAccessibilityFrontendTestAccess::Row(Menu,Settings,"bClosedCaptions",0,1,1); Menu->Adjust(Row,1);
	TestFalse(TEXT("Native toggle consumed in snapshot"),Settings->GetSettingsSnapshot().bClosedCaptions);
	Row=FSovAccessibilityFrontendTestAccess::Row(Menu,Settings,"bShowObjectiveText",0,1,1); Menu->Adjust(Row,1);
	TestFalse(TEXT("Objective text can be hidden through native settings"),Settings->GetSettingsSnapshot().bShowObjectiveText);
	Menu->Adjust(Row,1);
	TestTrue(TEXT("Objective text can be restored through native settings"),Settings->GetSettingsSnapshot().bShowObjectiveText);
	Row=FSovAccessibilityFrontendTestAccess::Row(Menu,Settings,"DialoguePressureMode",0,2,1); Menu->Adjust(Row,1);
	TestEqual(TEXT("Reflected enum uses actual settings"),Settings->GetSettingsSnapshot().DialoguePressureMode,ESovDialoguePressureMode::Extended);
	Row=FSovAccessibilityFrontendTestAccess::Row(Menu,Settings,"Cloud.Enabled",0,1,1);
	TestFalse(TEXT("Missing provider cannot be presented as available"),Menu->IsRowEnabled(Row));
	auto* OutputSettings = NewObject<USovPlatformOutputTestSettings>(); OutputSettings->InitializeOutput(true, 1000);
	Row=FSovAccessibilityFrontendTestAccess::Row(Menu,OutputSettings,"HDR.Enabled",0,1,1); Menu->Adjust(Row,1);
	Row=FSovAccessibilityFrontendTestAccess::Row(Menu,OutputSettings,"HDR.Preview",0,1,1);
	// The actual HDR-enabled toggle selected SDR; preview must use the output-only transaction.
	Menu->Adjust(Row,1);
	TestFalse(TEXT("Actual menu can preview switching HDR off"),OutputSettings->bOutputEnabled);
	Row=FSovAccessibilityFrontendTestAccess::Row(Menu,OutputSettings,"HDR.Confirm",0,1,1); Menu->Adjust(Row,1);
	TestTrue(TEXT("SDR preview is confirmable through native controls"),OutputSettings->Saves>0);
	TestFalse(TEXT("Confirmed SDR output persisted"),OutputSettings->bSavedEnabled);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAccessibilityTextPresentation,"ProjectVelkorran.UI.Accessibility.PagingPaletteAndScenePrivacy",EAutomationTestFlags_ApplicationContextMask|EAutomationTestFlags::EngineFilter)
bool FSovAccessibilityTextPresentation::RunTest(const FString& Parameters)
{
	const FString Original=TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
	const auto Pages=USovAccessibilityPresentation::PaginateText(Original,10,2); TestEqual(TEXT("Overflow paginates instead of ellipsis"),Pages.Num(),2);
	FString Recovered; for(const auto& Page:Pages) { Recovered += Page.Replace(TEXT("\n"),TEXT("")); }
	TestEqual(TEXT("All text survives pagination"),Recovered,Original);
	const auto Unicode=USovAccessibilityPresentation::PaginateText(TEXT("e\u0301e\u0301e\u0301"),1,1);
	TestEqual(TEXT("Combining sequence is one grapheme"),Unicode.Num(),3); if(Unicode.Num()==3) { TestEqual(TEXT("No isolated combining mark"),Unicode[0],FString(TEXT("e\u0301"))); }
	FSovUserSettingsSnapshot Settings; const auto Threat=USovAccessibilityPresentation::ThreatTint(Settings); Settings.bOverrideTeamColor=true; Settings.TeamColor=FLinearColor::Green;
	TestTrue(TEXT("Team override affects team presentation"),USovAccessibilityPresentation::TeamTint(Settings).Equals(FLinearColor::Green));
	TestTrue(TEXT("Team override cannot modify independent threat color"),USovAccessibilityPresentation::ThreatTint(Settings).Equals(Threat));
	auto* Presentation=NewObject<USovAccessibilityPresentation>();
	Presentation->PresentSpeech(FText::FromString(TEXT("Speaker")),FText::FromString(TEXT("Readable line")),-1,FVector::ZeroVector,true);
	TestFalse(TEXT("Indefinite real dialogue line accepted"),Presentation->GetCurrentSpeechText().IsEmpty());
	Presentation->ClearSpeech(); TestFalse(TEXT("Finish does not truncate readable line"),Presentation->GetCurrentSpeechText().IsEmpty());
	Presentation->PresentCaption(FText::FromString(TEXT("Shield broken")),4,FVector::ZeroVector);
	TestEqual(TEXT("Actual native caption consumer receives event"),Presentation->GetCurrentCaptionText().ToString(),FString(TEXT("Shield broken")));
	TestEqual(TEXT("History is current scene only"),Presentation->GetSceneHistory().Num(),2);
	Presentation->ClearSceneHistory(); TestEqual(TEXT("Scene replacement retires history"),Presentation->GetSceneHistory().Num(),0);
	TestTrue(TEXT("Scene replacement retires captions"),Presentation->GetCurrentCaptionText().IsEmpty());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEvidenceSummaryPresentation,"ProjectVelkorran.UI.Accessibility.EvidenceFactInterpretation",EAutomationTestFlags_ApplicationContextMask|EAutomationTestFlags::EngineFilter)
bool FSovEvidenceSummaryPresentation::RunTest(const FString& Parameters)
{
	auto* Evidence=NewObject<USovEvidenceDefinition>(); Evidence->Summary=FText::FromString(TEXT("A disputed record."));
	const FString Unknown=USovAccessibleRecordMenu::DescribeEvidence(Evidence,ESovEvidenceStage::Unknown).ToString();
	TestFalse(TEXT("Unknown stage cannot expose unacquired summary"),Unknown.Contains(TEXT("disputed")));
	const FString Legacy=USovAccessibleRecordMenu::DescribeEvidence(Evidence,ESovEvidenceStage::Observed).ToString();
	TestTrue(TEXT("Legacy summary is available as a record"),Legacy.Contains(TEXT("A disputed record.")));
	TestTrue(TEXT("Legacy summary never automatically relabelled fact"),Legacy.Contains(TEXT("not automatically an established fact")));
	Evidence->ObservedFacts=FText::FromString(TEXT("Two independent signatures.")); Evidence->Interpretation=FText::FromString(TEXT("Possibly a coordinated decision."));
	const FString Classified=USovAccessibleRecordMenu::DescribeEvidence(Evidence,ESovEvidenceStage::Authenticated).ToString();
	TestTrue(TEXT("Observed facts read separately"),Classified.Contains(TEXT("Observed facts: Two independent signatures.")));
	TestTrue(TEXT("Interpretation remains explicitly labelled"),Classified.Contains(TEXT("Interpretation: Possibly a coordinated decision.")));
	return true;
}
#endif
