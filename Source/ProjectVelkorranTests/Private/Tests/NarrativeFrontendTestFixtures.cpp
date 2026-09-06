// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/NarrativeFrontendTestFixtures.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/Spacer.h"

UWidget* UNarrativeFrontendTestMenu::MakeRoot()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this);
	}
	WidgetTree->RootWidget = WidgetTree->ConstructWidget<UVerticalBox>();
	return WidgetTree->RootWidget;
}

void UNarrativeFrontendTestMenu::NativeRegisterActions()
{
	++RegistrationCalls;
	if (bDeactivateDuringRegistration)
	{
		bDeactivateDuringRegistration = false;
		DeactivateWidget();
		if (bReactivateDuringRegistration)
		{
			bReactivateDuringRegistration = false;
			ActivateWidget();
		}
	}
}

UWidget* UNarrativeFrontendTestMenu::NativeGetDesiredFocusTarget() const
{
	++DirectFocusQueries;
	return nullptr;
}

void UNarrativeFrontendTestButton::BuildNativeButton()
{
	if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this); }
	WidgetTree->RootWidget = WidgetTree->ConstructWidget<USpacer>();
	// CommonUI builds and owns its real internal button around this content.
	Initialize();
	NativeTestWidget = TakeWidget();
}

void UNarrativeFrontendTestButton::ReleaseSlateResources(bool bReleaseChildren)
{
	NativeTestWidget.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UNarrativeFrontendTestButton::SetAuthoredAccessibleText(const FText& Text)
{
#if WITH_EDITORONLY_DATA
	bOverrideAccessibleDefaults = true;
	AccessibleBehavior = ESlateAccessibleBehavior::Custom;
	AccessibleSummaryBehavior = ESlateAccessibleBehavior::Custom;
	AccessibleText = Text;
	AccessibleSummaryText = Text;
	AccessibleTextDelegate.Unbind();
	AccessibleSummaryTextDelegate.Unbind();
	SynchronizeAccessibleData();
#endif
}
