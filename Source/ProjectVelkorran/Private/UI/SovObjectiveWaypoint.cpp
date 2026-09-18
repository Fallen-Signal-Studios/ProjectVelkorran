// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovObjectiveWaypoint.h"
#include "Campaign/SovAurelionRequestActor.h"
#include "Campaign/SovCampaignInteractionTerminal.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovCampaignRelayReceiver.h"
#include "Campaign/SovAurelionCrucibleDirector.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "World/SovWorldTransitActor.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Framework/SovPlayerController.h"

namespace
{
    bool CurrentActor(const AActor* Actor, const UWorld* World)
    { return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->GetWorld() == World; }

    const FSovCampaignBeatDefinition* CurrentBeat(const ASovPlayerController* PC, FName MissionId, FName BeatId)
    {
        const auto* Pawn = IsValid(PC) ? Cast<ASovPlayerCharacterBase>(PC->GetPawn()) : nullptr;
        const auto* State = IsValid(PC) ? PC->GetCampaignState() : nullptr;
        const auto* Mission = State ? State->GetActiveMission() : nullptr;
        if (!Pawn || Pawn->GetController() != PC || !CurrentActor(Pawn, PC->GetWorld())
            || PC->IsActorBeingDestroyed() || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle
            || !State || !State->IsStateValid() || State->IsMutationInProgress() || !IsValid(Mission)
            || Mission->MissionId != MissionId || State->GetActiveProtagonist() != Pawn->GetProtagonistIdentityTag()
            || !State->GetActiveProtagonist().IsValid()) { return nullptr; }
        const auto* Beat = Mission->FindBeat(BeatId);
        const auto Status = State->GetObjectiveState(MissionId, BeatId);
        return Beat && !Beat->ObjectiveText.IsEmpty()
            && (Status == ESovObjectiveState::Available || Status == ESovObjectiveState::Active)
            && State->GetActionableObjectiveIds().Contains(BeatId)
            && State->HasKnowledge(State->GetActiveProtagonist(), Beat->RequiredKnowledge)
            && (!Beat->RequiredProtagonist.IsValid() || Beat->RequiredProtagonist == State->GetActiveProtagonist()) ? Beat : nullptr;
    }

    void Add(TArray<FSovObjectiveWaypoint>& Candidates, AActor* Source, AActor* Target,
        USceneComponent* Anchor, FName Mission, FName Beat, FSovObjectiveWaypoint::EKind Kind)
    {
        if (!CurrentActor(Target, Source->GetWorld()) || !IsValid(Anchor) || !Anchor->IsRegistered()
            || Anchor->GetOwner() != Target || Anchor->GetComponentLocation().ContainsNaN()) { return; }
        FSovObjectiveWaypoint Row;
        Row.Source = Source; Row.Target = Target; Row.Anchor = Anchor; Row.MissionId = Mission; Row.BeatId = Beat; Row.Kind = Kind;
        Candidates.Add(Row);
    }
}

bool SovObjectiveWaypoint::IsSupportedSource(const AActor* Actor)
{
    return IsValid(Actor) && (Actor->IsA<ASovAurelionRequestActor>() || Actor->IsA<ASovCampaignInteractionTerminal>()
        || Actor->IsA<ASovCampaignEncounterObjective>());
}

bool SovObjectiveWaypoint::Resolve(const ASovPlayerController* PC, const TArray<FSovObjectivePresentationEntry>& Entries,
    const TArray<TWeakObjectPtr<AActor>>& Sources, FSovObjectiveWaypoint& Out)
{
    Out = {};
    const auto* State = IsValid(PC) ? PC->GetCampaignState() : nullptr;
    const auto* Mission = State ? State->GetActiveMission() : nullptr;
    if (!IsValid(Mission) || !PC->GetPawn()) { return false; }
    // Never choose a side of an authored choice on the player's behalf. Both options remain in the objective list.
    for (const auto& Entry : Entries)
    {
        const auto* Beat = CurrentBeat(PC, Mission->MissionId, Entry.BeatId);
        if (!Beat || Beat->bInteractiveChoice) { continue; }
        TArray<FSovObjectiveWaypoint> Candidates;
        TSet<FString> Identities;
        bool bAmbiguous = false;
        const auto Identity = [&](const FString& Id)
        { if (Identities.Contains(Id)) { bAmbiguous = true; } Identities.Add(Id); };
        for (const auto& Weak : Sources)
        {
            AActor* Source = Weak.Get();
            if (!CurrentActor(Source, PC->GetWorld())) { continue; }
            if (auto* Request = Cast<ASovAurelionRequestActor>(Source))
            {
                if (Request->MissionId != Mission->MissionId || Request->BeatId != Entry.BeatId || Request->RequestId.IsNone()) { continue; }
                if (Request->Operation == ESovAurelionRequest::RetryEncounter)
                {
                    const auto* Objective = Request->RetryObjective.Get();
                    const auto* Director = Request->RetryDirector.Get();
                    if (CurrentActor(Objective, PC->GetWorld()) && CurrentActor(Director, PC->GetWorld())
                        && Objective->MissionId == Mission->MissionId && Objective->CompletionBeat == Entry.BeatId
                        && Objective->EncounterDirector == Director && !Beat->RequiredEncounterId.IsNone()
                        && Director->EncounterId == Beat->RequiredEncounterId
                        && Director->GetCampaignProofType() == Beat->RequiredEncounterProof
                        && Director->GetEncounterState() == ESovEncounterState::Failed
                        && !Objective->IsResultPending() && !Director->IsCampaignReceiptPending())
                    {
                        Identity(TEXT("Request:") + Request->RequestId.ToString());
                        if (!Request->IsRequestPending())
                        { Add(Candidates, Source, Source, Request->Body, Mission->MissionId, Entry.BeatId, FSovObjectiveWaypoint::EKind::Retry); }
                    }
                    continue;
                }
                // Contextual thermal commands and completed-mission travel are not ordinary next-goal hints.
                if (Request->Operation != ESovAurelionRequest::PlayScene && Request->Operation != ESovAurelionRequest::Handoff
                    && Request->Operation != ESovAurelionRequest::CoAction) { continue; }
                Identity(TEXT("Request:") + Request->RequestId.ToString());
                if (!Request->IsRequestPending())
                { Add(Candidates, Source, Source, Request->Body, Mission->MissionId, Entry.BeatId, FSovObjectiveWaypoint::EKind::Interaction); }
            }
            else if (auto* Terminal = Cast<ASovCampaignInteractionTerminal>(Source))
            {
                if (Terminal->MissionId != Mission->MissionId || Terminal->CompletionBeat != Entry.BeatId || Terminal->TerminalId.IsNone()) { continue; }
                Identity(TEXT("Terminal:") + Terminal->TerminalId.ToString());
                if (!Terminal->IsRequestPending())
                { Add(Candidates, Source, Source, Terminal->Body, Mission->MissionId, Entry.BeatId, FSovObjectiveWaypoint::EKind::Interaction); }
            }
            else if (auto* Objective = Cast<ASovCampaignEncounterObjective>(Source))
            {
                const auto* Director = Objective->EncounterDirector.Get();
                if (Objective->MissionId != Mission->MissionId || Objective->CompletionBeat != Entry.BeatId
                    || !CurrentActor(Director, PC->GetWorld()) || Beat->RequiredEncounterId.IsNone()
                    || Director->EncounterId != Beat->RequiredEncounterId || Objective->IsResultPending()) { continue; }
                Identity(TEXT("Encounter:") + Director->EncounterId.ToString());
                const auto EncounterState = Director->GetEncounterState();
                if (EncounterState == ESovEncounterState::Active)
                {
                    // Some protection encounters cannot advance until the player opens
                    // their authored reinforcement door. Keep that physical action
                    // discoverable instead of leaving an active rescue without a marker.
                    const auto* Coordination = Director->GetCoordinationComponent();
                    if (IsValid(Coordination) && Coordination->GetOwner() == Director)
                    {
                        for (const auto& Rule : Coordination->WaveReleaseRules)
                        {
                            auto* Door = Rule.TransitDoor.Get();
                            if (Rule.Wave != Coordination->GetCurrentWave() + 1
                                || Rule.Condition != ESovEncounterWaveCondition::TransitDoorOpen
                                || !CurrentActor(Door, PC->GetWorld()) || Door->Kind != ESovWorldTransitKind::Door
                                || Door->RequiredMission != Mission->MissionId
                                || Door->GetTransitState() != ESovWorldTransitState::AtOrigin) { continue; }
                            Identity(TEXT("Transit:") + Door->TransitId.ToString());
                            if (!Door->TransitId.IsNone() && Door->StructuralHealth > 0.f
                                && (!Door->bRequiresPower || Door->bPowered) && Door->LockReason.IsEmpty())
                            { Add(Candidates, Source, Door, Door->MovingBody, Mission->MissionId, Entry.BeatId, FSovObjectiveWaypoint::EKind::Interaction); }
                        }
                    }
                    // A command link is the same shape as an outstanding receiver: a physical thing
                    // the player has to reach before an active encounter can advance. It is carried
                    // by an enemy rather than bolted to a wall, so the hint tracks that enemy.
                    if (const auto* Links = Cast<ASovAurelionLinkPhaseDirector>(Director))
                    {
                        TArray<ASovNPCCharacterBase*> Carriers; TArray<FName> LinkIds;
                        Links->GetOutstandingLinkCarriers(Carriers, LinkIds);
                        for (int32 Index = 0; Index < Carriers.Num(); ++Index)
                        {
                            AActor* const Carrier = Carriers[Index];
                            if (!CurrentActor(Carrier, PC->GetWorld())) { continue; }
                            Identity(TEXT("Link:") + LinkIds[Index].ToString());
                            Add(Candidates, Source, Carrier, Carrier->GetRootComponent(),
                                Mission->MissionId, Entry.BeatId, FSovObjectiveWaypoint::EKind::Receiver);
                        }
                    }
                    for (const auto& ReceiverPtr : Objective->RequiredReceivers)
                    {
                        auto* Receiver = ReceiverPtr.Get();
                        if (CurrentActor(Receiver, PC->GetWorld()) && Receiver->EncounterObjective == Objective
                            && Beat->RequiredReceiverIds.Contains(Receiver->ReceiverId))
                        {
                            Identity(TEXT("Receiver:") + Receiver->ReceiverId.ToString());
                            if (!Receiver->IsDisabled() && !Receiver->IsRequestPending())
                            { Add(Candidates, Source, Receiver, Receiver->Body, Mission->MissionId, Entry.BeatId, FSovObjectiveWaypoint::EKind::Receiver); }
                        }
                    }
                }
                // A live combat area is not a command to return to its entry; show only outstanding receivers then.
                else if (EncounterState == ESovEncounterState::Inactive && Objective->bStartOnPlayerOverlap)
                { Add(Candidates, Source, Source, Objective->StartVolume, Mission->MissionId, Entry.BeatId, FSovObjectiveWaypoint::EKind::Encounter); }
            }
        }
        // Preserve the frontend's primary goal; never redirect to an unrelated lower-priority task.
        if (bAmbiguous || Candidates.IsEmpty()) { return false; }
        const FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
        Candidates.StableSort([&](const auto& A, const auto& B)
        {
            return FVector::DistSquared(PlayerLocation, A.Anchor->GetComponentLocation())
                < FVector::DistSquared(PlayerLocation, B.Anchor->GetComponentLocation());
        });
        Out = Candidates[0]; return true;
    }
    return false;
}

bool SovObjectiveWaypoint::IsCurrent(const ASovPlayerController* PC, const FSovObjectiveWaypoint& Row)
{
    if (!IsValid(PC) || !CurrentBeat(PC, Row.MissionId, Row.BeatId) || !CurrentActor(Row.Source.Get(), PC->GetWorld())
        || !CurrentActor(Row.Target.Get(), PC->GetWorld()) || !Row.Anchor.IsValid() || !Row.Anchor->IsRegistered()
        || Row.Anchor->GetOwner() != Row.Target.Get()) { return false; }
    FSovObjectivePresentationEntry Entry; Entry.BeatId = Row.BeatId;
    FSovObjectiveWaypoint Current;
    return Resolve(PC, {Entry}, {Row.Source}, Current) && Current.Target == Row.Target && Current.Anchor == Row.Anchor;
}

bool SovObjectiveWaypoint::FitToSafeRect(FVector2D Projected, bool bProjectedInFront, FVector2D Bearing,
    FVector2D Min, FVector2D Max, FVector2D& Position, bool& bAtEdge)
{
    Position = FVector2D::ZeroVector; bAtEdge = false;
    if (Min.ContainsNaN() || Max.ContainsNaN() || Min.X >= Max.X || Min.Y >= Max.Y || Bearing.ContainsNaN()) { return false; }
    const FVector2D Center = (Min + Max) * .5;
    if (bProjectedInFront && !Projected.ContainsNaN() && Projected.X >= Min.X && Projected.X <= Max.X
        && Projected.Y >= Min.Y && Projected.Y <= Max.Y) { Position = Projected; return true; }
    FVector2D Direction = bProjectedInFront && !Projected.ContainsNaN() ? Projected - Center : Bearing;
    if (Direction.IsNearlyZero()) { Direction = FVector2D(0., 1.); }
    const FVector2D Half = (Max - Min) * .5;
    const double Factor = FMath::Max(FMath::Abs(Direction.X) / Half.X, FMath::Abs(Direction.Y) / Half.Y);
    Position = Center + Direction / Factor; bAtEdge = true;
    return !Position.ContainsNaN();
}
