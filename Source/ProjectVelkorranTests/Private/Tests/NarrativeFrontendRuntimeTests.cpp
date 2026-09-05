// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/NarrativeFrontendTestFixtures.h"
#include "Blueprint/WidgetNavigation.h"
#include "Components/SlateWrapperTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNarrativeMenuFocusRouting,
	"ProjectVelkorran.UI.Menu.CommonUIFocusAndReentry",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FNarrativeMenuFocusRouting::RunTest(const FString& Parameters)
{
	UNarrativeFrontendTestMenu* Menu = NewObject<UNarrativeFrontendTestMenu>();
	int32 RefreshRequests = 0;
	bool bRequestedAfterRegistration = false;
	Menu->OnRequestRefreshFocus().AddLambda([&]()
	{
		++RefreshRequests;
		bRequestedAfterRegistration = Menu->RegistrationCalls > 0;
	});
	Menu->ActivateWidget();
	TestEqual(TEXT("Activation delegates focus restoration to CommonUI"), RefreshRequests, 1);
	TestEqual(TEXT("No forced query bypasses CommonUI's focus memory"), Menu->DirectFocusQueries, 0);
	TestTrue(TEXT("Dynamic controls can register before focus refresh"), bRequestedAfterRegistration);
	Menu->DeactivateWidget();
	Menu->bDeactivateDuringRegistration = true;
	Menu->ActivateWidget();
	TestFalse(TEXT("Registration can close the menu"), Menu->IsActivated());
	TestEqual(TEXT("Closed menu cannot steal focus"), RefreshRequests, 1);
	Menu->bDeactivateDuringRegistration = true;
	Menu->bReactivateDuringRegistration = true;
	Menu->ActivateWidget();
	TestTrue(TEXT("Replacement activation remains active"), Menu->IsActivated());
	TestEqual(TEXT("Only replacement activation requests focus"), RefreshRequests, 2);
	Menu->OnRequestRefreshFocus().Clear();
	Menu->DeactivateWidget();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNarrativeMenuBoundaryWrap,
	"ProjectVelkorran.UI.Menu.BoundaryWrapOwnership",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FNarrativeMenuBoundaryWrap::RunTest(const FString& Parameters)
{
	UNarrativeFrontendTestMenu* Menu = NewObject<UNarrativeFrontendTestMenu>();
	UWidget* Root = Menu->MakeRoot();
	Root->SetNavigationRuleBase(EUINavigation::Left, EUINavigationRule::Stop);
	Root->SetNavigationRuleBase(EUINavigation::Previous, EUINavigationRule::Wrap);
	Menu->SetMenuNavigationWrap(true);
	TestEqual(TEXT("Controller boundary wraps"), Root->Navigation->GetNavigationRule(EUINavigation::Down), EUINavigationRule::Wrap);
	TestEqual(TEXT("Keyboard boundary wraps"), Root->Navigation->GetNavigationRule(EUINavigation::Next), EUINavigationRule::Wrap);
	TestEqual(TEXT("Authored boundary remains authoritative"), Root->Navigation->GetNavigationRule(EUINavigation::Left), EUINavigationRule::Stop);
	Root->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Stop);
	Menu->SetMenuNavigationWrap(false);
	TestEqual(TEXT("Owned defaults restored"), Root->Navigation->GetNavigationRule(EUINavigation::Next), EUINavigationRule::Escape);
	TestEqual(TEXT("Runtime override preserved"), Root->Navigation->GetNavigationRule(EUINavigation::Up), EUINavigationRule::Stop);
	TestEqual(TEXT("Authored wrap was never owned"), Root->Navigation->GetNavigationRule(EUINavigation::Previous), EUINavigationRule::Wrap);
	Menu->SetMenuNavigationWrap(true);
	UWidget* Replacement = Menu->MakeRoot();
	Menu->SetMenuNavigationWrap(true);
	TestEqual(TEXT("Detached root releases owned rules"), Root->Navigation->GetNavigationRule(EUINavigation::Down), EUINavigationRule::Escape);
	TestEqual(TEXT("Replacement receives its own boundary"), Replacement->Navigation->GetNavigationRule(EUINavigation::Down), EUINavigationRule::Wrap);
	Replacement->Navigation = NewObject<UWidgetNavigation>(Replacement);
	Replacement->Navigation->Down.Rule = EUINavigationRule::Explicit;
	Menu->SetMenuNavigationWrap(false);
	TestEqual(TEXT("Replacing navigation object retires old ownership"), Replacement->Navigation->GetNavigationRule(EUINavigation::Down), EUINavigationRule::Explicit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNarrativeButtonAccessibility,
	"ProjectVelkorran.UI.Menu.NativeButtonAccessibleLabels",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FNarrativeButtonAccessibility::RunTest(const FString& Parameters)
{
	UNarrativeFrontendTestButton* Button = NewObject<UNarrativeFrontendTestButton>();
	Button->BuildNativeButton();
	Button->SetButtonText(FText::FromString(TEXT("Review authenticated evidence")));
	Button->SynchronizeProperties();
	USlateAccessibleWidgetData* Data = Button->ReadAccessibleData();
	if (!TestNotNull(TEXT("Native accessible data exists without an editor widget"), Data)) { return false; }
	TestEqual(TEXT("Full label is available"), Data->CreateAccessibleTextAttribute().Get().ToString(), FString(TEXT("Review authenticated evidence")));
	Button->SetButtonText(FText::FromString(TEXT("Resume mission")));
	TestEqual(TEXT("Label follows changing/localized text without rebuilding"), Data->CreateAccessibleTextAttribute().Get().ToString(), FString(TEXT("Resume mission")));
	TestFalse(TEXT("Button label does not duplicate its decorative descendants"), Data->bCanChildrenBeAccessible);
#if WITH_ACCESSIBILITY
	TestEqual(TEXT("Actual native Slate button exposes current label"), Button->GetAccessibleText().ToString(), FString(TEXT("Resume mission")));
#endif
	Button->SetAuthoredAccessibleText(FText::FromString(TEXT("Continue from safe point")));
	Button->SynchronizeProperties();
	TestEqual(TEXT("Explicit authored semantics preserved"), Button->ReadAccessibleData()->CreateAccessibleTextAttribute().Get().ToString(), FString(TEXT("Continue from safe point")));
	Button->ReleaseSlateResources(true);
	return true;
}
#endif
