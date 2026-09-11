// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignRelayReceiver.h"
#include "Campaign/SovCampaignEncounterObjective.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Interaction/InteractionComponent.h"
#include "NarrativeGameplayTags.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "SovRelayReceiver"

bool USovCampaignRelayInteractable::CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error)
{
    const auto* Receiver = Cast<ASovCampaignRelayReceiver>(GetOwner());
    return Super::CanInteract_Implementation(Pawn, Interaction, Error) && Receiver && Receiver->CanUse(Pawn, Error);
}
FText USovCampaignRelayInteractable::GetInteractableActionText_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction) const
{
    const auto* Receiver = Cast<ASovCampaignRelayReceiver>(GetOwner()); FText Error;
    if (Receiver && Receiver->IsDisabled()) { return LOCTEXT("Disabled", "Receiver disabled"); }
    if (Receiver && !Receiver->CanUse(Pawn, Error)) { return Error; }
    return LOCTEXT("Disable", "Disable relay receiver");
}
bool USovCampaignRelayInteractable::Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction)
{
    auto* Receiver = Cast<ASovCampaignRelayReceiver>(GetOwner()); FText Error;
    if (!Receiver || !CanInteract(Pawn, Interaction, Error) || !Receiver->RequestUse(Pawn, Error)) { return false; }
    // Native receipt is deferred until all ordinary interaction callbacks have returned.
    OnInteract(Pawn, Interaction);
    if (IsValid(this)) { OnInteracted.Broadcast(Pawn, Interaction); }
    return true;
}

ASovCampaignRelayReceiver::ASovCampaignRelayReceiver()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .25f;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body")); Body->SetupAttachment(SceneRoot);
    Body->SetBoxExtent(FVector(30, 45, 65)); Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Visual->SetupAttachment(Body);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label")); Label->SetupAttachment(SceneRoot);
    Label->SetRelativeLocation(FVector(0, 0, 95)); Label->SetWorldSize(18.f); Label->SetHorizontalAlignment(EHTA_Center);
    Interactable = CreateDefaultSubobject<USovCampaignRelayInteractable>(TEXT("Interactable"));
    Interactable->InteractionDistance = 250.f; Interactable->InteractionTime = .5f;
    Interactable->InteractableNameText = LOCTEXT("Name", "Relay receiver");
}
void ASovCampaignRelayReceiver::BeginPlay()
{
    Super::BeginPlay(); BindCampaignState(); RefreshPresentation();
}
void ASovCampaignRelayReceiver::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // These two small receiver actors sample presentation/readiness only. This also repairs
    // binding after a controller replacement without introducing a second save owner.
    BindCampaignState(); RefreshPresentation();
}
void ASovCampaignRelayReceiver::BindCampaignState()
{
    const auto* PC = GetWorld() ? Cast<ASovPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
    auto* State = PC ? PC->GetCampaignState() : nullptr;
    if (BoundCampaign.Get() == State) { return; }
    RetireRequest();
    if (BoundCampaign.IsValid())
    {
        BoundCampaign->OnCampaignStateRestored.RemoveDynamic(this, &ThisClass::HandleCampaignRestored);
        BoundCampaign->OnMissionChanged.RemoveDynamic(this, &ThisClass::HandleMissionChanged);
        BoundCampaign->OnBeatCommitted.RemoveDynamic(this, &ThisClass::HandleBeatCommitted);
    }
    BoundCampaign = State;
    if (State)
    {
        State->OnCampaignStateRestored.AddUniqueDynamic(this, &ThisClass::HandleCampaignRestored);
        State->OnMissionChanged.AddUniqueDynamic(this, &ThisClass::HandleMissionChanged);
        State->OnBeatCommitted.AddUniqueDynamic(this, &ThisClass::HandleBeatCommitted);
    }
}
bool ASovCampaignRelayReceiver::IsDisabled() const
{
    if (ReceiverId.IsNone() || !IsValid(EncounterObjective)) { return false; }
    if (EncounterObjective->HasReceiverDisabled(this)) { return true; }
    const auto* PC = GetWorld() ? Cast<ASovPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
    const auto* State = PC ? PC->GetCampaignState() : nullptr;
    if (!State || !State->IsStateValid()) { return false; }
    const auto* Director = EncounterObjective->EncounterDirector.Get();
    return IsValid(Director) && State->GetJournal().ContainsByPredicate([this, Director](const auto& Entry)
    {
        return Entry.MissionId == EncounterObjective->MissionId && Entry.BeatId == EncounterObjective->CompletionBeat
            && Entry.EncounterId == Director->EncounterId && Entry.EncounterAttemptId.IsValid()
            && Entry.DisabledReceiverIds.Contains(ReceiverId);
    });
}
bool ASovCampaignRelayReceiver::CanUse(const APawn* Pawn, FText& Error) const
{ return CanUseInternal(Pawn, Error, false); }
bool ASovCampaignRelayReceiver::CanUseInternal(const APawn* Pawn, FText& Error, bool bExecutingRequest) const
{
    Error = FText::GetEmpty();
    const auto Fail = [&Error](const FText& Message) { Error = Message; return false; };
    const auto* Player = Cast<ASovPlayerCharacterBase>(Pawn);
    if (!HasAuthority() || GetNetMode() != NM_Standalone || bEnding || IsActorBeingDestroyed()
        || !IsValid(Player) || !IsValid(EncounterObjective) || IsDisabled())
    { return Fail(LOCTEXT("Unavailable", "Receiver unavailable")); }
    if (!bExecutingRequest && (bPending || bExecuting)) { return Fail(LOCTEXT("Pending", "Receiver operation pending")); }
    FString ContextError;
    if (!EncounterObjective->CanDisableReceiver(this, Player, ContextError))
    { return Fail(FText::FromString(ContextError)); }
    const auto* ASC = Cast<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent());
    const auto& NarrativeTags = FNarrativeGameplayTags::Get();
    if (!ASC || ASC->HasMatchingGameplayTag(NarrativeTags.State_Busy) || ASC->HasMatchingGameplayTag(NarrativeTags.State_SequencerControlled)
        || ASC->HasMatchingGameplayTag(NarrativeTags.State_DialogueControlled) || ASC->HasMatchingGameplayTag(NarrativeTags.State_Movement_Lock))
    { return Fail(LOCTEXT("Busy", "Finish the current action first")); }
    if (!IsValid(Interactable) || !Interactable->IsRegistered() || !Interactable->IsActive() || Interactable->GetOwner() != this
        || !FMath::IsFinite(Interactable->InteractionDistance) || Interactable->InteractionDistance <= 0.f
        || Interactable->InteractionDistance > 300.f || Player->GetActorLocation().ContainsNaN() || Player->GetPawnViewLocation().ContainsNaN() || GetActorLocation().ContainsNaN()
        || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(Interactable->InteractionDistance))
    { return Fail(LOCTEXT("Range", "Move within reach of the receiver")); }
    FCollisionQueryParams Query = Player->GetIgnoreCharacterParams(); Query.AddIgnoredActor(this); FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Player->GetPawnViewLocation(), GetActorLocation(), ECC_Visibility, Query))
    { return Fail(LOCTEXT("Obstructed", "Receiver access is obstructed")); }
    return true;
}
bool ASovCampaignRelayReceiver::RequestUse(APawn* Pawn, FText& Error)
{
    BindCampaignState();
    if (!CanUse(Pawn, Error)) { LastError = Error; return false; }
    FDisableRequest Request; Request.Player = CastChecked<ASovPlayerCharacterBase>(Pawn);
    Request.Objective = EncounterObjective; Request.Interactable = Interactable; Request.ReceiverId = ReceiverId;
    Request.AttemptId = EncounterObjective->GetReceiverAttemptId(); ActiveRequest = Request;
    bPending = true; LastError = FText::GetEmpty();
    RequestTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
        [this, Request]() { ExecuteRequest(Request); }));
    return true;
}
bool ASovCampaignRelayReceiver::OwnsRequest(const FDisableRequest& Request) const
{
    return !bEnding && !IsActorBeingDestroyed() && Request.Player.IsValid() && Request.Objective.IsValid()
        && Request.Interactable.IsValid() && Request.Objective.Get() == EncounterObjective && Request.Interactable.Get() == Interactable
        && ReceiverId == Request.ReceiverId && Request.AttemptId.IsValid()
        && Request.AttemptId == EncounterObjective->GetReceiverAttemptId()
        && ActiveRequest.AttemptId == Request.AttemptId && ActiveRequest.Player == Request.Player
        && ActiveRequest.ReceiverId == Request.ReceiverId;
}
bool ASovCampaignRelayReceiver::HasPhysicalDisableReceipt(const ASovCampaignEncounterObjective* Objective, const FGuid& AttemptId) const
{
    FText Error;
    return bExecuting && !bPending && Objective == EncounterObjective && AttemptId == ActiveRequest.AttemptId
        && OwnsRequest(ActiveRequest) && CanUseInternal(ActiveRequest.Player.Get(), Error, true);
}
void ASovCampaignRelayReceiver::ExecuteRequest(FDisableRequest Request)
{
    if (!bPending || bEnding || ActiveRequest.AttemptId != Request.AttemptId) { return; }
    bPending = false; TGuardValue<bool> Execution(bExecuting, true); FText Error;
    if (!OwnsRequest(Request) || !CanUseInternal(Request.Player.Get(), Error, true)
        || !EncounterObjective->AcceptReceiverDisable(this, Request.AttemptId))
    { LastError = Error.IsEmpty() ? LOCTEXT("Retired", "Receiver operation retired; retry from the current encounter") : Error; return; }
    LastError = FText::GetEmpty(); RefreshPresentation();
}
void ASovCampaignRelayReceiver::RefreshPresentation()
{
    if (bEnding || IsActorBeingDestroyed()) { return; }
    const bool bDisabled = IsDisabled();
    if (bHasPresentedState && bDisabled == bPresentedDisabled) { return; }
    bHasPresentedState = true;
    if (Label) { Label->SetText(bDisabled ? LOCTEXT("Disabled", "Receiver disabled") : LOCTEXT("Name", "Relay receiver")); }
    if (Visual) { Visual->SetCustomPrimitiveDataFloat(0, bDisabled ? 1.f : 0.f); }
    if (bDisabled != bPresentedDisabled)
    { bPresentedDisabled = bDisabled; OnDisabledChanged.Broadcast(bDisabled); }
}
void ASovCampaignRelayReceiver::RetireRequest()
{
    if (GetWorld()) { GetWorldTimerManager().ClearTimer(RequestTimer); }
    bPending = false; ActiveRequest = {};
}
void ASovCampaignRelayReceiver::HandleCampaignRestored(bool bValid) { RetireRequest(); RefreshPresentation(); }
void ASovCampaignRelayReceiver::HandleMissionChanged(FName MissionId, bool bSucceeded) { RetireRequest(); RefreshPresentation(); }
void ASovCampaignRelayReceiver::HandleBeatCommitted(const FSovCampaignJournalEntry& Entry) { RefreshPresentation(); }
void ASovCampaignRelayReceiver::EndPlay(EEndPlayReason::Type Reason)
{
    bEnding = true; RetireRequest();
    if (BoundCampaign.IsValid())
    {
        BoundCampaign->OnCampaignStateRestored.RemoveDynamic(this, &ThisClass::HandleCampaignRestored);
        BoundCampaign->OnMissionChanged.RemoveDynamic(this, &ThisClass::HandleMissionChanged);
        BoundCampaign->OnBeatCommitted.RemoveDynamic(this, &ThisClass::HandleBeatCommitted);
    }
    Super::EndPlay(Reason);
}
#undef LOCTEXT_NAMESPACE
