// Copyright Narrative Tools 2024. 


#include "Widgets/NarrativeCommonButtonBase.h"
#include "Widgets/SWidget.h"

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
	// Native Slate defaults also exist in cooked builds. UWidget applies any
	// serialized/authored override afterward, including deliberate exclusion.
#if WITH_ACCESSIBILITY
	if (const TSharedPtr<SWidget> AccessibleWidget = GetAccessibleWidget())
	{
		const TAttribute<FText> Label = TAttribute<FText>::Create(
			TAttribute<FText>::FGetter::CreateUObject(this, &ThisClass::GetDefaultAccessibleButtonText));
		AccessibleWidget->SetAccessibleBehavior(EAccessibleBehavior::Custom, Label, EAccessibleType::Main);
		AccessibleWidget->SetAccessibleBehavior(EAccessibleBehavior::Custom, Label, EAccessibleType::Summary);
		AccessibleWidget->SetCanChildrenBeAccessible(false);
	}
#endif
	Super::SynchronizeProperties();
}

#if WITH_ACCESSIBILITY
TSharedPtr<SWidget> UNarrativeCommonButtonBase::GetAccessibleWidget() const
{
	//Expose the real button role/action instead of a generic SObjectWidget wrapper.
	if (const UCommonButtonInternalBase* Button = Cast<UCommonButtonInternalBase>(GetRootWidget()))
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
