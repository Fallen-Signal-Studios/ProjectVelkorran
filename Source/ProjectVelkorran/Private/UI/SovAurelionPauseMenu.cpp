// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovAurelionPauseMenu.h"
#include "UI/SovAccessibilitySettingsMenu.h"
#include "Framework/SovPlayerController.h"
#include "Save/SovSaveSubsystem.h"
#include "Settings/SovGameUserSettings.h"
#include "Accessibility/SovAccessibleNarrationSubsystem.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "NarrativeGameplayTags.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "SovAurelionPause"
USovAurelionPauseMenu::USovAurelionPauseMenu()
{ InputConfig = ENarrativeWidgetInputMode::Menu; bIsBackHandler = true; }

TSharedRef<SWidget> USovAurelionPauseMenu::RebuildWidget()
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
        const auto Add = [&](const FText& Text)
        {
            auto* Button = WidgetTree->ConstructWidget<USovAccessibilityNativeButton>();
            auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
            Label->SetText(Text); Label->SetAutoWrapText(true); Label->SetMargin(FMargin(16));
            Button->AddChild(Label); Button->SetAccessibleLabel(Text); Box->AddChild(Button); Buttons.Add(Button);
            return Button;
        };
        ResumeButton = Add(LOCTEXT("Resume", "Resume game"));
        ResumeButton->OnClicked.AddDynamic(this, &ThisClass::Resume);
        LoadButton = Add(LOCTEXT("Load", "Load checkpoint"));
        LoadLabel = Cast<UTextBlock>(LoadButton->GetContent());
        LoadButton->OnClicked.AddDynamic(this, &ThisClass::LoadCheckpoint);
        auto* SettingsButton = Add(LOCTEXT("Settings", "Accessibility and settings"));
        SettingsButton->OnClicked.AddDynamic(this, &ThisClass::Settings);
        auto* QuitButton = Add(LOCTEXT("Quit", "Quit game"));
        QuitButton->OnClicked.AddDynamic(this, &ThisClass::Quit);
        Scroll->AddChild(Box); Background->AddChild(Scroll); Safe->AddChild(Background); WidgetTree->RootWidget = Safe;
    }
    return Super::RebuildWidget();
}

bool USovAurelionPauseMenu::IsAurelionCheckpoint(const FSovSaveSlotHeader& Header, const FString& Account)
{
    return !Account.IsEmpty() && Header.AccountNamespace == Account && Header.Kind == ESovSaveSlotKind::Checkpoint
        && Header.SlotIndex == 0 && Header.Generation > 0
        && ((Header.MissionId == TEXT("M12_FireAndFrost") && Header.MapPackage == TEXT("/Game/Aurelion/Maps/L_Aurelion_M12"))
            || (Header.MissionId == TEXT("M13_ContraryWitness") && Header.MapPackage == TEXT("/Game/Aurelion/Maps/L_Aurelion_M13")));
}
bool USovAurelionPauseMenu::SameCheckpoint(const FSovSaveSlotHeader& A, const FSovSaveSlotHeader& B)
{
    return A.AccountNamespace == B.AccountNamespace && A.Kind == B.Kind && A.SlotIndex == B.SlotIndex
        && A.Generation == B.Generation && A.TimestampUtc == B.TimestampUtc && A.MissionId == B.MissionId
        && A.MapPackage == B.MapPackage && A.BoundaryId == B.BoundaryId && A.BoundaryKind == B.BoundaryKind
        && A.MissionDefinition == B.MissionDefinition && A.ActiveProtagonist == B.ActiveProtagonist;
}
bool USovAurelionPauseMenu::IsCurrent(uint64 Generation, const ASovPlayerController* PC, const USovSaveSubsystem* Save) const
{
    return IsActivated() && MenuGeneration == Generation && IsValid(PC) && !PC->IsActorBeingDestroyed()
        && PC == GetOwningPlayer() && PausedController.Get() == PC && PC->IsLocalController() && PC->HasAuthority()
        && PC->GetNetMode() == NM_Standalone && IsValid(Save) && BoundSave.Get() == Save
        && PC->GetGameInstance() == Save->GetGameInstance() && PC->GetGameInstance()
        && PC->GetGameInstance()->GetFirstLocalPlayerController() == PC && PC->GetWorld() == GetWorld();
}
void USovAurelionPauseMenu::Present(const FText& Text)
{
    if (!Message) { return; }
    Message->SetText(Text);
    const auto* SettingsValue = USovGameUserSettings::Get();
    const auto Snapshot = SettingsValue ? SettingsValue->GetSettingsSnapshot() : FSovUserSettingsSnapshot();
    const auto Font = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(22 * Snapshot.UIScale));
    Message->SetFont(Font); Message->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    for (const auto& Button : Buttons)
    {
        Button->SetBackgroundColor(FLinearColor::Black);
        if (auto* Label = Cast<UTextBlock>(Button->GetContent()))
        { Label->SetFont(Font); Label->SetColorAndOpacity(FSlateColor(FLinearColor::White)); }
    }
    if (IsActivated() && Snapshot.bMenuNarration && GetOwningLocalPlayer())
    {
        if (auto* Narrator = GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>())
        { FGuid Request; Narrator->Announce(this, Text, Request); }
    }
}
void USovAurelionPauseMenu::RefreshCheckpoint()
{
    bHasCheckpoint = false; bAcceptRecovery = false;
    if (LoadButton) { LoadButton->SetIsEnabled(false); }
    if (LoadLabel) { LoadLabel->SetText(LOCTEXT("Load", "Load checkpoint")); LoadButton->SetAccessibleLabel(LoadLabel->GetText()); }
    const uint64 Generation = MenuGeneration;
    TStrongObjectPtr<ASovPlayerController> PC(PausedController.Get());
    TStrongObjectPtr<USovSaveSubsystem> Save(BoundSave.Get());
    if (!IsCurrent(Generation, PC.Get(), Save.Get()) || !Save->IsPlatformStorageOwnerAvailable() || Save->IsPlatformStorageSuspended())
    { Present(LOCTEXT("Unavailable", "Checkpoint storage is currently unavailable. Resume to continue playing.")); return; }
    if (Save->IsLoadPending()) { Present(LOCTEXT("Pending", "A campaign load is already in progress.")); return; }
    const auto Headers = Save->ListSlots();
    if (!IsCurrent(Generation, PC.Get(), Save.Get())) { return; }
    for (const auto& Header : Headers)
    {
        if (IsAurelionCheckpoint(Header, Save->GetAccountNamespace()))
        { DisplayedCheckpoint = Header; bHasCheckpoint = true; break; }
    }
    if (LoadButton) { LoadButton->SetIsEnabled(bHasCheckpoint && bOwnPause); }
    if (!bHasCheckpoint) { Present(LOCTEXT("Missing", "No Aurelion checkpoint is available in this profile.")); return; }
    Present(FText::Format(LOCTEXT("Checkpoint", "Aurelion — paused\nCheckpoint: {0}\nSaved {1} UTC\nLoading discards progress since this checkpoint. An encounter entry may require the nearby Retry encounter control after loading."),
        DisplayedCheckpoint.MissionLabel.IsEmpty() ? FText::FromName(DisplayedCheckpoint.MissionId) : DisplayedCheckpoint.MissionLabel,
        FText::AsDateTime(DisplayedCheckpoint.TimestampUtc)));
}
void USovAurelionPauseMenu::NativeOnActivated()
{
    const uint64 Generation = ++MenuGeneration;
    Super::NativeOnActivated();
    if (!IsActivated() || Generation != MenuGeneration) { return; }
    auto* PC = Cast<ASovPlayerController>(GetOwningPlayer());
    if (!IsValid(PC) || !PC->IsLocalController() || !PC->HasAuthority() || PC->GetNetMode() != NM_Standalone)
    { DeactivateWidget(); return; }
    PausedController = PC;
    // Per-instance name prevents a second menu from releasing this contribution.
    const FName Owner(*FString::Printf(TEXT("AurelionPause_%u_%llu"), GetUniqueID(), Generation));
    PauseOwner = Owner;
    const bool bAcquired = PC->AcquireSystemPause(Owner);
    if (!IsActivated() || Generation != MenuGeneration || PausedController.Get() != PC)
    { if (bAcquired && IsValid(PC)) { PC->ReleaseSystemPause(Owner); } return; }
    bOwnPause = bAcquired;
    BoundSave = PC->GetGameInstance() ? PC->GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr;
    RefreshCheckpoint();
}
void USovAurelionPauseMenu::Retire()
{
    ++MenuGeneration;
    if (BoundSave.IsValid()) { BoundSave->OnLoadCompleted.RemoveDynamic(this, &ThisClass::OnLoadResult); }
    BoundSave.Reset(); bSubmitting = false; bHasCheckpoint = false; bAcceptRecovery = false;
    const auto PC = PausedController; const bool bRelease = bOwnPause; const FName Owner = PauseOwner;
    PausedController.Reset(); bOwnPause = false; PauseOwner = NAME_None;
    if (bRelease && PC.IsValid()) { PC->ReleaseSystemPause(Owner); }
    if (GetOwningLocalPlayer())
    { if (auto* Narrator = GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->Cancel(this); } }
}
void USovAurelionPauseMenu::NativeOnDeactivated() { Retire(); Super::NativeOnDeactivated(); }
void USovAurelionPauseMenu::NativeDestruct() { Retire(); Super::NativeDestruct(); }
UWidget* USovAurelionPauseMenu::NativeGetDesiredFocusTarget() const { return ResumeButton; }
bool USovAurelionPauseMenu::NativeOnHandleBackAction() { Resume(); return true; }
void USovAurelionPauseMenu::Resume() { if (IsActivated() && !bSubmitting) { DeactivateWidget(); } }

void USovAurelionPauseMenu::OnLoadResult(ESovSaveResult Result, const FSovSaveSlotHeader& Header, const FString& Error)
{
    // Only the synchronous damaged-bank offer belongs to this click. Destination completion belongs to the native loader.
    if (!bSubmitting || Result != ESovSaveResult::RecoveryAvailable
        || !IsCurrent(MenuGeneration, PausedController.Get(), BoundSave.Get())
        || !IsAurelionCheckpoint(Header, BoundSave->GetAccountNamespace())) { return; }
    DisplayedCheckpoint = Header; bAcceptRecovery = true;
    if (LoadLabel) { LoadLabel->SetText(LOCTEXT("Recover", "Recover displayed checkpoint")); LoadButton->SetAccessibleLabel(LoadLabel->GetText()); }
    Present(FText::Format(LOCTEXT("Recovery", "{0}\nVerified checkpoint: {1}\nSaved {2} UTC\nChoose Recover displayed checkpoint to accept this previous state."),
        FText::FromString(Error), Header.MissionLabel.IsEmpty() ? FText::FromName(Header.MissionId) : Header.MissionLabel,
        FText::AsDateTime(Header.TimestampUtc)));
}
void USovAurelionPauseMenu::LoadCheckpoint()
{
    if (bSubmitting || !bHasCheckpoint || !bOwnPause) { return; }
    const uint64 Generation = MenuGeneration;
    TStrongObjectPtr<USovAurelionPauseMenu> KeepMenu(this);
    TStrongObjectPtr<ASovPlayerController> PC(PausedController.Get());
    TStrongObjectPtr<USovSaveSubsystem> Save(BoundSave.Get());
    if (!IsCurrent(Generation, PC.Get(), Save.Get())) { return; }
    if (Save->IsLoadPending() || Save->IsPlatformStorageSuspended() || !Save->IsPlatformStorageOwnerAvailable())
    { Present(LOCTEXT("Busy", "Checkpoint loading is unavailable while another load or storage interruption is active.")); return; }
    const FSovSaveSlotHeader Expected = DisplayedCheckpoint;
    const bool bRecover = bAcceptRecovery;
    // Re-read the public headers immediately before accepting the displayed state.
    const auto Headers = Save->ListSlots();
    if (!IsCurrent(Generation, PC.Get(), Save.Get())) { return; }
    const bool bSame = Headers.ContainsByPredicate([&](const FSovSaveSlotHeader& Header)
    { return IsAurelionCheckpoint(Header, Save->GetAccountNamespace()) && SameCheckpoint(Header, Expected); });
    if (!bSame)
    {
        RefreshCheckpoint();
        if (IsCurrent(Generation, PC.Get(), Save.Get()) && Message)
        { Present(FText::Format(LOCTEXT("Changed", "The checkpoint changed. Review it before loading.\n{0}"), Message->GetText())); }
        return;
    }
    bSubmitting = true; bAcceptRecovery = false;
    Save->OnLoadCompleted.AddUniqueDynamic(this, &ThisClass::OnLoadResult);
    FString Error;
    const ESovSaveResult Result = Save->LoadSlot(ESovSaveSlotKind::Checkpoint, 0, Error, bRecover);
    Save->OnLoadCompleted.RemoveDynamic(this, &ThisClass::OnLoadResult);
    if (!IsCurrent(Generation, PC.Get(), Save.Get())) { return; }
    bSubmitting = false;
    if (Result == ESovSaveResult::LoadStarted) { DeactivateWidget(); return; }
    if (Result != ESovSaveResult::RecoveryAvailable || !bAcceptRecovery)
    {
        if (LoadLabel) { LoadLabel->SetText(LOCTEXT("Load", "Load checkpoint")); LoadButton->SetAccessibleLabel(LoadLabel->GetText()); }
        Present(FText::FromString(Error.IsEmpty() ? TEXT("The checkpoint could not be loaded. Resume or try again.") : Error));
    }
}
void USovAurelionPauseMenu::Settings()
{
    if (!IsActivated() || bSubmitting) { return; }
    if (auto* PC = PausedController.Get())
    {
        if (auto* HUD = PC->GetNarrativeGameplayHUD())
        { HUD->OpenMenu(USovAccessibilitySettingsMenu::StaticClass(), FNarrativeGameplayTags::Get().UI_Layer_Modal); }
    }
}
void USovAurelionPauseMenu::Quit()
{
    if (IsActivated() && !bSubmitting)
    { UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false); }
}
#undef LOCTEXT_NAMESPACE
