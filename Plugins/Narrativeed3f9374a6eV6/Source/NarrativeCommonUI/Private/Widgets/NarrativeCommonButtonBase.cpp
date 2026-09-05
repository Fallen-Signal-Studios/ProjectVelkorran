// Copyright Narrative Tools 2024. 


#include "Widgets/NarrativeCommonButtonBase.h"
#include "Components/SlateWrapperTypes.h"

UNarrativeCommonButtonBase::UNarrativeCommonButtonBase()
{
	TextJustification = ETextJustify::Left;
	ButtonText = NSLOCTEXT("NarrativeCommonButtonBase", "ButtonDefaultText", "Button Text");
}

void UNarrativeCommonButtonBase::ForceSetIsSelected(const bool bInSelected, bool bAllowSound, bool bBroadcast)
{
	SetSelectedInternal(bInSelected, bAllowSound, bBroadcast);
}

void UNarrativeCommonButtonBase::SetButtonText(FText InText)
{
	ButtonText = InText;

	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetText(ButtonText);
	}
}

FText UNarrativeCommonButtonBase::GetDefaultAccessibleButtonText() const
{
	return ButtonText;
}

void UNarrativeCommonButtonBase::SynchronizeProperties()
{
	//Respect explicitly authored accessibility, including deliberate exclusion.
	//UMG editor properties are stripped from cooked builds, so the native fallback
	//must also populate the real serialized/runtime accessible data container.
#if WITH_EDITORONLY_DATA
	if (!bOverrideAccessibleDefaults)
	{
		bOverrideAccessibleDefaults = true;
		AccessibleBehavior = ESlateAccessibleBehavior::Custom;
		AccessibleSummaryBehavior = ESlateAccessibleBehavior::Custom;
		bCanChildrenBeAccessible = false;
		AccessibleTextDelegate.BindDynamic(this, &UNarrativeCommonButtonBase::GetDefaultAccessibleButtonText);
		AccessibleSummaryTextDelegate.BindDynamic(this, &UNarrativeCommonButtonBase::GetDefaultAccessibleButtonText);
	}
#endif
	if (!AccessibleWidgetData)
	{
		AccessibleWidgetData = NewObject<USlateAccessibleWidgetData>(this);
		AccessibleWidgetData->AccessibleBehavior = ESlateAccessibleBehavior::Custom;
		AccessibleWidgetData->AccessibleSummaryBehavior = ESlateAccessibleBehavior::Custom;
		AccessibleWidgetData->bCanChildrenBeAccessible = false;
		AccessibleWidgetData->AccessibleTextDelegate.BindDynamic(this, &UNarrativeCommonButtonBase::GetDefaultAccessibleButtonText);
		AccessibleWidgetData->AccessibleSummaryTextDelegate.BindDynamic(this, &UNarrativeCommonButtonBase::GetDefaultAccessibleButtonText);
	}
	Super::SynchronizeProperties();
}

#if WITH_ACCESSIBILITY
TSharedPtr<SWidget> UNarrativeCommonButtonBase::GetAccessibleWidget() const
{
	//Expose the real button role/action instead of a generic SObjectWidget wrapper.
	if (const UCommonButtonInternalBase* Button = RootButton.Get())
	{
		return Button->GetCachedWidget();
	}
	return Super::GetAccessibleWidget();
}
#endif

void UNarrativeCommonButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	SetButtonText(ButtonText);

	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetJustification(TextJustification);
	}
}

void UNarrativeCommonButtonBase::NativeOnCurrentTextStyleChanged()
{
	Super::NativeOnCurrentTextStyleChanged();

	if (ButtonTextBlock)
	{
		ButtonTextBlock->SetStyle(GetCurrentTextStyleClass());
	}
}
