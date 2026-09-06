// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/NarrativeFrontendTestFixtures.h"
#include "Blueprint/WidgetNavigation.h"
#include "Components/SlateWrapperTypes.h"
#include "Misc/AutomationTest.h"
#include "Widgets/SWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNarrativeMenuFocusRouting,
	"ProjectVelkorran.UI.Menu.CommonUIFocusAndReentry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
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
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FNarrativeMenuBoundaryWrap::RunTest(const FString& Parameters)
{
	UNarrativeFrontendTestMenu* Menu = NewObject<UNarrativeFrontendTestMenu>();
	UWidget* Root = Menu->MakeRoot();
	Root->SetNavigationRuleBase(EUINavigation::Left, EUINavigationRule::Stop);
	Root->SetNavigationRuleBase(EUINavigation::Previous, EUINavigationRule::Wrap);
	Menu->SetMenuNavigationWrap(true);
	TestEqual(TEXT("Controller boundary wraps"), Root->Navigation->Down.Rule, EUINavigationRule::Wrap);
	TestEqual(TEXT("Keyboard boundary wraps"), Root->Navigation->Next.Rule, EUINavigationRule::Wrap);
	TestEqual(TEXT("Authored boundary remains authoritative"), Root->Navigation->Left.Rule, EUINavigationRule::Stop);
	Root->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Stop);
	Menu->SetMenuNavigationWrap(false);
	TestEqual(TEXT("Owned defaults restored"), Root->Navigation->Next.Rule, EUINavigationRule::Escape);
	TestEqual(TEXT("Runtime override preserved"), Root->Navigation->Up.Rule, EUINavigationRule::Stop);
	TestEqual(TEXT("Authored wrap was never owned"), Root->Navigation->Previous.Rule, EUINavigationRule::Wrap);
	Menu->SetMenuNavigationWrap(true);
	UWidget* Replacement = Menu->MakeRoot();
	Menu->SetMenuNavigationWrap(true);
	TestEqual(TEXT("Detached root releases owned rules"), Root->Navigation->Down.Rule, EUINavigationRule::Escape);
	TestEqual(TEXT("Replacement receives its own boundary"), Replacement->Navigation->Down.Rule, EUINavigationRule::Wrap);
	Replacement->Navigation = NewObject<UWidgetNavigation>(Replacement);
	Replacement->Navigation->Down.Rule = EUINavigationRule::Explicit;
	Menu->SetMenuNavigationWrap(false);
	TestEqual(TEXT("Replacing navigation object retires old ownership"), Replacement->Navigation->Down.Rule, EUINavigationRule::Explicit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNarrativeButtonAccessibility,
	"ProjectVelkorran.UI.Menu.NativeButtonAccessibleLabels",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FNarrativeButtonAccessibility::RunTest(const FString& Parameters)
{
	UNarrativeFrontendTestButton* Button = NewObject<UNarrativeFrontendTestButton>();
	Button->BuildNativeButton();
	Button->SetButtonText(FText::FromString(TEXT("Review authenticated evidence")));
	Button->SynchronizeProperties();
#if WITH_ACCESSIBILITY
	const TSharedPtr<SWidget> AccessibleWidget = Button->ReadAccessibleWidget();
	if (!TestTrue(TEXT("CommonUI constructed its real accessible Slate button"), AccessibleWidget.IsValid()))
	{
		Button->ReleaseSlateResources(true);
		return false;
	}
	TestTrue(TEXT("Accessible role comes from the native button"), IsValid(Button->GetRootWidget()) && Button->GetRootWidget()->IsA<UCommonButtonInternalBase>());
	TestEqual(TEXT("Full label is available"), Button->GetAccessibleText().ToString(), FString(TEXT("Review authenticated evidence")));
	Button->SetButtonText(FText::FromString(TEXT("Resume mission")));
	TestEqual(TEXT("Label follows changing/localized text without rebuilding"), Button->GetAccessibleText().ToString(), FString(TEXT("Resume mission")));
	TestEqual(TEXT("Accessible summary also follows the current label"), AccessibleWidget->GetAccessibleText(EAccessibleType::Summary).ToString(), FString(TEXT("Resume mission")));
	TestFalse(TEXT("Button label does not duplicate its decorative descendants"), AccessibleWidget->CanChildrenBeAccessible());
#if WITH_EDITORONLY_DATA
	Button->SetAuthoredAccessibleText(FText::FromString(TEXT("Continue from safe point")));
	Button->SynchronizeProperties();
	TestEqual(TEXT("Explicit authored semantics preserved"), Button->GetAccessibleText().ToString(), FString(TEXT("Continue from safe point")));
	Button->AccessibleBehavior = ESlateAccessibleBehavior::NotAccessible;
	Button->SynchronizeProperties();
	TestEqual(TEXT("Authored exclusion survives native defaults"), AccessibleWidget->GetAccessibleBehavior(), EAccessibleBehavior::NotAccessible);
#endif
#else
	TestTrue(TEXT("Disabled engine accessibility exposes no Slate label"), Button->GetAccessibleText().IsEmpty());
#endif
	Button->ReleaseSlateResources(true);
	return true;
}
#endif
