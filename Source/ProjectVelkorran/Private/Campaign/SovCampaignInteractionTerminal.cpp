// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignInteractionTerminal.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Interaction/InteractionComponent.h"
#include "Misc/PackageName.h"
#include "NarrativeGameplayTags.h"
#include "Save/SovSaveSubsystem.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "SovCampaignTerminal"

bool USovCampaignTerminalInteractable::CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error)
{
    const auto* Terminal = Cast<ASovCampaignInteractionTerminal>(GetOwner());
    return Super::CanInteract_Implementation(Pawn, Interaction, Error) && Terminal && Terminal->CanUse(Pawn, Error);
}
FText USovCampaignTerminalInteractable::GetInteractableActionText_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction) const
{
    const auto* Terminal = Cast<ASovCampaignInteractionTerminal>(GetOwner()); FText Error;
    if (Terminal && !Terminal->CanUse(Pawn, Error)) { return Error; }
    return Terminal ? Terminal->ActionText : Super::GetInteractableActionText_Implementation(Pawn, Interaction);
}
bool USovCampaignTerminalInteractable::Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction)
{
    auto* Terminal = Cast<ASovCampaignInteractionTerminal>(GetOwner()); FText Error;
    if (!Terminal || !CanInteract(Pawn, Interaction, Error) || !Terminal->RequestUse(Pawn, Error)) { return false; }
    // Reserve first so ordinary authored callbacks cannot recursively queue another commit.
    // The deferred owner check rejects deactivation/replacement by any of these callbacks.
    OnInteract(Pawn, Interaction);
    if (IsValid(this)) { OnInteracted.Broadcast(Pawn, Interaction); }
    return true;
}

ASovCampaignInteractionTerminal::ASovCampaignInteractionTerminal()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body")); Body->SetupAttachment(SceneRoot);
    Body->SetBoxExtent(FVector(35, 55, 65)); Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Visual->SetupAttachment(Body);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(SceneRoot);
    Label->SetRelativeLocation(FVector(0, 0, 100)); Label->SetWorldSize(20.f);
    Label->SetHorizontalAlignment(EHTA_Center);
    Interactable = CreateDefaultSubobject<USovCampaignTerminalInteractable>(TEXT("Interactable"));
    Interactable->InteractionDistance = 300.f; Interactable->InteractionTime = .35f;
    Interactable->InteractableNameText = LOCTEXT("Name", "Terminal");
    ActionText = LOCTEXT("Operate", "Operate terminal");
}

bool ASovCampaignInteractionTerminal::CanUse(const APawn* Pawn, FText& Error) const
{ return CanUseInternal(Pawn, Error, false); }

bool ASovCampaignInteractionTerminal::CanUseInternal(const APawn* Pawn, FText& Error, bool bExecutingRequest) const
{
    Error = FText::GetEmpty();
    const auto Fail = [&Error](const FText& Message) { Error = Message; return false; };
    const auto* Player = Cast<ASovPlayerCharacterBase>(Pawn);
    const auto* PC = Player ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    const auto* ASC = Player ? Cast<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent()) : nullptr;
    const auto* State = PC ? PC->GetCampaignState() : nullptr;
    const auto* Mission = State ? State->GetActiveMission() : nullptr;
    const auto* Beat = Mission ? Mission->FindBeat(CompletionBeat) : nullptr;
    if (!HasAuthority() || GetNetMode() != NM_Standalone || bEnding || IsActorBeingDestroyed() || !IsValid(Player) || !PC || !ASC
        || Player->GetWorld() != GetWorld() || PC->GetPawn() != Player || ASC->GetAvatarActor() != Player
        || !Player->IsCharacterReady() || !Player->IsAlive() || !State || !State->IsStateValid()
        || State->IsMutationInProgress() || !Mission || Mission->MissionId != MissionId || TerminalId.IsNone() || !Beat
        || State->GetActiveProtagonist() != Player->GetProtagonistIdentityTag())
    { return Fail(LOCTEXT("Unavailable", "Terminal unavailable")); }
    if ((!bExecutingRequest && (bPending || bExecuting)) || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle)
    { return Fail(LOCTEXT("Pending", "Please wait for the current operation")); }
    if (!Beat->RequiredEncounterId.IsNone() || Beat->bCanonGate || Beat->bRequiresCinematicProof || !Beat->CinematicId.IsNone() || Beat->bRequiresCoActionProof
        || Beat->HandoffToProtagonist.IsValid() || Beat->bInteractiveChoice || !Beat->ChoiceGroupId.IsNone())
    { return Fail(LOCTEXT("SpecialProof", "This objective needs its authored event")); }
    if (Beat->RequiredProtagonist.IsValid() && Beat->RequiredProtagonist != Player->GetProtagonistIdentityTag())
    { return Fail(LOCTEXT("Lead", "This terminal belongs to the other route")); }
    if (Beat->StateWrites.ContainsByPredicate([](const FSovCampaignStateWrite& Write) { return Write.bCanonProtected; }))
    { return Fail(LOCTEXT("Protected", "This objective needs its authored canon event")); }
    const auto Status = State->GetObjectiveState(MissionId, CompletionBeat);
    if (Status != ESovObjectiveState::Available && Status != ESovObjectiveState::Active
        && !(Status == ESovObjectiveState::Succeeded && (DestinationMission || (bWriteCheckpoint && !bBoundarySucceeded))))
    { return Fail(LOCTEXT("NotReady", "Complete the current objective first")); }
    const auto& NarrativeTags = FNarrativeGameplayTags::Get();
    if (ASC->HasMatchingGameplayTag(NarrativeTags.State_Busy) || ASC->HasMatchingGameplayTag(NarrativeTags.State_SequencerControlled)
        || ASC->HasMatchingGameplayTag(NarrativeTags.State_DialogueControlled) || ASC->HasMatchingGameplayTag(NarrativeTags.State_Movement_Lock))
    { return Fail(LOCTEXT("Busy", "Finish the current action first")); }
    if (!IsValid(Interactable) || !Interactable->IsRegistered() || !Interactable->IsActive() || Interactable->GetOwner() != this
        || Player->GetActorLocation().ContainsNaN() || GetActorLocation().ContainsNaN()
        || !FMath::IsFinite(Interactable->InteractionDistance) || Interactable->InteractionDistance <= 0.f
        || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(Interactable->InteractionDistance))
    { return Fail(LOCTEXT("Range", "Move closer to the terminal")); }
    FCollisionQueryParams Query = Player->GetIgnoreCharacterParams(); Query.AddIgnoredActor(this);
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Player->GetPawnViewLocation(), GetActorLocation(), ECC_Visibility, Query))
    { return Fail(LOCTEXT("Occluded", "The terminal is obstructed")); }
    for (TActorIterator<ASovCampaignInteractionTerminal> It(GetWorld()); It; ++It)
    { if (*It != this && !It->IsActorBeingDestroyed() && It->TerminalId == TerminalId && It->MissionId == MissionId)
        { return Fail(LOCTEXT("Duplicate", "Terminal identity is ambiguous")); } }
    if (DestinationMission)
    {
        FString Validation;
        if (DestinationMission == Mission || !Mission->AllowedSuccessorMissions.Contains(DestinationMission->MissionId)
            || !DestinationMission->ValidateDefinition(Validation)
            || !FPackageName::DoesPackageExist(DestinationMission->Map.ToSoftObjectPath().GetLongPackageName()))
        { return Fail(LOCTEXT("Destination", "The next route is unavailable")); }
    }
    return true;
}

bool ASovCampaignInteractionTerminal::RequestUse(APawn* Pawn, FText& Error)
{
    if (!CanUse(Pawn, Error)) { return false; }
    FRequest Request;
    auto* Player = CastChecked<ASovPlayerCharacterBase>(Pawn);
    auto* PC = CastChecked<ASovPlayerController>(Player->GetController());
    auto* ASC = CastChecked<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent());
    Request.Pawn = Player; Request.Controller = PC; Request.ASC = ASC; Request.Mission = PC->GetCampaignState()->GetActiveMission();
    Request.Destination = DestinationMission; Request.TerminalId = TerminalId; Request.MissionId = MissionId;
    Request.BeatId = CompletionBeat; Request.ReadyEpoch = ASC->GetCharacterReadyEpoch();
    Request.TransitionEpoch = PC->GetCampaignTransitionEpoch(); Request.bCheckpoint = bWriteCheckpoint;
    Request.Interactable = Interactable;
    bPending = true;
    // Narrative consumes the hold and finishes its callbacks before mission/save/travel publication.
    RequestTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
        [this, Request]() { ExecuteRequest(Request); }));
    return true;
}

bool ASovCampaignInteractionTerminal::OwnsRequest(const FRequest& Request) const
{
    return !bEnding && !IsActorBeingDestroyed() && Request.Pawn.IsValid() && Request.Controller.IsValid() && Request.ASC.IsValid()
        && !Request.Controller->IsActorBeingDestroyed() && !Request.Pawn->IsActorBeingDestroyed()
        && Request.Interactable.IsValid() && Request.Interactable.Get() == Interactable
        && Interactable->IsRegistered() && Interactable->IsActive() && Interactable->GetOwner() == this
        && Request.Mission.IsValid() && !Request.Destination.IsStale() && Request.Pawn->GetWorld() == GetWorld()
        && Request.Controller->GetPawn() == Request.Pawn.Get() && Request.Pawn->GetController() == Request.Controller.Get()
        && Request.ASC->GetAvatarActor() == Request.Pawn.Get() && Request.Pawn->GetAbilitySystemComponent() == Request.ASC.Get()
        && Request.Pawn->IsCharacterReady() && Request.Pawn->IsAlive() && Request.ASC->GetCharacterReadyEpoch() == Request.ReadyEpoch
        && Request.Controller->GetCampaignTransitionEpoch() == Request.TransitionEpoch
        && Request.Controller->GetCampaignState()->GetActiveMission() == Request.Mission.Get()
        && TerminalId == Request.TerminalId && MissionId == Request.MissionId && CompletionBeat == Request.BeatId
        && DestinationMission == Request.Destination.Get() && bWriteCheckpoint == Request.bCheckpoint;
}

void ASovCampaignInteractionTerminal::ExecuteRequest(FRequest Request)
{
    if (!bPending || bEnding) { return; }
    TGuardValue<bool> Execution(bExecuting, true); bPending = false; FText Error;
    if (!OwnsRequest(Request) || !CanUseInternal(Request.Pawn.Get(), Error, true))
    { PublishResult(false, Error.IsEmpty() ? LOCTEXT("Retired", "Interaction changed; try again") : Error); return; }
    auto* State = Request.Controller->GetCampaignState();
    if (!State->IsBeatComplete(Request.MissionId, Request.BeatId))
    {
        const ESovCampaignResult Result = State->CompleteBeat(Request.BeatId);
        if (Result != ESovCampaignResult::Applied && Result != ESovCampaignResult::AlreadyApplied)
        { PublishResult(false, LOCTEXT("Rejected", "The objective could not be completed")); return; }
    }
    if (!OwnsRequest(Request))
    { PublishResult(false, LOCTEXT("Changed", "Objective recorded; the interaction context changed")); return; }
    if ((Request.bCheckpoint || Request.Destination.IsValid()) && !CanUseInternal(Request.Pawn.Get(), Error, true))
    { PublishResult(false, Error); return; }
    FString OperationError;
    if (Request.Destination.IsValid())
    {
        // TravelToMission checks completion, writes/readback-verifies the origin, then owns recovery.
        const bool bStarted = Request.Controller->TravelToMission(Request.Destination.Get(), OperationError);
        FRequest TravelPublication = Request;
        TravelPublication.TransitionEpoch = Request.TransitionEpoch + 1;
        const bool bOwnsPublication = bStarted
            ? OwnsRequest(TravelPublication) && Request.Controller->GetCampaignTransitionState() == ESovCampaignTransitionState::Travelling
            : OwnsRequest(Request);
        if (bOwnsPublication)
        { PublishResult(bStarted, bStarted ? LOCTEXT("Travelling", "Checkpoint saved. Travel requested...") : FText::FromString(OperationError)); }
        return;
    }
    if (Request.bCheckpoint)
    {
        auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr;
        const bool bSaved = Saves && Saves->WriteCheckpoint(ESovSaveBoundary::ExplicitCheckpoint, Request.TerminalId, OperationError) == ESovSaveResult::Success;
        if (!OwnsRequest(Request)) { return; }
        if (!bSaved)
        { PublishResult(false, OperationError.IsEmpty() ? LOCTEXT("SaveUnavailable", "Objective recorded. Checkpoint unavailable; try again") : FText::FromString(OperationError)); return; }
        bBoundarySucceeded = true;
    }
    PublishResult(true, Request.bCheckpoint ? LOCTEXT("Saved", "Objective complete. Checkpoint saved") : LOCTEXT("Complete", "Objective complete"));
}

void ASovCampaignInteractionTerminal::PublishResult(bool bSucceeded, const FText& Message)
{
    if (bEnding || IsActorBeingDestroyed()) { return; }
    LastResult = Message;
    if (Label) { Label->SetText(Message); }
    UE_LOG(LogTemp, Display, TEXT("Campaign terminal %s: %s (%s)"), *TerminalId.ToString(), *Message.ToString(), bSucceeded ? TEXT("accepted") : TEXT("not completed"));
    OnTerminalResult.Broadcast(bSucceeded, Message);
}

void ASovCampaignInteractionTerminal::EndPlay(EEndPlayReason::Type Reason)
{
    bEnding = true; bPending = false; GetWorldTimerManager().ClearTimer(RequestTimer);
    Super::EndPlay(Reason);
}
#undef LOCTEXT_NAMESPACE
