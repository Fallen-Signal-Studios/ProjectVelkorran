// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionCrucibleDirector.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovCampaignHandoffAnchor.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SovAurelionThermalFractureComponent.h"
#include "AI/NarrativeNPCController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"

ASovAurelionLinkPhaseDirector::ASovAurelionLinkPhaseDirector()
{
    bCompleteWhenRequiredParticipantsDefeated = false;
    OnEncounterStateChanged.AddDynamic(this, &ThisClass::HandlePhaseState);
}
USovCommandLinkComponent* ASovAurelionLinkPhaseDirector::ResolveLink(const FSovAurelionCrucibleLink& Binding) const
{
    auto* NPC = GetParticipant(Binding.ParticipantId); if (!IsValid(NPC)) { return nullptr; }
    TArray<USovCommandLinkComponent*> Links; NPC->GetComponents(Links);
    for (auto* Link : Links)
    { if (Link->GetFName() == Binding.ComponentName && Link->GetLinkId() == Binding.LinkId) { return Link; } }
    return nullptr;
}
void ASovAurelionLinkPhaseDirector::UnbindLinks()
{
    for (auto* Link : BoundLinks)
    { if (IsValid(Link)) { Link->OnCommandLinkSevered.RemoveDynamic(this, &ThisClass::HandleLinkSever); } }
    BoundLinks.Reset();
}
bool ASovAurelionLinkPhaseDirector::BindAttemptLinks(FString& Error)
{
    UnbindLinks();
    const auto* EliteEntry = Participants.FindByPredicate([this](const auto& P) { return P.ParticipantId == EliteParticipantId; });
    if (RequiredLinks.Num() != 2 || EliteParticipantId.IsNone() || !EliteEntry || !EliteEntry->bRequiredForVictory || !IsValid(GetParticipant(EliteParticipantId))
        || !GetParticipant(EliteParticipantId)->IsAlive() || ProtectedParticipantIds.Num() < 2
        || Participants.ContainsByPredicate([](const auto& P) { return P.bAllowMassRepresentation; }))
    { Error = TEXT("Crucible phase A requires its live elite, two protected survivors, two named links and actor-only participants."); return false; }
    TSet<FName> IDs;
    for (const auto& Binding : RequiredLinks)
    {
        auto* Link = ResolveLink(Binding);
        if (Binding.ParticipantId.IsNone() || Binding.ComponentName.IsNone() || Binding.LinkId.IsNone()
            || IDs.Contains(Binding.LinkId) || !IsValid(Link) || !Link->IsCommandLinkActive()
            || !Link->GetLinkInstanceId().IsValid() || !Link->HasValidCommandLinkConfiguration())
        { Error = TEXT("Crucible phase A requires two distinct live native command links before combat."); UnbindLinks(); return false; }
        IDs.Add(Binding.LinkId); BoundLinks.Add(Link);
        FSovAurelionCrucibleLinkReceipt Receipt; Receipt.LinkId = Binding.LinkId; Receipt.InstanceId = Link->GetLinkInstanceId();
        LinkReceipts.Add(Receipt);
        Link->OnCommandLinkSevered.AddUniqueDynamic(this, &ThisClass::HandleLinkSever);
    }
    return true;
}
void ASovAurelionLinkPhaseDirector::HandlePhaseState(ESovEncounterState Previous, ESovEncounterState Current)
{
    if (!HasAuthority() || GetEncounterState() != Current) { return; }
    if (Current != ESovEncounterState::Active) { UnbindLinks(); return; }
    LastPhaseError.Reset();
    ProofAttemptId = GetAttemptId(); LinkReceipts.Reset(); bBoundaryFrozen = false; bTransferred = false; SettleStartedAt = 0.f;
    auto* Player = GetEncounterPlayer(); auto* PC = IsValid(Player) ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    auto* ASC = IsValid(Player) ? Player->GetNarrativeAbilitySystemComponent() : nullptr;
    const auto* Campaign = PC ? PC->GetCampaignState() : nullptr;
    const auto* Mission = Campaign ? Campaign->GetActiveMission() : nullptr;
    const auto* Beat = Mission ? Mission->FindBeat(CompletionBeat) : nullptr;
    if (!Player || !PC || !ASC || Player->GetProtagonistIdentityTag() != FSovGameplayTags::Get().Character_Player_Selene
        || !Mission || Mission->MissionId != MissionId || !Beat || Beat->RequiredEncounterId != EncounterId
        || Beat->RequiredEncounterProof != ESovEncounterProofType::AurelionLinks)
    { LastPhaseError = TEXT("Crucible phase A belongs to its authored mission, link objective and current Selene player."); return; }
    ProofPlayer = Player; ProofController = PC; ProofASC = ASC;
    ProofReadyEpoch = ASC->GetCharacterReadyEpoch(); ProofActorInfoEpoch = ASC->GetCombatActorInfoEpoch();
    ProofTransitionEpoch = PC->GetCampaignTransitionEpoch();
    BindAttemptLinks(LastPhaseError); SetActorTickEnabled(true);
}
void ASovAurelionLinkPhaseDirector::HandleLinkSever(const FSovCommandLinkSeverResult& Result)
{
    if (!HasAuthority() || bPhaseMutation || GetEncounterState() != ESovEncounterState::Active || GetAttemptId() != ProofAttemptId
        || !ProofPlayer.IsValid() || !ProofController.IsValid() || !ProofASC.IsValid()
        || ProofController->GetPawn() != ProofPlayer.Get() || ProofPlayer->GetController() != ProofController.Get()
        || ProofPlayer->GetNarrativeAbilitySystemComponent() != ProofASC.Get() || ProofASC->GetAvatarActor() != ProofPlayer.Get()
        || !ProofPlayer->IsCharacterReady() || !ProofPlayer->IsAlive() || ProofASC->GetCharacterReadyEpoch() != ProofReadyEpoch
        || ProofASC->GetCombatActorInfoEpoch() != ProofActorInfoEpoch || ProofController->GetCampaignTransitionEpoch() != ProofTransitionEpoch
        || Result.SeveredBy != ProofPlayer.Get() || !Result.TransactionId.IsValid() || !Result.bEligibleForEchoReward) { return; }
    const int32 Index = RequiredLinks.IndexOfByPredicate([&Result](const auto& Binding) { return Binding.LinkId == Result.LinkId; });
    if (!RequiredLinks.IsValidIndex(Index) || !LinkReceipts.IsValidIndex(Index)) { return; }
    auto* Link = ResolveLink(RequiredLinks[Index]); if (!IsValid(Link) || !BoundLinks.Contains(Link)) { return; }
    const auto Snapshot = Link->CaptureCommandLinkState(); auto& Receipt = LinkReceipts[Index];
    if (Result.LinkOwner != Link->GetOwner() || Result.LinkInstanceId != Receipt.InstanceId || Snapshot.LinkInstanceId != Receipt.InstanceId
        || Snapshot.State != ESovCommandLinkState::Severed || Snapshot.LastSeverTransactionId != Result.TransactionId
        || Receipt.TransactionId.IsValid()) { return; }
    Receipt.TransactionId = Result.TransactionId;
}
bool ASovAurelionLinkPhaseDirector::HasLinkProof() const
{
    if (!ProofAttemptId.IsValid() || ProofAttemptId != GetAttemptId() || LinkReceipts.Num() != 2 || RequiredLinks.Num() != 2) { return false; }
    for (int32 Index = 0; Index < RequiredLinks.Num(); ++Index)
    { if (LinkReceipts[Index].LinkId != RequiredLinks[Index].LinkId || !LinkReceipts[Index].InstanceId.IsValid()
        || !LinkReceipts[Index].TransactionId.IsValid()) { return false; } }
    return LinkReceipts[0].TransactionId != LinkReceipts[1].TransactionId;
}
bool ASovAurelionLinkPhaseDirector::HasConfirmedVictory() const
{
    return HasAuthority() && !IsActorBeingDestroyed() && GetEncounterState() == ESovEncounterState::Succeeded
        && HasLinkProof() && bBoundaryFrozen && !bTransferred && AreProtectedParticipantsAlive();
}
bool ASovAurelionLinkPhaseDirector::CompleteEncounter()
{
    if (bPhaseMutation || !HasLinkProof() || !bBoundaryFrozen || !AreOwnedParticipantsQuiescent(true)) { return false; }
    const auto* Elite = GetParticipant(EliteParticipantId);
    if (!IsValid(Elite) || !Elite->IsAlive()) { FailEncounter(); return false; }
    const bool bCompleted = Super::CompleteEncounter();
    if (bCompleted && !IsActorBeingDestroyed()) { SetActorTickEnabled(true); }
    return bCompleted;
}
bool ASovAurelionLinkPhaseDirector::IsCompletedPhaseBoundaryQuiescentForSave(const ASovPlayerCharacterBase* Player) const
{
    const auto* PC = IsValid(Player) ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    const auto* Campaign = PC ? PC->GetCampaignState() : nullptr;
    const auto* Mission = Campaign ? Campaign->GetActiveMission() : nullptr;
    return HasConfirmedVictory() && !IsCampaignReceiptPending() && HasEncounterPlayer(Player)
        && IsValid(Player) && Player->IsCharacterReady() && Player->IsAlive() && Campaign && Campaign->IsStateValid()
        && Mission && Mission->MissionId == MissionId && Campaign->IsBeatComplete(MissionId, CompletionBeat)
        && AreOwnedParticipantsQuiescent(true);
}
bool ASovAurelionLinkPhaseDirector::HasHandoffJournal(const ASovPlayerCharacterBase* Player) const
{
    const auto* PC = IsValid(Player) ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    const auto* Campaign = PC ? PC->GetCampaignState() : nullptr;
    const auto* Mission = Campaign ? Campaign->GetActiveMission() : nullptr;
    return PC && PC->GetPawn() == Player && PC->GetCampaignTransitionState() == ESovCampaignTransitionState::Idle
        && Player->IsCharacterReady() && Player->IsAlive() && Player->GetProtagonistIdentityTag() == FSovGameplayTags::Get().Character_Player_Tarrik
        && Campaign && Campaign->IsStateValid() && !Campaign->IsMutationInProgress() && Mission && Mission->MissionId == MissionId
        && Campaign->GetJournal().ContainsByPredicate([this](const auto& Entry)
            { return Entry.MissionId == MissionId && Entry.BeatId == HandoffBeat && Entry.HandoffRequestId.IsValid()
                && Entry.HandoffToProtagonist == FSovGameplayTags::Get().Character_Player_Tarrik
                && HandoffAnchor && Entry.HandoffAnchorId == HandoffAnchor->AnchorId; });
}
bool ASovAurelionLinkPhaseDirector::CompletePhaseHandoff(ASovPlayerCharacterBase* Tarrik, FString& Error)
{
    Error.Reset();
    auto* Destination = PhaseBObjective ? Cast<ASovAurelionThermalPhaseDirector>(PhaseBObjective->EncounterDirector) : nullptr;
    if (!HasAuthority() || bPhaseMutation || GetEncounterState() != ESovEncounterState::Succeeded || IsCampaignReceiptPending()
        || !HasLinkProof() || !HasHandoffJournal(Tarrik) || !IsValid(Destination)
        || Destination->GetWorld() != GetWorld() || PhaseBObjective->MissionId != MissionId
        || Destination->EliteParticipantId != EliteParticipantId)
    { Error = TEXT("Crucible phase B requires the committed native Tarrik handoff and matching authored director."); return false; }
    const auto* PC = Cast<ASovPlayerController>(Tarrik->GetController());
    const auto* Campaign = PC->GetCampaignState(); const auto* Mission = Campaign->GetActiveMission();
    const auto* Next = Mission->FindBeat(PhaseBObjective->CompletionBeat);
    const auto* Elite = bTransferred ? Destination->GetParticipant(EliteParticipantId) : GetParticipant(EliteParticipantId);
    if (!Next || Next->RequiredEncounterId != Destination->EncounterId || Next->RequiredEncounterProof != ESovEncounterProofType::AurelionThermalFracture
        || Next->RequiredProtagonist != Tarrik->GetProtagonistIdentityTag() || Next->MinimumProtectedParticipants > ProtectedParticipantIds.Num()
        || !IsValid(Elite) || !Elite->FindComponentByClass<USovAurelionThermalFractureComponent>())
    { Error = TEXT("Crucible phase B must bind its typed Thermal Fracture objective and the same elite's native fracture component."); return false; }
    if (bTransferred) { return PhaseBObjective->StartEncounter(Tarrik, Error); }
    if (!TransferFrozenParticipants(Destination, Error)) { return false; }
    // The existing entry capture now records Tarrik and the current live elite/resources. Its
    // native begin writes/readback-verifies CP5b before releasing any carried participant.
    const bool bStarted = PhaseBObjective->StartEncounter(Tarrik, Error);
    if (bStarted) { SetActorTickEnabled(false); }
    return bStarted;
}
bool ASovAurelionLinkPhaseDirector::TransferFrozenParticipants(ASovAurelionThermalPhaseDirector* Destination, FString& Error)
{
    if (!HasAuthority() || bPhaseMutation || bTransferred || !IsValid(Destination) || Destination->GetWorld() != GetWorld()
        || GetEncounterState() != ESovEncounterState::Succeeded || IsCampaignReceiptPending() || !HasLinkProof())
    { Error = TEXT("Only the current completed native link phase can transfer its frozen roster."); return false; }
    if (!bBoundaryFrozen || !AreOwnedParticipantsQuiescent(true) || Destination->GetEncounterState() != ESovEncounterState::Inactive
        || Destination->bHasEntryCheckpoint || !Destination->Participants.IsEmpty() || !Destination->SuspendedASCs.IsEmpty()
        || !Destination->PausedBrains.IsEmpty() || !Destination->MassParticipants.IsEmpty())
    { Error = TEXT("The exact completed source roster must remain frozen and the destination must be empty."); return false; }
    auto* Elite = GetParticipant(EliteParticipantId);
    if (!IsValid(Elite) || !Elite->IsAlive() || !AreProtectedParticipantsAlive())
    { Error = TEXT("The elite and protected survivor groups must persist alive across the phase boundary."); return false; }
    TGuardValue<bool> Mutation(bPhaseMutation, true);
    // Transfer native ownership without releasing frozen actors or altering resources, links, or transforms.
    for (const auto& Participant : Participants)
    {
        TransferredParticipantIds.Add(Participant.ParticipantId);
        if (IsValid(Participant.Character) && Participant.Character->IsAlive()) { Destination->Participants.Add(Participant); }
    }
    Destination->bRequiresPhaseEntryCapture = true;
    Destination->ProtectedParticipantIds = ProtectedParticipantIds;
    Destination->SuspendedASCs = MoveTemp(SuspendedASCs); Destination->SuspendedAvatars = MoveTemp(SuspendedAvatars);
    Destination->OwnedBusySuspensions = MoveTemp(OwnedBusySuspensions); Destination->OwnedProtectionSuspensions = MoveTemp(OwnedProtectionSuspensions);
    Destination->PausedBrains = MoveTemp(PausedBrains); Destination->PausedBrainPawns = MoveTemp(PausedBrainPawns);
    Destination->SuspendedThreatControllers = MoveTemp(SuspendedThreatControllers);
    Participants.Reset(); bTransferred = true; bBoundaryFrozen = false;
    for (const auto& Pair : Destination->SuspendedThreatControllers)
    {
        if (Pair.Key.IsValid() && Pair.Value.IsValid() && Pair.Key->GetPawn() == Pair.Value.Get())
        {
            Pair.Key->SetThreatMemorySuspended(Destination, true);
            if (Pair.Key.IsValid() && Pair.Value.IsValid() && Pair.Key->GetPawn() == Pair.Value.Get())
            { Pair.Key->SetThreatMemorySuspended(this, false); }
        }
    }
    ForceNetUpdate(); Destination->ForceNetUpdate();
    return true;
}
void ASovAurelionLinkPhaseDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || IsActorBeingDestroyed() || bPhaseMutation) { return; }
    if (GetEncounterState() == ESovEncounterState::Active)
    {
        const auto* Elite = GetParticipant(EliteParticipantId);
        if (!IsValid(Elite) || !Elite->IsAlive())
        { LastPhaseError = TEXT("The phase A elite was defeated before its preserved phase B entry."); FailEncounter(); return; }
        if (!LastPhaseError.IsEmpty()) { FailEncounter(); return; }
        if (!HasLinkProof())
        {
            for (int32 Index = 0; Index < RequiredLinks.Num(); ++Index)
            {
                auto* Link = ResolveLink(RequiredLinks[Index]);
                if (!LinkReceipts.IsValidIndex(Index) || (!LinkReceipts[Index].TransactionId.IsValid() && (!IsValid(Link) || !Link->IsCommandLinkActive())))
                { LastPhaseError = TEXT("A required link disappeared without its native Selene sever; retry the phase entry."); FailEncounter(); return; }
            }
            return;
        }
        if (!bBoundaryFrozen)
        {
            TGuardValue<bool> Mutation(bPhaseMutation, true);
            const FGuid Attempt = GetAttemptId(); const uint64 Generation = GetLifecycleGeneration();
            const auto FrozenParticipants = Participants;
            for (const auto& Participant : FrozenParticipants)
            {
                if (IsValid(Participant.Character) && Participant.Character->IsAlive()) { SuspendActor(Participant.Character); }
                if (IsActorBeingDestroyed() || GetAttemptId() != Attempt || GetLifecycleGeneration() != Generation || GetEncounterState() != ESovEncounterState::Active) { return; }
            }
            bBoundaryFrozen = true; SettleStartedAt = GetWorld()->GetTimeSeconds();
        }
        if (AreOwnedParticipantsQuiescent(true)) { CompleteEncounter(); }
        else if (GetWorld()->GetTimeSeconds() - SettleStartedAt > FMath::Clamp(BoundarySettleTimeout, 1.f, 60.f))
        { LastPhaseError = TEXT("Crucible phase boundary did not settle safely; retry the phase entry."); FailEncounter(); }
        return;
    }
    if (GetEncounterState() != ESovEncounterState::Succeeded || IsCampaignReceiptPending()) { return; }
    auto* PC = Cast<ASovPlayerController>(GetWorld()->GetFirstPlayerController());
    auto* Player = PC ? Cast<ASovPlayerCharacterBase>(PC->GetPawn()) : nullptr;
    if (bAutoStartPhaseB && HasHandoffJournal(Player)) { CompletePhaseHandoff(Player, LastPhaseError); return; }
    if (bAutoRequestHandoff && IsValid(HandoffAnchor) && IsCompletedPhaseBoundaryQuiescentForSave(Player))
    { HandoffAnchor->RequestHandoff(PC, LastPhaseError); }
}
void ASovAurelionLinkPhaseDirector::Load_Implementation()
{
    Super::Load_Implementation(); UnbindLinks(); ProofPlayer.Reset(); ProofController.Reset(); ProofASC.Reset();
    if (GetEncounterState() == ESovEncounterState::Succeeded && bBoundaryFrozen && !bTransferred)
    {
        for (const auto& Participant : Participants)
        { if (IsValid(Participant.Character) && Participant.Character->IsAlive()) { SuspendActor(Participant.Character); } }
        SetActorTickEnabled(true);
    }
}
void ASovAurelionLinkPhaseDirector::EndPlay(EEndPlayReason::Type Reason)
{ UnbindLinks(); Super::EndPlay(Reason); }

ASovAurelionThermalPhaseDirector::ASovAurelionThermalPhaseDirector()
{ OnEncounterStateChanged.AddDynamic(this, &ThisClass::HandlePhaseState); }
void ASovAurelionThermalPhaseDirector::BindFracture()
{
    auto* Elite = GetParticipant(EliteParticipantId);
    auto* Component = IsValid(Elite) ? Elite->FindComponentByClass<USovAurelionThermalFractureComponent>() : nullptr;
    if (FractureSource == Component) { return; }
    if (IsValid(FractureSource)) { FractureSource->OnThermalFractureCompleted.RemoveDynamic(this, &ThisClass::HandleFracture); }
    FractureSource = Component;
    if (IsValid(FractureSource))
    {
        FractureSource->InitializeBindings();
        FractureSource->OnThermalFractureCompleted.AddUniqueDynamic(this, &ThisClass::HandleFracture);
    }
}
void ASovAurelionThermalPhaseDirector::HandlePhaseState(ESovEncounterState Previous, ESovEncounterState Current)
{
    if (Current == ESovEncounterState::Active)
    {
        FractureAttemptId.Invalidate(); FractureFrostId.Invalidate(); FractureHeatId.Invalidate(); FracturePayoffId.Invalidate();
        BindFracture(); SetActorTickEnabled(true);
    }
}
void ASovAurelionThermalPhaseDirector::HandleFracture(const FSovAurelionThermalFractureReceipt& Receipt)
{
    if (!HasAuthority() || GetEncounterState() != ESovEncounterState::Active || !IsValid(FractureSource)
        || Receipt.EncounterId != EncounterId || Receipt.AttemptId != GetAttemptId()
        || !FractureSource->HasCompletedFracture(this, GetAttemptId())) { return; }
    const auto Current = FractureSource->GetFractureReceipt();
    if (Current.FrostApplicationId != Receipt.FrostApplicationId || Current.HeatTransactionId != Receipt.HeatTransactionId
        || Current.PayoffTransactionId != Receipt.PayoffTransactionId) { return; }
    FractureAttemptId = Receipt.AttemptId; FractureFrostId = Receipt.FrostApplicationId;
    FractureHeatId = Receipt.HeatTransactionId; FracturePayoffId = Receipt.PayoffTransactionId;
}
bool ASovAurelionThermalPhaseDirector::HasConfirmedVictory() const
{
    return Super::HasConfirmedVictory() && DefeatedParticipants.Contains(EliteParticipantId)
        && FractureAttemptId == GetAttemptId() && FractureAttemptId.IsValid()
        && FractureFrostId.IsValid() && FractureHeatId.IsValid() && FracturePayoffId.IsValid();
}
bool ASovAurelionThermalPhaseDirector::CompleteEncounter()
{
    if (FractureAttemptId != GetAttemptId() || !FractureAttemptId.IsValid())
    { FailEncounter(); return false; } // A premature conventional kill restarts from the retained phase B entry.
    return Super::CompleteEncounter();
}
void ASovAurelionThermalPhaseDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetEncounterState() == ESovEncounterState::Active)
    {
        const auto* Elite = GetParticipant(EliteParticipantId);
        if ((!IsValid(Elite) || !Elite->IsAlive()) && FractureAttemptId != GetAttemptId()) { FailEncounter(); return; }
        BindFracture();
    }
}
