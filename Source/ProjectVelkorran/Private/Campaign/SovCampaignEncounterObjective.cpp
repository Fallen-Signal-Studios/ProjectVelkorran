// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignRelayReceiver.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "TimerManager.h"

ASovCampaignEncounterObjective::ASovCampaignEncounterObjective()
{
    PrimaryActorTick.bCanEverTick = false;
    StartVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("StartVolume")); SetRootComponent(StartVolume);
    StartVolume->SetBoxExtent(FVector(150, 200, 120));
    StartVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    StartVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    StartVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    StartVolume->SetGenerateOverlapEvents(true);
    StartVolume->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleStartOverlap);
    StartVolume->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);
}
void ASovCampaignEncounterObjective::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority()) { BindDirector(); }
}
void ASovCampaignEncounterObjective::BindDirector()
{
    if (BoundDirector.Get() == EncounterDirector) { return; }
    RetireAttempt();
    if (BoundDirector.IsValid()) { BoundDirector->OnEncounterStateChanged.RemoveDynamic(this, &ThisClass::HandleEncounterState); }
    BoundDirector = EncounterDirector;
    if (BoundDirector.IsValid()) { BoundDirector->OnEncounterStateChanged.AddUniqueDynamic(this, &ThisClass::HandleEncounterState); }
    // No retrospective adoption of an active or loaded successful attempt.
}
bool ASovCampaignEncounterObjective::ValidateContext(ASovPlayerCharacterBase* Player, FString& Error) const
{
    Error.Reset();
    const auto Reject = [&Error](const TCHAR* Reason) { Error = Reason; return false; };
    const auto* PC = IsValid(Player) ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    const auto* ASC = IsValid(Player) ? Cast<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent()) : nullptr;
    const auto* State = PC ? PC->GetCampaignState() : nullptr;
    const auto* Mission = State ? State->GetActiveMission() : nullptr;
    const auto* Beat = Mission ? Mission->FindBeat(CompletionBeat) : nullptr;
    if (!HasAuthority() || GetNetMode() != NM_Standalone || bEnding || IsActorBeingDestroyed()
        || !IsValid(Player) || Player->IsActorBeingDestroyed() || Player->GetWorld() != GetWorld()
        || !IsValid(PC) || PC->IsActorBeingDestroyed() || PC->GetPawn() != Player
        || !ASC || ASC->GetAvatarActor() != Player || !Player->IsCharacterReady() || !Player->IsAlive()
        || !State || !State->IsStateValid() || State->IsMutationInProgress()
        || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle
        || !Mission || Mission->MissionId != MissionId || !Beat || MissionId.IsNone() || CompletionBeat.IsNone()
        || State->GetActiveProtagonist() != Player->GetProtagonistIdentityTag()
        || Beat->RequiredProtagonist != Player->GetProtagonistIdentityTag())
    { return Reject(TEXT("Encounter objective requires its current ready, living protagonist and active campaign.")); }
    FString DefinitionError;
    if (!Mission->ValidateDefinition(DefinitionError)) { Error = DefinitionError; return false; }
    if (!IsValid(EncounterDirector) || EncounterDirector->IsActorBeingDestroyed() || EncounterDirector->GetWorld() != GetWorld()
        || EncounterDirector->EncounterId.IsNone() || Beat->RequiredEncounterId != EncounterDirector->EncounterId
        || EncounterDirector->GetCampaignProofType() != Beat->RequiredEncounterProof
        || (Beat->RequiredEncounterProof == ESovEncounterProofType::RequiredDefeats && !EncounterDirector->bCompleteWhenRequiredParticipantsDefeated))
    { return Reject(TEXT("Encounter objective requires its authored director and confirmed-defeat victory policy.")); }
    if (EncounterDirector->ProtectedParticipantIds.Num() < Beat->MinimumProtectedParticipants)
    { return Reject(TEXT("Encounter objective is missing its required protected survivor participants.")); }
    for (TActorIterator<ASovCampaignEncounterObjective> It(GetWorld()); It; ++It)
    {
        if (*It != this && !It->IsActorBeingDestroyed()
            && ((It->MissionId == MissionId && It->CompletionBeat == CompletionBeat) || It->EncounterDirector == EncounterDirector))
        { return Reject(TEXT("Encounter objective binding is ambiguous.")); }
    }
    for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
    {
        if (*It != EncounterDirector && !It->IsActorBeingDestroyed() && It->EncounterId == EncounterDirector->EncounterId)
        { return Reject(TEXT("Encounter director identity is ambiguous.")); }
    }
    if (!ValidateReceiverConfiguration(Beat->RequiredReceiverIds, Error, false)) { return false; }
    const auto Objective = State->GetObjectiveState(MissionId, CompletionBeat);
    if (Objective != ESovObjectiveState::Available && Objective != ESovObjectiveState::Active)
    { return Reject(TEXT("Complete the preceding objective before starting this encounter.")); }
    return true;
}
bool ASovCampaignEncounterObjective::StartEncounter(ASovPlayerCharacterBase* Player, FString& Error)
{
    Error.Reset();
    if (bStarting || bExecuting || bPending)
    { Error = TEXT("Encounter objective is processing its current request."); LastError = Error; return false; }
    if (!ValidateContext(Player, Error)) { LastError = Error; return false; }
    if (bRetryingInitialEntry && !OwnsInitialEntryRetry())
    { Error = TEXT("Initial encounter entry was retired during validation."); LastError = Error; return false; }
    TGuardValue<bool> Starting(bStarting, true);
    BindDirector();
    const auto Current = EncounterDirector->GetEncounterState();
    EncounterDirector->bAwaitingCampaignReceipt = true;
    if (Current == ESovEncounterState::Failed)
    {
        if (bRetryingInitialEntry)
        { Error = TEXT("Initial entry polling never retries a failed combat attempt."); LastError = Error; return false; }
        const bool bStarted = EncounterDirector->RetryEncounter(Error); LastError = Error; return bStarted;
    }
    if (Current == ESovEncounterState::Succeeded && OwnsAttempt(Attempt, Current) && !HasRequiredReceiverProof())
    { Error = TEXT("Defeat confirmed. Disable the remaining relay receivers to secure the overlook."); LastError = Error; return false; }
    if (Current != ESovEncounterState::Inactive)
    { Error = TEXT("Encounter already started. A retired victory requires reloading its entry checkpoint."); LastError = Error; return false; }
    // CaptureEntryCheckpoint is intentionally not repeatable. A pre-captured entry is owned by this player.
    if (!EncounterDirector->HasEncounterPlayer(Player) && !EncounterDirector->CaptureEntryCheckpoint(Player, Error))
    { LastError = Error; return false; }
    if (!ValidateContext(Player, Error) || !EncounterDirector->HasEncounterPlayer(Player)) { LastError = Error; return false; }
    if (bRetryingInitialEntry && !OwnsInitialEntryRetry())
    { Error = TEXT("Initial encounter entry was retired during checkpoint capture."); LastError = Error; return false; }
    const bool bStarted = EncounterDirector->BeginEncounter();
    if (!bStarted && Error.IsEmpty()) { Error = TEXT("Encounter entry was not released. Check its restore failure and save status."); }
    LastError = Error; return bStarted;
}
void ASovCampaignEncounterObjective::HandleStartOverlap(UPrimitiveComponent* Component, AActor* Actor,
    UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit)
{
    if (bStartOnPlayerOverlap && HasAuthority())
    {
        if (auto* Player = Cast<ASovPlayerCharacterBase>(Actor))
        {
            // Multiple overlapping body components must not reset the same retry cadence.
            if (InitialEntry.Player == Player && OwnsInitialEntryRetry()) { return; }
            FString Error;
            if (!StartEncounter(Player, Error))
            { LastError = Error; ArmInitialEntryRetry(Player); }
        }
    }
}
void ASovCampaignEncounterObjective::ArmInitialEntryRetry(ASovPlayerCharacterBase* Player)
{
    StopInitialEntryRetry();
    if (!bRetryInitialEntryWhileOverlapping || !bStartOnPlayerOverlap || !IsValid(Player)) { return; }
    auto* PC = Cast<ASovPlayerController>(Player->GetController());
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent());
    auto* State = PC ? PC->GetCampaignState() : nullptr;
    if (!PC || !ASC || !State || !IsValid(EncounterDirector)) { return; }
    InitialEntry.Player = Player; InitialEntry.Controller = PC; InitialEntry.ASC = ASC; InitialEntry.State = State;
    InitialEntry.Mission = State->GetActiveMission(); InitialEntry.Director = EncounterDirector;
    InitialEntry.MissionId = MissionId; InitialEntry.BeatId = CompletionBeat; InitialEntry.EncounterId = EncounterDirector->EncounterId;
    InitialEntry.ReadyEpoch = ASC->GetCharacterReadyEpoch(); InitialEntry.ActorInfoEpoch = ASC->GetCombatActorInfoEpoch();
    InitialEntry.TransitionEpoch = PC->GetCampaignTransitionEpoch(); InitialEntry.DirectorGeneration = EncounterDirector->GetLifecycleGeneration();
    InitialEntryVolume = StartVolume;
    if (!OwnsInitialEntryRetry()) { StopInitialEntryRetry(); return; }
    BindDirector();
    State->OnCampaignStateRestored.AddUniqueDynamic(this, &ThisClass::HandleInitialEntryCampaignRestored);
    State->OnMissionChanged.AddUniqueDynamic(this, &ThisClass::HandleInitialEntryMissionChanged);
    GetWorldTimerManager().SetTimer(InitialEntryTimer, this, &ThisClass::RetryInitialEntry, .2f, true);
}
bool ASovCampaignEncounterObjective::OwnsInitialEntryRetry() const
{
    const auto* Player = InitialEntry.Player.Get(); const auto* PC = InitialEntry.Controller.Get();
    const auto* ASC = InitialEntry.ASC.Get(); const auto* State = InitialEntry.State.Get();
    const auto* Mission = InitialEntry.Mission.Get(); const auto* Beat = Mission ? Mission->FindBeat(InitialEntry.BeatId) : nullptr;
    const auto* Capsule = Player ? Player->GetCapsuleComponent() : nullptr;
    if (!HasAuthority() || GetNetMode() != NM_Standalone || bEnding || IsActorBeingDestroyed()
        || !bStartOnPlayerOverlap || !bRetryInitialEntryWhileOverlapping
        || !Player || Player->IsActorBeingDestroyed() || Player->GetWorld() != GetWorld()
        || !PC || PC->IsActorBeingDestroyed() || PC->GetWorld() != GetWorld() || PC->GetPawn() != Player || Player->GetController() != PC
        || !ASC || Player->GetAbilitySystemComponent() != ASC || ASC->GetAvatarActor() != Player
        || !Player->IsCharacterReady() || !Player->IsAlive() || ASC->GetCharacterReadyEpoch() != InitialEntry.ReadyEpoch
        || ASC->GetCombatActorInfoEpoch() != InitialEntry.ActorInfoEpoch || PC->GetCampaignTransitionEpoch() != InitialEntry.TransitionEpoch
        || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle
        || !State || PC->GetCampaignState() != State || !State->IsStateValid() || State->IsMutationInProgress()
        || !Mission || State->GetActiveMission() != Mission || !Beat || Mission->MissionId != MissionId
        || MissionId != InitialEntry.MissionId || CompletionBeat != InitialEntry.BeatId
        || State->GetActiveProtagonist() != Player->GetProtagonistIdentityTag() || Beat->RequiredProtagonist != Player->GetProtagonistIdentityTag()
        || !InitialEntry.Director.IsValid() || EncounterDirector != InitialEntry.Director.Get()
        || EncounterDirector->IsActorBeingDestroyed() || EncounterDirector->GetWorld() != GetWorld()
        || EncounterDirector->GetEncounterState() != ESovEncounterState::Inactive
        || EncounterDirector->GetLifecycleGeneration() != InitialEntry.DirectorGeneration || EncounterDirector->EncounterId != InitialEntry.EncounterId
        || Beat->RequiredEncounterId != EncounterDirector->EncounterId
        || !InitialEntryVolume.IsValid() || StartVolume != InitialEntryVolume.Get() || StartVolume->GetOwner() != this
        || !StartVolume->IsRegistered() || !StartVolume->IsQueryCollisionEnabled() || !StartVolume->GetGenerateOverlapEvents()
        || !Capsule || !Capsule->IsRegistered() || !StartVolume->IsOverlappingComponent(Capsule)) { return false; }
    const auto Status = State->GetObjectiveState(MissionId, CompletionBeat);
    return Status == ESovObjectiveState::Available || Status == ESovObjectiveState::Active;
}
void ASovCampaignEncounterObjective::RetryInitialEntry()
{
    if (!OwnsInitialEntryRetry()) { StopInitialEntryRetry(); return; }
    if (bStarting || bExecuting || bPending) { return; }
    const uint64 Serial = InitialEntrySerial;
    FString Error;
    TGuardValue<bool> InitialEntryScope(bRetryingInitialEntry, true);
    // Exactly the ordinary entry validation/capture/release path. OwnsInitialEntryRetry
    // rejects Failed before this call, so the existing explicit combat retry is unreachable.
    const bool bStarted = StartEncounter(InitialEntry.Player.Get(), Error);
    if (Serial != InitialEntrySerial) { return; }
    if (bStarted || !OwnsInitialEntryRetry()) { StopInitialEntryRetry(); }
}
void ASovCampaignEncounterObjective::StopInitialEntryRetry()
{
    ++InitialEntrySerial;
    if (GetWorld()) { GetWorldTimerManager().ClearTimer(InitialEntryTimer); }
    if (InitialEntry.State.IsValid())
    {
        InitialEntry.State->OnCampaignStateRestored.RemoveDynamic(this, &ThisClass::HandleInitialEntryCampaignRestored);
        InitialEntry.State->OnMissionChanged.RemoveDynamic(this, &ThisClass::HandleInitialEntryMissionChanged);
    }
    InitialEntry = {}; InitialEntryVolume.Reset();
}
void ASovCampaignEncounterObjective::HandleEndOverlap(UPrimitiveComponent* Component, AActor* Actor,
    UPrimitiveComponent* OtherComponent, int32 BodyIndex)
{
    if (Actor == InitialEntry.Player.Get() && !OwnsInitialEntryRetry()) { StopInitialEntryRetry(); }
}
void ASovCampaignEncounterObjective::HandleInitialEntryCampaignRestored(bool bValid) { StopInitialEntryRetry(); }
void ASovCampaignEncounterObjective::HandleInitialEntryMissionChanged(FName ChangedMissionId, bool bSucceeded) { StopInitialEntryRetry(); }
void ASovCampaignEncounterObjective::HandleEncounterState(ESovEncounterState Previous, ESovEncounterState Current)
{
    if (bEnding || !HasAuthority() || !BoundDirector.IsValid() || BoundDirector.Get() != EncounterDirector
        || EncounterDirector->GetEncounterState() != Current) { return; }
    if (Current != ESovEncounterState::Inactive) { StopInitialEntryRetry(); }
    if (Current == ESovEncounterState::Active)
    {
        RetireAttempt(); FString Error;
        auto* Player = EncounterDirector->GetEncounterPlayer();
        if ((Previous != ESovEncounterState::Inactive && Previous != ESovEncounterState::Restoring)
            || !ValidateContext(Player, Error) || !EncounterDirector->HasEncounterPlayer(Player)
            || !EncounterDirector->GetAttemptId().IsValid()) { LastError = Error; return; }
        auto* PC = CastChecked<ASovPlayerController>(Player->GetController());
        auto* ASC = CastChecked<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent());
        EncounterDirector->bAwaitingCampaignReceipt = true;
        Attempt.Player = Player; Attempt.Controller = PC; Attempt.ASC = ASC; Attempt.State = PC->GetCampaignState();
        Attempt.Mission = Attempt.State->GetActiveMission(); Attempt.Director = EncounterDirector;
        Attempt.AttemptId = EncounterDirector->GetAttemptId(); Attempt.DirectorGeneration = EncounterDirector->GetLifecycleGeneration();
        Attempt.ReadyEpoch = ASC->GetCharacterReadyEpoch(); Attempt.ActorInfoEpoch = ASC->GetCombatActorInfoEpoch();
        Attempt.TransitionEpoch = PC->GetCampaignTransitionEpoch(); Attempt.MissionId = MissionId;
        Attempt.BeatId = CompletionBeat; Attempt.EncounterId = EncounterDirector->EncounterId;
        Attempt.ProtectedIds = EncounterDirector->ProtectedParticipantIds;
        for (ASovCampaignRelayReceiver* Receiver : RequiredReceivers) { Attempt.Receivers.Add(Receiver->ReceiverId, Receiver); }
        BoundCampaign = Attempt.State;
        BoundCampaign->OnCampaignStateRestored.AddUniqueDynamic(this, &ThisClass::HandleCampaignRestored);
        BoundCampaign->OnMissionChanged.AddUniqueDynamic(this, &ThisClass::HandleMissionChanged);
        LastError.Reset(); return;
    }
    if (Current != ESovEncounterState::Succeeded) { RetireAttempt(); return; }
    if (Previous != ESovEncounterState::Active || bPending || bExecuting || !EncounterDirector->bAwaitingCampaignReceipt || !OwnsAttempt(Attempt, Current)) { return; }
    QueueVictoryIfReady();
}
void ASovCampaignEncounterObjective::QueueVictoryIfReady()
{
    if (bPending || bExecuting || !IsValid(EncounterDirector) || !EncounterDirector->bAwaitingCampaignReceipt
        || !OwnsAttempt(Attempt, ESovEncounterState::Succeeded)) { return; }
    if (!HasRequiredReceiverProof())
    { LastError = TEXT("Defeat confirmed. Disable the remaining relay receivers to secure the overlook."); return; }
    bPending = true;
    const FAttemptContext Captured = Attempt;
    ResultTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
        [this, Captured]() { CommitVictory(Captured); }));
}
bool ASovCampaignEncounterObjective::OwnsAttempt(const FAttemptContext& Context, ESovEncounterState ExpectedState) const
{
    FString ReceiverError;
    const auto* Beat = Context.Mission.IsValid() ? Context.Mission->FindBeat(Context.BeatId) : nullptr;
    return Beat && ValidateReceiverConfiguration(Beat->RequiredReceiverIds, ReceiverError, true)
        && !bEnding && !IsActorBeingDestroyed() && Context.AttemptId.IsValid() && Context.AttemptId == Attempt.AttemptId
        && Context.Director.IsValid() && Context.Director.Get() == EncounterDirector && BoundDirector == Context.Director
        && !EncounterDirector->IsActorBeingDestroyed() && EncounterDirector->GetEncounterState() == ExpectedState
        && EncounterDirector->GetAttemptId() == Context.AttemptId && EncounterDirector->GetLifecycleGeneration() == Context.DirectorGeneration
        && EncounterDirector->EncounterId == Context.EncounterId && EncounterDirector->ProtectedParticipantIds.Difference(Context.ProtectedIds).IsEmpty()
        && EncounterDirector->ProtectedParticipantIds.Num() == Context.ProtectedIds.Num()
        && Context.Player.IsValid() && Context.Controller.IsValid() && Context.ASC.IsValid() && Context.State.IsValid() && Context.Mission.IsValid()
        && !Context.Player->IsActorBeingDestroyed() && !Context.Controller->IsActorBeingDestroyed()
        && Context.Player->GetWorld() == GetWorld() && EncounterDirector->HasEncounterPlayer(Context.Player.Get())
        && Context.Player->GetController() == Context.Controller.Get() && Context.Controller->GetPawn() == Context.Player.Get()
        && Context.Player->GetAbilitySystemComponent() == Context.ASC.Get() && Context.ASC->GetAvatarActor() == Context.Player.Get()
        && Context.Player->IsCharacterReady() && Context.Player->IsAlive() && Context.ASC->GetCharacterReadyEpoch() == Context.ReadyEpoch
        && Context.ASC->GetCombatActorInfoEpoch() == Context.ActorInfoEpoch && Context.Controller->GetCampaignTransitionEpoch() == Context.TransitionEpoch
        && Context.Controller->GetCampaignTransitionState() == ESovCampaignTransitionState::Idle
        && Context.Controller->GetCampaignState() == Context.State.Get() && Context.State->GetActiveMission() == Context.Mission.Get()
        && Context.State->IsStateValid() && MissionId == Context.MissionId && CompletionBeat == Context.BeatId;
}
bool ASovCampaignEncounterObjective::HasCommitReceipt(const USovCampaignStateComponent* State, FName BeatId) const
{
    FString Error;
    return bExecuting && !bPending && State == Attempt.State.Get() && BeatId == Attempt.BeatId
        && OwnsAttempt(Attempt, ESovEncounterState::Succeeded) && ValidateContext(Attempt.Player.Get(), Error)
        && EncounterDirector->HasConfirmedVictory() && HasRequiredReceiverProof();
}
bool ASovCampaignEncounterObjective::ValidateReceiverConfiguration(const TSet<FName>& RequiredIds, FString& Error, bool bCheckFrozen) const
{
    if (RequiredIds.Num() > 8 || RequiredIds.Contains(NAME_None) || RequiredReceivers.Num() != RequiredIds.Num())
    { Error = TEXT("Encounter requires its exact authored relay receiver set."); return false; }
    TSet<FName> Seen;
    for (const ASovCampaignRelayReceiver* Receiver : RequiredReceivers)
    {
        if (!IsValid(Receiver) || Receiver->IsActorBeingDestroyed() || !Receiver->IsActorInitialized() || Receiver->GetWorld() != GetWorld()
            || Receiver->EncounterObjective != this || !RequiredIds.Contains(Receiver->ReceiverId) || Seen.Contains(Receiver->ReceiverId))
        { Error = TEXT("Relay receivers must be distinct, living same-world actors bound to this objective and its named IDs."); return false; }
        Seen.Add(Receiver->ReceiverId);
        if (bCheckFrozen)
        {
            const auto* Frozen = Attempt.Receivers.Find(Receiver->ReceiverId);
            if (!Frozen || Frozen->Get() != Receiver)
            { Error = TEXT("Relay receiver identity changed during the encounter; reload its entry checkpoint."); return false; }
        }
        for (TActorIterator<ASovCampaignRelayReceiver> It(GetWorld()); It; ++It)
        {
            if (*It != Receiver && !It->IsActorBeingDestroyed() && It->ReceiverId == Receiver->ReceiverId)
            { Error = TEXT("Relay receiver identity is duplicated in this world."); return false; }
        }
    }
    if (bCheckFrozen && Attempt.Receivers.Num() != RequiredIds.Num())
    { Error = TEXT("Relay receiver contract changed after the attempt started."); return false; }
    return true;
}
bool ASovCampaignEncounterObjective::HasRequiredReceiverProof() const
{
    const auto* Beat = Attempt.Mission.IsValid() ? Attempt.Mission->FindBeat(Attempt.BeatId) : nullptr;
    FString Error;
    return Beat && ValidateReceiverConfiguration(Beat->RequiredReceiverIds, Error, true)
        && Attempt.DisabledReceiverIds.Num() == Beat->RequiredReceiverIds.Num()
        && Attempt.DisabledReceiverIds.Difference(Beat->RequiredReceiverIds).IsEmpty();
}
bool ASovCampaignEncounterObjective::HasReceiverDisabled(const ASovCampaignRelayReceiver* Receiver) const
{
    if (!IsValid(Receiver) || !IsValid(EncounterDirector) || Receiver->EncounterObjective != this
        || !Attempt.AttemptId.IsValid() || EncounterDirector->GetAttemptId() != Attempt.AttemptId
        || !Attempt.State.IsValid() || !Attempt.State->IsStateValid()) { return false; }
    const auto State = EncounterDirector->GetEncounterState();
    const auto* Frozen = Attempt.Receivers.Find(Receiver->ReceiverId);
    return (State == ESovEncounterState::Active || State == ESovEncounterState::Succeeded)
        && Frozen && Frozen->Get() == Receiver && Attempt.DisabledReceiverIds.Contains(Receiver->ReceiverId);
}
bool ASovCampaignEncounterObjective::CanDisableReceiver(const ASovCampaignRelayReceiver* Receiver,
    const ASovPlayerCharacterBase* Player, FString& Error) const
{
    Error.Reset();
    if (!IsValid(Receiver) || !IsValid(EncounterDirector) || Player != Attempt.Player.Get() || bEnding || bExecuting
        || HasReceiverDisabled(Receiver))
    { Error = TEXT("Receiver has no current encounter operation."); return false; }
    const auto State = EncounterDirector->GetEncounterState();
    const auto* Frozen = Attempt.Receivers.Find(Receiver->ReceiverId);
    if ((State != ESovEncounterState::Active && State != ESovEncounterState::Succeeded)
        || !OwnsAttempt(Attempt, State) || !Frozen || Frozen->Get() != Receiver
        || !ValidateContext(Attempt.Player.Get(), Error))
    { if (Error.IsEmpty()) { Error = TEXT("Receiver operation lost its live encounter context; reload its entry checkpoint."); } return false; }
    return true;
}
bool ASovCampaignEncounterObjective::AcceptReceiverDisable(ASovCampaignRelayReceiver* Receiver, const FGuid& AttemptId)
{
    FString Error;
    if (!IsValid(Receiver) || AttemptId != Attempt.AttemptId || !CanDisableReceiver(Receiver, Attempt.Player.Get(), Error)
        || !Receiver->HasPhysicalDisableReceipt(this, AttemptId)) { return false; }
    Attempt.DisabledReceiverIds.Add(Receiver->ReceiverId);
    // Both deaths-before-receivers and receivers-before-deaths use the same deferred commit path.
    QueueVictoryIfReady();
    return true;
}
void ASovCampaignEncounterObjective::AcknowledgeCommitReceipt(const USovCampaignStateComponent* State)
{
    if (!State || !Attempt.Director.IsValid() || Attempt.Director->GetAttemptId() != Attempt.AttemptId) { return; }
    if (State->GetJournal().ContainsByPredicate([this](const auto& Entry)
        { return Entry.MissionId == Attempt.MissionId && Entry.BeatId == Attempt.BeatId
            && Entry.EncounterId == Attempt.EncounterId && Entry.EncounterAttemptId == Attempt.AttemptId
            && Entry.DisabledReceiverIds.Num() == Attempt.DisabledReceiverIds.Num()
            && Entry.DisabledReceiverIds.Difference(Attempt.DisabledReceiverIds).IsEmpty(); }))
    { Attempt.Director->bAwaitingCampaignReceipt = false; }
}
void ASovCampaignEncounterObjective::CommitVictory(FAttemptContext Context)
{
    if (!bPending || bEnding || Context.AttemptId != Attempt.AttemptId) { return; }
    bPending = false; TGuardValue<bool> Executing(bExecuting, true);
    if (!OwnsAttempt(Context, ESovEncounterState::Succeeded) || !HasCommitReceipt(Context.State.Get(), Context.BeatId))
    { LastError = TEXT("Encounter result retired before campaign commit; reload the verified entry checkpoint."); return; }
    const auto Result = Context.State->CompleteEncounterObjective(this);
    if (Result != ESovCampaignResult::Applied && Result != ESovCampaignResult::AlreadyApplied)
    { LastError = TEXT("Encounter victory could not commit its campaign objective; reload the entry checkpoint."); return; }
    LastError.Reset();
}
bool ASovCampaignEncounterObjective::IsResultPending() const
{
    if (bPending || bExecuting) { return true; }
    if (bEnding || !IsValid(EncounterDirector) || EncounterDirector->GetEncounterState() != ESovEncounterState::Succeeded) { return false; }
    const auto* PC = GetWorld() ? Cast<ASovPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
    const auto* State = PC ? PC->GetCampaignState() : nullptr;
    return !State || !State->IsBeatComplete(MissionId, CompletionBeat);
}
void ASovCampaignEncounterObjective::RetireAttempt()
{
    if (GetWorld()) { GetWorldTimerManager().ClearTimer(ResultTimer); }
    bPending = false; Attempt = {};
    if (BoundCampaign.IsValid())
    {
        BoundCampaign->OnCampaignStateRestored.RemoveDynamic(this, &ThisClass::HandleCampaignRestored);
        BoundCampaign->OnMissionChanged.RemoveDynamic(this, &ThisClass::HandleMissionChanged);
    }
    BoundCampaign.Reset();
}
void ASovCampaignEncounterObjective::HandleCampaignRestored(bool bValid) { RetireAttempt(); }
void ASovCampaignEncounterObjective::HandleMissionChanged(FName ChangedMissionId, bool bSucceeded) { RetireAttempt(); }
void ASovCampaignEncounterObjective::EndPlay(EEndPlayReason::Type Reason)
{
    bEnding = true; StopInitialEntryRetry(); RetireAttempt();
    if (BoundDirector.IsValid()) { BoundDirector->OnEncounterStateChanged.RemoveDynamic(this, &ThisClass::HandleEncounterState); }
    Super::EndPlay(Reason);
}
