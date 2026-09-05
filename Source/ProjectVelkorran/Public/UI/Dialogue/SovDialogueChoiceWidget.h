// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Widgets/NarrativeMenu.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "SovDialogueChoiceWidget.generated.h"

DECLARE_DELEGATE_OneParam(FSovDialogueChoiceEvent, int32);

/** Native content inside CommonUI's real internal button, retaining its role/navigation/actions. */
UCLASS()
class PROJECTVELKORRAN_API USovDialogueChoiceButton : public UNarrativeCommonButtonBase
{
	GENERATED_BODY()
public:
	void Configure(const FText& Label, float Scale, bool bHighContrast);
	FSimpleDelegate OnFocused;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
};

/** Presentation only: all graph selection remains in UTalesComponent. No Blueprint widget is required. */
UCLASS()
class PROJECTVELKORRAN_API USovDialogueChoiceWidget : public UNarrativeMenu
{
	GENERATED_BODY()
public:
	USovDialogueChoiceWidget();
	void Present(const FText& Speaker, const TArray<FText>& Choices, float Scale, bool bHighContrast);
	void SetTimerText(const FText& Text);
	bool IsTextPresented() const;
	void SetChoicesEnabled(bool bEnabled);
	void Retire();
	FSovDialogueChoiceEvent OnChoiceRequested;
	FSovDialogueChoiceEvent OnSelectionChanged;
	FSimpleDelegate OnPresentationRemoved;
	int32 GetSelectedIndex() const { return SelectedIndex; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void NativeDestruct() override;
	virtual void NativeOnDeactivated() override;
private:
	friend struct FSovDialogueTestAccess;
	void RebuildChoices();
	FText ChoiceLabel(int32 Index) const;
	void Select(int32 Index);
	void RequestChoice(int32 Index);
	void NotifyRemoved();
	UPROPERTY(Transient) TObjectPtr<class UVerticalBox> ChoiceList;
	UPROPERTY(Transient) TObjectPtr<class UTextBlock> SpeakerText;
	UPROPERTY(Transient) TObjectPtr<class UTextBlock> TimerText;
	UPROPERTY(Transient) TObjectPtr<class UBorder> Panel;
	UPROPERTY(Transient) TObjectPtr<class UScrollBox> Scroller;
	UPROPERTY(Transient) TArray<TObjectPtr<USovDialogueChoiceButton>> Buttons;
	FText PresentedSpeaker;
	FText PresentedTimer;
	TArray<FText> PresentedChoices;
	float FontScale = 1.f;
	bool bContrast = false;
	bool bRetired = false;
	int32 SelectedIndex = INDEX_NONE;
};
