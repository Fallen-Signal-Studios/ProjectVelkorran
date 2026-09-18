// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionCrucibleDirector.h"

#include "UI/SovFrontendComponent.h"
#include "UI/SovAccessibilityPresentation.h"

#include "Campaign/SovLethalFloorComponent.h"
#include "AI/SovAurelionEnemyRoles.h"
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovCampaignHandoffAnchor.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SovAurelionThermalFractureComponent.h"
#include "Components/SovCommandLinkComponent.h"
#include "AI/NarrativeNPCController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

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
void ASovAurelionLinkPhaseDirector::FailPhaseWithReason(const FText& Reason)
{
    LastPhaseError = Reason.ToString();
    // Straight to the caption channel the player already reads for warnings. A failure the player
    // cannot see the cause of is indistinguishable from the game being broken.
    if (const auto* PC = Cast<ASovPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
    {
        const auto* Frontend = PC->GetFrontend();
        if (auto* Presentation = Frontend ? Frontend->GetPresentation() : nullptr)
        { Presentation->PresentCaption(Reason, 6.f, GetActorLocation(), ESovCaptionPriority::Critical); }
    }
    FailEncounter();
}
void ASovAurelionLinkPhaseDirector::GetOutstandingLinkCarriers(TArray<ASovNPCCharacterBase*>& OutCarriers,
    TArray<FName>& OutLinkIds) const
{
    OutCarriers.Reset(); OutLinkIds.Reset();
    if (GetEncounterState() != ESovEncounterState::Active || HasLinkProof()) { return; }
    for (int32 Index = 0; Index < RequiredLinks.Num(); ++Index)
    {
        // A severed link is done with even if its carrier is still fighting, so the hint clears as
        // each one is spent rather than when the whole phase resolves.
        if (LinkReceipts.IsValidIndex(Index) && LinkReceipts[Index].TransactionId.IsValid()) { continue; }
        auto* Carrier = GetParticipant(RequiredLinks[Index].ParticipantId);
        const auto* Link = ResolveLink(RequiredLinks[Index]);
        if (!IsValid(Carrier) || !Carrier->IsAlive() || !Link || !Link->IsCommandLinkActive()) { continue; }
        OutCarriers.Add(Carrier); OutLinkIds.Add(RequiredLinks[Index].LinkId);
    }
}
void ASovAurelionLinkPhaseDirector::UnbindLinks()
{
    for (USovCommandLinkComponent* Link : BoundLinks)
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
bool ASovAurelionLinkPhaseDirector::HasAcceptedCurrentLinkReceipt() const
{
    if (!HasAuthority() || IsActorBeingDestroyed() || bPhaseMutation || GetEncounterState() != ESovEncounterState::Active
        || !ProofAttemptId.IsValid() || GetAttemptId() != ProofAttemptId || !LastPhaseError.IsEmpty()
        || !ProofPlayer.IsValid() || !ProofController.IsValid() || !ProofASC.IsValid()
        || ProofController->GetPawn() != ProofPlayer.Get() || ProofPlayer->GetController() != ProofController.Get()
        || GetEncounterPlayer() != ProofPlayer.Get() || ProofPlayer->GetNarrativeAbilitySystemComponent() != ProofASC.Get()
        || ProofASC->GetAvatarActor() != ProofPlayer.Get() || !ProofPlayer->IsCharacterReady() || !ProofPlayer->IsAlive()
        || ProofASC->GetCharacterReadyEpoch() != ProofReadyEpoch || ProofASC->GetCombatActorInfoEpoch() != ProofActorInfoEpoch
        || ProofController->GetCampaignTransitionEpoch() != ProofTransitionEpoch || RequiredLinks.Num() != 2 || LinkReceipts.Num() != 2) { return false; }
    for (int32 Index = 0; Index < RequiredLinks.Num(); ++Index)
    {
        const auto& Receipt = LinkReceipts[Index];
        if (Receipt.LinkId == RequiredLinks[Index].LinkId && Receipt.InstanceId.IsValid() && Receipt.TransactionId.IsValid()) { return true; }
    }
    return false;
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
    if (bPhaseMutation || !HasLinkProof() || !bBoundaryFrozen || !AreOwnedParticipantsQuiescent(true)
        || !GetCoordinationComponent() || GetCoordinationComponent()->HasUnreleasedWaves()) { return false; }
    const auto* Elite = GetParticipant(EliteParticipantId);
    if (!IsValid(Elite) || !Elite->IsAlive()) { FailEncounter(); return false; }
    CompletedReleaseWave = GetCoordinationComponent()->GetCurrentWave();
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
        && AreOwnedParticipantsQuiescent(true)
        // Owned participants are only half the roster. Summoned adds are attempt-scoped, so a boundary
        // that ignored them could admit a save with live hostiles still swinging (audit EA2-01).
        && !HasLiveAttemptCombatants();
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
    if (!GetCoordinationComponent() || !GetCoordinationComponent()->ReleaseCompletedPhaseBindings())
    { Error = TEXT("The source coordinator must release its completed roster before phase transfer."); return false; }
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
namespace
{
	/**
	 * Holds or releases the Elite's lethal floor, creating the component the first time a phase needs it.
	 *
	 * A phase that requires a mechanic must not be losable by killing its boss first. Keeping the floor
	 * here rather than in the damage path means each phase owns exactly the window it is responsible for.
	 */
	void HoldLethalFloor(ASovNPCCharacterBase* Elite, const bool bHold, const float Fraction)
	{
		if (!IsValid(Elite) || !Elite->HasAuthority()) { return; }
		auto* Component = Elite->FindComponentByClass<USovLethalFloorComponent>();
		if (!Component)
		{
			// Nothing to release, and an unheld floor is not worth adding a component for.
			if (!bHold) { return; }
			Component = NewObject<USovLethalFloorComponent>(Elite);
			Elite->AddInstanceComponent(Component);
			Component->RegisterComponent();
		}
		// Resolved from the Elite's current maximum every time, so a phase that raises its health, or a
		// retune, moves the floor with it rather than leaving a threshold that no longer means anything.
		const auto* ASC = Elite->GetNarrativeAbilitySystemComponent();
		const float Maximum = ASC ? ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute()) : 0.f;
		const float Safe = FMath::IsFinite(Fraction) ? FMath::Clamp(Fraction, .01f, .5f) : .12f;
		Component->MinimumHealth = FMath::IsFinite(Maximum) && Maximum > 0.f ? FMath::Max(Maximum * Safe, 1.f) : 1.f;
		Component->SetFloorHeld(bHold);
	}
}

void ASovAurelionLinkPhaseDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || IsActorBeingDestroyed() || bPhaseMutation) { return; }
    if (GetEncounterState() == ESovEncounterState::Active)
    {
        auto* Elite = GetParticipant(EliteParticipantId);
        // Held until the links are proven, so a player who out-damages the mechanic cannot lose the run
        // to their own success. The failure below now only catches causes the player did not create.
        HoldLethalFloor(Elite, !HasLinkProof(), EliteLethalFloorFraction);
        // The carriers need the same protection for the same reason. A carrier killed before its
        // link is severed takes the link with it, and the check below then fails the phase - so the
        // most ordinary thing a player can do, killing the enemy in front of them, ended the run
        // with no way back and no explanation.
        for (int32 Index = 0; Index < RequiredLinks.Num(); ++Index)
        {
            const bool bSevered = LinkReceipts.IsValidIndex(Index) && LinkReceipts[Index].TransactionId.IsValid();
            HoldLethalFloor(GetParticipant(RequiredLinks[Index].ParticipantId),
                !HasLinkProof() && !bSevered, EliteLethalFloorFraction);
        }
        if (!IsValid(Elite) || !Elite->IsAlive())
        {
            FailPhaseWithReason(NSLOCTEXT("SovCrucible", "EliteDefeatedEarly",
                "The Elite was destroyed before the links were severed. Retry the phase."));
            return;
        }
        if (!LastPhaseError.IsEmpty()) { FailEncounter(); return; }
        if (!HasLinkProof())
        {
            for (int32 Index = 0; Index < RequiredLinks.Num(); ++Index)
            {
                auto* Link = ResolveLink(RequiredLinks[Index]);
                if (!LinkReceipts.IsValidIndex(Index) || (!LinkReceipts[Index].TransactionId.IsValid() && (!IsValid(Link) || !Link->IsCommandLinkActive())))
                {
                    FailPhaseWithReason(NSLOCTEXT("SovCrucible", "LinkLost",
                        "A command link was destroyed instead of severed. Retry the phase."));
                    return;
                }
            }
            return;
        }
        // Both receipts can arrive before the coordinator tick. Never freeze a still-hidden reinforcement.
        if (!GetCoordinationComponent() || GetCoordinationComponent()->HasUnreleasedWaves()) { return; }
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
        {
            FailPhaseWithReason(NSLOCTEXT("SovCrucible", "BoundaryUnsettled",
                "The phase could not settle safely. Retry the phase."));
        }
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
        if (!HasConfirmedVictory() || !GetCoordinationComponent() || !GetCoordinationComponent()->RestoreCompletedWaveState(CompletedReleaseWave))
        { LastPhaseError = TEXT("The completed phase's saved wave release does not match its authored roster."); return; }
        for (const auto& Participant : Participants)
        { if (IsValid(Participant.Character) && Participant.Character->IsAlive()) { SuspendActor(Participant.Character); } }
        SetActorTickEnabled(true);
    }
}
void ASovAurelionLinkPhaseDirector::EndPlay(EEndPlayReason::Type Reason)
{ UnbindLinks(); Super::EndPlay(Reason); }

ASovAurelionThermalPhaseDirector::ASovAurelionThermalPhaseDirector()
{ OnEncounterStateChanged.AddDynamic(this, &ThisClass::HandlePhaseState); }
void ASovAurelionThermalPhaseDirector::Load_Implementation()
{
    const uint64 PreviousGeneration = RestoreGeneration;
    Super::Load_Implementation();
    const uint64 ExpectedGeneration = PreviousGeneration + 1;
    const ESovEncounterState LoadedState = GetEncounterState();
    if (!HasAuthority() || IsActorBeingDestroyed() || RestoreGeneration != ExpectedGeneration
        || !bHasEntryCheckpoint || SnapshotSchemaVersion != 1 || !ProtectedParticipantIds.IsEmpty()
        || EntryProtectedParticipantIds.IsEmpty() || !TransferredParticipantIds.IsEmpty()
        || (LoadedState != ESovEncounterState::Succeeded && LoadedState != ESovEncounterState::Failed)) { return; }

    // Phase B is authored empty and receives these roles through the actual phase transfer.
    // They already exist in the saved entry snapshot; fresh map construction must reconstruct
    // that metadata before the unchanged protection/victory checks can evaluate the saved result.
    const TSet<FName> SavedProtection = EntryProtectedParticipantIds;
    TArray<TStrongObjectPtr<ASovNPCCharacterBase>> ProtectedPins;
    TArray<FName> CheckedIds;
    TArray<FGuid> CheckedGUIDs;
    TSet<FGuid> ProtectedGUIDs;
    const auto StillOwnsLoad = [this, ExpectedGeneration, LoadedState, &SavedProtection]()
    {
        if (!IsValid(this) || IsActorBeingDestroyed() || RestoreGeneration != ExpectedGeneration
            || GetEncounterState() != LoadedState || !ProtectedParticipantIds.IsEmpty()
            || EntryProtectedParticipantIds.Num() != SavedProtection.Num()) { return false; }
        for (const FName Id : SavedProtection) { if (!EntryProtectedParticipantIds.Contains(Id)) { return false; } }
        return true;
    };
    for (const FName Id : SavedProtection)
    {
        const FSovEncounterNPCRecord* Record = nullptr;
        for (const auto& Candidate : EntryParticipants)
        { if (Candidate.ParticipantId == Id) { if (Record) { return; } Record = &Candidate; } }
        if (Id.IsNone() || !Record || Record->bRequiredForVictory || Record->bAllowMassRepresentation
            || !Record->ActorRecord.ActorGUID.IsValid()) { return; }
        const FGuid SavedGUID = Record->ActorRecord.ActorGUID;
        const FSovEncounterParticipant* Participant = nullptr;
        for (const auto& Candidate : Participants)
        { if (Candidate.ParticipantId == Id) { if (Participant) { return; } Participant = &Candidate; } }
        if (!Participant || Participant->bRequiredForVictory || Participant->bAllowMassRepresentation
            || IsParticipantMassRepresented(Id)) { return; }
        auto* NPC = Participant->Character.Get();
        if (!IsValid(NPC) || NPC->IsActorBeingDestroyed() || NPC->GetWorld() != GetWorld()) { return; }
        if (ProtectedGUIDs.Contains(SavedGUID)) { return; }
        ProtectedGUIDs.Add(SavedGUID); ProtectedPins.Emplace(NPC);
        CheckedIds.Add(Id); CheckedGUIDs.Add(SavedGUID);
        const FGuid CurrentGUID = INarrativeSavableActor::Execute_GetActorGUID(NPC);
        if (!StillOwnsLoad() || !IsValid(NPC) || NPC->IsActorBeingDestroyed() || NPC->GetWorld() != GetWorld()
            || GetParticipant(Id) != NPC || CurrentGUID != SavedGUID) { return; }
    }
    if (!StillOwnsLoad()) { return; }
    // A later actor's GUID callback may have retired or replaced an earlier validated row.
    for (int32 Index = 0; Index < ProtectedPins.Num(); ++Index)
    {
        const auto* NPC = ProtectedPins[Index].Get();
        const FName Id = CheckedIds[Index];
        if (!IsValid(NPC) || NPC->IsActorBeingDestroyed() || NPC->GetWorld() != GetWorld() || GetParticipant(Id) != NPC) { return; }
        int32 SavedMatches = 0, LiveMatches = 0;
        for (const auto& Record : EntryParticipants)
        {
            if (Record.ParticipantId != Id) { continue; }
            if (Record.bRequiredForVictory || Record.bAllowMassRepresentation || Record.ActorRecord.ActorGUID != CheckedGUIDs[Index]) { return; }
            ++SavedMatches;
        }
        for (const auto& Participant : Participants)
        {
            if (Participant.ParticipantId != Id) { continue; }
            if (Participant.bRequiredForVictory || Participant.bAllowMassRepresentation || Participant.Character != NPC) { return; }
            ++LiveMatches;
        }
        if (SavedMatches != 1 || LiveMatches != 1 || IsParticipantMassRepresented(Id)) { return; }
    }
    ProtectedParticipantIds = SavedProtection;
}
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
namespace
{
/** The link phase is fought at the link phase's durability; the same elite re-arms for the real
 * fight as this phase goes active, including on a retry, which restores the full crucible pool. */
void ApplyCrucibleDurability(ASovAurelionThermalPhaseDirector* Director)
{
    if (!IsValid(Director) || !Director->HasAuthority()) { return; }
    ASovNPCCharacterBase* const Elite = Director->GetParticipant(Director->EliteParticipantId);
    if (!IsValid(Elite) || !Elite->IsAlive()) { return; }
    UNarrativeAbilitySystemComponent* const ASC = Elite->GetNarrativeAbilitySystemComponent();
    if (!IsValid(ASC) || ASC->GetAvatarActor() != Elite) { return; }
    FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
    Context.AddInstigator(Elite, Elite);
    const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(USovAurelionEliteCrucibleDurability::StaticClass(), 1.f, Context);
    if (Spec.IsValid()) { ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()); }
}
}

void ASovAurelionThermalPhaseDirector::HandlePhaseState(ESovEncounterState Previous, ESovEncounterState Current)
{
    if (Current == ESovEncounterState::Active)
    {
        FractureAttemptId.Invalidate(); FractureFrostId.Invalidate(); FractureHeatId.Invalidate(); FracturePayoffId.Invalidate();
        BindFracture(); SetActorTickEnabled(true);
        ApplyCrucibleDurability(this);
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
        auto* Elite = GetParticipant(EliteParticipantId);
        // Released the moment this attempt's thermal fracture receipt lands, which is the point the
        // Elite is meant to become finishable.
        HoldLethalFloor(Elite, FractureAttemptId != GetAttemptId(), EliteLethalFloorFraction);
        if ((!IsValid(Elite) || !Elite->IsAlive()) && FractureAttemptId != GetAttemptId()) { FailEncounter(); return; }
        BindFracture();
    }
    else { HoldLethalFloor(GetParticipant(EliteParticipantId), false, EliteLethalFloorFraction); }
}
