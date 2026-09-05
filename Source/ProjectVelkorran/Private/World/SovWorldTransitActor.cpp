// Copyright Fallen Signal Studios. All Rights Reserved.
#include "World/SovWorldTransitActor.h"
#include "World/SovWorldMotionPolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Exertion/SovGameplayAbility_Exertion.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "NarrativeGameplayTags.h"
#include "NavLinkCustomComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "Net/UnrealNetwork.h"
#include "Save/SovSaveSubsystem.h"
#include "Sovereign/SovGameplayTags.h"

bool USovWorldTransitInteractable::CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error)
{
    const auto* Transit = Cast<ASovWorldTransitActor>(GetOwner());
    return Super::CanInteract_Implementation(Pawn, Interaction, Error) && Transit && Transit->CanUse(Pawn, Error);
}
FText USovWorldTransitInteractable::GetInteractableActionText_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction) const
{
    FText Error; const auto* Transit = Cast<ASovWorldTransitActor>(GetOwner());
    if (Transit && !Transit->CanUse(Pawn, Error)) { return Error; }
    return Transit && Transit->GetTransitState() == ESovWorldTransitState::AtDestination
        ? FText::FromString(TEXT("Return")) : Super::GetInteractableActionText_Implementation(Pawn, Interaction);
}
bool USovWorldTransitInteractable::Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction)
{ auto* Transit = Cast<ASovWorldTransitActor>(GetOwner()); FText Error; return Transit && CanInteract(Pawn, Interaction, Error) && Transit->RequestUse(Pawn, Error); }

ASovWorldTransitActor::ASovWorldTransitActor()
{
    bReplicates = true; PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
    MovingBody = CreateDefaultSubobject<UBoxComponent>(TEXT("MovingBody")); MovingBody->SetupAttachment(SceneRoot);
    MovingBody->SetBoxExtent(FVector(35,110,130)); MovingBody->SetCollisionProfileName(TEXT("BlockAllDynamic")); MovingBody->SetIsReplicated(true);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Visual->SetupAttachment(MovingBody); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EntryBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryBounds")); EntryBounds->SetupAttachment(SceneRoot);
    EntryBounds->SetBoxExtent(FVector(220,220,180)); EntryBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Interactable = CreateDefaultSubobject<USovWorldTransitInteractable>(TEXT("Interactable"));
    Interactable->InteractionDistance = 350.f; Interactable->InteractionTime = .25f; Interactable->InteractableActionText = FText::FromString(TEXT("Use"));
    OriginLink = CreateDefaultSubobject<UNavLinkCustomComponent>(TEXT("OriginLink"));
    DestinationLink = CreateDefaultSubobject<UNavLinkCustomComponent>(TEXT("DestinationLink"));
    OriginLink->SetDisabledArea(UNavArea_Null::StaticClass()); DestinationLink->SetDisabledArea(UNavArea_Null::StaticClass());
}
void ASovWorldTransitActor::BeginPlay()
{
    Super::BeginPlay();
    OriginLink->SetLinkData(FVector(-160,0,0), FVector(160,0,0), ENavLinkDirection::BothWays);
    const FVector Offset = Kind == ESovWorldTransitKind::Lift ? DestinationOffset : FVector::ZeroVector;
    DestinationLink->SetLinkData(Offset + FVector(-160,0,0), Offset + FVector(160,0,0), ENavLinkDirection::BothWays);
    ReconcileEndpoint();
}
FGuid ASovWorldTransitActor::GetActorGUID_Implementation() const
{
    if (!SaveGuid.IsValid())
    { FGuid Stable; FGuid::ParseExact(FMD5::HashAnsiString(*GetPathName()), EGuidFormats::Digits, Stable); const_cast<ASovWorldTransitActor*>(this)->SaveGuid = Stable; }
    return SaveGuid;
}
bool ASovWorldTransitActor::CanUse(const APawn* Pawn, FText& Error) const
{
    const auto* Player = Cast<ASovPlayerCharacterBase>(Pawn);
    const auto* PC = Player ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    const auto* ASC = Player ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ASovPlayerCharacterBase*>(Player)) : nullptr;
    const auto Reject = [&Error](const TCHAR* Message) { Error = FText::FromString(Message); return false; };
    if (!HasAuthority() || !Player || !PC || !ASC || !Player->IsCharacterReady() || ASC->GetAvatarActor() != Player
        || GetWorld() != Player->GetWorld() || TransitId.IsNone() || bMutating || bEnding) { return Reject(TEXT("Unavailable")); }
    if (State == ESovWorldTransitState::Moving || State == ESovWorldTransitState::WaitingForDestination) { return Reject(TEXT("Transit in progress")); }
    if (State == ESovWorldTransitState::Blocked) { return Reject(TEXT("Transit blocked. Reload the checkpoint to recover.")); }
    if (!FMath::IsFinite(StructuralHealth) || StructuralHealth <= 0.f || State == ESovWorldTransitState::Broken) { return Reject(TEXT("Mechanism damaged")); }
    if (bRequiresPower && !bPowered) { return Reject(TEXT("Power required")); }
    if (!LockReason.IsEmpty()) { Error = LockReason; return false; }
    if (RequiredProtagonist.IsValid() && RequiredProtagonist != Player->GetProtagonistIdentityTag()) { return Reject(TEXT("Requires the other protagonist's capability")); }
    const auto* Campaign = PC->GetCampaignState(); const auto* Mission = Campaign ? Campaign->GetActiveMission() : nullptr;
    if ((!RequiredMission.IsNone() && (!Mission || Mission->MissionId != RequiredMission))
        || (!RequiredBeat.IsNone() && (!Mission || !Campaign->IsBeatComplete(Mission->MissionId, RequiredBeat)))) { return Reject(TEXT("Mission authorization required")); }
    const auto& N = FNarrativeGameplayTags::Get();
    if (ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f || ASC->HasMatchingGameplayTag(N.State_Busy)
        || ASC->HasMatchingGameplayTag(N.State_SequencerControlled) || ASC->HasMatchingGameplayTag(N.State_Movement_Lock)
        || ASC->HasMatchingGameplayTag(N.State_Movement_Ragdoll) || ASC->HasMatchingGameplayTag(N.State_DialogueControlled)
        || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle)
    { return Reject(TEXT("Finish the current action first")); }
    if (!Player->GetCharacterMovement() || !Player->GetCharacterMovement()->IsMovingOnGround())
    { return Reject(TEXT("Stand on stable ground first")); }
    const FVector Local = EntryBounds->GetComponentTransform().InverseTransformPosition(Player->GetActorLocation());
    const FVector Extent = EntryBounds->GetUnscaledBoxExtent();
    if (Local.ContainsNaN() || FMath::Abs(Local.X)>Extent.X || FMath::Abs(Local.Y)>Extent.Y || FMath::Abs(Local.Z)>Extent.Z)
    { return Reject(TEXT("Move into the interaction area")); }
    FHitResult Sight;
    FCollisionQueryParams SightQuery(SCENE_QUERY_STAT(SovTransitInteraction), false, Player); SightQuery.AddIgnoredActor(this);
    if (GetWorld()->LineTraceSingleByChannel(Sight, Player->GetPawnViewLocation(), EntryBounds->GetComponentLocation(), ECC_Visibility, SightQuery))
    { return Reject(TEXT("Interaction is obstructed")); }
    if (Kind == ESovWorldTransitKind::Lift && Player->GetMovementBase() != MovingBody)
    { return Reject(TEXT("Step onto the lift first")); }
    for (TActorIterator<ASovWorldTransitActor> It(GetWorld()); It; ++It)
    { if (*It != this && It->TransitId == TransitId) { return Reject(TEXT("Transit identity is ambiguous")); } }
    if (DestinationOffset.ContainsNaN() || !FMath::IsFinite(TravelSeconds) || TravelSeconds < .1f || TravelSeconds > 30.f
        || !FMath::IsFinite(StreamingTimeoutSeconds) || StreamingTimeoutSeconds < 1.f || StreamingTimeoutSeconds > 60.f)
    { return Reject(TEXT("Transit configuration is invalid")); }
    return true;
}
bool ASovWorldTransitActor::RequestUse(APawn* Pawn, FText& Error)
{
    if (!CanUse(Pawn, Error)) { return false; }
    TGuardValue<bool> Mutation(bMutating, true);
    auto* Player = CastChecked<ASovPlayerCharacterBase>(Pawn); auto* PC = CastChecked<ASovPlayerController>(Player->GetController());
    Streaming = nullptr;
    if (!bAtDestination && !DestinationStreamingLevel.IsNone())
    {
        if (!FPackageName::DoesPackageExist(DestinationStreamingLevel.ToString())
            || !(Streaming = UGameplayStatics::GetStreamingLevel(this, DestinationStreamingLevel)))
        { Error = FText::FromString(TEXT("Destination level is unavailable; origin preserved.")); return false; }
    }
    if (bIrreversibleTransition)
    {
        auto* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr; FString SaveError;
        if (!Save || (!Save->ConsumeAcknowledgedBoundary(ESovSaveBoundary::LongTransition, TransitId)
            && Save->WriteCheckpoint(ESovSaveBoundary::LongTransition, TransitId, SaveError) != ESovSaveResult::Success))
        { Error = FText::FromString(SaveError.IsEmpty() ? TEXT("Checkpoint unavailable") : SaveError); return false; }
    }
    if (!IsValid(this) || !IsValid(Player) || PC->GetPawn() != Player || (bRequiresPower && !bPowered)
        || StructuralHealth <= 0.f || !LockReason.IsEmpty()) { return false; }
    TransitPlayer = Player; PlayerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);
    MoveStart = MovingBody->GetRelativeLocation(); MoveTarget = bAtDestination ? FVector::ZeroVector : DestinationOffset;
    MoveElapsed = 0; StartedAt = GetWorld()->GetTimeSeconds();
    FGameplayEffectSpecHandle Spec = PlayerASC->MakeOutgoingSpec(USovGameplayEffect_ExertionWindow::StaticClass(), 1.f, PlayerASC->MakeEffectContext());
    if (!Spec.IsValid()) { return false; }
    Spec.Data->DynamicGrantedTags.AddTag(FNarrativeGameplayTags::Get().State_Interacting);
    Spec.Data->DynamicGrantedTags.AddTag(FNarrativeGameplayTags::Get().State_Busy);
    Spec.Data->DynamicGrantedTags.AddTag(FSovGameplayTags::Get().State_Traversal);
    Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Duration, StreamingTimeoutSeconds + TravelSeconds + 1.f);
    Spec.Data->SetDuration(StreamingTimeoutSeconds + TravelSeconds + 1.f, true);
    UAbilitySystemComponent* StartingASC = PlayerASC.Get();
    const FActiveGameplayEffectHandle AppliedWindow = StartingASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    if (!IsValid(this) || !AppliedWindow.IsValid() || !OwnsPlayer())
    {
        if (IsValid(StartingASC) && AppliedWindow.IsValid()) { StartingASC->RemoveActiveGameplayEffect(AppliedWindow); }
        Finish(false, FText::FromString(TEXT("Transit ownership changed"))); return false;
    }
    TransitWindow = AppliedWindow;
    LockedController = PC; PC->SetIgnoreMoveInput(true); bOwnInputLock = true;
    if (Streaming && (!Streaming->IsLevelLoaded() || !Streaming->IsLevelVisible()))
    { Streaming->SetShouldBeLoaded(true); Streaming->SetShouldBeVisible(true); State = ESovWorldTransitState::WaitingForDestination; }
    else { State = ESovWorldTransitState::Moving; }
    UpdateLinks(); OnTransitChanged.Broadcast(State, FText()); return true;
}
bool ASovWorldTransitActor::OwnsPlayer() const
{
    return TransitPlayer.IsValid() && PlayerASC.IsValid() && PlayerASC->GetAvatarActor() == TransitPlayer.Get()
        && TransitPlayer->GetController() && TransitPlayer->GetController()->GetPawn() == TransitPlayer.Get()
        && PlayerASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f;
}
void ASovWorldTransitActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || bMutating || (State != ESovWorldTransitState::Moving && State != ESovWorldTransitState::WaitingForDestination)) { return; }
    if (!OwnsPlayer() || !PlayerASC->GetActiveGameplayEffect(TransitWindow)
        || (bRequiresPower && !bPowered) || StructuralHealth <= 0.f || !LockReason.IsEmpty())
    { Finish(false, FText::FromString(TEXT("Transit interrupted; use checkpoint recovery."))); return; }
    if (State == ESovWorldTransitState::WaitingForDestination)
    {
        if (Streaming && Streaming->IsLevelLoaded() && Streaming->IsLevelVisible())
        { State = ESovWorldTransitState::Moving; OnTransitChanged.Broadcast(State, FText()); }
        else if (SovWorldMotionPolicy::TimedOut(GetWorld()->GetTimeSeconds(), StartedAt, StreamingTimeoutSeconds))
        { Finish(false, FText::FromString(TEXT("Destination loading timed out; origin preserved."))); }
        return;
    }
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return; }
    MoveElapsed = static_cast<float>(SovWorldMotionPolicy::Advance(MoveElapsed, DeltaSeconds, TravelSeconds));
    const FVector Desired = FMath::Lerp(MoveStart, MoveTarget, MoveElapsed/TravelSeconds);
    const FVector Delta = SceneRoot->GetComponentTransform().TransformVector(Desired - MovingBody->GetRelativeLocation());
    TArray<ACharacter*> Riders;
    if (Kind == ESovWorldTransitKind::Lift)
    {
        for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
        {
            if (It->GetMovementBase() != MovingBody) { continue; }
            const UCapsuleComponent* Capsule = It->GetCapsuleComponent();
            FCollisionQueryParams Query(SCENE_QUERY_STAT(SovLiftPassenger), false, *It); Query.AddIgnoredActor(this);
            FHitResult Clearance;
            if (GetWorld()->SweepSingleByChannel(Clearance, It->GetActorLocation(), It->GetActorLocation()+Delta, FQuat::Identity,
                Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(FMath::Max(1.f,Capsule->GetScaledCapsuleRadius()-2.f),
                    FMath::Max(1.f,Capsule->GetScaledCapsuleHalfHeight()-2.f)), Query))
            { Finish(false, FText::FromString(TEXT("Lift passenger clearance blocked; no crushing movement applied."))); return; }
            Riders.Add(*It);
            if (auto* Movement = It->GetCharacterMovement(); Movement && !RiderMovements.ContainsByPredicate([Movement](const auto& Existing) { return Existing.Get() == Movement; }))
            { Movement->AddTickPrerequisiteActor(this); RiderMovements.Add(Movement); }
        }
    }
    FHitResult Hit;
    FCollisionQueryParams BodyQuery(SCENE_QUERY_STAT(SovTransitBody), false, this);
    for (ACharacter* Rider : Riders) { BodyQuery.AddIgnoredActor(Rider); }
    // Sweep the actual moving primitive explicitly. A scene root/visual child is not a collision guarantee.
    if (GetWorld()->SweepSingleByChannel(Hit, MovingBody->GetComponentLocation(), MovingBody->GetComponentLocation()+Delta,
        MovingBody->GetComponentQuat(), MovingBody->GetCollisionObjectType(), FCollisionShape::MakeBox(MovingBody->GetScaledBoxExtent()), BodyQuery))
    { Finish(false, FText::FromString(TEXT("Transit blocked; no endpoint or story state committed."))); return; }
    MovingBody->SetRelativeLocation(Desired, false);
    if (!IsValid(this) || State != ESovWorldTransitState::Moving) { return; }
    if (MoveElapsed >= TravelSeconds) { Finish(true, FText()); }
}
void ASovWorldTransitActor::Finish(bool bSuccess, const FText& Message)
{
    if (bEnding) { return; }
    TGuardValue<bool> Ending(bEnding, true);
    if (bSuccess) { bAtDestination = !bAtDestination; State = bAtDestination ? ESovWorldTransitState::AtDestination : ESovWorldTransitState::AtOrigin; }
    else if (StructuralHealth <= 0.f) { State = ESovWorldTransitState::Broken; }
    else if (MovingBody->GetRelativeLocation().Equals(bAtDestination ? DestinationOffset : FVector::ZeroVector, .5f))
    { State = bAtDestination ? ESovWorldTransitState::AtDestination : ESovWorldTransitState::AtOrigin; }
    else { State = ESovWorldTransitState::Blocked; }
    if (PlayerASC.IsValid() && TransitWindow.IsValid()) { PlayerASC->RemoveActiveGameplayEffect(TransitWindow); }
    TransitWindow.Invalidate();
    if (bOwnInputLock && LockedController.IsValid()) { LockedController->SetIgnoreMoveInput(false); }
    bOwnInputLock = false; LockedController.Reset(); TransitPlayer.Reset(); PlayerASC.Reset(); Streaming = nullptr;
    for (const auto& Movement : RiderMovements) { if (Movement.IsValid()) { Movement->RemoveTickPrerequisiteActor(this); } }
    RiderMovements.Reset();
    if (Kind == ESovWorldTransitKind::Lift && bSuccess) { EntryBounds->SetRelativeLocation(bAtDestination ? DestinationOffset : FVector::ZeroVector); }
    UpdateLinks(); OnTransitChanged.Broadcast(State, Message);
    if (bSuccess && bIrreversibleTransition && GetGameInstance())
    { if (auto* Save = GetGameInstance()->GetSubsystem<USovSaveSubsystem>()) { Save->QueueAutosave(ESovSaveBoundary::LongTransition, TransitId); } }
}
void ASovWorldTransitActor::SetPower(bool Value)
{
    if (HasAuthority()) { ++PowerRevision; bPowered = Value; UpdateLinks(); OnTransitChanged.Broadcast(State, Value ? FText() : FText::FromString(TEXT("Power required"))); }
}
void ASovWorldTransitActor::SetLockReason(const FText& Reason)
{ if (HasAuthority()) { ++LockRevision; LockReason = Reason; UpdateLinks(); OnTransitChanged.Broadcast(State, Reason); } }
float ASovWorldTransitActor::TakeDamage(float Amount, const FDamageEvent& Event, AController* Instigator, AActor* Causer)
{
    if (!HasAuthority() || !FMath::IsFinite(Amount) || Amount <= 0.f || StructuralHealth <= 0.f) { return 0.f; }
    const float Applied = FMath::Min(StructuralHealth, Amount); StructuralHealth -= Applied;
    if (StructuralHealth <= 0.f) { Finish(false, FText::FromString(TEXT("Mechanism damaged"))); }
    return Applied;
}
void ASovWorldTransitActor::UpdateLinks()
{
    const bool Stable = SovWorldMotionPolicy::CanEnableLink(State == ESovWorldTransitState::AtOrigin || State == ESovWorldTransitState::AtDestination,
        StructuralHealth > 0.f, !bRequiresPower || bPowered, LockReason.IsEmpty());
    OriginLink->SetEnabled(Stable && Kind == ESovWorldTransitKind::Lift && !bAtDestination);
    DestinationLink->SetEnabled(Stable && bAtDestination);
}
void ASovWorldTransitActor::ReconcileEndpoint()
{
    MovingBody->SetRelativeLocation(bAtDestination ? DestinationOffset : FVector::ZeroVector);
    if (Kind == ESovWorldTransitKind::Lift) { EntryBounds->SetRelativeLocation(bAtDestination ? DestinationOffset : FVector::ZeroVector); }
    State = StructuralHealth <= 0.f ? ESovWorldTransitState::Broken : bAtDestination ? ESovWorldTransitState::AtDestination : ESovWorldTransitState::AtOrigin;
    UpdateLinks();
}
void ASovWorldTransitActor::Load_Implementation()
{ if (HasAuthority()) { ++PowerRevision; ++LockRevision; Finish(false, FText()); ReconcileEndpoint(); } }
void ASovWorldTransitActor::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    // RequestUse captures its origin while holding the reentrancy guard, before any movement mutation.
    if (Ar.ArIsSaveGame && Ar.IsSaving() && !SovWorldMotionPolicy::CanSaveEndpoint(bEnding,
        State == ESovWorldTransitState::Moving, State == ESovWorldTransitState::WaitingForDestination,
        MovingBody && MovingBody->GetRelativeLocation().Equals(bAtDestination ? DestinationOffset : FVector::ZeroVector, .5f))) { Ar.SetError(); }
    if (Ar.ArIsSaveGame && Ar.IsLoading() && (!FMath::IsFinite(StructuralHealth) || StructuralHealth < 0.f)) { Ar.SetError(); }
}
void ASovWorldTransitActor::EndPlay(EEndPlayReason::Type Reason)
{ Finish(false, FText()); Super::EndPlay(Reason); }
void ASovWorldTransitActor::OnRep_State() { UpdateLinks(); OnTransitChanged.Broadcast(State, FText()); }
void ASovWorldTransitActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ASovWorldTransitActor, State);
    DOREPLIFETIME(ASovWorldTransitActor, bAtDestination); DOREPLIFETIME(ASovWorldTransitActor, bPowered);
    DOREPLIFETIME(ASovWorldTransitActor, LockReason); DOREPLIFETIME(ASovWorldTransitActor, StructuralHealth);
}
