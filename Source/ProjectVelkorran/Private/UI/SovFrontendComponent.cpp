// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovFrontendComponent.h"
#include "UI/SovCombatVitalsWidget.h"
#include "UI/SovHolographicHUDWidget.h"
#include "UI/SovHolographicHUDSurface.h"
#include "UI/SovAccessibilityPresentation.h"
#include "UI/SovAccessibilitySettingsMenu.h"
#include "Framework/SovPlayerController.h"
#include "Narrative/SovNarrativeCueComponent.h"
#include "Settings/SovGameUserSettings.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Tales/TalesComponent.h"
#include "AI/NPCDefinition.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "NarrativeGameplayTags.h"
#include "Engine/World.h"
#include "AudioDeviceHandle.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Save/SovSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Platform/SovPlatformServicesSubsystem.h"

#define LOCTEXT_NAMESPACE "SovNativeFrontend"
USovFrontendComponent::USovFrontendComponent()
{
    bAutoActivate = true;
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bTickEvenWhenPaused = true;
    PrimaryComponentTick.TickInterval = 0.f;
}
void USovFrontendComponent::BeginPlay() { Super::BeginPlay(); RefreshFrontend(); }
bool USovFrontendComponent::IsInitialAccessibilitySetupPending(const APlayerController* Player)
{
    if (IsRunningCommandlet() || !FSlateApplication::IsInitialized() || !IsValid(Player) || !Player->IsLocalController()) { return false; }
    const ULocalPlayer* LocalPlayer = Player->GetLocalPlayer();
    if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport) { return false; }
    const USovGameUserSettings* Settings = USovGameUserSettings::Get();
    return Settings && !Settings->HasCompletedAccessibilitySetup();
}
void USovFrontendComponent::RefreshFrontend()
{
    auto* PC = Cast<ASovPlayerController>(GetOwner());
    if (bEnding || !IsActive() || !PC || !PC->IsLocalController() || !PC->GetLocalPlayer()
        || IsRunningCommandlet() || !FSlateApplication::IsInitialized())
    { UnbindObjectives(); RemoveCombatVitals(); RemoveHolographicHUD(); return; }
    // The holographic surface carries the resources itself, so the plain readout stands down while
    // it is up rather than drawing a second set of bars over the same information.
    if (bShowHolographicHUD)
    {
        if (!HolographicHUD)
        {
            HolographicHUD = CreateWidget<USovHolographicHUDWidget>(PC, USovHolographicHUDWidget::StaticClass());
            // Beneath the subtitle and caption surface (-1), so speech is never drawn under a readout.
            // An earlier capture suggested a negative depth drew nothing; that was the screenshot path
            // excluding UI, not the depth.
            if (HolographicHUD) { HolographicHUD->AddToPlayerScreen(-2); }
        }
        if (HolographicHUD)
        {
            // Resolved each refresh rather than cached: a soft class is not loaded when the HUD is created,
            // and an author swapping the widget should take effect without restarting the session.
            HolographicHUD->SetSurfaceClass(HolographicHUDSurfaceClass.IsNull()
                ? nullptr : TSubclassOf<USovHolographicHUDSurface>(HolographicHUDSurfaceClass.LoadSynchronous()));
            HolographicHUD->RefreshHolographicHUD();
        }
    }
    else { RemoveHolographicHUD(); }
    if (bShowCombatVitals && !bShowHolographicHUD)
    {
        if (!CombatVitals)
        {
            CombatVitals = CreateWidget<USovCombatVitalsWidget>(PC, USovCombatVitalsWidget::StaticClass());
            if (CombatVitals) { CombatVitals->AddToPlayerScreen(-1); }
        }
        if (CombatVitals) { CombatVitals->RefreshVitals(); }
    }
    else { RemoveCombatVitals(); }
    if (!Presentation)
    {
        Presentation = CreateWidget<USovAccessibilityPresentation>(PC, USovAccessibilityPresentation::StaticClass());
        // HUD menus/modal layers render above this non-interactive gameplay overlay.
        if (Presentation) { Presentation->AddToPlayerScreen(-1); }
    }
    if (Presentation)
    {
        // Both surfaces share the text safe area, and text keeps clear of what the HUD draws.
        if (HolographicHUD) { HolographicHUD->SetSafeAreaSource(Presentation); }
        const bool bHUDShown = HolographicHUD && HolographicHUD->GetVisibility() != ESlateVisibility::Collapsed;
        Presentation->SetHolographicHUDClearance(bHUDShown, bHUDShown && HolographicHUD->GetDisplayed().AmmoInClip >= 0);
    }
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(PC->GetAbilitySystemComponent());
    if (ASC && ASC->GetAvatarActor() != PC->GetPawn()) { ASC = nullptr; }
    BindProducers(PC->GetTalesComponent(), PC->GetNarrativeCues(), ASC);
    auto* Save = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr;
    if (BoundSave.Get() != Save)
    {
        if (BoundSave.IsValid()) { BoundSave->OnLoadCompleted.RemoveDynamic(this, &ThisClass::OnLoadCompleted); }
        BoundSave = Save;
        if (Save)
        {
            Save->OnLoadCompleted.AddUniqueDynamic(this, &ThisClass::OnLoadCompleted);
            bRecoveryMenuPending = Save->HasTravelRecovery() && !Save->IsLoadPending();
        }
    }
    if (bRecoveryMenuPending && Save && Save->HasTravelRecovery() && !Save->IsLoadPending() && OpenAccessibilitySettings())
    { bRecoveryMenuPending = false; SetupMenu->PresentTravelRecovery(RecoveryMessage); }
    BindObjectives(PC, PC->GetCampaignState());
    RefreshObjectives(false);
    auto* Settings = USovGameUserSettings::Get();
    UWorld* World = GetWorld();
    if (Settings && World && World->bAllowAudioPlayback)
    {
        const FAudioDeviceHandle Device = World->GetAudioDevice();
        if (Device.IsValid() && (AudioAppliedWorld.Get() != World || AudioAppliedDevice != Device.GetDeviceID()))
        {
            // Early settings load may precede a playable device. Retry only until one exists,
            // then apply once per world/device; subsequent edits are handled by settings setters.
            AudioAppliedWorld = World; AudioAppliedDevice = Device.GetDeviceID();
            Settings->ApplySoundSettings();
            if (bEnding || !IsValid(PC)) { return; }
        }
    }
    if (PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle)
    { ReleaseSetupPause(); return; }
    if (Settings && !Settings->HasCompletedAccessibilitySetup() && PC->GetNarrativeGameplayHUD())
    {
        if ((!SetupMenu || !SetupMenu->IsActivated()) && OpenAccessibilitySettings()) { SetupMenu->SetFirstBoot(true); }
        if (SetupMenu && SetupMenu->IsActivated() && !bOwnSetupPause)
        { PausedController = PC; bOwnSetupPause = PC->AcquireSystemPause(TEXT("AccessibilitySetup")); }
    }
    else { ReleaseSetupPause(); }
}
void USovFrontendComponent::BindObjectives(ASovPlayerController* Controller, USovCampaignStateComponent* Campaign)
{
    auto* Platform = GetWorld() && GetWorld()->GetGameInstance()
        ? GetWorld()->GetGameInstance()->GetSubsystem<USovPlatformServicesSubsystem>() : nullptr;
    if (ObjectiveController.Get() == Controller && BoundCampaign.Get() == Campaign && ObjectivePlatform.Get() == Platform) { return; }
    UnbindObjectives();
    if (bEnding || !IsValid(Controller) || !IsValid(Campaign) || Campaign->GetOwner() != Controller) { return; }
    ObjectiveController = Controller; BoundCampaign = Campaign; ObjectivePlatform = Platform;
    ObjectiveAccountNamespace = BoundSave.IsValid() ? BoundSave->GetAccountNamespace() : FString();
    ObjectiveAccountUserIndex = BoundSave.IsValid() ? BoundSave->GetLocalSaveUserIndex() : INDEX_NONE;
    Controller->OnCampaignTransitionChanged.AddUniqueDynamic(this, &ThisClass::OnObjectiveTransitionChanged);
    Campaign->OnObjectiveStateChanged.AddUniqueDynamic(this, &ThisClass::OnObjectiveChanged);
    Campaign->OnBeatCommitted.AddUniqueDynamic(this, &ThisClass::OnObjectiveBeatCommitted);
    Campaign->OnEvidenceRecorded.AddUniqueDynamic(this, &ThisClass::OnObjectiveEvidenceRecorded);
    Campaign->OnMissionChanged.AddUniqueDynamic(this, &ThisClass::OnObjectiveMissionChanged);
    Campaign->OnCampaignStateRestored.AddUniqueDynamic(this, &ThisClass::OnObjectiveStateRestored);
    if (Platform) { Platform->OnPlatformAccountChanged.AddUniqueDynamic(this, &ThisClass::OnObjectiveAccountChanged); }
}
void USovFrontendComponent::UnbindObjectives()
{
    if (ObjectiveController.IsValid())
    { ObjectiveController->OnCampaignTransitionChanged.RemoveDynamic(this, &ThisClass::OnObjectiveTransitionChanged); }
    if (BoundCampaign.IsValid())
    {
        BoundCampaign->OnObjectiveStateChanged.RemoveDynamic(this, &ThisClass::OnObjectiveChanged);
        BoundCampaign->OnBeatCommitted.RemoveDynamic(this, &ThisClass::OnObjectiveBeatCommitted);
        BoundCampaign->OnEvidenceRecorded.RemoveDynamic(this, &ThisClass::OnObjectiveEvidenceRecorded);
        BoundCampaign->OnMissionChanged.RemoveDynamic(this, &ThisClass::OnObjectiveMissionChanged);
        BoundCampaign->OnCampaignStateRestored.RemoveDynamic(this, &ThisClass::OnObjectiveStateRestored);
    }
    if (ObjectivePlatform.IsValid())
    { ObjectivePlatform->OnPlatformAccountChanged.RemoveDynamic(this, &ThisClass::OnObjectiveAccountChanged); }
    ObjectiveController.Reset(); BoundCampaign.Reset(); ObjectivePlatform.Reset();
    ObjectiveAccountNamespace.Reset(); ObjectiveAccountUserIndex = INDEX_NONE;
    bObjectiveAccountInvalidated = false; bObjectiveAccountChanged = false; bObjectiveUpdatePending = true;
    if (Presentation) { Presentation->ClearObjectives(); }
}
void USovFrontendComponent::RefreshObjectives(bool bForce)
{
    if (!Presentation) { return; }
    auto* PC = ObjectiveController.Get();
    auto* Campaign = BoundCampaign.Get();
    const auto* Pawn = PC ? Cast<ASovPlayerCharacterBase>(PC->GetPawn()) : nullptr;
    auto* Mission = Campaign ? Campaign->GetActiveMission() : nullptr;
    if (BoundSave.IsValid() && !ObjectiveAccountNamespace.IsEmpty()
        && (BoundSave->GetAccountNamespace() != ObjectiveAccountNamespace || BoundSave->GetLocalSaveUserIndex() != ObjectiveAccountUserIndex))
    { bObjectiveAccountInvalidated = true; bObjectiveAccountChanged = true; }
    if ((BoundSave.IsValid() && !BoundSave->IsPlatformStorageOwnerAvailable())
        || (ObjectivePlatform.IsValid() && ObjectivePlatform->IsAccountSelectionDeferred())) { bObjectiveAccountInvalidated = true; }
    else if (bObjectiveAccountInvalidated && !bObjectiveAccountChanged && BoundSave.IsValid()
        && BoundSave->GetAccountNamespace() == ObjectiveAccountNamespace && BoundSave->GetLocalSaveUserIndex() == ObjectiveAccountUserIndex)
    { bObjectiveAccountInvalidated = false; bObjectiveUpdatePending = true; }
    if (bEnding || !IsActive() || !PC || PC != GetOwner() || !PC->IsLocalController()
        || !Campaign || !Campaign->IsStateValid() || !IsValid(Mission) || !IsValid(Pawn)
        || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle
        || Pawn->GetProtagonistIdentityTag() != Campaign->GetActiveProtagonist()
        || !Campaign->GetActiveProtagonist().IsValid() || bObjectiveAccountInvalidated
        || (BoundSave.IsValid() && (BoundSave->IsLoadPending() || !BoundSave->IsPlatformStorageOwnerAvailable()))
        || (ObjectivePlatform.IsValid() && ObjectivePlatform->IsAccountSelectionDeferred()))
    { Presentation->ClearObjectives(); bObjectiveUpdatePending = true; return; }
    if (!bForce && !bObjectiveUpdatePending && ObjectiveViewGeneration == Presentation->GetObjectiveViewGeneration()) { return; }
    bObjectiveUpdatePending = false;
    TArray<FSovObjectivePresentationEntry> Entries;
    for (FName Id : Campaign->GetActionableObjectiveIds())
    {
        const auto* Beat = Mission->FindBeat(Id);
        const ESovObjectiveState State = Campaign->GetObjectiveState(Mission->MissionId, Id);
        if (!Beat || Beat->ObjectiveText.IsEmpty()
            || (State != ESovObjectiveState::Available && State != ESovObjectiveState::Active)
            || !Campaign->HasKnowledge(Campaign->GetActiveProtagonist(), Beat->RequiredKnowledge)
            || (Beat->RequiredProtagonist.IsValid() && Beat->RequiredProtagonist != Campaign->GetActiveProtagonist())) { continue; }
        FSovObjectivePresentationEntry Entry;
        Entry.BeatId = Id; Entry.Text = Beat->ObjectiveText; Entry.State = State;
        Entry.bOptional = Beat->bOptional; Entry.bCanonGate = Beat->bCanonGate;
        if (Beat->bOptional && !Beat->FailureReasonId.IsNone()) { Entry.FailureRule = Beat->FailureRuleText; }
        Entries.Add(MoveTemp(Entry));
    }
    // Keep authored order within each priority. Empty/internal beats and undiscovered
    // facts never enter the view, including its overflow count.
    Entries.StableSort([](const FSovObjectivePresentationEntry& A, const FSovObjectivePresentationEntry& B)
    {
        if (A.State != B.State) { return A.State == ESovObjectiveState::Active; }
        if (A.bOptional != B.bOptional) { return !A.bOptional; }
        return A.bCanonGate && !B.bCanonGate;
    });
    ObjectiveViewGeneration = Presentation->GetObjectiveViewGeneration();
    Presentation->PresentObjectives(Entries);
}
void USovFrontendComponent::OnObjectiveChanged(FName, FName, ESovObjectiveState) { RefreshObjectives(); }
void USovFrontendComponent::OnObjectiveBeatCommitted(const FSovCampaignJournalEntry&) { RefreshObjectives(); }
void USovFrontendComponent::OnObjectiveEvidenceRecorded(const FSovEvidenceAcquisition&) { RefreshObjectives(); }
void USovFrontendComponent::OnObjectiveMissionChanged(FName, bool) { RefreshObjectives(); }
void USovFrontendComponent::OnObjectiveStateRestored(bool bValid)
{
    if (bValid)
    {
        ObjectiveAccountNamespace = BoundSave.IsValid() ? BoundSave->GetAccountNamespace() : FString();
        ObjectiveAccountUserIndex = BoundSave.IsValid() ? BoundSave->GetLocalSaveUserIndex() : INDEX_NONE;
        bObjectiveAccountInvalidated = false; bObjectiveAccountChanged = false;
    }
    RefreshObjectives();
}
void USovFrontendComponent::OnObjectiveTransitionChanged(ESovCampaignTransitionState, const FString&) { RefreshObjectives(); }
void USovFrontendComponent::OnObjectiveAccountChanged(bool, bool)
{
    // Offline desktop play can have an authorized local profile without an online
    // sign-in. The native storage-owner and deferred-selection fences are decisive.
    RefreshObjectives();
}
void USovFrontendComponent::BindProducers(UTalesComponent* Tales, USovNarrativeCueComponent* Cues, UNarrativeAbilitySystemComponent* ASC)
{
    if (bEnding) { return; }
    if (BoundTales.Get() != Tales || BoundCues.Get() != Cues)
    {
        Unbind(); BoundTales = Tales; BoundCues = Cues;
        if (BoundTales.IsValid())
        {
            BoundTales->OnNPCDialogueLineStarted.AddUniqueDynamic(this, &ThisClass::OnNPCLine);
            BoundTales->OnPlayerDialogueLineStarted.AddUniqueDynamic(this, &ThisClass::OnPlayerLine);
            BoundTales->OnNPCDialogueLineFinished.AddUniqueDynamic(this, &ThisClass::OnNPCLineFinished);
            BoundTales->OnPlayerDialogueLineFinished.AddUniqueDynamic(this, &ThisClass::OnPlayerLineFinished);
            BoundTales->OnDialogueFinished.AddUniqueDynamic(this, &ThisClass::OnDialogueEnded);
            BoundTales->OnDialogueSuspensionChanged.AddUniqueDynamic(this, &ThisClass::OnDialogueSuspensionChanged);
        }
        if (BoundCues.IsValid())
        {
            BoundCues->OnCueStarted.AddUniqueDynamic(this, &ThisClass::OnCueStarted);
            BoundCues->OnCueEnded.AddUniqueDynamic(this, &ThisClass::OnCueEnded);
        }
    }
    if (BoundASC.Get() != ASC)
    {
        if (BoundASC.IsValid()) { BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::OnDamage); }
        BoundASC = ASC;
        if (ASC) { ASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::OnDamage); }
    }
}
bool USovFrontendComponent::OpenAccessibilitySettings()
{
    auto* PC = Cast<ASovPlayerController>(GetOwner());
    if (bEnding || !PC || !PC->IsLocalController() || !PC->GetNarrativeGameplayHUD()) { return false; }
    if (SetupMenu && SetupMenu->IsActivated()) { return true; }
    auto* HUD = PC->GetNarrativeGameplayHUD();
    const FGameplayTag Layer = FNarrativeGameplayTags::Get().UI_Layer_Menu;
    if (auto* Container = HUD->GetLayerContainer(Layer))
    {
        if (auto* Existing = Cast<USovAccessibilitySettingsMenu>(Container->GetActiveWidget()))
        { SetupMenu = Existing; return true; }
    }
    SetupMenu = Cast<USovAccessibilitySettingsMenu>(HUD->OpenMenu(USovAccessibilitySettingsMenu::StaticClass(), Layer));
    return IsValid(SetupMenu);
}
void USovFrontendComponent::ReleaseSetupPause()
{
    const TWeakObjectPtr<ASovPlayerController> PC = PausedController;
    const bool bRelease = bOwnSetupPause;
    bOwnSetupPause = false; PausedController.Reset();
    if (bRelease && PC.IsValid()) { PC->ReleaseSystemPause(TEXT("AccessibilitySetup")); }
}
void USovFrontendComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
    Super::TickComponent(Delta, TickType, Tick); RefreshFrontend();
}
void USovFrontendComponent::OnNPCLine(UDialogue* Dialogue, UDialogueNode_NPC* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker)
{
    if (bEnding || !Presentation || !IsValid(Dialogue) || !IsValid(Node) || Dialogue->GetCurrentNode() != Node
        || !BoundTales.IsValid() || BoundTales->GetCurrentDialogue() != Dialogue) { return; }
    RetirePreviousSpeech(Dialogue);
    const uint64 Epoch = ++SpeechEpoch;
    SpeechDialogue = Dialogue; SpeechNode = Node; SpeechCue.Reset();
    FText Text = Line.Text; Dialogue->ReplaceStringVariables(Node, Line, Text);
    if (bEnding || SpeechEpoch != Epoch || !SpeechDialogue.IsValid() || !BoundTales.IsValid()
        || BoundTales->GetCurrentDialogue() != Dialogue || !IsValid(Presentation)) { return; }
    AActor* Avatar = Dialogue->GetAvatar(Speaker.GetSpeakerID());
    const auto* Character = Cast<ANarrativeCharacter>(Avatar);
    const FText Name = Character ? Character->GetCharacterName()
        : Speaker.NPCDataAsset ? Speaker.NPCDataAsset->NPCName : FText::FromName(Speaker.GetSpeakerID());
    Presentation->PresentSpeech(Name, Text, -1.f, Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector, true);
    RememberLine(Dialogue, Node, Name, Text, Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector);
}
void USovFrontendComponent::OnPlayerLine(UDialogue* Dialogue, UDialogueNode_Player* Node, const FDialogueLine& Line)
{
    if (bEnding || !Presentation || !IsValid(Dialogue) || !IsValid(Node) || Dialogue->GetCurrentNode() != Node
        || !BoundTales.IsValid() || BoundTales->GetCurrentDialogue() != Dialogue) { return; }
    RetirePreviousSpeech(Dialogue);
    const uint64 Epoch = ++SpeechEpoch;
    SpeechDialogue = Dialogue; SpeechNode = Node; SpeechCue.Reset();
    FText Text = Line.Text; Dialogue->ReplaceStringVariables(Node, Line, Text);
    if (bEnding || SpeechEpoch != Epoch || !SpeechDialogue.IsValid() || !BoundTales.IsValid()
        || BoundTales->GetCurrentDialogue() != Dialogue || !IsValid(Presentation)) { return; }
    AActor* Avatar = Dialogue->GetPlayerAvatar(); const auto* Character = Cast<ANarrativeCharacter>(Avatar);
    const FText Name = Character ? Character->GetCharacterName() : LOCTEXT("Player", "You");
    Presentation->PresentSpeech(Name, Text, -1.f, Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector, true);
    RememberLine(Dialogue, Node, Name, Text, Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector);
}
void USovFrontendComponent::RememberLine(UDialogue* Dialogue, UDialogueNode* Node, const FText& Speaker, const FText& Text, const FVector& Location)
{
    LineDialogue = Dialogue; LineNode = Node; LineSpeaker = Speaker; LineText = Text; LineLocation = Location; bLineInterrupted = false;
}
void USovFrontendComponent::OnNPCLineFinished(UDialogue* Dialogue, UDialogueNode_NPC* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker)
{
    if (Presentation && SpeechDialogue.Get() == Dialogue && SpeechNode.Get() == Node)
    { ++SpeechEpoch; SpeechNode.Reset(); Presentation->ClearSpeech(); }
}
void USovFrontendComponent::OnPlayerLineFinished(UDialogue* Dialogue, UDialogueNode_Player* Node, const FDialogueLine& Line)
{
    if (Presentation && SpeechDialogue.Get() == Dialogue && SpeechNode.Get() == Node)
    { ++SpeechEpoch; SpeechNode.Reset(); Presentation->ClearSpeech(); }
}
void USovFrontendComponent::OnDialogueEnded(UDialogue* Dialogue, bool bStartingNew, EExitDialogueReason Reason)
{
    if (SpeechDialogue.Get() != Dialogue) { return; }
    ++SpeechEpoch; SpeechDialogue.Reset(); SpeechNode.Reset();
    if (Presentation) { Presentation->ClearSpeech(); }
}
void USovFrontendComponent::OnCueStarted(USovNarrativeCue* Cue, AActor* Speaker, const FText& Caption, float Seconds)
{
    // A playing conversation owns the speech surface. A suspended one does not: the cue arbiter suspends it
    // precisely so a bark can be heard, and a voiced bark with no subtitle is lost to players who read speech.
    UDialogue* const Current = BoundTales.IsValid() ? BoundTales->GetCurrentDialogue() : nullptr;
    if (bEnding || !Presentation || !IsValid(Cue) || (Current && !Current->IsPlaybackSuspended())) { return; }
    if (Current && SpeechDialogue.Get() == Current && SpeechNode.IsValid() && LineDialogue.Get() == Current && LineNode == SpeechNode)
    { bLineInterrupted = true; }
    RetirePreviousSpeech(nullptr);
    ++SpeechEpoch; SpeechDialogue.Reset(); SpeechNode.Reset(); SpeechCue = Cue;
    const auto* Character = Cast<ANarrativeCharacter>(Speaker);
    Presentation->PresentSpeech(Character ? Character->GetCharacterName() : FText::FromName(Cue->SpeakerId), Caption,
        Seconds, IsValid(Speaker) ? Speaker->GetActorLocation() : FVector::ZeroVector, false);
}
void USovFrontendComponent::OnDialogueSuspensionChanged(UDialogue* Dialogue, bool bSuspended)
{
    if (bEnding || bSuspended || !Presentation || !bLineInterrupted || LineDialogue.Get() != Dialogue) { return; }
    bLineInterrupted = false;
    UDialogueNode* const Node = LineNode.Get();
    // Resume in place: only the same conversation, still on the line the bark displaced, is shown again.
    if (!IsValid(Dialogue) || !IsValid(Node) || !BoundTales.IsValid() || BoundTales->GetCurrentDialogue() != Dialogue
        || Dialogue->GetCurrentNode() != Node) { return; }
    ++SpeechEpoch; SpeechDialogue = Dialogue; SpeechNode = Node; SpeechCue.Reset();
    Presentation->PresentSpeech(LineSpeaker, LineText, -1.f, LineLocation, true);
}
void USovFrontendComponent::OnCueEnded(USovNarrativeCue* Cue, bool bInterrupted)
{
    if (Presentation && SpeechCue.Get() == Cue) { ++SpeechEpoch; SpeechCue.Reset(); Presentation->ClearSpeech(); }
}
void USovFrontendComponent::OnDamage(const FSovDamageResult& Result)
{
    auto* PC = Cast<ASovPlayerController>(GetOwner());
    if (bEnding || !Presentation || !PC || !BoundASC.IsValid() || Result.TargetActor != PC->GetPawn()
        || BoundASC->GetAvatarActor() != PC->GetPawn()) { return; }
    FText Caption;
    if (Result.bPerfectDefense) { Caption = LOCTEXT("PerfectDefense", "Perfect defense"); }
    else if (Result.bGuardBroken) { Caption = LOCTEXT("GuardBreak", "Guard broken"); }
    else if (Result.bShieldBroken) { Caption = LOCTEXT("ShieldBreak", "Shield broken"); }
    else if (Result.bPoiseBroken) { Caption = LOCTEXT("PoiseBreak", "Staggered"); }
    else if (Result.bDeflected) { Caption = LOCTEXT("Deflected", "Attack deflected"); }
    else if (Result.AppliedHealthDamage > 0.f || Result.AppliedShieldDamage > 0.f) { Caption = LOCTEXT("IncomingDamage", "Incoming damage"); }
    if (!Caption.IsEmpty())
    {
        const ESovCaptionPriority Priority = Result.bGuardBroken || Result.bShieldBroken || Result.bPoiseBroken
            ? ESovCaptionPriority::Critical : Result.bPerfectDefense || Result.bDeflected
            ? ESovCaptionPriority::Important : ESovCaptionPriority::Routine;
        Presentation->PresentCaption(Caption, 3.f, IsValid(Result.SourceActor) ? Result.SourceActor->GetActorLocation() : FVector::ZeroVector, Priority);
    }
}
void USovFrontendComponent::RetirePreviousSpeech(UDialogue* NewDialogue)
{
    if (!Presentation) { return; }
    if (SpeechDialogue.IsValid() && SpeechDialogue.Get() != NewDialogue)
    { Presentation->RetireSpeechPresentation(); }
    else if (SpeechNode.IsValid() || SpeechCue.IsValid())
    { Presentation->ClearSpeech(); }
}
void USovFrontendComponent::OnLoadCompleted(ESovSaveResult Result, const FSovSaveSlotHeader&, const FString& Message)
{
    if (bEnding) { return; }
    if (Result == ESovSaveResult::RecoveryAvailable)
    { RecoveryMessage = Message; bRecoveryMenuPending = true; }
    else if (Result == ESovSaveResult::Success)
    { RecoveryMessage.Reset(); bRecoveryMenuPending = false; }
    RefreshObjectives();
}
void USovFrontendComponent::Unbind()
{
    ++SpeechEpoch; SpeechDialogue.Reset(); SpeechNode.Reset(); SpeechCue.Reset();
    LineDialogue.Reset(); LineNode.Reset(); bLineInterrupted = false;
    if (BoundTales.IsValid())
    {
        BoundTales->OnNPCDialogueLineStarted.RemoveDynamic(this, &ThisClass::OnNPCLine);
        BoundTales->OnPlayerDialogueLineStarted.RemoveDynamic(this, &ThisClass::OnPlayerLine);
        BoundTales->OnNPCDialogueLineFinished.RemoveDynamic(this, &ThisClass::OnNPCLineFinished);
        BoundTales->OnPlayerDialogueLineFinished.RemoveDynamic(this, &ThisClass::OnPlayerLineFinished);
        BoundTales->OnDialogueFinished.RemoveDynamic(this, &ThisClass::OnDialogueEnded);
        BoundTales->OnDialogueSuspensionChanged.RemoveDynamic(this, &ThisClass::OnDialogueSuspensionChanged);
    }
    if (BoundCues.IsValid())
    {
        BoundCues->OnCueStarted.RemoveDynamic(this, &ThisClass::OnCueStarted);
        BoundCues->OnCueEnded.RemoveDynamic(this, &ThisClass::OnCueEnded);
    }
    if (BoundASC.IsValid()) { BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::OnDamage); }
    BoundTales.Reset(); BoundCues.Reset(); BoundASC.Reset();
    if (Presentation) { Presentation->ClearSpeech(); Presentation->ClearSceneHistory(); }
}
void USovFrontendComponent::RemoveCombatVitals()
{
    if (CombatVitals) { CombatVitals->RemoveFromParent(); CombatVitals = nullptr; }
}

/** Kept separate from the vitals teardown: folding the two together made the plain readout's
 * every-tick stand-down destroy and rebuild the holographic surface on every frame, so it never
 * survived long enough to paint. */
void USovFrontendComponent::RemoveHolographicHUD()
{
    if (HolographicHUD) { HolographicHUD->RemoveFromParent(); HolographicHUD = nullptr; }
}
void USovFrontendComponent::Deactivate()
{
    Super::Deactivate();
    RemoveCombatVitals();
    RemoveHolographicHUD();
}
void USovFrontendComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding = true; UnbindObjectives(); Unbind(); ReleaseSetupPause(); RemoveCombatVitals(); RemoveHolographicHUD();
    if (BoundSave.IsValid()) { BoundSave->OnLoadCompleted.RemoveDynamic(this, &ThisClass::OnLoadCompleted); }
    BoundSave.Reset(); bRecoveryMenuPending = false; RecoveryMessage.Reset();
    if (SetupMenu) { SetupMenu->DeactivateWidget(); SetupMenu = nullptr; }
    if (Presentation) { Presentation->RemoveFromParent(); Presentation = nullptr; }
    Super::EndPlay(Reason);
}
#undef LOCTEXT_NAMESPACE
