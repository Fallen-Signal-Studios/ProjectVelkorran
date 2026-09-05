// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovApplicationInterruptionMenu.h"
#include "UI/SovAccessibilitySettingsMenu.h"
#include "Framework/SovApplicationLifecycleComponent.h"
#include "Accessibility/SovAccessibleNarrationSubsystem.h"
#include "Settings/SovGameUserSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SovInterruptionMenu"
USovApplicationInterruptionMenu::USovApplicationInterruptionMenu() { InputConfig = ENarrativeWidgetInputMode::Menu; }
TSharedRef<SWidget> USovApplicationInterruptionMenu::RebuildWidget()
{
    if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree")); }
    if (!Message)
    {
        auto* Safe = WidgetTree->ConstructWidget<USafeZone>();
        auto* Background = WidgetTree->ConstructWidget<UBorder>();
        Background->SetBrushColor(FLinearColor(.015f, .02f, .025f, .99f)); Background->SetPadding(FMargin(30));
        auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
        auto* Box = WidgetTree->ConstructWidget<UVerticalBox>();
        Message = WidgetTree->ConstructWidget<UTextBlock>(); Message->SetAutoWrapText(true); Message->SetMargin(FMargin(10, 20));
        Box->AddChild(Message);
        ResumeButton = WidgetTree->ConstructWidget<USovAccessibilityNativeButton>();
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(LOCTEXT("Resume", "Resume game"));
        Label->SetAutoWrapText(true); Label->SetMargin(FMargin(16)); ResumeButton->AddChild(Label);
        ResumeButton->SetAccessibleLabel(Label->GetText()); ResumeButton->OnClicked.AddDynamic(this, &ThisClass::Resume);
        Box->AddChild(ResumeButton); Scroll->AddChild(Box); Background->AddChild(Scroll); Safe->AddChild(Background);
        WidgetTree->RootWidget = Safe;
    }
    RefreshMessage(false); return Super::RebuildWidget();
}
void USovApplicationInterruptionMenu::RefreshMessage(bool bAnnounce)
{
    const auto* PC = GetOwningPlayer();
    const auto* Lifecycle = PC ? PC->FindComponentByClass<USovApplicationLifecycleComponent>() : nullptr;
    if (!Lifecycle || !Message || !ResumeButton) { return; }
    const auto* Settings = USovGameUserSettings::Get();
    const auto Snapshot = Settings ? Settings->GetSettingsSnapshot() : FSovUserSettingsSnapshot();
    const FText Current = Lifecycle->GetInterruptionMessage(); const bool bChanged = !Current.EqualTo(LastMessage);
    LastMessage = Current; Message->SetText(Current);
    const auto Font = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(22 * Snapshot.UIScale));
    Message->SetFont(Font); Message->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    ResumeButton->SetBackgroundColor(FLinearColor::Black);
    // Retain an accessible focus target while input/account recovery is pending.
    // The click boundary revalidates availability and does not dismiss on failure.
    if (auto* Label = Cast<UTextBlock>(ResumeButton->GetContent()))
    { Label->SetFont(Font); Label->SetColorAndOpacity(FSlateColor(FLinearColor::White)); }
    if ((bAnnounce || bChanged) && IsActivated() && Snapshot.bMenuNarration && GetOwningLocalPlayer())
    {
        if (auto* Narrator = GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>())
        { FGuid Request; Narrator->Announce(this, Current, Request); }
    }
}
void USovApplicationInterruptionMenu::NativeOnActivated() { Super::NativeOnActivated(); RefreshMessage(true); }
void USovApplicationInterruptionMenu::NativeOnDeactivated()
{
    Super::NativeOnDeactivated();
    if (!IsActivated() && GetOwningLocalPlayer())
    { if (auto* Narrator = GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->Cancel(this); } }
}
void USovApplicationInterruptionMenu::NativeTick(const FGeometry& Geometry, float Delta)
{ Super::NativeTick(Geometry, Delta); if (IsActivated()) { RefreshMessage(false); } }
UWidget* USovApplicationInterruptionMenu::NativeGetDesiredFocusTarget() const { return ResumeButton; }
void USovApplicationInterruptionMenu::Resume()
{
    if (!IsActivated()) { return; }
    if (const auto* PC = GetOwningPlayer())
    { if (auto* Lifecycle = PC->FindComponentByClass<USovApplicationLifecycleComponent>()) { Lifecycle->ResumeGameplay(); } }
    if (IsActivated()) { RefreshMessage(true); }
}
#undef LOCTEXT_NAMESPACE
