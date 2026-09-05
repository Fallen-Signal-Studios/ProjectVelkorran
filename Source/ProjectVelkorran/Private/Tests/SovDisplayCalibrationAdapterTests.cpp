// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPlatformOutputTestFixtures.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDisplayCVarOwnershipTest,
	"ProjectVelkorran.Campaign.PlatformOutput.ProductionCVarOwnership",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FSovDisplayCVarOwnershipTest::RunTest(const FString& Parameters)
{
	const TCHAR* BlackName = TEXT("r.HDR.Display.MinLuminanceLog10");
	const TCHAR* GrayName = TEXT("r.HDR.Display.MidLuminance");
	const TCHAR* UIName = TEXT("r.HDR.UI.Level");
	const TCHAR* ModeName = TEXT("r.HDR.UI.CompositeMode");
	FSovHDRCalibration Requested; Requested.BlackFloorNits = .01f; Requested.PaperWhiteNits = 200.f; Requested.UIWhiteNits = 240.f;
	FString Error; FGuid Receipt;
	TStrongObjectPtr<USovDisplayCVarTestSettings> Settings(NewObject<USovDisplayCVarTestSettings>());
	Settings->InitializeVariables(); ON_SCOPE_EXIT { Settings->ResetVariables(); };
	TestTrue(TEXT("Production renderer mapping admits isolated writable CVars"), Settings->PreviewHDRDisplay(true, 1000, Requested, Receipt, Error));
	TestEqual(TEXT("Actual black log mapping"), Settings->GetVariable(BlackName)->GetFloat(), -2.f);
	TestTrue(TEXT("Actual 18-percent scene mapping"), FMath::IsNearlyEqual(Settings->GetVariable(GrayName)->GetFloat(), 36.f));
	Settings->GetVariable(UIName)->Set(.5f, ECVF_SetByConsole);
	TestFalse(TEXT("Higher-priority external field invalidates confirmation"), Settings->ConfirmHDRCalibration(Receipt, Error));
	TestEqual(TEXT("Independent black restores despite immutable UI"), Settings->GetVariable(BlackName)->GetFloat(), -4.f);
	TestEqual(TEXT("Independent scene restores despite immutable UI"), Settings->GetVariable(GrayName)->GetFloat(), 15.f);
	TestEqual(TEXT("External UI override retained"), Settings->GetVariable(UIName)->GetFloat(), .5f);

	Settings->ResetVariables(); Settings->InitializeVariables();
	TestTrue(TEXT("Fresh production preview"), Settings->PreviewHDRDisplay(true, 1000, Requested, Receipt, Error));
	Settings->GetVariable(ModeName)->Set(0.f, ECVF_SetByConsole);
	TestFalse(TEXT("Compositor loss cannot confirm ineffective calibration"), Settings->ConfirmHDRCalibration(Receipt, Error));
	TestEqual(TEXT("Black restored without compatible compositor"), Settings->GetVariable(BlackName)->GetFloat(), -4.f);
	TestEqual(TEXT("UI gain restored independently of compositor"), Settings->GetVariable(UIName)->GetFloat(), 1.f);
	TestEqual(TEXT("External compositor mode untouched"), Settings->GetVariable(ModeName)->GetFloat(), 0.f);

	Settings->ResetVariables(); Settings->InitializeVariables();
	TestTrue(TEXT("Preview captures raw UI gain ownership"), Settings->PreviewHDRDisplay(true, 1000, Requested, Receipt, Error));
	Settings->GetVariable(TEXT("r.HDR.UI.Luminance"))->Set(400.f, ECVF_SetByConsole);
	TestFalse(TEXT("Changed base luminance invalidates receipt"), Settings->ConfirmHDRCalibration(Receipt, Error));
	TestEqual(TEXT("Raw owned gain restores despite changed reference"), Settings->GetVariable(UIName)->GetFloat(), 1.f);
	TestEqual(TEXT("External base luminance retained"), Settings->GetVariable(TEXT("r.HDR.UI.Luminance"))->GetFloat(), 400.f);

	Settings->ResetVariables(); Settings->InitializeVariables();
	IConsoleVariable* Gray = Settings->GetVariable(GrayName);
	Settings->GetVariable(BlackName)->SetOnChangedCallback(FConsoleVariableDelegate::CreateLambda([Gray](IConsoleVariable*) { Gray->Set(90.f, ECVF_SetByConsole); }));
	TestFalse(TEXT("Synchronous earlier setter can revoke later field ownership"), Settings->WriteProductionCalibration(Requested));
	TestEqual(TEXT("Partial write restores its earlier field"), Settings->GetVariable(BlackName)->GetFloat(), -4.f);
	TestEqual(TEXT("Callback-owned later field retained"), Gray->GetFloat(), 90.f);
	TestEqual(TEXT("Unreached field was not changed"), Settings->GetVariable(UIName)->GetFloat(), 1.f);
	return true;
}
#endif
