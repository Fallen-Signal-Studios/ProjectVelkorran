// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
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
        || !EncounterDirector->bCompleteWhenRequiredParticipantsDefeated)
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
    TGuardValue<bool> Starting(bStarting, true);
    BindDirector();
    const auto Current = EncounterDirector->GetEncounterState();
    EncounterDirector->bAwaitingCampaignReceipt = true;
    if (Current == ESovEncounterState::Failed)
    {
        const bool bStarted = EncounterDirector->RetryEncounter(Error); LastError = Error; return bStarted;
    }
    if (Current != ESovEncounterState::Inactive)
    { Error = TEXT("Encounter already started. A retired victory requires reloading its entry checkpoint."); LastError = Error; return false; }
    // CaptureEntryCheckpoint is intentionally not repeatable. A pre-captured entry is owned by this player.
    if (!EncounterDirector->HasEncounterPlayer(Player) && !EncounterDirector->CaptureEntryCheckpoint(Player, Error))
    { LastError = Error; return false; }
    if (!ValidateContext(Player, Error) || !EncounterDirector->HasEncounterPlayer(Player)) { LastError = Error; return false; }
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
        { FString Error; if (!StartEncounter(Player, Error)) { LastError = Error; } }
    }
}
void ASovCampaignEncounterObjective::HandleEncounterState(ESovEncounterState Previous, ESovEncounterState Current)
{
    if (bEnding || !HasAuthority() || !BoundDirector.IsValid() || BoundDirector.Get() != EncounterDirector
        || EncounterDirector->GetEncounterState() != Current) { return; }
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
        BoundCampaign = Attempt.State;
        BoundCampaign->OnCampaignStateRestored.AddUniqueDynamic(this, &ThisClass::HandleCampaignRestored);
        BoundCampaign->OnMissionChanged.AddUniqueDynamic(this, &ThisClass::HandleMissionChanged);
        LastError.Reset(); return;
    }
    if (Current != ESovEncounterState::Succeeded) { RetireAttempt(); return; }
    if (Previous != ESovEncounterState::Active || bPending || bExecuting || !EncounterDirector->bAwaitingCampaignReceipt || !OwnsAttempt(Attempt, Current)) { return; }
    bPending = true;
    const FAttemptContext Captured = Attempt;
    ResultTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
        [this, Captured]() { CommitVictory(Captured); }));
}
bool ASovCampaignEncounterObjective::OwnsAttempt(const FAttemptContext& Context, ESovEncounterState ExpectedState) const
{
    return !bEnding && !IsActorBeingDestroyed() && Context.AttemptId.IsValid() && Context.AttemptId == Attempt.AttemptId
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
        && EncounterDirector->HasConfirmedVictory();
}
void ASovCampaignEncounterObjective::AcknowledgeCommitReceipt(const USovCampaignStateComponent* State)
{
    if (!State || !Attempt.Director.IsValid() || Attempt.Director->GetAttemptId() != Attempt.AttemptId) { return; }
    if (State->GetJournal().ContainsByPredicate([this](const auto& Entry)
        { return Entry.MissionId == Attempt.MissionId && Entry.BeatId == Attempt.BeatId
            && Entry.EncounterId == Attempt.EncounterId && Entry.EncounterAttemptId == Attempt.AttemptId; }))
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
    bEnding = true; RetireAttempt();
    if (BoundDirector.IsValid()) { BoundDirector->OnEncounterStateChanged.RemoveDynamic(this, &ThisClass::HandleEncounterState); }
    Super::EndPlay(Reason);
}
