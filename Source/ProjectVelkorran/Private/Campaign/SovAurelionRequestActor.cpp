// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionRequestActor.h"
#include "Campaign/SovAurelionMissionDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovCampaignHandoffAnchor.h"
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Cinematics/SovAurelionStorySequenceActor.h"
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovCoActionAnchor.h"
#include "Components/SovAurelionThermalFractureComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Misc/PackageName.h"
#include "NarrativeGameplayTags.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "SovAurelionRequest"
namespace
{
    ASovProtagonistCompanionCharacter* GetCompanion(const ASovPlayerController* PC)
    { return PC && PC->GetConvergenceCompanionState() ? PC->GetConvergenceCompanionState()->GetActiveCompanion() : nullptr; }
    bool IsThermal(ESovAurelionRequest Operation)
    { return Operation == ESovAurelionRequest::MoveFrostPartner || Operation == ESovAurelionRequest::FrostSetup || Operation == ESovAurelionRequest::HeatConfirm; }
    bool IsCurrentActor(const AActor* Actor, const UWorld* World)
    { return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->GetWorld() == World; }
}
bool USovAurelionRequestInteractable::CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error)
{
    const auto* Request = Cast<ASovAurelionRequestActor>(GetOwner());
    return Super::CanInteract_Implementation(Pawn, Interaction, Error) && Request && Request->CanUse(Pawn, Error);
}
FText USovAurelionRequestInteractable::GetInteractableActionText_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction) const
{
    const auto* Request = Cast<ASovAurelionRequestActor>(GetOwner()); FText Error;
    if (Request && !Request->CanUse(Pawn, Error)) { return Error; }
    if (Request && Request->Operation == ESovAurelionRequest::RetryEncounter) { return LOCTEXT("RetryEncounter", "Retry encounter"); }
    return Request ? Request->ActionText : Super::GetInteractableActionText_Implementation(Pawn, Interaction);
}
bool USovAurelionRequestInteractable::Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction)
{
    auto* Request = Cast<ASovAurelionRequestActor>(GetOwner()); FText Error;
    if (!Request || !CanInteract(Pawn, Interaction, Error) || !Request->RequestUse(Pawn, Error)) { return false; }
    // Preserve Narrative's normal authored events, after reserving the one deferred request.
    OnInteract(Pawn, Interaction);
    if (IsValid(this)) { OnInteracted.Broadcast(Pawn, Interaction); }
    return true;
}
ASovAurelionRequestActor::ASovAurelionRequestActor()
{
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = .15f;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body")); Body->SetupAttachment(SceneRoot);
    Body->SetBoxExtent(FVector(25, 35, 45)); Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Visual->SetupAttachment(Body);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(SceneRoot);
    Label->SetRelativeLocation(FVector(0, 0, 65)); Label->SetWorldSize(8.f); Label->SetHorizontalAlignment(EHTA_Center);
    Interactable = CreateDefaultSubobject<USovAurelionRequestInteractable>(TEXT("Interactable"));
    Interactable->InteractionDistance = 300.f; Interactable->InteractionTime = .35f;
    // A required mission control outranks incidental interactables, so a hostile standing beside it
    // cannot take the prompt during a fight. Matches the rescue destination's mission-critical priority.
    Interactable->InteractionPriority = 20;
    Interactable->InteractableNameText = LOCTEXT("Name", "Aurelion");
    ActionText = LOCTEXT("Interact", "Interact");
}
bool ASovAurelionRequestActor::CanUse(const APawn* Pawn, FText& Error) const
{ return CanUseInternal(Pawn, Error, false); }
USovAurelionThermalFractureComponent* ASovAurelionRequestActor::GetCurrentThermalTarget() const
{
    if (!IsThermal(Operation)) { return nullptr; }
    const bool bStableBinding = ThermalDirector != nullptr || !ThermalParticipantId.IsNone();
    if (!bStableBinding)
    {
        return IsValid(Thermal) && Thermal->IsRegistered() && Thermal->IsActive()
            && IsCurrentActor(Thermal->GetOwner(), GetWorld()) ? Thermal.Get() : nullptr;
    }
    if (Thermal || ThermalParticipantId.IsNone() || !IsCurrentActor(ThermalDirector, GetWorld())) { return nullptr; }
    auto* Participant = ThermalDirector->GetParticipant(ThermalParticipantId);
    if (!IsCurrentActor(Participant, GetWorld())) { return nullptr; }
    TInlineComponentArray<USovAurelionThermalFractureComponent*> Components(Participant);
    USovAurelionThermalFractureComponent* Current = nullptr;
    for (auto* Candidate : Components)
    {
        if (IsValid(Candidate) && Candidate->GetOwner() == Participant && Candidate->IsRegistered()
            && Candidate->IsActive() && Candidate->EncounterDirector == ThermalDirector)
        {
            if (Current) { return nullptr; }
            Current = Candidate;
        }
    }
    return Current;
}
bool ASovAurelionRequestActor::CanUseInternal(const APawn* Pawn, FText& Error, bool bExecutingRequest) const
{
    Error = FText::GetEmpty();
    const auto Fail = [&Error](const FText& Message) { Error = Message; return false; };
    const auto* Player = Cast<ASovPlayerCharacterBase>(Pawn);
    const auto* PC = Player ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    const auto* ASC = Player ? Cast<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent()) : nullptr;
    const auto* State = PC ? PC->GetCampaignState() : nullptr;
    const auto* Mission = State ? State->GetActiveMission() : nullptr;
    const auto* Beat = Mission ? Mission->FindBeat(BeatId) : nullptr;
    if (!HasAuthority() || GetNetMode() != NM_Standalone || bEnding || IsActorBeingDestroyed()
        || !IsCurrentActor(Player, GetWorld()) || !IsCurrentActor(PC, GetWorld()) || !ASC
        || PC->GetPawn() != Player || ASC->GetAvatarActor() != Player || !Player->IsCharacterReady() || !Player->IsAlive()
        || !State || !State->IsStateValid() || State->IsMutationInProgress() || !Mission || Mission->MissionId != MissionId
        || (MissionId != TEXT("M12_FireAndFrost") && MissionId != TEXT("M13_ContraryWitness")) || RequestId.IsNone() || !Beat
        || State->GetActiveProtagonist() != Player->GetProtagonistIdentityTag())
    { return Fail(LOCTEXT("Unavailable", "Interaction unavailable")); }
    if ((!bExecutingRequest && (bPending || bExecuting)) || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle)
    { return Fail(LOCTEXT("Wait", "Please wait for the current action")); }
    const auto Status = State->GetObjectiveState(MissionId, BeatId);
    const bool bTravel = Operation == ESovAurelionRequest::TravelToMission;
    if ((!bTravel && Status != ESovObjectiveState::Available && Status != ESovObjectiveState::Active)
        || (bTravel && (Status != ESovObjectiveState::Succeeded || !State->IsMissionComplete(MissionId)))
        || (Beat->RequiredProtagonist.IsValid() && Beat->RequiredProtagonist != Player->GetProtagonistIdentityTag()))
    { return Fail(LOCTEXT("Objective", "Continue the current objective first")); }
    const auto& NarrativeTags = FNarrativeGameplayTags::Get();
    if (ASC->HasMatchingGameplayTag(NarrativeTags.State_Busy) || ASC->HasMatchingGameplayTag(NarrativeTags.State_SequencerControlled)
        || ASC->HasMatchingGameplayTag(NarrativeTags.State_DialogueControlled) || ASC->HasMatchingGameplayTag(NarrativeTags.State_Movement_Lock))
    { return Fail(LOCTEXT("Busy", "Finish the current action first")); }
    if (!IsValid(Interactable) || !Interactable->IsRegistered() || !Interactable->IsActive() || Interactable->GetOwner() != this
        || Player->GetActorLocation().ContainsNaN() || GetActorLocation().ContainsNaN()
        || !FMath::IsFinite(Interactable->InteractionDistance) || Interactable->InteractionDistance <= 0.f || Interactable->InteractionDistance > 300.f
        || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(Interactable->InteractionDistance))
    { return Fail(LOCTEXT("Range", "Move closer to interact")); }
    FCollisionQueryParams Query = Player->GetIgnoreCharacterParams(); Query.AddIgnoredActor(this); FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Player->GetPawnViewLocation(), GetActorLocation(), ECC_Visibility, Query))
    { return Fail(LOCTEXT("Obstructed", "The interaction is obstructed")); }
    for (TActorIterator<ASovAurelionRequestActor> It(GetWorld()); It; ++It)
    { if (*It != this && !It->IsActorBeingDestroyed() && It->MissionId == MissionId && It->RequestId == RequestId)
        { return Fail(LOCTEXT("Identity", "Interaction identity is ambiguous")); } }
    const int32 TargetCount = int32(Story != nullptr) + int32(HandoffAnchor != nullptr) + int32(CoActionAnchor != nullptr)
        + int32(Thermal != nullptr || ThermalDirector != nullptr || !ThermalParticipantId.IsNone()) + int32(DestinationMission != nullptr)
        + int32(RetryObjective != nullptr || RetryDirector != nullptr);
    if (TargetCount != 1) { return Fail(LOCTEXT("Targets", "Interaction target is not configured")); }
    FString NativeError;
    if (Operation == ESovAurelionRequest::PlayScene)
    {
        if (!IsCurrentActor(Story, GetWorld()) || !Story->CampaignCinematic || !Story->CampaignCinematic->IsRegistered()
            || !Story->CampaignCinematic->IsActive() || Story->CampaignCinematic->MissionId != MissionId || Story->CampaignCinematic->BeatId != BeatId
            || !Beat->bRequiresCinematicProof || Beat->CinematicId.IsNone() || !Story->ValidateStoryContent(NativeError))
        { return Fail(LOCTEXT("Scene", "The scene is unavailable")); }
        const auto Phase = Story->CampaignCinematic->GetPhase();
        if (Phase != ESovCinematicPhase::Idle && Phase != ESovCinematicPhase::Failed && Phase != ESovCinematicPhase::Completed)
        { return Fail(LOCTEXT("SceneActive", "The scene is already in progress")); }
    }
    else if (Operation == ESovAurelionRequest::Handoff)
    {
        FTransform Destination;
        if (!IsCurrentActor(HandoffAnchor, GetWorld()) || HandoffAnchor->MissionId != MissionId || HandoffAnchor->HandoffBeat != BeatId
            || !Beat->HandoffToProtagonist.IsValid() || !HandoffAnchor->ValidateRequest(PC, Destination, NativeError))
        { return Fail(NativeError.IsEmpty() ? LOCTEXT("Handoff", "The perspective change is unavailable") : FText::FromString(NativeError)); }
    }
    else if (Operation == ESovAurelionRequest::CoAction)
    {
        auto* Companion = GetCompanion(PC); auto* Component = Companion ? Companion->GetCompanionComponent() : nullptr;
        if (!IsCurrentActor(CoActionAnchor, GetWorld()) || CoActionAnchor->MissionId != MissionId || CoActionAnchor->CompletionBeat != BeatId
            || !Beat->bRequiresCoActionProof || !IsCurrentActor(Companion, GetWorld()) || !Component
            || !Component->CanRequestCoAction(const_cast<ASovPlayerCharacterBase*>(Player), CoActionAnchor, NativeError))
        { return Fail(NativeError.IsEmpty() ? LOCTEXT("Companion", "The companion action is unavailable") : FText::FromString(NativeError)); }
    }
    else if (IsThermal(Operation))
    {
        auto* CurrentThermal = GetCurrentThermalTarget();
        auto* Companion = GetCompanion(PC); auto* Component = Companion ? Companion->GetCompanionComponent() : nullptr;
        auto* Director = CurrentThermal ? CurrentThermal->EncounterDirector.Get() : nullptr;
        if (!CurrentThermal
            || !IsCurrentActor(Director, GetWorld()) || Director->EncounterId != Beat->RequiredEncounterId
            || Director->GetCampaignProofType() != ESovEncounterProofType::AurelionThermalFracture
            || Director->GetEncounterState() != ESovEncounterState::Active || !IsCurrentActor(Companion, GetWorld()) || !Component
            || !IsCurrentActor(CurrentThermal->FrostAnchor, GetWorld()))
        { return Fail(LOCTEXT("Thermal", "The thermal setup is unavailable")); }
        if (Operation == ESovAurelionRequest::MoveFrostPartner
            && !Component->CanRequestCommand(const_cast<ASovPlayerCharacterBase*>(Player), ESovCompanionCommand::HoldPosition, CurrentThermal->FrostAnchor, NativeError))
        { return Fail(FText::FromString(NativeError)); }
        if (Operation == ESovAurelionRequest::HeatConfirm && CurrentThermal->GetFractureWindowRemainingSeconds() <= 0.f)
        { return Fail(LOCTEXT("FrostFirst", "Prepare frost before confirming heat")); }
    }
    else if (Operation == ESovAurelionRequest::RetryEncounter)
    {
        if (ASC->IsDead() || !FMath::IsFinite(Player->GetHealth()) || Player->GetHealth() <= 0.f
            || MissionId != TEXT("M12_FireAndFrost") || !IsCurrentActor(RetryObjective, GetWorld()) || !IsCurrentActor(RetryDirector, GetWorld())
            || RetryObjective->MissionId != MissionId || RetryObjective->CompletionBeat != BeatId || RetryObjective->EncounterDirector != RetryDirector
            || Beat->RequiredEncounterId.IsNone() || RetryDirector->EncounterId != Beat->RequiredEncounterId
            || RetryDirector->GetCampaignProofType() != Beat->RequiredEncounterProof
            || RetryDirector->GetEncounterState() != ESovEncounterState::Failed || !RetryDirector->HasEncounterPlayer(Player)
            || RetryObjective->IsResultPending() || RetryDirector->IsCampaignReceiptPending())
        { return Fail(LOCTEXT("RetryUnavailable", "No failed encounter is ready to retry here")); }
        // The objective remains the native admission/proof owner. Reject an ambiguous
        // authored binding before presenting a retry or reserving a deferred request.
        for (TActorIterator<ASovCampaignEncounterObjective> It(GetWorld()); It; ++It)
        {
            if (*It != RetryObjective && !It->IsActorBeingDestroyed()
                && ((It->MissionId == MissionId && It->CompletionBeat == BeatId) || It->EncounterDirector == RetryDirector))
            { return Fail(LOCTEXT("RetryObjectiveAmbiguous", "Encounter entry is ambiguous")); }
        }
        for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
        {
            if (*It != RetryDirector && !It->IsActorBeingDestroyed() && It->EncounterId == RetryDirector->EncounterId)
            { return Fail(LOCTEXT("RetryDirectorAmbiguous", "Encounter checkpoint is ambiguous")); }
        }
    }
    else if (bTravel)
    {
        if (!Mission->IsA<USovAurelionFireAndFrostMissionDefinition>() || BeatId != TEXT("ContraryWitnessRecognized")
            || !IsValid(DestinationMission) || !DestinationMission->IsA<USovAurelionContraryWitnessMissionDefinition>()
            || DestinationMission->MissionId != TEXT("M13_ContraryWitness")
            || !Mission->AllowedSuccessorMissions.Contains(DestinationMission->MissionId)
            || Mission->Map.ToSoftObjectPath().GetLongPackageName() == DestinationMission->Map.ToSoftObjectPath().GetLongPackageName()
            || !DestinationMission->ValidateDefinition(NativeError)
            || !FPackageName::DoesPackageExist(DestinationMission->Map.ToSoftObjectPath().GetLongPackageName()))
        { return Fail(LOCTEXT("Travel", "The next Aurelion mission is unavailable")); }
    }
    else { return Fail(LOCTEXT("Invalid", "Interaction is not configured")); }
    return true;
}
bool ASovAurelionRequestActor::RequestUse(APawn* Pawn, FText& Error)
{
    if (!CanUse(Pawn, Error)) { return false; }
    FRequest Request;
    auto* Player = CastChecked<ASovPlayerCharacterBase>(Pawn); auto* PC = CastChecked<ASovPlayerController>(Player->GetController());
    auto* ASC = CastChecked<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent());
    Request.Pawn = Player; Request.Controller = PC; Request.ASC = ASC; Request.Interactable = Interactable;
    Request.Mission = PC->GetCampaignState()->GetActiveMission(); Request.Destination = DestinationMission;
    Request.RequestId = RequestId; Request.MissionId = MissionId; Request.BeatId = BeatId; Request.Operation = Operation;
    Request.ReadyEpoch = ASC->GetCharacterReadyEpoch(); Request.ActorInfoEpoch = ASC->GetCombatActorInfoEpoch(); Request.TransitionEpoch = PC->GetCampaignTransitionEpoch();
    Request.Story = Story; Request.Handoff = HandoffAnchor; Request.CoAction = CoActionAnchor;
    Request.AuthoredThermal = Thermal; Request.AuthoredThermalDirector = ThermalDirector; Request.ThermalParticipantId = ThermalParticipantId;
    Request.Thermal = GetCurrentThermalTarget();
    Request.RetryObjective = RetryObjective; Request.AuthoredRetryDirector = RetryDirector;
    if (Story) { Request.Sequence = Story->CampaignCinematic->Sequence.ToSoftObjectPath(); }
    if (Operation == ESovAurelionRequest::CoAction || IsThermal(Operation)) { Request.Companion = GetCompanion(PC); }
    if (Request.Thermal.IsValid())
    { Request.Director = Request.Thermal->EncounterDirector; Request.AttemptId = Request.Director->GetAttemptId(); Request.EncounterGeneration = Request.Director->GetLifecycleGeneration(); }
    if (Operation == ESovAurelionRequest::RetryEncounter)
    { Request.Director = RetryDirector; Request.AttemptId = RetryDirector->GetAttemptId(); Request.EncounterGeneration = RetryDirector->GetLifecycleGeneration(); }
    bPending = true;
    RequestTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Request]() { ExecuteRequest(Request); }));
    return true;
}
bool ASovAurelionRequestActor::OwnsRequest(const FRequest& Request) const
{
    if (bEnding || IsActorBeingDestroyed() || !Request.Pawn.IsValid() || !Request.Controller.IsValid() || !Request.ASC.IsValid()
        || !IsCurrentActor(Request.Pawn.Get(), GetWorld()) || !IsCurrentActor(Request.Controller.Get(), GetWorld())
        || !Request.Interactable.IsValid() || Request.Interactable.Get() != Interactable || !Interactable->IsRegistered() || !Interactable->IsActive()
        || Interactable->GetOwner() != this || Request.Controller->GetPawn() != Request.Pawn.Get() || Request.Pawn->GetController() != Request.Controller.Get()
        || Request.ASC->GetAvatarActor() != Request.Pawn.Get() || Request.Pawn->GetAbilitySystemComponent() != Request.ASC.Get()
        || !Request.Pawn->IsCharacterReady() || !Request.Pawn->IsAlive() || Request.ASC->GetCharacterReadyEpoch() != Request.ReadyEpoch
        || Request.ASC->GetCombatActorInfoEpoch() != Request.ActorInfoEpoch || Request.Controller->GetCampaignTransitionEpoch() != Request.TransitionEpoch
        || !Request.Mission.IsValid() || !Request.Controller->GetCampaignState() || Request.Controller->GetCampaignState()->GetActiveMission() != Request.Mission.Get()
        || RequestId != Request.RequestId || MissionId != Request.MissionId || BeatId != Request.BeatId || Operation != Request.Operation
        || Story != Request.Story.Get() || HandoffAnchor != Request.Handoff.Get() || CoActionAnchor != Request.CoAction.Get()
        || Thermal != Request.AuthoredThermal.Get() || ThermalDirector != Request.AuthoredThermalDirector.Get()
        || ThermalParticipantId != Request.ThermalParticipantId || DestinationMission != Request.Destination.Get()
        || RetryObjective != Request.RetryObjective.Get() || RetryDirector != Request.AuthoredRetryDirector.Get()) { return false; }
    if (Story && (!IsCurrentActor(Story, GetWorld()) || !Story->CampaignCinematic || Story->CampaignCinematic->Sequence.ToSoftObjectPath() != Request.Sequence)) { return false; }
    if ((Operation == ESovAurelionRequest::CoAction || IsThermal(Operation)) && (!Request.Companion.IsValid() || Request.Companion.Get() != GetCompanion(Request.Controller.Get()))) { return false; }
    if (IsThermal(Operation) && (!Request.Thermal.IsValid() || GetCurrentThermalTarget() != Request.Thermal.Get()
        || !Request.Director.IsValid() || Request.Thermal->EncounterDirector != Request.Director.Get()
        || Request.Director->GetAttemptId() != Request.AttemptId || Request.Director->GetLifecycleGeneration() != Request.EncounterGeneration)) { return false; }
    if (Operation == ESovAurelionRequest::RetryEncounter && (!Request.RetryObjective.IsValid() || !Request.Director.IsValid()
        || !IsCurrentActor(Request.RetryObjective.Get(), GetWorld()) || !IsCurrentActor(Request.Director.Get(), GetWorld())
        || Request.RetryObjective->EncounterDirector != Request.Director.Get() || RetryDirector != Request.Director.Get()
        || Request.RetryObjective->MissionId != Request.MissionId || Request.RetryObjective->CompletionBeat != Request.BeatId
        || Request.Director->GetAttemptId() != Request.AttemptId || Request.Director->GetLifecycleGeneration() != Request.EncounterGeneration)) { return false; }
    return true;
}
void ASovAurelionRequestActor::ExecuteRequest(FRequest Request)
{
    if (!bPending || bEnding) { return; }
    TGuardValue<bool> Execution(bExecuting, true); bPending = false; FText Error;
    if (!OwnsRequest(Request) || !CanUseInternal(Request.Pawn.Get(), Error, true))
    { PublishResult(false, Error.IsEmpty() ? LOCTEXT("Retired", "Interaction changed; try again") : Error); return; }
    FString NativeError; bool bAccepted = false;
    switch (Request.Operation)
    {
    case ESovAurelionRequest::PlayScene: bAccepted = Request.Story->CampaignCinematic->RequestPlay(Request.Controller.Get(), NativeError); break;
    case ESovAurelionRequest::Handoff:
        bAccepted = Request.Controller->RequestAuthoredHandoff(Request.Handoff.Get(), NativeError);
        // The handoff intentionally replaces the ready avatar. Its native owner publishes the result.
        if (bAccepted) { return; } break;
    case ESovAurelionRequest::CoAction: bAccepted = Request.Companion->GetCompanionComponent()->RequestCoAction(Request.Pawn.Get(), Request.CoAction.Get(), NativeError); break;
    case ESovAurelionRequest::MoveFrostPartner: bAccepted = Request.Companion->GetCompanionComponent()->RequestCommand(Request.Pawn.Get(), ESovCompanionCommand::HoldPosition, Request.Thermal->FrostAnchor, NativeError); break;
    case ESovAurelionRequest::FrostSetup: bAccepted = Request.Thermal->RequestFrostSetup(Request.Pawn.Get(), NativeError); break;
    case ESovAurelionRequest::HeatConfirm: bAccepted = Request.Thermal->RequestConfirmFracture(Request.Pawn.Get(), NativeError); break;
    case ESovAurelionRequest::RetryEncounter:
        bAccepted = Request.RetryObjective->StartEncounter(Request.Pawn.Get(), NativeError);
        if (bAccepted)
        {
            // A successful native request advances exactly one restore generation.
            // Its eventual replacement, resources and new attempt remain director-owned.
            FRequest Publication = Request; ++Publication.EncounterGeneration;
            if (OwnsRequest(Publication) && Request.Director->GetEncounterState() == ESovEncounterState::Restoring)
            { PublishResult(true, LOCTEXT("RetryRequested", "Restoring encounter checkpoint...")); }
            return;
        }
        break;
    case ESovAurelionRequest::TravelToMission:
        bAccepted = Request.Controller->TravelToMission(Request.Destination.Get(), NativeError);
        if (bAccepted)
        {
            FRequest Publication = Request; ++Publication.TransitionEpoch;
            if (OwnsRequest(Publication) && Request.Controller->GetCampaignTransitionState() == ESovCampaignTransitionState::Travelling)
            { PublishResult(true, LOCTEXT("Travelling", "Checkpoint saved. Travel requested...")); }
            return;
        }
        break;
    default: NativeError = TEXT("Unknown Aurelion request."); break;
    }
    if (OwnsRequest(Request)) { PublishResult(bAccepted, bAccepted ? LOCTEXT("Accepted", "Request accepted") : FText::FromString(NativeError)); }
}
void ASovAurelionRequestActor::PublishResult(bool bAccepted, const FText& Message)
{
    if (bEnding || IsActorBeingDestroyed()) { return; }
    LastResult = Message;
    UE_LOG(LogTemp, Display, TEXT("Aurelion request %s: %s (%s)"), *RequestId.ToString(), *Message.ToString(), bAccepted ? TEXT("accepted") : TEXT("rejected"));
    OnRequestResult.Broadcast(bAccepted, Message);
}
void ASovAurelionRequestActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); FText Error;
    auto* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (Label)
    {
        Label->SetText(Operation == ESovAurelionRequest::RetryEncounter ? LOCTEXT("RetryEncounter", "Retry encounter") : ActionText);
        Label->SetVisibility(PC && CanUse(PC->GetPawn(), Error));
    }
}
void ASovAurelionRequestActor::EndPlay(EEndPlayReason::Type Reason)
{
    bEnding = true; bPending = false; GetWorldTimerManager().ClearTimer(RequestTimer); Super::EndPlay(Reason);
}
#undef LOCTEXT_NAMESPACE
