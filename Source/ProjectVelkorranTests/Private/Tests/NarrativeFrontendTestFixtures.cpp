// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/NarrativeFrontendTestFixtures.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/SlateWrapperTypes.h"

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
	UCommonButtonInternalBase* InternalButton = NewObject<UCommonButtonInternalBase>(this);
	RootButton = InternalButton;
	InternalButton->TakeWidget();
}

void UNarrativeFrontendTestButton::SetAuthoredAccessibleText(const FText& Text)
{
	AccessibleWidgetData = NewObject<USlateAccessibleWidgetData>(this);
	AccessibleWidgetData->AccessibleBehavior = ESlateAccessibleBehavior::Custom;
	AccessibleWidgetData->AccessibleSummaryBehavior = ESlateAccessibleBehavior::Custom;
	AccessibleWidgetData->AccessibleText = Text;
	AccessibleWidgetData->AccessibleSummaryText = Text;
#if WITH_EDITORONLY_DATA
	bOverrideAccessibleDefaults = true;
	AccessibleBehavior = ESlateAccessibleBehavior::Custom;
	AccessibleSummaryBehavior = ESlateAccessibleBehavior::Custom;
	AccessibleText = Text;
	AccessibleSummaryText = Text;
	AccessibleTextDelegate.Unbind();
	AccessibleSummaryTextDelegate.Unbind();
#endif
}
