// Copyright Fallen Signal Studios. All Rights Reserved.
#include "AI/SovAurelionRoleActivities.h"
#include "AI/NarrativeNPCController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "ArsenalStatics.h"
#include "Campaign/SovEncounterDirector.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

namespace
{
constexpr float PressureDistance = 650.f;
constexpr float RetreatDistance = 900.f;
constexpr float MaximumSupportDistance = 1800.f;
constexpr float MaximumRetreatPath = 1400.f;
constexpr float MoveAcceptance = 45.f;

bool ReadyWeaver(const ASovAurelionWeaver* NPC, ASovEncounterDirector*& Director)
{
    Director = nullptr;
    const auto* ASC = IsValid(NPC) ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
    const auto* AI = IsValid(NPC) ? Cast<ANarrativeNPCController>(NPC->GetController()) : nullptr;
    const auto* Movement = IsValid(NPC) ? NPC->GetCharacterMovement() : nullptr;
    if (!IsValid(NPC) || !NPC->HasAuthority() || NPC->IsActorBeingDestroyed() || !NPC->IsEncounterSnapshotReady()
        || !NPC->IsAlive() || NPC->IsHidden() || !NPC->GetActorEnableCollision() || !IsValid(ASC)
        || ASC->GetAvatarActor() != NPC || !ASC->GetSet<UNarrativeAttributeSetBase>()
        || !IsValid(AI) || AI->GetPawn() != NPC || AI->IsThreatMemorySuspended()
        || !Movement || !Movement->IsMovingOnGround()) { return false; }
    const auto& N = FNarrativeGameplayTags::Get(); const auto& S = FSovGameplayTags::Get();
    for (const FGameplayTag Tag : {N.State_Busy, N.State_IsDead, N.State_SequencerControlled,
        N.State_Movement_Ragdoll, S.State_Fatal, S.State_Poise_Broken, S.State_Status_Frozen, S.State_Status_DeviceDisabled})
    { if (ASC->HasMatchingGameplayTag(Tag)) { return false; } }
    // The director, including its pre-entry/retry hold, remains authoritative.
    for (TActorIterator<ASovEncounterDirector> It(NPC->GetWorld()); It; ++It)
    {
        if (It->FindParticipantId(NPC).IsNone()) { continue; }
        if (Director || It->IsActorBeingDestroyed() || It->GetEncounterState() != ESovEncounterState::Active
            || !It->GetAttemptId().IsValid()) { return false; }
        Director = *It;
    }
    return true;
}

bool IsNearSupportedAlly(const ASovAurelionWeaver* NPC, const FVector& Position)
{
    for (const auto* Link : {NPC->GetAnchorA(), NPC->GetAnchorB()})
    {
        if (!IsValid(Link) || !Link->IsCommandLinkActive() || Link->GetCommandSource() != NPC) { continue; }
        for (AActor* Actor : Link->GetLinkedActors())
        {
            const auto* Ally = Cast<ASovNPCCharacterBase>(Actor);
            const auto* ASC = IsValid(Ally) ? Ally->GetNarrativeAbilitySystemComponent() : nullptr;
            if (Ally && Ally != NPC && Ally->GetWorld() == NPC->GetWorld() && !Ally->IsActorBeingDestroyed()
                && Ally->IsEncounterSnapshotReady() && Ally->IsAlive() && !Ally->IsHidden()
                && Ally->GetActorEnableCollision() && IsValid(ASC) && ASC->GetAvatarActor() == Ally
                && UArsenalStatics::GetAttitude(NPC, Ally) == ETeamAttitude::Friendly
                && FVector::DistSquared(Position, Ally->GetActorLocation()) <= FMath::Square(MaximumSupportDistance)) { return true; }
        }
    }
    return false;
}

void AbortExactMove(UObject& Owner, UPathFollowingComponent* Paths, const FAIRequestID Id)
{
    if (IsValid(Paths) && Id.IsValid() && Paths->GetCurrentRequestId() == Id)
    { Paths->AbortMove(Owner, FPathFollowingResultFlags::OwnerFinished, Id, EPathFollowingVelocityMode::Reset); }
}
}

USovAurelionWallTraversalActivity::USovAurelionWallTraversalActivity(const FObjectInitializer& Initializer) : Super(Initializer)
{ bIsInterruptable = true; }

float USovAurelionWallTraversalActivity::ScoreActivity_Implementation(const FNPCGoalContainer& Goals,
    UNPCGoalItem*& OutBestGoal, TArray<UNPCGoalItem*>& OutInvalidGoals)
{
    OutBestGoal = nullptr;
    const auto* NPC = IsValid(OwnerController) ? Cast<ASovAurelionWallRunner>(OwnerController->GetPawn()) : nullptr;
    const auto* Traversal = IsValid(NPC) ? NPC->GetWallTraversal() : nullptr;
    return Traversal && (Traversal->IsTraversing() || Traversal->CanBeginTraversal()) ? 1000.f : 0.f;
}

USovAurelionWeaverSupportActivity::USovAurelionWeaverSupportActivity(const FObjectInitializer& Initializer) : Super(Initializer)
{ bIsInterruptable = true; }

float USovAurelionWeaverSupportActivity::ScoreActivity_Implementation(const FNPCGoalContainer& Goals,
    UNPCGoalItem*& OutBestGoal, TArray<UNPCGoalItem*>& OutInvalidGoals)
{
    OutBestGoal = nullptr;
    const auto* NPC = IsValid(OwnerController) ? Cast<ASovAurelionWeaver>(OwnerController->GetPawn()) : nullptr;
    const auto* ASC = IsValid(NPC) ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
    if (!NPC || !NPC->HasAuthority() || !NPC->IsEncounterSnapshotReady() || !NPC->IsAlive()
        || NPC->IsHidden() || !ASC || ASC->GetAvatarActor() != NPC || OwnerController->IsThreatMemorySuspended()
        || ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy)
        || ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled)
        || ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Status_Frozen)
        || ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Poise_Broken)) { return 0.f; }
    // Deliberate role: prioritize protecting the line over its fallback weapon activity while a link survives.
    // After both anchors are severed the existing authored ordinary attack/idle activities take over.
    const auto Fresh = [](const USovAurelionWeaverLink* Link)
    { return Link && !Link->GetLinkId().IsNone() && !Link->GetLinkInstanceId().IsValid() && Link->HasValidCommandLinkConfiguration(); };
    return NPC->HasActiveSupportLink() || Fresh(NPC->GetAnchorA()) || Fresh(NPC->GetAnchorB()) ? 500.f : 0.f;
}

UBTTask_SovAurelionTraverseWall::UBTTask_SovAurelionTraverseWall(const FObjectInitializer& Initializer) : Super(Initializer)
{
    NodeName = TEXT("Traverse Authored Wall");
    bCreateNodeInstance = true; bIgnoreRestartSelf = true;
    INIT_TASK_NODE_NOTIFY_FLAGS();
}

void UBTTask_SovAurelionTraverseWall::ReleaseLease()
{
    const auto OldTraversal = Traversal;
    const uint64 OldLease = Lease;
    Traversal.Reset(); Lease = 0;
    if (OldTraversal.IsValid()) { OldTraversal->CancelTraversal(this, OldLease); }
}

EBTNodeResult::Type UBTTask_SovAurelionTraverseWall::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    ReleaseLease();
    const auto* Controller = OwnerComp.GetAIOwner();
    auto* NPC = Controller ? Cast<ASovAurelionWallRunner>(Controller->GetPawn()) : nullptr;
    if (!NPC || !NPC->GetWallTraversal()) { return EBTNodeResult::Failed; }
    Traversal = NPC->GetWallTraversal();
    Lease = Traversal->BeginTraversal(this);
    if (!Lease) { Traversal.Reset(); return EBTNodeResult::Failed; }
    return EBTNodeResult::InProgress;
}

void UBTTask_SovAurelionTraverseWall::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    const auto Result = Traversal.IsValid() ? Traversal->AdvanceTraversal(this, Lease, DeltaSeconds) : ESovAurelionTraversalResult::Cancelled;
    if (Result == ESovAurelionTraversalResult::Running) { return; }
    ReleaseLease();
    FinishLatentTask(OwnerComp, Result == ESovAurelionTraversalResult::Completed ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
}

EBTNodeResult::Type UBTTask_SovAurelionTraverseWall::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{ ReleaseLease(); return EBTNodeResult::Aborted; }

void UBTTask_SovAurelionTraverseWall::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{ ReleaseLease(); Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult); }

UBTTask_SovAurelionWeaverSupport::UBTTask_SovAurelionWeaverSupport(const FObjectInitializer& Initializer) : Super(Initializer)
{
    NodeName = TEXT("Maintain Weaver Support / Retreat From Pressure");
    bCreateNodeInstance = true; bIgnoreRestartSelf = true; bNotifyTick = true; bNotifyTaskFinished = true;
}

EBTNodeResult::Type UBTTask_SovAurelionWeaverSupport::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    const uint64 RetiringSerial = ExecutionSerial + 1;
    ReleaseMove();
    if (ExecutionSerial != RetiringSerial) { return EBTNodeResult::Failed; }
    auto* AI = Cast<ANarrativeNPCController>(OwnerComp.GetAIOwner());
    auto* NPC = AI ? Cast<ASovAurelionWeaver>(AI->GetPawn()) : nullptr;
    if (!NPC || !NPC->HasAuthority()) { return EBTNodeResult::Failed; }
    const TWeakObjectPtr<ASovAurelionWeaver> ExpectedNPC = NPC;
    const TWeakObjectPtr<ANarrativeNPCController> ExpectedAI = AI;
    const auto StillPreparing = [&]()
    {
        return ExecutionSerial == RetiringSerial && ExpectedNPC.IsValid() && ExpectedAI.IsValid()
            && !ExpectedNPC->IsActorBeingDestroyed() && ExpectedNPC->GetController() == ExpectedAI.Get()
            && ExpectedAI->GetPawn() == ExpectedNPC.Get();
    };
    NPC->InitializeFreshLinks();
    if (!StillPreparing()) { return EBTNodeResult::Failed; }
    NPC->ShareObservedThreatWithLinkedAllies();
    if (!StillPreparing()) { return EBTNodeResult::Failed; }
    if (!NPC->HasActiveSupportLink()) { return EBTNodeResult::Failed; }
    ASovEncounterDirector* Director = nullptr;
    auto* Following = AI->GetPathFollowingComponent();
    const double Now = NPC->GetWorld()->GetTimeSeconds();
    if (Now < NextDecisionAt || !ReadyWeaver(NPC, Director) || !Following
        || Following->GetStatus() != EPathFollowingStatus::Idle) { return EBTNodeResult::Succeeded; }
    NextDecisionAt = Now + 2.;
    FVector Destination; AActor* ObservedThreat = nullptr;
    if (!FindRetreat(NPC, Destination, ObservedThreat)) { return EBTNodeResult::Succeeded; }
    Weaver = NPC; Controller = AI; Abilities = NPC->GetNarrativeAbilitySystemComponent(); Paths = Following;
    Encounter = Director; Attempt = Director ? Director->GetAttemptId() : FGuid(); Threat = ObservedThreat;
    ActorInfoEpoch = Abilities->GetCombatActorInfoEpoch();
    const USovAurelionWeaverLink* Links[] = {NPC->GetAnchorA(), NPC->GetAnchorB()};
    for (int32 Index = 0; Index < 2; ++Index)
    { AnchorInstances[Index] = Links[Index]->GetLinkInstanceId(); AnchorStates[Index] = Links[Index]->GetCommandLinkState(); }
    MoveGoal = Destination; ProgressLocation = NPC->GetActorLocation();
    MoveDeadline = Now + 6.; ProgressDeadline = Now + 1.5; NextShareAt = Now + .5;
    const uint64 Serial = ++ExecutionSerial;
    FAIMoveRequest Request(Destination);
    Request.SetUsePathfinding(true); Request.SetAllowPartialPath(false); Request.SetProjectGoalLocation(false);
    Request.SetAcceptanceRadius(MoveAcceptance); Request.SetReachTestIncludesAgentRadius(false);
    Request.SetReachTestIncludesGoalRadius(false); Request.SetCanStrafe(true);
    const FPathFollowingRequestResult Result = AI->MoveTo(Request);
    // MoveTo can synchronously retire another activity. Never publish into a reentered task.
    if (ExecutionSerial != Serial || !OwnsSupport())
    {
        AbortExactMove(*this, Following, Result.MoveId);
        if (ExecutionSerial == Serial) { ReleaseMove(); }
        return EBTNodeResult::Failed;
    }
    if (Result.Code != EPathFollowingRequestResult::RequestSuccessful || !Result.MoveId.IsValid())
    { ReleaseMove(); return EBTNodeResult::Succeeded; }
    MoveId = Result.MoveId;
    return EBTNodeResult::InProgress;
}

bool UBTTask_SovAurelionWeaverSupport::FindRetreat(ASovAurelionWeaver* NPC, FVector& Destination, AActor*& ThreatActor) const
{
    Destination = FVector::ZeroVector; ThreatActor = nullptr;
    ASovEncounterDirector* Director = nullptr;
    if (!ReadyWeaver(NPC, Director) || !NPC->HasActiveSupportLink()) { return false; }
    const auto* AI = Cast<ANarrativeNPCController>(NPC->GetController());
    const FVector Start = NPC->GetNavAgentLocation();
    FVector ThreatPosition = FVector::ZeroVector;
    double Nearest = FMath::Square(PressureDistance);
    for (const FNarrativeThreatMemory& Memory : AI->GetThreatDebugSnapshot())
    {
        // Support has no attack-focus goal. Read actual sight; never upgrade an alert,
        // cloak memory or the target actor's live position into fresh perception.
        if (Memory.Source != ENarrativeThreatSource::Sight || !Memory.bDirectObservation
            || Memory.LastKnownPosition.ContainsNaN() || !AI->CanDirectlyTargetThreat(Memory.Target.Get())) { continue; }
        const double Distance = FVector::DistSquared2D(Start, Memory.LastKnownPosition);
        if (Distance < Nearest)
        { Nearest = Distance; ThreatPosition = Memory.LastKnownPosition; ThreatActor = Memory.Target.Get(); }
    }
    if (!ThreatActor || Nearest < 1. || !IsNearSupportedAlly(NPC, NPC->GetActorLocation())) { return false; }
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(NPC->GetWorld());
    const auto* Capsule = NPC->GetCapsuleComponent();
    const auto* Data = Nav ? Nav->GetNavDataForProps(NPC->GetNavAgentPropertiesRef(), Start) : nullptr;
    if (!Data || !Capsule || !Capsule->IsQueryCollisionEnabled()) { return false; }
    const FVector Away = (Start - ThreatPosition).GetSafeNormal2D();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight);
    const FCollisionQueryParams Query = NPC->GetIgnoreCharacterParams();
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
    // At most five complete paths per two game seconds, only while actually pressed.
    for (const float Angle : {0.f, 40.f, -40.f, 70.f, -70.f})
    {
        FVector Desired = ThreatPosition + Away.RotateAngleAxis(Angle, FVector::UpVector) * RetreatDistance;
        Desired.Z = Start.Z;
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(Desired, Projected, FVector(60,60,100), Data)
            || FVector::Dist2D(Projected.Location, Desired) > 60.f || FMath::Abs(Projected.Location.Z - Start.Z) > 80.f
            || FVector::Dist2D(Projected.Location, ThreatPosition) < FMath::Sqrt(Nearest) + 150.f
            || !IsNearSupportedAlly(NPC, Projected.Location + FVector(0,0,HalfHeight))) { continue; }
        FPathFindingQuery PathQuery(AI, *Data, Start, Projected.Location, Data->GetDefaultQueryFilter());
        PathQuery.SetAllowPartialPaths(false);
        const FPathFindingResult Route = Nav->FindPathSync(NPC->GetNavAgentPropertiesRef(), PathQuery);
        if (!Route.IsSuccessful() || !Route.Path.IsValid() || Route.Path->IsPartial()) { continue; }
        const TArray<FNavPathPoint>& Points = Route.Path->GetPathPoints();
        if (Points.Num() < 2 || Points.Num() > 32 || !Points.Last().Location.Equals(Projected.Location, 10.f)) { continue; }
        double Length = 0.; bool bClear = true;
        FVector Previous = NPC->GetActorLocation();
        for (const FNavPathPoint& Point : Points)
        {
            const FVector Center = Point.Location + FVector(0,0,HalfHeight + 2.f);
            if (Point.CustomNavLinkId.IsValid() || Point.Location.ContainsNaN()
                || FVector::Dist2D(Point.Location, ThreatPosition) + 30.f < FMath::Sqrt(Nearest)
                || !IsNearSupportedAlly(NPC, Center)) { bClear = false; break; }
            Length += FVector::Distance(Previous, Center);
            const FVector Closest = FMath::ClosestPointOnSegment(FVector(ThreatPosition.X, ThreatPosition.Y, 0),
                FVector(Previous.X, Previous.Y, 0), FVector(Center.X, Center.Y, 0));
            if (FVector::Dist2D(Closest, ThreatPosition) + 30.f < FMath::Sqrt(Nearest)) { bClear = false; break; }
            FHitResult Hit;
            if (Length > MaximumRetreatPath || NPC->GetWorld()->SweepSingleByChannel(Hit, Previous, Center,
                Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Responses))
            { bClear = false; break; }
            Previous = Center;
        }
        if (bClear) { Destination = Projected.Location; return true; }
    }
    return false;
}

bool UBTTask_SovAurelionWeaverSupport::OwnsSupport() const
{
    const auto* NPC = Weaver.Get(); ASovEncounterDirector* Director = nullptr;
    if (!ReadyWeaver(NPC, Director) || !NPC->HasActiveSupportLink() || Controller.Get() != NPC->GetController()
        || Abilities.Get() != NPC->GetNarrativeAbilitySystemComponent() || !Abilities.IsValid()
        || Abilities->GetCombatActorInfoEpoch() != ActorInfoEpoch || !Paths.IsValid()
        || Controller->GetPathFollowingComponent() != Paths.Get() || Director != Encounter.Get()
        || (Director && Director->GetAttemptId() != Attempt) || !Threat.IsValid()
        || !Controller->CanDirectlyTargetThreat(Threat.Get()) || !IsNearSupportedAlly(NPC, MoveGoal)) { return false; }
    const USovAurelionWeaverLink* Links[] = {NPC->GetAnchorA(), NPC->GetAnchorB()};
    for (int32 Index = 0; Index < 2; ++Index)
    {
        if (!IsValid(Links[Index]) || Links[Index]->GetLinkInstanceId() != AnchorInstances[Index]
            || Links[Index]->GetCommandLinkState() != AnchorStates[Index]) { return false; }
    }
    // Damage/network memory cannot prolong a retreat after its original sight is gone.
    for (const FNarrativeThreatMemory& Memory : Controller->GetThreatDebugSnapshot())
    { if (Memory.Target == Threat && Memory.Source == ENarrativeThreatSource::Sight && Memory.bDirectObservation) { return true; } }
    return false;
}

void UBTTask_SovAurelionWeaverSupport::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    if (!OwnsSupport() || OwnerComp.GetAIOwner() != Controller.Get())
    { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }
    const double Now = Weaver->GetWorld()->GetTimeSeconds();
    if (Paths->GetStatus() == EPathFollowingStatus::Idle)
    {
        const bool bArrived = FVector::Dist2D(Weaver->GetNavAgentLocation(), MoveGoal) <= MoveAcceptance + 10.f;
        FinishLatentTask(OwnerComp, bArrived ? EBTNodeResult::Succeeded : EBTNodeResult::Failed); return;
    }
    if (Paths->GetCurrentRequestId() != MoveId || Now >= MoveDeadline || Now >= ProgressDeadline)
    { FinishLatentTask(OwnerComp, EBTNodeResult::Failed); return; }
    if (FVector::Dist2D(ProgressLocation, Weaver->GetActorLocation()) >= 25.f)
    { ProgressLocation = Weaver->GetActorLocation(); ProgressDeadline = Now + 1.5; }
    if (Now >= NextShareAt)
    { NextShareAt = Now + .5; Weaver->ShareObservedThreatWithLinkedAllies(); }
}

void UBTTask_SovAurelionWeaverSupport::ReleaseMove()
{
    const auto PreviousPaths = Paths; const FAIRequestID PreviousId = MoveId;
    // Retire before abort callbacks. A reentrant/replacement move is never ours to cancel.
    ++ExecutionSerial; MoveId = FAIRequestID::InvalidRequest;
    Paths.Reset(); Weaver.Reset(); Controller.Reset(); Abilities.Reset(); Encounter.Reset(); Threat.Reset();
    AbortExactMove(*this, PreviousPaths.Get(), PreviousId);
}

EBTNodeResult::Type UBTTask_SovAurelionWeaverSupport::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{ ReleaseMove(); return EBTNodeResult::Aborted; }

void UBTTask_SovAurelionWeaverSupport::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{ ReleaseMove(); Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult); }
