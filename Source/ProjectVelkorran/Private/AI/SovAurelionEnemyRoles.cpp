// Copyright Fallen Signal Studios. All Rights Reserved.
#include "AI/SovAurelionEnemyRoles.h"
#include "Presentation/SovBloodFeedbackComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "AI/NarrativeNPCController.h"
#include "ArsenalStatics.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SovAurelionThermalFractureComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"

namespace
{
    bool LivingReady(const ASovNPCCharacterBase* NPC)
    {
        const auto* ASC = IsValid(NPC) ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
        return IsValid(NPC) && NPC->HasAuthority() && !NPC->IsActorBeingDestroyed()
            && NPC->IsEncounterSnapshotReady() && NPC->IsAlive() && IsValid(ASC)
            && ASC->GetAvatarActor() == NPC && ASC->GetSet<UNarrativeAttributeSetBase>();
    }
    // Authored rosters exist before the director's first readiness/hold tick. Readiness alone
    // must not let their traversal activity start in that interval. Standalone NPCs keep their route.
    bool ResolveTraversalEncounter(const ASovNPCCharacterBase* NPC, ASovEncounterDirector*& OutDirector)
    {
        OutDirector = nullptr;
        if (!IsValid(NPC) || !NPC->GetWorld()) { return false; }
        for (TActorIterator<ASovEncounterDirector> It(NPC->GetWorld()); It; ++It)
        {
            if (It->FindParticipantId(NPC).IsNone()) { continue; }
            if (OutDirector || It->IsActorBeingDestroyed() || It->GetEncounterState() != ESovEncounterState::Active
                || !It->GetAttemptId().IsValid()) { return false; }
            OutDirector = *It;
        }
        return true;
    }
    bool Interrupted(const UNarrativeAbilitySystemComponent* ASC)
    {
        if (!IsValid(ASC)) { return true; }
        const auto& N = FNarrativeGameplayTags::Get();
        const auto& S = FSovGameplayTags::Get();
        return ASC->HasMatchingGameplayTag(N.State_IsDead) || ASC->HasMatchingGameplayTag(N.State_SequencerControlled)
            || ASC->HasMatchingGameplayTag(N.State_Movement_Ragdoll) || ASC->HasMatchingGameplayTag(S.State_Fatal)
            || ASC->HasMatchingGameplayTag(S.State_Poise_Broken) || ASC->HasMatchingGameplayTag(S.State_Status_Frozen)
            || ASC->HasMatchingGameplayTag(S.State_Status_DeviceDisabled);
    }
}

ASovAurelionWallRoute::ASovAurelionWallRoute()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RouteRoot"));
    PrimaryActorTick.bCanEverTick = false;
}

TArray<FVector> ASovAurelionWallRoute::GetWorldPoints() const
{
    TArray<FVector> Result;
    for (const FVector& Point : LocalPoints) { Result.Add(GetActorTransform().TransformPosition(Point)); }
    return Result;
}

void ASovAurelionWallRoute::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority()) { RebuildPresentationSurfaces(); }
}

void ASovAurelionWallRoute::RebuildPresentationSurfaces()
{
    for (auto Source : PresentationSurfaces)
    {
        if (IsValid(Source) && Source->GetStaticMesh())
        {
            if (auto* Body = Source->GetStaticMesh()->GetBodySetup()) { Body->CreatePhysicsMeshes(); }
        }
    }
}

bool ASovAurelionWallRoute::ResolvePresentationContact(const FHitResult& PhysicalHit, const FVector& Probe, FHitResult& Contact) const
{
    if (!PhysicalHit.bBlockingHit || !Probe.IsNormalized()) { return false; }
    const FVector Start = PhysicalHit.ImpactPoint - Probe * 150.;
    const FVector End = PhysicalHit.ImpactPoint + Probe * 50.;
    bool Found = false;
    for (auto Source : PresentationSurfaces)
    {
        if (!IsValid(Source) || !Source->IsRegistered() || !Source->IsVisible() || !Source->GetStaticMesh()) { continue; }
        const auto* Body = Source->GetStaticMesh()->GetBodySetup();
        if (!Body) { continue; }
        const auto* Instances = Cast<UInstancedStaticMeshComponent>(Source);
        const int32 Count = Instances ? Instances->GetInstanceCount() : 1;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FTransform Transform = Source->GetComponentTransform();
            if (Instances && !Instances->GetInstanceTransform(Index, Transform, true)) { continue; }
            const FVector Scale = Transform.GetScale3D();
            if (Transform.ContainsNaN() || FMath::Min3(FMath::Abs(Scale.X), FMath::Abs(Scale.Y), FMath::Abs(Scale.Z)) < SMALL_NUMBER) { continue; }
            const FVector LocalStart = Transform.InverseTransformPosition(Start);
            const FVector Delta = Transform.InverseTransformPosition(End) - LocalStart;
            const double Length = Delta.Size();
            if (Length < SMALL_NUMBER) { continue; }
            // Read cooked geometry directly. No physics body is registered, so
            // neither channel traces nor object-type queries gain cosmetic hits.
            for (const auto& Geometry : Body->TriMeshGeometries)
            {
                if (!Geometry) { continue; }
                Chaos::FReal Time; Chaos::FVec3 Position, Normal; int32 FaceIndex;
                if (!Geometry->Raycast(LocalStart, Delta / Length, Length, 0., Time, Position, Normal, FaceIndex)) { continue; }
                const FVector Point = Transform.TransformPosition(Position);
                const FVector WorldNormal = Transform.TransformVectorNoScale(FVector(Normal) / Scale).GetSafeNormal();
                if (FVector::DotProduct(WorldNormal, PhysicalHit.ImpactNormal) <= .9
                    || (Found && FVector::DistSquared(Start, Point) >= FVector::DistSquared(Start, Contact.ImpactPoint))) { continue; }
                Contact = PhysicalHit; Contact.ImpactPoint = Point; Contact.ImpactNormal = WorldNormal;
                Found = true;
            }
        }
    }
    return Found;
}

bool ASovAurelionWallRoute::ValidateRoute(FString& Error) const
{
    Error.Reset();
    if (RouteId.IsNone() || LocalPoints.Num() < 3 || LocalPoints.Num() > 8 || GetActorTransform().ContainsNaN()
        || !GetActorScale3D().Equals(FVector::OneVector, .001f)
        || WallProbeDirection.ContainsNaN() || WallProbeDirection.SizeSquared2D() < .9
        || FMath::Abs(WallProbeDirection.Z) > .01 || !FMath::IsFinite(WallProbeDistance) || WallProbeDistance < 50.f || WallProbeDistance > 400.f
        || !FMath::IsFinite(Speed) || Speed < 50.f || Speed > 800.f
        || !FMath::IsFinite(EntryTolerance) || EntryTolerance < 10.f || EntryTolerance > 200.f
        || !FMath::IsFinite(MaximumDuration) || MaximumDuration < .5f || MaximumDuration > 15.f)
    { Error = TEXT("Wall route requires 3..8 finite local points, unit scale, horizontal wall probe and bounded speed/timing."); return false; }
    const TArray<FVector> Points = GetWorldPoints();
    double Distance = 0.;
    for (int32 Index = 0; Index < Points.Num(); ++Index)
    {
        if (Points[Index].ContainsNaN()) { Error = TEXT("Wall route points must be finite."); return false; }
        if (Index > 0)
        {
            const double Segment = FVector::Distance(Points[Index-1], Points[Index]);
            if (Segment < 10. || Segment > 1200.) { Error = TEXT("Each route segment must be 10..1200 cm."); return false; }
            Distance += Segment;
        }
    }
    if (Points[1].Z - Points[0].Z < 120. || FMath::Abs(Points[1].Z - Points[0].Z) < FVector::Dist2D(Points[0], Points[1])
        || FVector::Dist2D(Points[0], Points.Last()) < 80. || Distance > 3000. || Distance / Speed > MaximumDuration)
    { Error = TEXT("Route must climb a wall at least 120 cm, relocate onto a landing and fit its duration budget."); return false; }
    return true;
}

USovAurelionWallTraversalComponent::USovAurelionWallTraversalComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void USovAurelionWallTraversalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USovAurelionWallTraversalComponent, bTraversing);
    DOREPLIFETIME(USovAurelionWallTraversalComponent, bOnWall);
    DOREPLIFETIME(USovAurelionWallTraversalComponent, WallNormal);
    DOREPLIFETIME(USovAurelionWallTraversalComponent, WallTangent);
    DOREPLIFETIME(USovAurelionWallTraversalComponent, WallPoint);
}

void USovAurelionWallTraversalComponent::UpdateWallSurface(const FVector& Direction)
{
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(WallPresentation), false, GetOwner());
    Query.AddIgnoredActor(ActiveRoute.Get());
    const FVector Start = GetOwner()->GetActorLocation();
    const FVector Probe = ActiveRoute->GetActorTransform().TransformVectorNoScale(ActiveRoute->WallProbeDirection).GetSafeNormal();
    bOnWall = PointIndex <= 2 && GetWorld()->LineTraceSingleByChannel(Hit, Start,
        Start + Probe * ActiveRoute->WallProbeDistance, ECC_Visibility, Query)
        && Hit.bBlockingHit && FMath::Abs(Hit.ImpactNormal.Z) < .3;
    if (bOnWall)
    {
        FHitResult VisualHit;
        if (ActiveRoute->ResolvePresentationContact(Hit, Probe, VisualHit)) { Hit = VisualHit; }
        WallNormal = Hit.ImpactNormal;
        WallTangent = FVector::VectorPlaneProject(Direction, WallNormal).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
        WallPoint = Hit.ImpactPoint;
    }
}

void USovAurelionWallTraversalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    auto* NPC = Cast<ASovNPCCharacterBase>(GetOwner());
    auto* Mesh = NPC ? NPC->GetMesh() : nullptr;
    if (!Mesh || !Mesh->GetSkeletalMeshAsset() || Mesh->IsSimulatingPhysics() || !NPC->IsAlive()) { return; }
    auto* Anim = Mesh->GetAnimInstance();
    const bool OnWall = bTraversing && bOnWall;
    if (OnWall && !bPresentingWall)
    {
        PresentedMesh = Mesh;
        GroundMeshTransform = Mesh->GetRelativeTransform();
        bPresentingWall = true;
        if (Anim && WallRunMontage && Anim->Montage_Play(WallRunMontage, 1.15f) > 0.f)
        {
            const FName Section = WallRunMontage->GetSectionName(0);
            Anim->Montage_SetNextSection(Section, Section, WallRunMontage);
        }
    }
    if (!bPresentingWall || PresentedMesh.Get() != Mesh) { return; }
    FTransform Target = GroundMeshTransform;
    if (OnWall)
    {
        const FQuat Rotation = FRotationMatrix::MakeFromXZ(WallTangent, WallNormal).ToQuat() * GroundMeshTransform.GetRotation();
        const FBoxSphereBounds Bounds = Mesh->GetSkeletalMeshAsset()->GetImportedBounds();
        const double FootHeight = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Mesh->GetComponentScale().Z;
        const FVector Origin = WallPoint + WallNormal * (2. - FootHeight);
        Target.SetLocation(Mesh->GetAttachParent()->GetComponentTransform().InverseTransformPosition(Origin));
        Target.SetRotation(Mesh->GetAttachParent()->GetComponentQuat().Inverse() * Rotation);
    }
    const float Alpha = 1.f - FMath::Exp(-DeltaTime * 14.f);
    Mesh->SetRelativeLocationAndRotation(FMath::Lerp(Mesh->GetRelativeLocation(), Target.GetLocation(), Alpha),
        FQuat::Slerp(Mesh->GetRelativeRotation().Quaternion(), Target.GetRotation(), Alpha));
    if (!OnWall)
    {
        if (Anim && WallRunMontage && Anim->Montage_IsPlaying(WallRunMontage)) { Anim->Montage_Stop(.15f, WallRunMontage); }
        if (Mesh->GetRelativeLocation().Equals(Target.GetLocation(), .5)
            && Mesh->GetRelativeRotation().Quaternion().AngularDistance(Target.GetRotation()) < .01)
        { Mesh->SetRelativeTransform(GroundMeshTransform); bPresentingWall = false; PresentedMesh.Reset(); }
    }
}

bool USovAurelionWallTraversalComponent::HasReadyOwner(bool bContinuing, bool bMovementAcquired) const
{
    const auto* NPC = Cast<ASovNPCCharacterBase>(GetOwner());
    const auto* ASC = IsValid(NPC) ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
    const auto* Controller = IsValid(NPC) ? Cast<ANarrativeNPCController>(NPC->GetController()) : nullptr;
    if (!LivingReady(NPC) || !IsRegistered() || NPC->IsHidden() || !NPC->GetActorEnableCollision()
        || !IsValid(Controller) || Controller->GetPawn() != NPC || Controller->IsThreatMemorySuspended() || Interrupted(ASC)) { return false; }
    if (ASC->GetTagCount(FNarrativeGameplayTags::Get().State_Busy) != (bContinuing && bOwnsBusy ? 1 : 0)) { return false; }
    ASovEncounterDirector* Director = nullptr;
    if (!ResolveTraversalEncounter(NPC, Director)) { return false; }
    if (!bContinuing) { return NPC->GetCharacterMovement() && NPC->GetCharacterMovement()->IsMovingOnGround(); }
    if (ActiveEncounter.Get() != Director || ActiveEncounterAttempt != (Director ? Director->GetAttemptId() : FGuid())) { return false; }
    return ActiveCharacter.Get() == NPC && ActiveController.Get() == Controller && ActiveASC.Get() == ASC
        && ASC->GetCombatActorInfoEpoch() == ActorInfoEpoch && ActiveMovement.IsValid()
        && ActiveMovement.Get() == NPC->GetCharacterMovement()
        && ActiveMovement->MovementMode == (bMovementAcquired ? MOVE_Flying : static_cast<EMovementMode>(PriorMovementMode))
        && (bMovementAcquired || ActiveMovement->CustomMovementMode == PriorCustomMode)
        && ActiveRoute.IsValid() && ActiveRoute.Get() == Route;
}

bool USovAurelionWallTraversalComponent::ValidatePhysicalRoute(TArray<FVector>& Points) const
{
    const auto* NPC = Cast<ASovNPCCharacterBase>(GetOwner());
    if (!IsValid(Route) || !IsValid(NPC) || Route->GetWorld() != NPC->GetWorld() || Route->IsActorBeingDestroyed()) { return false; }
    FString Error;
    if (!Route->ValidateRoute(Error)) { return false; }
    Points = Route->GetWorldPoints();
    const auto* Capsule = NPC->GetCapsuleComponent();
    if (!Capsule || FVector::Dist(NPC->GetActorLocation(), Points[0]) > Route->EntryTolerance) { return false; }
    const auto Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AurelionWallRoute), false, NPC);
    Query.AddIgnoredActor(Route);
    FCollisionResponseParams Response(Capsule->GetCollisionResponseToChannels());
    FVector Previous = NPC->GetActorLocation();
    for (const FVector& Point : Points)
    {
        FHitResult Hit;
        if (GetWorld()->SweepSingleByChannel(Hit, Previous, Point, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Response)) { return false; }
        Previous = Point;
    }
    // The climb must be next to actual blocking near-vertical geometry, not an arbitrary air path.
    const FVector Midpoint = (Points[0] + Points[1]) * .5;
    const FVector Probe = Route->GetActorTransform().TransformVectorNoScale(Route->WallProbeDirection).GetSafeNormal();
    FHitResult Wall;
    if (!GetWorld()->LineTraceSingleByChannel(Wall, Midpoint, Midpoint + Probe * Route->WallProbeDistance, ECC_Visibility, Query)
        || !Wall.bBlockingHit || FMath::Abs(Wall.ImpactNormal.Z) > .3) { return false; }
    // End only on a real walkable landing; the capsule-centre endpoint must not float above it.
    return HasWalkableLanding(Points.Last());
}

bool USovAurelionWallTraversalComponent::HasWalkableLanding(const FVector& Point) const
{
    const auto* NPC = Cast<ASovNPCCharacterBase>(GetOwner());
    if (!IsValid(NPC) || !GetWorld() || !NPC->GetCapsuleComponent() || !NPC->GetCharacterMovement()) { return false; }
    const double HalfHeight = NPC->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AurelionWallLanding), false, NPC);
    if (IsValid(Route)) { Query.AddIgnoredActor(Route); }
    FHitResult Floor;
    return GetWorld()->LineTraceSingleByChannel(Floor, Point, Point - FVector(0., 0., HalfHeight + 30.), ECC_Visibility, Query)
        && Floor.bBlockingHit && Floor.ImpactNormal.Z >= NPC->GetCharacterMovement()->GetWalkableFloorZ()
        && FMath::Abs((Point.Z - Floor.ImpactPoint.Z) - HalfHeight) <= 25.;
}

bool USovAurelionWallTraversalComponent::CanBeginTraversal() const
{
    if (bTraversing || bMutating || bRouteCompleted || !HasReadyOwner(false)) { return false; }
    TArray<FVector> Points;
    return ValidatePhysicalRoute(Points);
}

bool USovAurelionWallTraversalComponent::Owns(UObject* RequestOwner, uint64 Lease) const
{
    return bTraversing && Lease != 0 && Generation == Lease && LeaseOwner.IsValid() && LeaseOwner.Get() == RequestOwner;
}

uint64 USovAurelionWallTraversalComponent::BeginTraversal(UObject* RequestOwner)
{
    if (!IsValid(RequestOwner) || !CanBeginTraversal()) { return 0; }
    TArray<FVector> Points;
    if (!ValidatePhysicalRoute(Points)) { return 0; }
    TGuardValue<bool> Mutation(bMutating, true);
    auto* NPC = CastChecked<ASovNPCCharacterBase>(GetOwner());
    ASovEncounterDirector* Director = nullptr;
    if (!ResolveTraversalEncounter(NPC, Director)) { return 0; }
    ActiveEncounter = Director; ActiveEncounterAttempt = Director ? Director->GetAttemptId() : FGuid();
    ActiveCharacter = NPC; ActiveController = CastChecked<ANarrativeNPCController>(NPC->GetController());
    ActiveASC = NPC->GetNarrativeAbilitySystemComponent(); ActiveMovement = NPC->GetCharacterMovement(); ActiveRoute = Route;
    ActorInfoEpoch = ActiveASC->GetCombatActorInfoEpoch();
    ActivePoints = MoveTemp(Points); PointIndex = 0; Elapsed = 0.f;
    CapturedSpeed = Route->Speed; CapturedMaximumDuration = Route->MaximumDuration;
    PriorMovementMode = ActiveMovement->MovementMode; PriorCustomMode = ActiveMovement->CustomMovementMode;
    LeaseOwner = RequestOwner; if (++Generation == 0) { ++Generation; }
    const uint64 Lease = Generation;
    bTraversing = true; bOwnsMovementMode = false; LastResult = ESovAurelionTraversalResult::Running;
    ActiveController->StopMovement();
    if (!Owns(RequestOwner, Lease) || !HasReadyOwner(true, false)) { FinishTraversal(Lease, ESovAurelionTraversalResult::Cancelled); return 0; }
    bOwnsBusy = true;
    ActiveASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll);
    if (!Owns(RequestOwner, Lease) || !HasReadyOwner(true, false)) { FinishTraversal(Lease, ESovAurelionTraversalResult::Cancelled); return 0; }
    ActiveMovement->StopMovementImmediately();
    if (!Owns(RequestOwner, Lease) || !HasReadyOwner(true, false)) { FinishTraversal(Lease, ESovAurelionTraversalResult::Cancelled); return 0; }
    bOwnsMovementMode = true;
    ActiveMovement->SetMovementMode(MOVE_Flying);
    if (!Owns(RequestOwner, Lease) || !HasReadyOwner(true)) { FinishTraversal(Lease, ESovAurelionTraversalResult::Cancelled); return 0; }
    // A director suspends perception and adds its Busy count before pausing the BT. Its task
    // will not tick while paused, so retire this lease synchronously on the existing tag signal.
    BusyChangedHandle = ActiveASC->RegisterGameplayTagEvent(FNarrativeGameplayTags::Get().State_Busy, EGameplayTagEventType::AnyCountChange)
        .AddUObject(this, &ThisClass::OnTraversalBusyChanged, ActiveASC, Lease);
    NPC->ForceNetUpdate();
    return Lease;
}

ESovAurelionTraversalResult USovAurelionWallTraversalComponent::AdvanceTraversal(UObject* RequestOwner, uint64 Lease, float DeltaSeconds)
{
    if (!Owns(RequestOwner, Lease)) { return ESovAurelionTraversalResult::Unavailable; }
    if (bMutating) { return ESovAurelionTraversalResult::Running; }
    TGuardValue<bool> Mutation(bMutating, true);
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f || DeltaSeconds > .5f || !HasReadyOwner(true))
    { FinishTraversal(Lease, ESovAurelionTraversalResult::Cancelled); return ESovAurelionTraversalResult::Cancelled; }
    Elapsed += DeltaSeconds;
    if (Elapsed > CapturedMaximumDuration) { FinishTraversal(Lease, ESovAurelionTraversalResult::Cancelled); return ESovAurelionTraversalResult::Cancelled; }
    double Budget = CapturedSpeed * DeltaSeconds;
    while (Budget > KINDA_SMALL_NUMBER && PointIndex < ActivePoints.Num())
    {
        const FVector Offset = ActivePoints[PointIndex] - ActiveCharacter->GetActorLocation();
        const double Distance = Offset.Size();
        if (Distance <= 1.) { ++PointIndex; continue; }
        const double Step = FMath::Min(Budget, Distance);
        FHitResult Hit;
        ActiveMovement->MoveUpdatedComponent(Offset / Distance * Step, ActiveCharacter->GetActorQuat(), true, &Hit);
        if (!Owns(RequestOwner, Lease)) { return ESovAurelionTraversalResult::Cancelled; }
        if (!HasReadyOwner(true)) { FinishTraversal(Lease, ESovAurelionTraversalResult::Cancelled); return ESovAurelionTraversalResult::Cancelled; }
        if (Hit.bBlockingHit || Hit.bStartPenetrating) { FinishTraversal(Lease, ESovAurelionTraversalResult::Blocked); return ESovAurelionTraversalResult::Blocked; }
        Budget -= Step;
        UpdateWallSurface(Offset / Distance);
    }
    if (PointIndex == ActivePoints.Num())
    {
        if (!HasWalkableLanding(ActiveCharacter->GetActorLocation()))
        { FinishTraversal(Lease, ESovAurelionTraversalResult::Blocked); return ESovAurelionTraversalResult::Blocked; }
        bRouteCompleted = true;
        FinishTraversal(Lease, ESovAurelionTraversalResult::Completed);
        return ESovAurelionTraversalResult::Completed;
    }
    return ESovAurelionTraversalResult::Running;
}

void USovAurelionWallTraversalComponent::OnTraversalBusyChanged(FGameplayTag Tag, int32 NewCount,
    TWeakObjectPtr<UNarrativeAbilitySystemComponent> ExpectedASC, uint64 ExpectedLease)
{
    if (!bTraversing || !bOwnsBusy || Generation != ExpectedLease || !ExpectedASC.IsValid() || ActiveASC != ExpectedASC) { return; }
    // Nested tag callbacks can leave an older count in the current broadcast. Read the actual
    // container and fence its captured lease; never let a retired callback cancel a successor.
    const int32 CurrentCount = ExpectedASC->GetTagCount(FNarrativeGameplayTags::Get().State_Busy);
    if (CurrentCount != 1)
    {
        // An external reset already removed our contribution. Movement teardown may call
        // another owner; never remove that owner's newly installed count afterward.
        if (CurrentCount == 0) { bOwnsBusy = false; }
        FinishTraversal(ExpectedLease, ESovAurelionTraversalResult::Cancelled);
    }
}

void USovAurelionWallTraversalComponent::FinishTraversal(uint64 Lease, ESovAurelionTraversalResult Result)
{
    if (!bTraversing || Lease != Generation) { return; }
    TGuardValue<bool> Mutation(bMutating, true);
    const auto SavedASC = ActiveASC;
    const FDelegateHandle SavedBusyHandle = BusyChangedHandle;
    BusyChangedHandle.Reset();
    if (SavedASC.IsValid() && SavedBusyHandle.IsValid())
    { SavedASC->RegisterGameplayTagEvent(FNarrativeGameplayTags::Get().State_Busy, EGameplayTagEventType::AnyCountChange).Remove(SavedBusyHandle); }
    const auto SavedNPC = ActiveCharacter;
    const auto SavedMovement = ActiveMovement;
    const auto SavedController = ActiveController;
    const bool OwnedBusy = bOwnsBusy;
    const bool OwnedMovement = bOwnsMovementMode;
    const uint8 SavedPriorMode = PriorMovementMode;
    const uint8 SavedPriorCustomMode = PriorCustomMode;
    bTraversing = false; bOnWall = false; bOwnsBusy = false; bOwnsMovementMode = false; LastResult = Result;
    LeaseOwner.Reset(); ActivePoints.Reset(); ActiveRoute.Reset(); ActiveASC.Reset(); ActiveCharacter.Reset(); ActiveController.Reset(); ActiveMovement.Reset();
    ActiveEncounter.Reset(); ActiveEncounterAttempt.Invalidate();
    // Retire the lease before outward movement/tag callbacks; never restore over a newer movement owner.
    if (OwnedMovement && SavedNPC.IsValid() && SavedMovement.IsValid() && SavedNPC->IsAlive()
        && SavedController.IsValid() && SavedNPC->GetController() == SavedController.Get()
        && SavedNPC->GetCharacterMovement() == SavedMovement.Get() && SavedMovement->MovementMode == MOVE_Flying)
    {
        SavedMovement->StopMovementImmediately();
        // A cancelled climb falls safely from its real current position. Only a finished route has a verified floor.
        SavedMovement->SetMovementMode(Result == ESovAurelionTraversalResult::Completed ? static_cast<EMovementMode>(SavedPriorMode) : MOVE_Falling,
            Result == ESovAurelionTraversalResult::Completed ? SavedPriorCustomMode : 0);
    }
    if (OwnedBusy && SavedASC.IsValid()) { SavedASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll); }
    if (SavedNPC.IsValid()) { SavedNPC->ForceNetUpdate(); }
}

bool USovAurelionWallTraversalComponent::CancelTraversal(UObject* RequestOwner, uint64 Lease)
{
    if (!Owns(RequestOwner, Lease) || !GetOwner() || !GetOwner()->HasAuthority()) { return false; }
    FinishTraversal(Lease, ESovAurelionTraversalResult::Cancelled);
    return true;
}

void USovAurelionWallTraversalComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    FinishTraversal(Generation, ESovAurelionTraversalResult::Cancelled);
    Super::EndPlay(Reason);
}

void USovAurelionWallTraversalComponent::PrepareForSave_Implementation()
{ SavedRouteId = IsValid(Route) ? Route->RouteId : NAME_None; }

void USovAurelionWallTraversalComponent::Load_Implementation()
{
    FinishTraversal(Generation, ESovAurelionTraversalResult::Cancelled);
    Route = nullptr; bAcceptedRestore = false;
    if (!GetWorld() || SavedRouteId.IsNone()) { return; }
    for (TActorIterator<ASovAurelionWallRoute> It(GetWorld()); It; ++It)
    {
        if (!It->IsActorBeingDestroyed() && It->RouteId == SavedRouteId)
        {
            if (Route) { Route = nullptr; return; }
            Route = *It;
        }
    }
    FString Error;
    bAcceptedRestore = IsValid(Route) && Route->ValidateRoute(Error);
}

USovAurelionWeaverWard::USovAurelionWeaverWard()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    FGameplayModifierInfo Modifier;
    Modifier.Attribute = UNarrativeAttributeSetBase::GetArmorAttribute();
    Modifier.ModifierOp = EGameplayModOp::Additive;
    Modifier.ModifierMagnitude = FScalableFloat(15.f);
    Modifiers.Add(Modifier);
}

USovAurelionElitePoiseAttributes::USovAurelionElitePoiseAttributes()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    for (const FGameplayAttribute& Attribute : {UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), UNarrativeAttributeSetBase::GetPoiseAttribute()})
    {
        FGameplayModifierInfo Modifier; Modifier.Attribute = Attribute;
        Modifier.ModifierOp = EGameplayModOp::Override; Modifier.ModifierMagnitude = FScalableFloat(100.f);
        Modifiers.Add(Modifier);
    }
}

USovAurelionFreshCommandLink::USovAurelionFreshCommandLink()
{
    bStartsActive = false; // Actual member readiness and authored identity are required before activation.
    LinkId = TEXT("Aurelion.Formation"); // Inert non-leading drone snapshots still need a stable valid identity.
}

bool USovAurelionFreshCommandLink::InitializeFreshLink()
{
    if (bInitializing || !IsRegistered()) { return false; }
    // An inactive instance after death is still a used identity, just like a severed or restored one.
    if (GetLinkInstanceId().IsValid() || GetCommandLinkState() != ESovCommandLinkState::Inactive) { return IsCommandLinkActive(); }
    if (bRestoredConfiguration) { return false; } // The director publishes restored live state, never bootstrap.
    auto* NPC = Cast<ASovNPCCharacterBase>(GetOwner());
    const auto* ASC = IsValid(NPC) ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
    bool bOwnedEntryHold = false;
    if (LivingReady(NPC))
    {
        int32 RegisteringDirectors = 0;
        for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
        {
            if (!It->IsActorBeingDestroyed() && !It->FindParticipantId(NPC).IsNone())
            { ++RegisteringDirectors; bOwnedEntryHold = It->IsOwnedPreEntryHold(NPC); }
        }
        bOwnedEntryHold = bOwnedEntryHold && RegisteringDirectors == 1;
    }
    const auto* NPCController = IsValid(NPC) ? Cast<ANarrativeNPCController>(NPC->GetController()) : nullptr;
    if (!LivingReady(NPC) || NPC->IsHidden() || !NPC->GetActorEnableCollision() || Interrupted(ASC)
        || (ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy) && !bOwnedEntryHold)
        || (NPCController && NPCController->IsThreatMemorySuspended() && !bOwnedEntryHold)
        || !HasValidCommandLinkConfiguration()) { return false; }
    for (AActor* Member : GetLinkedActors())
    {
        const auto* Ally = Cast<ASovNPCCharacterBase>(Member);
        if (!LivingReady(Ally) || Ally->GetWorld() != GetWorld() || Ally == NPC
            || UArsenalStatics::GetAttitude(NPC, Ally) != ETeamAttitude::Friendly) { return false; }
    }
    TGuardValue<bool> Initializing(bInitializing, true);
    return ActivateCommandLink(NPC);
}

void USovAurelionFreshCommandLink::BeginPlay()
{
    Super::BeginPlay();
    if (!bAutoInitializeFreshLink || !GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || GetLinkId().IsNone()) { return; }
    // Cold appearance/weapon streaming can exceed 30 game seconds on the authored
    // roster. Keep startup bounded without abandoning a still-loading formation.
    BootstrapDeadline = GetWorld()->GetTimeSeconds() + 180.;
    PollReadiness();
    if (!GetLinkInstanceId().IsValid())
    { GetWorld()->GetTimerManager().SetTimer(ReadinessTimer, this, &ThisClass::PollReadiness, .1f, true); }
}

void USovAurelionFreshCommandLink::PollReadiness()
{
    if (!GetWorld()) { return; }
    if (bAutoInitializeFreshLink && !bRestoredConfiguration && GetWorld()->GetTimeSeconds() <= BootstrapDeadline) { InitializeFreshLink(); }
    if (GetLinkInstanceId().IsValid() || GetCommandLinkState() != ESovCommandLinkState::Inactive
        || bRestoredConfiguration || !bAutoInitializeFreshLink || GetWorld()->GetTimeSeconds() >= BootstrapDeadline || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed())
    { GetWorld()->GetTimerManager().ClearTimer(ReadinessTimer); }
}

void USovAurelionFreshCommandLink::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ReadinessTimer); }
    Super::EndPlay(Reason);
}

void USovAurelionFreshCommandLink::PrepareForSave_Implementation() { SavedConfigurationId = GetLinkId(); }
void USovAurelionFreshCommandLink::Load_Implementation()
{
    bRestoredConfiguration = true;
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ReadinessTimer); }
    bAcceptedRestore = !SavedConfigurationId.IsNone() && (GetLinkId() == SavedConfigurationId || ConfigureLinkId(SavedConfigurationId));
}

USovAurelionWeaverLink::USovAurelionWeaverLink()
{
    LinkId = NAME_None; bAutoInitializeFreshLink = true;
    bIncludeOwnerAsParticipant = false;
    ActiveLinkEffectClass = USovAurelionWeaverWard::StaticClass();
}

USovAurelionCoreWeakPoints::USovAurelionCoreWeakPoints()
{
    FSovWeakPointZone Core; Core.ZoneId = TEXT("Core");
    WeakPointZones.Add(Core);
}

void USovAurelionEliteThermalFracture::PrepareForSave_Implementation()
{
    Super::PrepareForSave_Implementation();
    SavedAnchorId = IsValid(FrostAnchor) && FrostAnchor->ActorHasTag(FrostAnchorId) ? FrostAnchorId : NAME_None;
}
void USovAurelionEliteThermalFracture::Load_Implementation()
{
    FrostAnchor = nullptr; bAcceptedAnchorRestore = false;
    if (GetWorld() && !SavedAnchorId.IsNone())
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (!It->IsActorBeingDestroyed() && It->ActorHasTag(SavedAnchorId))
            {
                if (FrostAnchor) { FrostAnchor = nullptr; Super::Load_Implementation(); return; }
                FrostAnchor = *It;
            }
        }
        if (FrostAnchor) { FrostAnchorId = SavedAnchorId; bAcceptedAnchorRestore = true; }
    }
    Super::Load_Implementation(); // Retires receipts; it cannot manufacture a new frost application.
}

ASovAurelionSecurityDrone::ASovAurelionSecurityDrone(const FObjectInitializer& Initializer) : Super(Initializer)
{ FormationLink = CreateDefaultSubobject<USovAurelionFreshCommandLink>(TEXT("AurelionFormation")); }

ASovAurelionLinkbound::ASovAurelionLinkbound(const FObjectInitializer& Initializer) : Super(Initializer)
{ if (auto* Blood = FindComponentByClass<USovBloodFeedbackComponent>()) { Blood->bBlackBlood = true; } }
ASovAurelionWallRunner::ASovAurelionWallRunner(const FObjectInitializer& Initializer) : Super(Initializer)
{ WallTraversal = CreateDefaultSubobject<USovAurelionWallTraversalComponent>(TEXT("AurelionWallTraversal")); }
ASovAurelionWeaver::ASovAurelionWeaver(const FObjectInitializer& Initializer) : Super(Initializer)
{
    AnchorA = CreateDefaultSubobject<USovAurelionWeaverLink>(TEXT("AurelionAnchorA"));
    if (auto* Blood = FindComponentByClass<USovBloodFeedbackComponent>()) { Blood->bBlackBlood = true; }
    AnchorB = CreateDefaultSubobject<USovAurelionWeaverLink>(TEXT("AurelionAnchorB"));
}
ASovAurelionElite::ASovAurelionElite(const FObjectInitializer& Initializer) : Super(Initializer)
{
    CoreWeakPoints = CreateDefaultSubobject<USovAurelionCoreWeakPoints>(TEXT("AurelionCoreWeakPoints"));
    ThermalFracture = CreateDefaultSubobject<USovAurelionEliteThermalFracture>(TEXT("AurelionThermalFracture"));
    ElitePoise = CreateDefaultSubobject<USovPoiseComponent>(TEXT("AurelionPoise"));
}

bool ASovAurelionWeaver::HasReadySupportOwner() const
{
    const auto* ASC = GetNarrativeAbilitySystemComponent();
    return LivingReady(this) && !IsHidden() && GetActorEnableCollision() && !Interrupted(ASC)
        && !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy);
}

bool ASovAurelionWeaver::HasActiveSupportLink() const
{ return (AnchorA && AnchorA->IsCommandLinkActive()) || (AnchorB && AnchorB->IsCommandLinkActive()); }

bool ASovAurelionWeaver::InitializeFreshLinks()
{
    if (bUpdatingLinks || !HasReadySupportOwner() || !AnchorA || !AnchorB
        || AnchorA->GetLinkId().IsNone() || AnchorB->GetLinkId().IsNone() || AnchorA->GetLinkId() == AnchorB->GetLinkId()) { return false; }
    TGuardValue<bool> Updating(bUpdatingLinks, true);
    const auto* ExpectedASC = GetNarrativeAbilitySystemComponent();
    const uint64 ExpectedEpoch = ExpectedASC->GetCombatActorInfoEpoch();
    bool bCreatedLink = false;
    for (auto* Link : {AnchorA.Get(), AnchorB.Get()})
    {
        if (!Link->HasValidCommandLinkConfiguration()) { return false; }
        for (AActor* Member : Link->GetLinkedActors())
        {
            const auto* NPC = Cast<ASovNPCCharacterBase>(Member);
            if (!LivingReady(NPC) || NPC == this || NPC->GetWorld() != GetWorld()
                || UArsenalStatics::GetAttitude(this, NPC) != ETeamAttitude::Friendly) { return false; }
        }
    }
    for (auto* Link : {AnchorA.Get(), AnchorB.Get()})
    {
        if (!HasReadySupportOwner() || GetNarrativeAbilitySystemComponent() != ExpectedASC || ExpectedASC->GetCombatActorInfoEpoch() != ExpectedEpoch) { return false; }
        if (Link->GetCommandLinkState() == ESovCommandLinkState::Inactive && !Link->GetLinkInstanceId().IsValid())
        { if (!Link->InitializeFreshLink()) { return false; } bCreatedLink = true; }
        // A Severed or death-deactivated instance belongs to its native lifecycle. Never call ResetCommandLink here.
    }
    if (bCreatedLink && HasActiveSupportLink()) { MulticastSupportCast(); }
    return HasActiveSupportLink();
}

void ASovAurelionWeaver::MulticastSupportCast_Implementation()
{
    // A real fresh-link transition owns this cue; polling an existing tether never replays it.
    if (GetNetMode() == NM_DedicatedServer || !IsAlive() || !SupportCastMontage || !GetMesh()) { return; }
    if (auto* Anim = GetMesh()->GetAnimInstance(); Anim && !Anim->IsAnyMontagePlaying())
    { Anim->Montage_Play(SupportCastMontage); }
}

bool ASovAurelionWeaver::ShareObservedThreatWithLinkedAllies()
{
    if (!HasReadySupportOwner()) { return false; }
    auto* NPCController = Cast<ANarrativeNPCController>(GetController());
    AActor* Target = NPCController ? NPCController->GetFocusActor() : nullptr;
    if (!NPCController || NPCController->GetPawn() != this || !IsValid(Target) || !NPCController->CanDirectlyTargetThreat(Target)) { return false; }
    const auto* ExpectedASC = GetNarrativeAbilitySystemComponent();
    const uint64 ExpectedEpoch = ExpectedASC->GetCombatActorInfoEpoch();
    TSet<AActor*> Shared;
    bool bAnyShared = false;
    for (auto* Link : {AnchorA.Get(), AnchorB.Get()})
    {
        if (!Link || !Link->IsCommandLinkActive() || Link->GetCommandSource() != this) { continue; }
        const FGuid Instance = Link->GetLinkInstanceId();
        for (AActor* Member : Link->GetLinkedActors())
        {
            if (!HasReadySupportOwner() || GetController() != NPCController || NPCController->GetPawn() != this || GetNarrativeAbilitySystemComponent() != ExpectedASC
                || ExpectedASC->GetCombatActorInfoEpoch() != ExpectedEpoch || !NPCController->CanDirectlyTargetThreat(Target)) { return bAnyShared; }
            if (!IsValid(Link) || !Link->IsCommandLinkActive() || Link->GetLinkInstanceId() != Instance) { break; }
            auto* NPC = Cast<ASovNPCCharacterBase>(Member);
            auto* Ally = NPC ? Cast<ANarrativeNPCController>(NPC->GetController()) : nullptr;
            if (!Shared.Contains(Member) && LivingReady(NPC) && Ally && Link->ContainsLinkedActor(Member))
            { Shared.Add(Member); bAnyShared |= NPCController->ShareThreatWith(Ally, Target); }
        }
    }
    return bAnyShared;
}
