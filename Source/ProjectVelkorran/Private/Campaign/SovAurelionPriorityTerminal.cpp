// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionPriorityTerminal.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Interaction/InteractionComponent.h"
#include "NarrativeGameplayTags.h"
#include "Save/SovSaveSubsystem.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "SovAurelionPriority"
namespace
{
    ASovAurelionPrioritySupport* ResolveSupport(UWorld* World)
    {
        if (!World) { return nullptr; }
        ASovAurelionPrioritySupport* Found = nullptr;
        for (TActorIterator<ASovAurelionPrioritySupport> It(World); It; ++It)
        {
            if (It->IsActorBeingDestroyed()) { continue; }
            if (Found || !IsValid(It->WestCacheBarrier) || !IsValid(It->EastFlankBarrier)
                || !It->WestCacheBarrier->IsRegistered() || !It->EastFlankBarrier->IsRegistered()
                || It->WestCacheBarrier->GetOwner() != *It || It->EastFlankBarrier->GetOwner() != *It)
            { return nullptr; }
            Found = *It;
        }
        return Found;
    }
}
bool USovAurelionPriorityInteractable::CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error)
{
    const auto* Terminal = Cast<ASovAurelionPriorityTerminal>(GetOwner());
    return Super::CanInteract_Implementation(Pawn, Interaction, Error) && Terminal && Terminal->CanUse(Pawn, Error);
}
FText USovAurelionPriorityInteractable::GetInteractableActionText_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction) const
{
    const auto* Terminal = Cast<ASovAurelionPriorityTerminal>(GetOwner());
    return Terminal ? Terminal->ActionText() : Super::GetInteractableActionText_Implementation(Pawn, Interaction);
}
bool USovAurelionPriorityInteractable::Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction)
{
    auto* Terminal = Cast<ASovAurelionPriorityTerminal>(GetOwner()); FText Error;
    if (!Terminal || !CanInteract(Pawn, Interaction, Error) || !Terminal->RequestUse(Pawn, Error)) { return false; }
    OnInteract(Pawn, Interaction); if (IsValid(this)) { OnInteracted.Broadcast(Pawn, Interaction); } return true;
}
ASovAurelionPriorityTerminal::ASovAurelionPriorityTerminal()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body")); Body->SetupAttachment(SceneRoot);
    Body->SetBoxExtent(FVector(25.f, 35.f, 60.f)); Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Interactable = CreateDefaultSubobject<USovAurelionPriorityInteractable>(TEXT("Interactable"));
    Interactable->InteractionDistance = 250.f; Interactable->InteractionTime = .35f;
    Interactable->InteractableNameText = LOCTEXT("Panel", "Recovery priority");
}
FText ASovAurelionPriorityTerminal::ActionText() const
{
    if (Priority == ESovAurelionRescuePriority::WestStretchers) { return LOCTEXT("West", "West: stretcher cases first"); }
    if (Priority == ESovAurelionRescuePriority::EastWalkers) { return LOCTEXT("East", "East: walking wounded first"); }
    return LOCTEXT("Unavailable", "Recovery panel unavailable");
}
bool ASovAurelionPriorityTerminal::CanUse(const APawn* Pawn, FText& Error) const { return Validate(Pawn, Error, false); }
bool ASovAurelionPriorityTerminal::Validate(const APawn* Pawn, FText& Error, bool bDuringExecution) const
{
    Error = FText::GetEmpty();
    const auto Reject = [&Error](const FText& Text) { Error = Text; return false; };
    const auto* Player = Cast<ASovPlayerCharacterBase>(Pawn);
    const auto* PC = IsValid(Player) ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    const auto* ASC = IsValid(Player) ? Cast<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent()) : nullptr;
    const auto* State = IsValid(PC) ? PC->GetCampaignState() : nullptr;
    const auto* Mission = State ? State->GetActiveMission() : nullptr;
    const FName Outcome = ASovAurelionPrioritySupport::OutcomeBeat(Priority);
    if (!HasAuthority() || GetNetMode() != NM_Standalone || bEnding || IsActorBeingDestroyed() || !IsValid(Player) || !IsValid(PC)
        || Player->IsActorBeingDestroyed() || PC->IsActorBeingDestroyed() || Player->GetWorld() != GetWorld()
        || PC->GetPawn() != Player || !ASC || ASC->GetAvatarActor() != Player || !Player->IsCharacterReady() || !Player->IsAlive()
        || !State || !State->IsStateValid() || State->IsMutationInProgress() || !Mission || Mission->MissionId != TEXT("M12_FireAndFrost")
        || State->GetActiveProtagonist() != Player->GetProtagonistIdentityTag() || Outcome.IsNone()
        || (!bDuringExecution && (bPending || bExecuting)) || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle)
    { return Reject(LOCTEXT("NotReady", "Finish the current action before selecting priority")); }
    const auto* Beat = Mission->FindBeat(Outcome); FString DefinitionError;
    if (!Mission->ValidateDefinition(DefinitionError) || !Beat || !Beat->bInteractiveChoice || Beat->ChoiceGroupId != TEXT("ImmediateProtection")
        || Beat->RequiredProtagonist != Player->GetProtagonistIdentityTag() || !Mission->FindBeat(TEXT("LocalPriorityCommitted"))
        || !Mission->FindBeat(TEXT("SeverCrucibleLinks")))
    { return Reject(LOCTEXT("Binding", "The recovery priority is not configured")); }
    if (State->IsBeatComplete(Mission->MissionId, TEXT("SeverCrucibleLinks"))
        || State->GetObjectiveState(Mission->MissionId, TEXT("SeverCrucibleLinks")) == ESovObjectiveState::Active)
    { return Reject(LOCTEXT("PastPriority", "The support order is already in effect")); }
    if (!ResolveSupport(GetWorld())) { return Reject(LOCTEXT("SupportBinding", "The recovery support position is missing or ambiguous")); }
    const FName Selected = State->GetSelectedChoice(Mission->MissionId, TEXT("ImmediateProtection"));
    if (!Selected.IsNone() && Selected != Outcome) { return Reject(LOCTEXT("Selected", "The other route already has priority")); }
    const auto Status = State->GetObjectiveState(Mission->MissionId, Outcome);
    if (Status != ESovObjectiveState::Available && Status != ESovObjectiveState::Active && Status != ESovObjectiveState::Succeeded)
    { return Reject(LOCTEXT("Earlier", "Complete the current objective first")); }
    const auto& NarrativeTags = FNarrativeGameplayTags::Get();
    if (ASC->HasMatchingGameplayTag(NarrativeTags.State_Busy) || ASC->HasMatchingGameplayTag(NarrativeTags.State_SequencerControlled)
        || ASC->HasMatchingGameplayTag(NarrativeTags.State_DialogueControlled) || ASC->HasMatchingGameplayTag(NarrativeTags.State_Movement_Lock))
    { return Reject(LOCTEXT("Busy", "Finish the current action first")); }
    if (!IsValid(Interactable) || !Interactable->IsRegistered() || !Interactable->IsActive() || Interactable->GetOwner() != this
        || Player->GetActorLocation().ContainsNaN() || GetActorLocation().ContainsNaN()
        || !FMath::IsFinite(Interactable->InteractionDistance) || Interactable->InteractionDistance <= 0.f || Interactable->InteractionDistance > 400.f
        || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(Interactable->InteractionDistance))
    { return Reject(LOCTEXT("Range", "Move closer to the recovery panel")); }
    FCollisionQueryParams Query = Player->GetIgnoreCharacterParams(); Query.AddIgnoredActor(this); FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Player->GetPawnViewLocation(), GetActorLocation(), ECC_Visibility, Query))
    { return Reject(LOCTEXT("Blocked", "The recovery panel is obstructed")); }
    int32 OtherOptionCount = 0;
    for (TActorIterator<ASovAurelionPriorityTerminal> It(GetWorld()); It; ++It)
    {
        if (*It != this && !It->IsActorBeingDestroyed() && (It->Priority == Priority || It->IsRequestPending()))
        { return Reject(LOCTEXT("Pending", "Another recovery panel is handling this request")); }
        if (*It != this && !It->IsActorBeingDestroyed() && !ASovAurelionPrioritySupport::OutcomeBeat(It->Priority).IsNone()
            && IsValid(It->Interactable) && It->Interactable->IsRegistered() && It->Interactable->IsActive() && It->Interactable->GetOwner() == *It
            && FMath::IsFinite(It->Interactable->InteractionDistance) && It->Interactable->InteractionDistance > 0.f
            && It->Interactable->InteractionDistance <= 400.f
            && It->Priority != Priority && FVector::DistSquared(It->GetActorLocation(), GetActorLocation()) <= FMath::Square(500.f))
        { ++OtherOptionCount; }
    }
    if (OtherOptionCount != 1) { return Reject(LOCTEXT("MissingOption", "Both recovery options must be available at this panel")); }
    return true;
}
bool ASovAurelionPriorityTerminal::RequestUse(APawn* Pawn, FText& Error)
{
    if (!CanUse(Pawn, Error)) { return false; }
    FRequest Request; auto* Player = CastChecked<ASovPlayerCharacterBase>(Pawn);
    auto* PC = CastChecked<ASovPlayerController>(Player->GetController());
    auto* ASC = CastChecked<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent());
    Request.Pawn = Player; Request.Controller = PC; Request.ASC = ASC; Request.State = PC->GetCampaignState();
    Request.Mission = Request.State->GetActiveMission(); Request.Source = Interactable; Request.Priority = Priority;
    Request.Support = ResolveSupport(GetWorld());
    Request.ReadyEpoch = ASC->GetCharacterReadyEpoch(); Request.TransitionEpoch = PC->GetCampaignTransitionEpoch();
    Request.ActorInfoEpoch = ASC->GetCombatActorInfoEpoch();
    if (ObservedState.IsValid()) { ObservedState->OnCampaignStateRestored.RemoveDynamic(this, &ThisClass::HandleStateRestored); }
    ObservedState = Request.State;
    ObservedState->OnCampaignStateRestored.AddUniqueDynamic(this, &ThisClass::HandleStateRestored);
    Request.StateEpoch = StateEpoch;
    Request.JournalSize = Request.State->GetJournal().Num(); bPending = true;
    RequestTimer = GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Request]() { Execute(Request); }));
    return true;
}
bool ASovAurelionPriorityTerminal::OwnsRequest(const FRequest& R) const
{
    return !bEnding && !IsActorBeingDestroyed() && R.Pawn.IsValid() && R.Controller.IsValid() && R.ASC.IsValid() && R.State.IsValid()
        && R.Mission.IsValid() && R.Source.IsValid() && !R.Pawn->IsActorBeingDestroyed() && !R.Controller->IsActorBeingDestroyed()
        && R.Support.IsValid() && R.Support.Get() == ResolveSupport(GetWorld())
        && R.Pawn->GetWorld() == GetWorld() && R.Controller->GetPawn() == R.Pawn.Get() && R.Pawn->GetController() == R.Controller.Get()
        && R.Pawn->GetAbilitySystemComponent() == R.ASC.Get() && R.ASC->GetAvatarActor() == R.Pawn.Get()
        && R.Pawn->IsCharacterReady() && R.Pawn->IsAlive() && R.ASC->GetCharacterReadyEpoch() == R.ReadyEpoch
        && R.ASC->GetCombatActorInfoEpoch() == R.ActorInfoEpoch
        && R.Controller->GetCampaignTransitionEpoch() == R.TransitionEpoch && R.Controller->GetCampaignTransitionState() == ESovCampaignTransitionState::Idle
        && R.Controller->GetCampaignState() == R.State.Get() && R.State->IsStateValid() && R.State->GetActiveMission() == R.Mission.Get()
        && R.StateEpoch == StateEpoch && ObservedState == R.State
        && R.State->GetJournal().Num() == R.JournalSize && Priority == R.Priority && Interactable == R.Source.Get()
        && Interactable->IsActive() && Interactable->IsRegistered();
}
void ASovAurelionPriorityTerminal::Execute(FRequest Request)
{
    if (!bPending || bEnding) { return; }
    bPending = false; TGuardValue<bool> Executing(bExecuting, true); FText Error;
    if (!OwnsRequest(Request) || !Validate(Request.Pawn.Get(), Error, true))
    { LastResult = Error.IsEmpty() ? LOCTEXT("Retired", "Selection changed; try again") : Error; return; }
    auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr; FString SaveError;
    auto* State = Request.State.Get(); const FName Outcome = ASovAurelionPrioritySupport::OutcomeBeat(Request.Priority);
    if (State->GetSelectedChoice(TEXT("M12_FireAndFrost"), TEXT("ImmediateProtection")).IsNone())
    {
        if (!Saves || Saves->EnsureCheckpointBoundary(ESovSaveBoundary::ExplicitCheckpoint, TEXT("Aurelion.CP4b"), SaveError) != ESovSaveResult::Success)
        { LastResult = SaveError.IsEmpty() ? LOCTEXT("BeforeSave", "Save the recovery checkpoint before selecting priority") : FText::FromString(SaveError); return; }
        if (!OwnsRequest(Request) || !Validate(Request.Pawn.Get(), Error, true)) { LastResult = LOCTEXT("Changed", "Selection context changed; try again"); return; }
        const auto Result = State->ResolveChoice(TEXT("ImmediateProtection"), Outcome);
        if (Result != ESovCampaignResult::Applied) { LastResult = LOCTEXT("Rejected", "The support order could not be acknowledged"); return; }
        ++Request.JournalSize;
    }
    if (!OwnsRequest(Request)) { return; }
    if (!State->IsBeatComplete(TEXT("M12_FireAndFrost"), TEXT("LocalPriorityCommitted")))
    {
        if (State->CompleteBeat(TEXT("LocalPriorityCommitted")) != ESovCampaignResult::Applied)
        { LastResult = LOCTEXT("Rejoin", "Priority recorded; acknowledgment is pending"); return; }
        ++Request.JournalSize;
    }
    if (!OwnsRequest(Request)) { return; }
    for (TActorIterator<ASovAurelionPrioritySupport> It(GetWorld()); It; ++It) { It->RefreshFromCampaign(); if (!OwnsRequest(Request)) { return; } }
    const bool bSaved = Saves && Saves->EnsureCheckpointBoundary(ESovSaveBoundary::ExplicitCheckpoint, TEXT("Aurelion.CP5"), SaveError) == ESovSaveResult::Success;
    if (!OwnsRequest(Request)) { return; }
    LastResult = bSaved ? LOCTEXT("Acknowledged", "Priority acknowledged. Checkpoint saved.")
        : (SaveError.IsEmpty() ? LOCTEXT("Retry", "Priority recorded. Resolve the save failure before continuing.") : FText::FromString(SaveError));
}
void ASovAurelionPriorityTerminal::HandleStateRestored(bool bValid)
{ ++StateEpoch; bPending = false; GetWorldTimerManager().ClearTimer(RequestTimer); }
void ASovAurelionPriorityTerminal::EndPlay(EEndPlayReason::Type Reason)
{
    bEnding = true; bPending = false; GetWorldTimerManager().ClearTimer(RequestTimer);
    if (ObservedState.IsValid()) { ObservedState->OnCampaignStateRestored.RemoveDynamic(this, &ThisClass::HandleStateRestored); }
    Super::EndPlay(Reason);
}
#undef LOCTEXT_NAMESPACE
