// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/Dialogue/SovDialogueChoiceWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "SovDialogueChoices"

TSharedRef<SWidget> USovDialogueChoiceButton::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		ButtonTextBlock = WidgetTree->ConstructWidget<UNarrativeCommonTextBlock>(UNarrativeCommonTextBlock::StaticClass(), TEXT("ChoiceText"));
		ButtonTextBlock->SetAutoWrapText(true);
		WidgetTree->RootWidget = ButtonTextBlock;
	}
	return Super::RebuildWidget();
}

void USovDialogueChoiceButton::Configure(const FText& Label, float Scale, bool bHighContrast)
{
	TakeWidget();
	SetButtonText(Label);
	if (ButtonTextBlock)
	{
		FSlateFontInfo Font = ButtonTextBlock->GetFont();
		Font.Size = FMath::RoundToInt(24.f * Scale);
		ButtonTextBlock->SetFont(Font);
		ButtonTextBlock->SetColorAndOpacity(FSlateColor(bHighContrast ? FLinearColor::Yellow : FLinearColor::White));
	}
}

void USovDialogueChoiceButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnFocused.ExecuteIfBound();
}

USovDialogueChoiceWidget::USovDialogueChoiceWidget()
{
	bDeactivateOnBack = false;
	InputConfig = ENarrativeWidgetInputMode::GameAndMenu;
}

TSharedRef<SWidget> USovDialogueChoiceWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>();
		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>();
		SafeZone->SetContent(Overlay);
		Panel = WidgetTree->ConstructWidget<UBorder>();
		Panel->SetPadding(FMargin(24.f));
		UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(Panel);
		OverlaySlot->SetHorizontalAlignment(HAlign_Center);
		OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
		OverlaySlot->SetPadding(FMargin(24.f));
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetMaxDesiredWidth(1100.f);
		Size->SetMaxDesiredHeight(600.f);
		Panel->SetContent(Size);
		Scroller = WidgetTree->ConstructWidget<UScrollBox>();
		Scroller->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
		Size->SetContent(Scroller);
		ChoiceList = WidgetTree->ConstructWidget<UVerticalBox>();
		Scroller->AddChild(ChoiceList);
		WidgetTree->RootWidget = SafeZone;
	}
	RebuildChoices();
	return Super::RebuildWidget();
}

void USovDialogueChoiceWidget::Present(const FText& Speaker, const TArray<FText>& Choices, float Scale, bool bHighContrast)
{
	PresentedSpeaker = Speaker;
	PresentedChoices = Choices;
	FontScale = FMath::Clamp(Scale, 1.f, 2.f);
	bContrast = bHighContrast;
	bRetired = false;
	SelectedIndex = Choices.IsEmpty() ? INDEX_NONE : 0;
	RebuildChoices();
	RequestRefreshFocus();
}

void USovDialogueChoiceWidget::RebuildChoices()
{
	if (!ChoiceList) { return; }
	ChoiceList->ClearChildren();
	Buttons.Reset();
	Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, bContrast ? 1.f : .9f));
	SpeakerText = WidgetTree->ConstructWidget<UTextBlock>();
	SpeakerText->SetText(FText::Format(LOCTEXT("SpeakerCount", "{0} — {1} choices"), PresentedSpeaker, FText::AsNumber(PresentedChoices.Num())));
	SpeakerText->SetAutoWrapText(true);
	FSlateFontInfo Font = SpeakerText->GetFont();
	Font.Size = FMath::RoundToInt(22.f * FontScale);
	SpeakerText->SetFont(Font);
	ChoiceList->AddChildToVerticalBox(SpeakerText)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	for (int32 Index = 0; Index < PresentedChoices.Num(); ++Index)
	{
		USovDialogueChoiceButton* Button = WidgetTree->ConstructWidget<USovDialogueChoiceButton>();
		Button->Configure(ChoiceLabel(Index), FontScale, bContrast);
		Button->OnFocused.BindUObject(this, &ThisClass::Select, Index);
		Button->OnClicked().AddUObject(this, &ThisClass::RequestChoice, Index);
		ChoiceList->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.f, 6.f));
		Buttons.Add(Button);
	}
	TimerText = WidgetTree->ConstructWidget<UTextBlock>();
	TimerText->SetText(PresentedTimer);
	TimerText->SetFont(Font);
	TimerText->SetAutoWrapText(true);
	ChoiceList->AddChildToVerticalBox(TimerText)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
}

FText USovDialogueChoiceWidget::ChoiceLabel(int32 Index) const
{
	const FText Numbered = FText::Format(LOCTEXT("NumberedChoice", "{0}. {1}"), FText::AsNumber(Index + 1), PresentedChoices[Index]);
	return Index == SelectedIndex ? FText::Format(LOCTEXT("SelectedChoiceLabel", "Selected: {0}"), Numbered) : Numbered;
}

void USovDialogueChoiceWidget::SetTimerText(const FText& Text)
{
	PresentedTimer = Text;
	if (TimerText) { TimerText->SetText(Text); }
}

bool USovDialogueChoiceWidget::IsTextPresented() const
{
	if (bRetired || !IsActivated() || !IsVisible() || !ChoiceList || Buttons.IsEmpty()
		|| GetCachedGeometry().GetLocalSize().GetMin() <= 0.f) { return false; }
	for (const USovDialogueChoiceButton* Button : Buttons)
	{
		if (!Button || !Button->GetCachedWidget().IsValid()) { return false; }
	}
	return true;
}

void USovDialogueChoiceWidget::SetChoicesEnabled(bool bEnabled)
{
	for (USovDialogueChoiceButton* Button : Buttons) { if (Button) { Button->SetIsEnabled(bEnabled); } }
}

UWidget* USovDialogueChoiceWidget::NativeGetDesiredFocusTarget() const
{
	return Buttons.IsValidIndex(SelectedIndex) ? Buttons[SelectedIndex].Get() : nullptr;
}

void USovDialogueChoiceWidget::Select(int32 Index)
{
	if (bRetired || !Buttons.IsValidIndex(Index)) { return; }
	SelectedIndex = Index;
	// Explicit localized text retains a non-color focus state even without an authored button style.
	for (int32 ButtonIndex = 0; ButtonIndex < Buttons.Num(); ++ButtonIndex)
	{ Buttons[ButtonIndex]->SetButtonText(ChoiceLabel(ButtonIndex)); }
	if (Scroller) { Scroller->ScrollWidgetIntoView(Buttons[Index], false); }
	OnSelectionChanged.ExecuteIfBound(Index);
}

void USovDialogueChoiceWidget::RequestChoice(int32 Index)
{
	if (!bRetired && Buttons.IsValidIndex(Index) && Buttons[Index]->GetIsEnabled())
	{ OnChoiceRequested.ExecuteIfBound(Index); }
}

void USovDialogueChoiceWidget::NotifyRemoved()
{
	if (bRetired) { return; }
	bRetired = true;
	OnPresentationRemoved.ExecuteIfBound();
}

void USovDialogueChoiceWidget::Retire()
{
	bRetired = true;
	OnChoiceRequested.Unbind();
	OnSelectionChanged.Unbind();
	OnPresentationRemoved.Unbind();
	SetChoicesEnabled(false);
	DeactivateWidget();
}

void USovDialogueChoiceWidget::NativeDestruct() { NotifyRemoved(); Super::NativeDestruct(); }
void USovDialogueChoiceWidget::NativeOnDeactivated() { NotifyRemoved(); Super::NativeOnDeactivated(); }
#undef LOCTEXT_NAMESPACE
