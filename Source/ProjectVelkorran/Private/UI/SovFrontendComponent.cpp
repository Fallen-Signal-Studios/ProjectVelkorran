// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovFrontendComponent.h"
#include "UI/SovAccessibilityPresentation.h"
#include "UI/SovAccessibilitySettingsMenu.h"
#include "Framework/SovPlayerController.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovCampaignDefinition.h"
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

#define LOCTEXT_NAMESPACE "SovNativeFrontend"
USovFrontendComponent::USovFrontendComponent()
{
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
        || IsRunningCommandlet() || !FSlateApplication::IsInitialized()) { return; }
    const auto* Campaign=PC->GetCampaignState();
    const FName Mission=Campaign && Campaign->GetActiveMission()?Campaign->GetActiveMission()->MissionId:NAME_None;
    if(PresentationAvatar.Get()!=PC->GetPawn() || PresentationMission!=Mission)
    { Unbind(); if(bEnding || !IsValid(PC)) { return; } PresentationAvatar=PC->GetPawn(); PresentationMission=Mission; }
    if (!Presentation)
    {
        Presentation = CreateWidget<USovAccessibilityPresentation>(PC, USovAccessibilityPresentation::StaticClass());
        // HUD menus/modal layers render above this non-interactive gameplay overlay.
        if (Presentation) { Presentation->AddToPlayerScreen(-1); }
    }
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(PC->GetAbilitySystemComponent());
    if (ASC && ASC->GetAvatarActor() != PC->GetPawn()) { ASC = nullptr; }
    BindProducers(PC->GetTalesComponent(), PC->GetNarrativeCues(), ASC);
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
    if (Settings && !Settings->AreAccountPreferencesReady()) { return; }
    if (Settings && !Settings->HasCompletedAccessibilitySetup() && PC->GetNarrativeGameplayHUD())
    {
        if ((!SetupMenu || !SetupMenu->IsActivated()) && OpenAccessibilitySettings()) { SetupMenu->SetFirstBoot(true); }
        if (SetupMenu && SetupMenu->IsActivated() && !bOwnSetupPause)
        { PausedController = PC; bOwnSetupPause = PC->AcquireSystemPause(TEXT("AccessibilitySetup")); }
    }
    else { ReleaseSetupPause(); }
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
            BoundTales->OnDialogueSuspensionChanged.AddUniqueDynamic(this, &ThisClass::OnDialogueSuspended);
        }
        if (BoundCues.IsValid())
        {
            BoundCues->OnCueStarted.AddUniqueDynamic(this, &ThisClass::OnCueStarted);
            BoundCues->OnCueEnded.AddUniqueDynamic(this, &ThisClass::OnCueEnded);
            BoundCues->OnCueAudioReady.AddUniqueDynamic(this, &ThisClass::OnCueAudioReady);
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
    if (BoundCues.IsValid() && SpeechCue.IsValid())
    { BoundCues->SetBarkSubtitleHold(SpeechCue.Get(), Presentation, Presentation && Presentation->IsInViewport() && Presentation->HasUnreadSpeech(CueReceipt)); }
}
void USovFrontendComponent::OnNPCLine(UDialogue* Dialogue, UDialogueNode_NPC* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker)
{
    if (bEnding || !Presentation || !IsValid(Dialogue) || !IsValid(Node) || Dialogue->GetCurrentNode() != Node
        || !BoundTales.IsValid() || BoundTales->GetCurrentDialogue() != Dialogue) { return; }
    const uint64 Epoch = ++SpeechEpoch; RetirePreviousSpeech(Dialogue);
    if(bEnding || SpeechEpoch!=Epoch || !Presentation || !BoundTales.IsValid() || BoundTales->GetCurrentDialogue()!=Dialogue) { return; }
    SpeechDialogue = Dialogue; SpeechNode = Node; SpeechCue.Reset();
    FText Text = Line.Text; Dialogue->ReplaceStringVariables(Node, Line, Text);
    if (bEnding || SpeechEpoch != Epoch || !SpeechDialogue.IsValid() || !BoundTales.IsValid()
        || BoundTales->GetCurrentDialogue() != Dialogue || !IsValid(Presentation)) { return; }
    AActor* Avatar = Dialogue->GetAvatar(Speaker.GetSpeakerID());
    const auto* Character = Cast<ANarrativeCharacter>(Avatar);
    const FText Name = Character ? Character->GetCharacterName()
        : Speaker.NPCDataAsset ? Speaker.NPCDataAsset->NPCName : FText::FromName(Speaker.GetSpeakerID());
    USovAccessibilityPresentation* View=Presentation; const FGuid Receipt=View->PresentOwnedSpeech(Name, Text, -1.f, Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector, true, IsValid(Avatar));
    if(!bEnding && SpeechEpoch==Epoch && Presentation==View) { SpeechReceipt=Receipt; }
}
void USovFrontendComponent::OnPlayerLine(UDialogue* Dialogue, UDialogueNode_Player* Node, const FDialogueLine& Line)
{
    if (bEnding || !Presentation || !IsValid(Dialogue) || !IsValid(Node) || Dialogue->GetCurrentNode() != Node
        || !BoundTales.IsValid() || BoundTales->GetCurrentDialogue() != Dialogue) { return; }
    const uint64 Epoch = ++SpeechEpoch; RetirePreviousSpeech(Dialogue);
    if(bEnding || SpeechEpoch!=Epoch || !Presentation || !BoundTales.IsValid() || BoundTales->GetCurrentDialogue()!=Dialogue) { return; }
    SpeechDialogue = Dialogue; SpeechNode = Node; SpeechCue.Reset();
    FText Text = Line.Text; Dialogue->ReplaceStringVariables(Node, Line, Text);
    if (bEnding || SpeechEpoch != Epoch || !SpeechDialogue.IsValid() || !BoundTales.IsValid()
        || BoundTales->GetCurrentDialogue() != Dialogue || !IsValid(Presentation)) { return; }
    AActor* Avatar = Dialogue->GetPlayerAvatar(); const auto* Character = Cast<ANarrativeCharacter>(Avatar);
    USovAccessibilityPresentation* View=Presentation; const FGuid Receipt=View->PresentOwnedSpeech(Character ? Character->GetCharacterName() : LOCTEXT("Player", "You"), Text,
        -1.f, Avatar ? Avatar->GetActorLocation() : FVector::ZeroVector, true, IsValid(Avatar));
    if(!bEnding && SpeechEpoch==Epoch && Presentation==View) { SpeechReceipt=Receipt; }
}
void USovFrontendComponent::OnNPCLineFinished(UDialogue* Dialogue, UDialogueNode_NPC* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker)
{
    if (Presentation && SpeechDialogue.Get() == Dialogue && SpeechNode.Get() == Node)
    { ++SpeechEpoch; SpeechNode.Reset(); Presentation->FinishOwnedSpeech(SpeechReceipt); }
}
void USovFrontendComponent::OnPlayerLineFinished(UDialogue* Dialogue, UDialogueNode_Player* Node, const FDialogueLine& Line)
{
    if (Presentation && SpeechDialogue.Get() == Dialogue && SpeechNode.Get() == Node)
    { ++SpeechEpoch; SpeechNode.Reset(); Presentation->FinishOwnedSpeech(SpeechReceipt); }
}
void USovFrontendComponent::OnDialogueEnded(UDialogue* Dialogue, bool bStartingNew, EExitDialogueReason Reason)
{
    if (SpeechDialogue.Get() != Dialogue) { return; }
    ++SpeechEpoch; SpeechDialogue.Reset(); SpeechNode.Reset();
    bRetireSceneOnNextLine = true; const FGuid FinishedReceipt=SpeechReceipt; SpeechReceipt.Invalidate();
    if (Presentation)
    {
        Presentation->FinishOwnedSpeech(FinishedReceipt);
        if (bStartingNew || Reason != EExitDialogueReason::EDR_NoLines) { Presentation->ClearSceneHistory(); }
    }
}
void USovFrontendComponent::OnDialogueSuspended(UDialogue* Dialogue, bool bSuspended)
{
    if (!Presentation || SpeechDialogue.Get() != Dialogue) { return; }
    if (bSuspended) { Presentation->SuspendDialogueSpeech(); }
    else { Presentation->ResumeDialogueSpeech(); }
}
void USovFrontendComponent::OnCueStarted(USovNarrativeCue* Cue, AActor* Speaker, const FText& Caption, float Seconds)
{
    UDialogue* Dialogue = BoundTales.IsValid() ? BoundTales->GetCurrentDialogue() : nullptr;
    if (bEnding || !Presentation || !IsValid(Cue) || (Dialogue && !Dialogue->IsPlaybackSuspended())) { return; }
    const uint64 Epoch=++SpeechEpoch; if (!Dialogue) { RetirePreviousSpeech(nullptr); }
    if(bEnding || SpeechEpoch!=Epoch || !Presentation) { return; }
    SpeechCue = Cue;
    const auto* Character = Cast<ANarrativeCharacter>(Speaker);
    USovAccessibilityPresentation* View=Presentation; const FGuid Receipt=View->PresentOwnedSpeech(Character ? Character->GetCharacterName() : FText::FromName(Cue->SpeakerId), Caption,
        Seconds, IsValid(Speaker) ? Speaker->GetActorLocation() : FVector::ZeroVector, false, IsValid(Speaker));
    if(bEnding || SpeechEpoch!=Epoch || Presentation!=View || SpeechCue.Get()!=Cue) { return; }
    CueReceipt=Receipt;
    if (BoundCues.IsValid()) { BoundCues->SetBarkSubtitleHold(Cue, View, View->IsInViewport() && View->HasUnreadSpeech(CueReceipt)); }
}
void USovFrontendComponent::OnCueEnded(USovNarrativeCue* Cue, bool bInterrupted)
{
    if (Presentation && SpeechCue.Get() == Cue) { ++SpeechEpoch; SpeechCue.Reset(); Presentation->FinishOwnedSpeech(CueReceipt); CueReceipt.Invalidate(); }
}
void USovFrontendComponent::OnCueAudioReady(USovNarrativeCue* Cue, float Seconds)
{
    if (!bEnding && Presentation && SpeechCue.Get() == Cue) { Presentation->RestartOwnedSpeech(CueReceipt, Seconds); }
}
void USovFrontendComponent::OnDamage(const FSovDamageResult& Result)
{
    auto* PC = Cast<ASovPlayerController>(GetOwner());
    if (bEnding || !Presentation || !PC || !BoundASC.IsValid() || Result.TargetActor != PC->GetPawn()
        || BoundASC->GetAvatarActor() != PC->GetPawn()) { return; }
    const bool bDirection = IsValid(Result.SourceActor);
    const FVector Location = bDirection ? Result.SourceActor->GetActorLocation() : FVector::ZeroVector;
    USovAccessibilityPresentation* View=Presentation; const uint64 Epoch=SpeechEpoch;
    const auto Caption = [&](bool bShow, FName Key, const FText& Text, int32 Priority)
    { if (bShow && !bEnding && SpeechEpoch==Epoch && Presentation==View && IsValid(PC) && PC->GetPawn()==Result.TargetActor) { View->PresentPrioritizedCaption(Text, 3.f, Location, Key, Priority, bDirection); } };
    Caption(Result.bPerfectDefense,"PerfectDefense",LOCTEXT("PerfectDefense","Perfect defense"),80);
    Caption(Result.bGuardBroken,"GuardBreak",LOCTEXT("GuardBreak","Guard broken"),100);
    Caption(Result.bShieldBroken,"ShieldBreak",LOCTEXT("ShieldBreak","Shield broken"),100);
    Caption(Result.bPoiseBroken,"PoiseBreak",LOCTEXT("PoiseBreak","Staggered"),90);
    Caption(Result.bDeflected,"Deflected",LOCTEXT("Deflected","Attack deflected"),80);
    Caption(Result.AppliedHealthDamage > 0.f || Result.AppliedShieldDamage > 0.f,"IncomingDamage",LOCTEXT("IncomingDamage","Incoming damage"),0);
}
void USovFrontendComponent::RetirePreviousSpeech(UDialogue* NewDialogue)
{
    if (!Presentation) { return; }
    const bool Clear=(bRetireSceneOnNextLine && NewDialogue) || (SpeechDialogue.IsValid() && SpeechDialogue.Get() != NewDialogue);
    bRetireSceneOnNextLine = false;
    if (Clear)
    { Presentation->ClearSceneHistory(); }
    else if (SpeechNode.IsValid() || SpeechCue.IsValid())
    { Presentation->FinishOwnedSpeech(SpeechReceipt); Presentation->FinishOwnedSpeech(CueReceipt); }
}
void USovFrontendComponent::Unbind()
{
    ++SpeechEpoch; SpeechDialogue.Reset(); SpeechNode.Reset(); SpeechCue.Reset();
    if (BoundCues.IsValid()) { BoundCues->SetBarkSubtitleHold(nullptr, Presentation, false); }
    SpeechReceipt.Invalidate(); CueReceipt.Invalidate(); bRetireSceneOnNextLine = false;
    if (BoundTales.IsValid())
    {
        BoundTales->OnNPCDialogueLineStarted.RemoveDynamic(this, &ThisClass::OnNPCLine);
        BoundTales->OnPlayerDialogueLineStarted.RemoveDynamic(this, &ThisClass::OnPlayerLine);
        BoundTales->OnNPCDialogueLineFinished.RemoveDynamic(this, &ThisClass::OnNPCLineFinished);
        BoundTales->OnPlayerDialogueLineFinished.RemoveDynamic(this, &ThisClass::OnPlayerLineFinished);
        BoundTales->OnDialogueFinished.RemoveDynamic(this, &ThisClass::OnDialogueEnded);
        BoundTales->OnDialogueSuspensionChanged.RemoveDynamic(this, &ThisClass::OnDialogueSuspended);
    }
    if (BoundCues.IsValid())
    {
        BoundCues->OnCueStarted.RemoveDynamic(this, &ThisClass::OnCueStarted);
        BoundCues->OnCueEnded.RemoveDynamic(this, &ThisClass::OnCueEnded);
        BoundCues->OnCueAudioReady.RemoveDynamic(this, &ThisClass::OnCueAudioReady);
    }
    if (BoundASC.IsValid()) { BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::OnDamage); }
    BoundTales.Reset(); BoundCues.Reset(); BoundASC.Reset();
    if (Presentation) { Presentation->ClearSpeech(); Presentation->ClearSceneHistory(); }
}
void USovFrontendComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding = true; Unbind(); ReleaseSetupPause();
    if (SetupMenu) { SetupMenu->DeactivateWidget(); SetupMenu = nullptr; }
    if (Presentation) { Presentation->RemoveFromParent(); Presentation = nullptr; }
    Super::EndPlay(Reason);
}
#undef LOCTEXT_NAMESPACE
