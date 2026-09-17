// Copyright Fallen Signal Studios. All Rights Reserved.
#include "World/SovTraversalAnchor.h"
#include "World/SovWorldMotionPolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Exertion/SovGameplayAbility_Exertion.h"
#include "Framework/SovPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Recovery/SovFatalRecoveryComponent.h"
#include "Save/SovSaveSubsystem.h"
#include "Sovereign/SovGameplayTags.h"

bool USovTraversalInteractable::CanInteract_Implementation(APawn* Player, UNarrativeInteractionComponent* Interaction, FText& Error)
{
    const auto* Anchor = Cast<ASovTraversalAnchor>(GetOwner());
    return Super::CanInteract_Implementation(Player, Interaction, Error) && Anchor && Anchor->CanTraverse(Player, Error);
}
bool USovTraversalInteractable::Interact(APawn* Player, UNarrativeInteractionComponent* Interaction)
{ auto* Anchor = Cast<ASovTraversalAnchor>(GetOwner()); FText Error; return Anchor && CanInteract(Player, Interaction, Error) && Anchor->RequestTraverse(Player, Error); }

ASovTraversalAnchor::ASovTraversalAnchor()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
    EntryBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryBounds")); EntryBounds->SetupAttachment(SceneRoot);
    EntryBounds->SetBoxExtent(FVector(150,150,140)); EntryBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Interactable = CreateDefaultSubobject<USovTraversalInteractable>(TEXT("Interactable"));
    Interactable->InteractionDistance = 300.f; Interactable->InteractionTime = .15f;
    ActionText = NSLOCTEXT("SovTraversal", "Traverse", "Traverse");
}
void ASovTraversalAnchor::BeginPlay()
{ Super::BeginPlay(); Interactable->SetInteractableActionText(ActionText); }
bool ASovTraversalAnchor::SweepClear(const ASovPlayerCharacterBase* Player, FVector From, FVector To) const
{
    if (!Player || !Player->GetCapsuleComponent() || From.ContainsNaN() || To.ContainsNaN()) { return false; }
    const auto* Capsule = Player->GetCapsuleComponent();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SovTraversalCapsule), false, Player); Query.AddIgnoredActor(this);
    FHitResult Hit;
    return !GetWorld()->SweepSingleByChannel(Hit, From, To, Player->GetActorQuat(), Capsule->GetCollisionObjectType(),
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query);
}
bool ASovTraversalAnchor::BuildPath(const ASovPlayerCharacterBase* Player, TArray<FVector>& Out, FText& Error) const
{
    Out.Reset();
    if (!Player || PathPoints.Num() < 2 || PathPoints.Num() > 32 || !SovWorldMotionPolicy::ValidDuration(DurationSeconds))
    { Error = NSLOCTEXT("SovTraversal", "Configuration", "Traversal route is unavailable."); return false; }
    Out.Add(Player->GetActorLocation());
    float Length = 0.f;
    for (const FVector Local : PathPoints)
    {
        const FVector Point = GetActorTransform().TransformPosition(Local);
        const float Distance = FVector::Distance(Out.Last(), Point);
        if (Local.ContainsNaN() || !FMath::IsFinite(Distance) || Distance > 1500.f || (Length += Distance) > 5000.f
            || !SweepClear(Player, Out.Last(), Point))
        { Error = NSLOCTEXT("SovTraversal", "Blocked", "Traversal route is obstructed."); Out.Reset(); return false; }
        if (Distance > .1f) { Out.Add(Point); }
    }
    if (Out.Num() < 2 || !USovFatalRecoveryComponent::IsSafeRecoveryPosition(Player, Out.Last()))
    { Error = NSLOCTEXT("SovTraversal", "Exit", "Traversal exit is not safe."); Out.Reset(); return false; }
    return true;
}
bool ASovTraversalAnchor::CanTraverse(const APawn* Pawn, FText& Error) const
{
    const auto* Player = Cast<ASovPlayerCharacterBase>(Pawn);
    const auto* PC = Player ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    const auto* ASC = Player ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ASovPlayerCharacterBase*>(Player)) : nullptr;
    const auto& N = FNarrativeGameplayTags::Get();
    if (!HasAuthority() || GetNetMode() != NM_Standalone || bActive || bMutating || bEnding || TraversalId.IsNone()
        || !Player || !PC || !ASC || Player->GetWorld() != GetWorld() || !Player->IsCharacterReady()
        || ASC->GetAvatarActor() != Player || !Player->IsAlive() || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle
        || !Player->GetCharacterMovement()->IsMovingOnGround() || ASC->HasMatchingGameplayTag(N.State_Busy)
        || ASC->HasMatchingGameplayTag(N.State_SequencerControlled) || ASC->HasMatchingGameplayTag(N.State_DialogueControlled)
        || ASC->HasMatchingGameplayTag(N.State_Movement_Lock) || ASC->HasMatchingGameplayTag(N.State_Movement_Ragdoll))
    { Error = NSLOCTEXT("SovTraversal", "Unavailable", "Finish the current action first."); return false; }
    const auto* State = PC->GetCampaignState(); const auto* Mission = State ? State->GetActiveMission() : nullptr;
    if (!State || !State->IsStateValid() || State->IsMutationInProgress()
        || (RequiredProtagonist.IsValid() && RequiredProtagonist != Player->GetProtagonistIdentityTag())
        || (!RequiredMission.IsNone() && (!Mission || RequiredMission != Mission->MissionId))
        || (!RequiredBeat.IsNone() && (!Mission || !State->IsBeatComplete(Mission->MissionId, RequiredBeat))))
    { Error = NSLOCTEXT("SovTraversal", "Capability", "Required capability or mission authorization is unavailable."); return false; }
    const FVector Position = EntryBounds->GetComponentTransform().InverseTransformPosition(Player->GetActorLocation());
    const FVector Extent = EntryBounds->GetUnscaledBoxExtent();
    if (Position.ContainsNaN() || FMath::Abs(Position.X)>Extent.X || FMath::Abs(Position.Y)>Extent.Y || FMath::Abs(Position.Z)>Extent.Z)
    { Error = NSLOCTEXT("SovTraversal", "Alignment", "Move into the traversal area."); return false; }
    for (TActorIterator<ASovTraversalAnchor> It(GetWorld()); It; ++It)
    { if (*It != this && It->TraversalId == TraversalId) { Error = NSLOCTEXT("SovTraversal", "Identity", "Traversal route identity is ambiguous."); return false; } }
    TArray<FVector> Path; return BuildPath(Player, Path, Error);
}
bool ASovTraversalAnchor::RequestTraverse(APawn* Pawn, FText& Error)
{
    if (!CanTraverse(Pawn, Error)) { return false; }
    TGuardValue<bool> Mutation(bMutating, true);
    auto* Player = CastChecked<ASovPlayerCharacterBase>(Pawn); auto* PC = CastChecked<ASovPlayerController>(Player->GetController());
    if (bCheckpointTransition)
    {
        auto* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr; FString Why;
        if (!Save || Save->EnsureCheckpointBoundary(ESovSaveBoundary::LongTransition, TraversalId, Why) != ESovSaveResult::Success)
        { Error = FText::FromString(Why.IsEmpty() ? TEXT("Origin checkpoint is unavailable.") : Why); return false; }
    }
    if (!IsValid(Player) || PC->GetPawn() != Player || !BuildPath(Player, WorldPath, Error)) { return false; }
    TraversingPlayer = Player; PlayerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player); Origin = Player->GetActorLocation();
    auto* ASC = PlayerASC.Get(); if (!ASC || ASC->GetAvatarActor() != Player || !Player->IsAlive()) { return false; }
    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(USovGameplayEffect_ExertionWindow::StaticClass(), 1.f, ASC->MakeEffectContext());
    if (!Spec.IsValid()) { return false; }
    Spec.Data->DynamicGrantedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
    Spec.Data->DynamicGrantedTags.AddTag(FSovGameplayTags::Get().State_Traversal);
    Spec.Data->SetDuration(DurationSeconds + 1.f, true);
    const FActiveGameplayEffectHandle AppliedWindow = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    if (!IsValid(this) || !AppliedWindow.IsValid() || !OwnsPlayer())
    {
        if (IsValid(ASC) && AppliedWindow.IsValid()) { ASC->RemoveActiveGameplayEffect(AppliedWindow); }
        Finish(false, NSLOCTEXT("SovTraversal", "Ownership", "Traversal interrupted.")); return false;
    }
    TraversalWindow = AppliedWindow;
    CumulativeDistance.Reset(); CumulativeDistance.Add(0.f);
    for (int32 I=1; I<WorldPath.Num(); ++I) { CumulativeDistance.Add(CumulativeDistance.Last() + FVector::Distance(WorldPath[I-1], WorldPath[I])); }
    auto* Movement = Player->GetCharacterMovement(); PreviousMovementMode = Movement->MovementMode; PreviousCustomMode = Movement->CustomMovementMode;
    bOwnMovementMode = true; Movement->StopMovementImmediately(); Movement->DisableMovement();
    if (!IsValid(this) || !OwnsPlayer()) { Finish(false, FText()); return false; }
    LockedController = PC; PC->SetIgnoreMoveInput(true); bOwnInputLock = true;
    Elapsed = 0.f; Segment = 1; bActive = true;
    return true;
}
bool ASovTraversalAnchor::OwnsPlayer() const
{
    return TraversingPlayer.IsValid() && PlayerASC.IsValid() && PlayerASC->GetAvatarActor() == TraversingPlayer.Get()
        && TraversingPlayer->IsAlive() && TraversingPlayer->GetController()
        && TraversingPlayer->GetController()->GetPawn() == TraversingPlayer.Get();
}
void ASovTraversalAnchor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bActive || bMutating || bEnding || !HasAuthority()) { return; }
    TGuardValue<bool> Mutation(bMutating, true);
    if (!OwnsPlayer() || !PlayerASC->GetActiveGameplayEffect(TraversalWindow)
        || TraversingPlayer->GetCharacterMovement()->MovementMode != MOVE_None)
    { Finish(false, NSLOCTEXT("SovTraversal", "Interrupted", "Traversal interrupted.")); return; }
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return; }
    Elapsed = static_cast<float>(SovWorldMotionPolicy::Advance(Elapsed, DeltaSeconds, DurationSeconds));
    const float Distance = CumulativeDistance.Last() * Elapsed / DurationSeconds;
    // Visit every crossed corner even on a slow frame. Never sweep across the shortcut chord.
    while (Segment < WorldPath.Num())
    {
        const bool bPastCorner = Distance >= CumulativeDistance[Segment];
        const float Alpha = FMath::Clamp((Distance-CumulativeDistance[Segment-1]) /
            (CumulativeDistance[Segment]-CumulativeDistance[Segment-1]), 0.f, 1.f);
        const FVector Target = bPastCorner ? WorldPath[Segment] : FMath::Lerp(WorldPath[Segment-1], WorldPath[Segment], Alpha);
        FHitResult Hit;
        TraversingPlayer->SetActorLocation(Target, true, &Hit);
        if (Hit.bBlockingHit || !OwnsPlayer()) { Finish(false, NSLOCTEXT("SovTraversal", "DynamicBlock", "Traversal path became obstructed.")); return; }
        if (!bPastCorner) { break; }
        ++Segment;
    }
    if (Elapsed >= DurationSeconds)
    { Finish(USovFatalRecoveryComponent::IsSafeRecoveryPosition(TraversingPlayer.Get(), WorldPath.Last()), FText()); }
}
void ASovTraversalAnchor::Finish(bool bSuccess, const FText& Message)
{
    if (bEnding) { return; }
    TGuardValue<bool> Ending(bEnding, true);
    auto* Player = TraversingPlayer.Get();
    if (!bSuccess && Player && OwnsPlayer() && !USovFatalRecoveryComponent::IsSafeRecoveryPosition(Player, Player->GetActorLocation())
        && USovFatalRecoveryComponent::IsSafeRecoveryPosition(Player, Origin))
    { Player->SetActorLocation(Origin, false, nullptr, ETeleportType::TeleportPhysics); }
    if (bOwnMovementMode && Player && OwnsPlayer() && Player->GetCharacterMovement()->MovementMode == MOVE_None)
    { Player->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(PreviousMovementMode), PreviousCustomMode); }
    bOwnMovementMode = false;
    if (PlayerASC.IsValid() && TraversalWindow.IsValid()) { PlayerASC->RemoveActiveGameplayEffect(TraversalWindow); }
    TraversalWindow.Invalidate();
    if (bOwnInputLock && LockedController.IsValid()) { LockedController->SetIgnoreMoveInput(false); }
    bOwnInputLock = false; bActive = false; LockedController.Reset(); TraversingPlayer.Reset(); PlayerASC.Reset();
    WorldPath.Reset(); CumulativeDistance.Reset();
    if (bSuccess && bCheckpointTransition && GetGameInstance())
    { if (auto* Save = GetGameInstance()->GetSubsystem<USovSaveSubsystem>()) { Save->QueueAutosave(ESovSaveBoundary::LongTransition, TraversalId); } }
    OnTraversalCompleted.Broadcast(bSuccess, Message);
}
void ASovTraversalAnchor::EndPlay(EEndPlayReason::Type Reason)
{ Finish(false, NSLOCTEXT("SovTraversal", "Unloaded", "Traversal route unloaded.")); Super::EndPlay(Reason); }
