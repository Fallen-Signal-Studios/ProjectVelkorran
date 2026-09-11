// Copyright Fallen Signal Studios. All Rights Reserved.
#include "AI/SovAurelionCrossfireQuery.h"
#include "AI/SovAurelionEnemyRoles.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/Activities/NPCActivity.h"
#include "AI/Activities/NPCGoalItem.h"
#include "ArsenalStatics.h"
#include "ArsenalSettings.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NarrativeGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Sovereign/SovGameplayTags.h"
#include "Logging/LogMacros.h"

// Opt in for a bounded diagnostic run with -LogCmds="LogSovCrossfireQuery VeryVerbose".
// Default Log verbosity suppresses every message below. No gameplay/debugger setting is changed.
DEFINE_LOG_CATEGORY_STATIC(LogSovCrossfireQuery, Log, All);

namespace
{
constexpr int32 MaximumCandidates = 8;
constexpr float MaximumPathLength = 1800.f;
constexpr float MinimumRange = 1000.f;
constexpr float MaximumRange = 3000.f;

bool TraceResult(const FEnvQueryInstance* Trace, bool bAccepted, const TCHAR* Family,
    const UObject* Subject = nullptr, int32 Candidate = INDEX_NONE)
{
    if (Trace && !bAccepted)
    {
        UE_LOG(LogSovCrossfireQuery, VeryVerbose,
            TEXT("Crossfire id=%d owner=%s candidate=%d reject=%s subject=%s"),
            Trace->QueryID, *GetPathNameSafe(Trace->Owner.Get()), Candidate, Family, *GetPathNameSafe(Subject));
    }
    return bAccepted;
}
// Raw EQS context storage deliberately contains only value types and weak references.
// It never keeps an old pawn, goal, encounter or link alive across a sliced query.
struct FLease
{
    TWeakObjectPtr<ASovAurelionSecurityDrone> Pawn;
    TWeakObjectPtr<ANarrativeNPCController> AI;
    TWeakObjectPtr<UNPCActivity> Activity;
    TWeakObjectPtr<UNPCGoalItem> Goal;
    TWeakObjectPtr<AActor> Target;
    TWeakObjectPtr<ASovEncounterDirector> Encounter;
    TWeakObjectPtr<USovCommandLinkComponent> Link;
    TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC;
    TWeakObjectPtr<UBlackboardComponent> Blackboard;
    FGuid Attempt, LinkInstance;
    uint64 PawnGeneration = 0, ActorInfoEpoch = 0, EncounterGeneration = 0;
    FVector Observed = FVector::ZeroVector;
    double CapturedAt = 0.;
};

bool Ready(const ASovAurelionSecurityDrone* Pawn, const FEnvQueryInstance* Trace = nullptr)
{
    const auto* ASC = IsValid(Pawn) ? Pawn->GetNarrativeAbilitySystemComponent() : nullptr;
    const auto* Movement = IsValid(Pawn) ? Pawn->GetCharacterMovement() : nullptr;
    if (!IsValid(Pawn) || !Pawn->HasAuthority() || Pawn->IsActorBeingDestroyed() || !Pawn->IsEncounterSnapshotReady()
        || !Pawn->IsAlive() || Pawn->IsHidden() || !Pawn->GetActorEnableCollision() || !IsValid(ASC)
        || ASC->GetAvatarActor() != Pawn || !ASC->GetSet<UNarrativeAttributeSetBase>()
        || !Movement || !Movement->IsMovingOnGround())
    {
        if (Trace)
        {
            UE_LOG(LogSovCrossfireQuery, VeryVerbose,
                TEXT("Crossfire id=%d ready_core pawn=%s movement_mode=%d asc=%s avatar=%s"),
                Trace->QueryID, *GetPathNameSafe(Pawn), Movement ? static_cast<int32>(Movement->MovementMode) : -1,
                *GetPathNameSafe(ASC), *GetPathNameSafe(ASC ? ASC->GetAvatarActor() : nullptr));
        }
        return TraceResult(Trace, false, TEXT("ready.identity_resources_collision_or_ground"), Pawn);
    }
    const auto& N = FNarrativeGameplayTags::Get(); const auto& S = FSovGameplayTags::Get();
    for (const auto Tag : {N.State_Busy, N.State_IsDead, N.State_Interacting, N.State_SequencerControlled,
        N.State_Movement_Ragdoll, S.State_Fatal, S.State_Poise_Broken, S.State_Status_Frozen, S.State_Status_DeviceDisabled})
    { if (ASC->HasMatchingGameplayTag(Tag))
        {
            if (Trace) { UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d blocked_tag=%s pawn=%s"), Trace->QueryID, *Tag.ToString(), *GetPathNameSafe(Pawn)); }
            return TraceResult(Trace, false, TEXT("ready.blocking_tag"), Pawn);
        } }
    return true;
}

bool ReadSight(const ANarrativeNPCController* AI, AActor* Target, FVector& Position, const FEnvQueryInstance* Trace = nullptr)
{
    if (!IsValid(AI) || AI->IsThreatMemorySuspended() || !IsValid(Target) || !AI->CanDirectlyTargetThreat(Target)) { return TraceResult(Trace, false, TEXT("sight.target_or_direct_admission"), AI); }
    for (const FNarrativeThreatMemory& Memory : AI->GetThreatDebugSnapshot())
    {
        if (Memory.Target.Get() == Target && Memory.Source == ENarrativeThreatSource::Sight
            && Memory.bDirectObservation && Memory.Confidence > 0.f && Memory.ExpiresAt > AI->GetWorld()->GetTimeSeconds()
            && !Memory.LastKnownPosition.ContainsNaN())
        { Position = Memory.LastKnownPosition; return true; }
    }
    return TraceResult(Trace, false, TEXT("sight.no_unexpired_direct_memory"), AI);
}

bool SameOwner(const FLease& L, const FEnvQueryInstance* Trace = nullptr)
{
    if (!Ready(L.Pawn.Get(), Trace) || L.Pawn->GetWorld()->GetTimeSeconds() - L.CapturedAt > .75
        || !L.AI.IsValid() || L.AI->GetPawn() != L.Pawn.Get()
        || L.Pawn->GetController() != L.AI.Get() || L.AI->GetPawnAssignmentGeneration() != L.PawnGeneration
        || !L.ASC.IsValid() || L.Pawn->GetNarrativeAbilitySystemComponent() != L.ASC.Get()
        || L.ASC->GetCombatActorInfoEpoch() != L.ActorInfoEpoch || !L.Blackboard.IsValid()
        || L.AI->GetBlackboardComponent() != L.Blackboard.Get() || !L.Activity.IsValid() || !L.Goal.IsValid()
        || !L.Target.IsValid() || !L.Encounter.IsValid() || L.Encounter->IsActorBeingDestroyed()
        || L.Encounter->GetEncounterState() != ESovEncounterState::Active || L.Encounter->GetAttemptId() != L.Attempt
        || L.Encounter->GetLifecycleGeneration() != L.EncounterGeneration || L.Encounter->FindParticipantId(L.Pawn.Get()).IsNone()
        || L.Encounter->IsParticipantMassRepresented(L.Encounter->FindParticipantId(L.Pawn.Get()))
        || !L.Link.IsValid() || !L.Link->IsCommandLinkActive() || L.Link->GetLinkInstanceId() != L.LinkInstance)
    { return TraceResult(Trace, false, TEXT("lease.identity_age_encounter_or_link"), L.Pawn.Get()); }
    auto* Activities = L.AI->GetActivityComponent();
    if (!IsValid(Activities) || Activities->GetCurrentActivity() != L.Activity.Get()
        || Activities->GetCurrentActivityGoal() != L.Goal.Get() || Activities->HasStaleRegisteredGoalKey(L.Goal.Get())
        || L.Goal->OwnerController != L.AI.Get()
        || Activities->GetGoals(L.Goal->GetClass()).GoalUniqueObjectMap.FindRef(L.Target.Get()) != L.Goal.Get()
        || L.Blackboard->GetValueAsObject(GetDefault<UArsenalSettings>()->BBKey_AttackTarget) != L.Target.Get()) { return TraceResult(Trace, false, TEXT("lease.activity_goal_or_blackboard"), L.Pawn.Get()); }
    float ActiveTime = 0.f;
    if (!L.Activity->IsActivityActive(ActiveTime)) { return TraceResult(Trace, false, TEXT("lease.activity_inactive"), L.Pawn.Get()); }
    auto* Source = Cast<ASovAurelionSecurityDrone>(L.Link->GetCommandSource());
    if (!Ready(Source, Trace) || L.Encounter->FindParticipantId(Source).IsNone()
        || (L.Pawn.Get() != Source && !L.Link->GetLinkedActors().Contains(L.Pawn.Get()))) { return TraceResult(Trace, false, TEXT("lease.command_source_or_membership"), Source); }
    FVector Current;
    // A moving target can invalidate this small, cached query. The next stock decision gets fresh sight.
    return TraceResult(Trace, ReadSight(L.AI.Get(), L.Target.Get(), Current, Trace) && Current.Equals(L.Observed, 75.f), TEXT("lease.sight_or_observed_target_moved"), L.Pawn.Get());
}

bool Capture(UObject* Owner, FLease& L, const FEnvQueryInstance* Trace = nullptr)
{
    auto* AI = Cast<ANarrativeNPCController>(Owner);
    if (!AI) { if (auto* Pawn = Cast<ASovAurelionSecurityDrone>(Owner)) { AI = Cast<ANarrativeNPCController>(Pawn->GetController()); } }
    auto* Pawn = IsValid(AI) ? Cast<ASovAurelionSecurityDrone>(AI->GetPawn()) : nullptr;
    if (!Ready(Pawn, Trace) || AI->IsThreatMemorySuspended()) { return TraceResult(Trace, false, TEXT("capture.ready_or_suspended"), Pawn); }
    auto* Activities = AI->GetActivityComponent(); auto* BB = AI->GetBlackboardComponent();
    if (!IsValid(Activities) || !IsValid(BB)) { return TraceResult(Trace, false, TEXT("capture.activities_or_blackboard_missing"), Pawn); }
    L.Pawn = Pawn; L.AI = AI; L.ASC = Pawn->GetNarrativeAbilitySystemComponent(); L.Blackboard = BB;
    L.Activity = Activities->GetCurrentActivity(); L.Goal = Activities->GetCurrentActivityGoal();
    L.Target = Cast<AActor>(BB->GetValueAsObject(GetDefault<UArsenalSettings>()->BBKey_AttackTarget));
    L.PawnGeneration = AI->GetPawnAssignmentGeneration(); L.ActorInfoEpoch = L.ASC->GetCombatActorInfoEpoch();
    L.CapturedAt = Pawn->GetWorld()->GetTimeSeconds();
    if (!L.Activity.IsValid() || !L.Goal.IsValid() || !ReadSight(AI, L.Target.Get(), L.Observed, Trace)) { return TraceResult(Trace, false, TEXT("capture.activity_goal_or_sight"), Pawn); }
    for (TActorIterator<ASovEncounterDirector> It(Pawn->GetWorld()); It; ++It)
    {
        if (It->FindParticipantId(Pawn).IsNone()) { continue; }
        if (L.Encounter.IsValid()) { return TraceResult(Trace, false, TEXT("capture.multiple_encounters"), Pawn); }
        L.Encounter = *It;
    }
    if (!L.Encounter.IsValid() || !L.Encounter->GetAttemptId().IsValid()) { return TraceResult(Trace, false, TEXT("capture.encounter_or_attempt_missing"), Pawn); }
    L.Attempt = L.Encounter->GetAttemptId(); L.EncounterGeneration = L.Encounter->GetLifecycleGeneration();
    for (const auto& Participant : L.Encounter->Participants)
    {
        auto* Source = Cast<ASovAurelionSecurityDrone>(L.Encounter->GetParticipant(Participant.ParticipantId));
        auto* Link = IsValid(Source) ? Source->GetFormationLink() : nullptr;
        if (!Ready(Source, Trace) || !IsValid(Link) || !Link->IsCommandLinkActive() || !Link->GetLinkInstanceId().IsValid()
            || Link->GetCommandSource() != Source || (Source != Pawn && !Link->GetLinkedActors().Contains(Pawn))) { continue; }
        if (L.Link.IsValid()) { return TraceResult(Trace, false, TEXT("capture.multiple_command_links"), Pawn); } // Never arbitrate overlapping command owners here.
        L.Link = Link; L.LinkInstance = Link->GetLinkInstanceId();
    }
    if (Trace)
    {
        UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d capture pawn=%s observed=%s encounter=%s attempt=%s link=%s age=%.4f"),
            Trace->QueryID, *GetPathNameSafe(Pawn), *L.Observed.ToString(), *GetPathNameSafe(L.Encounter.Get()),
            *L.Attempt.ToString(), *GetPathNameSafe(L.Link.Get()), Pawn->GetWorld()->GetTimeSeconds() - L.CapturedAt);
    }
    if (!SameOwner(L, Trace)) { return TraceResult(Trace, false, TEXT("capture.owner_lease"), Pawn); }
    // Authored GetGoalKey may reenter. Recheck every native identity after that callback.
    UObject* ActualKey = L.Goal->GetGoalKey();
    return TraceResult(Trace, ActualKey == L.Target.Get() && SameOwner(L, Trace), TEXT("capture.goal_key_or_reentry"), Pawn);
}

bool GetLease(FEnvQueryInstance& Query, const FSovAurelionCrossfireBounds& Bounds, FLease& L)
{
    const FEnvQueryInstance* Trace = &Query;
    FEnvQueryContextData Data;
    if (!Bounds.IsValid() || !Query.PrepareContext(UEnvQueryContext_SovCrossfireLease::StaticClass(), Data)
        || Data.NumValues != 1 || Data.RawData.Num() != sizeof(FLease)) { return TraceResult(Trace, false, TEXT("context.bounds_or_unavailable_lease")); }
    FMemory::Memcpy(&L, Data.RawData.GetData(), sizeof(FLease));
    if (!SameOwner(L, Trace) || L.Encounter->EncounterId != Bounds.EncounterId) { return TraceResult(Trace, false, TEXT("context.owner_or_encounter_id"), L.Pawn.Get()); }
    UObject* ActualKey = L.Goal->GetGoalKey();
    return TraceResult(Trace, ActualKey == L.Target.Get() && SameOwner(L, Trace), TEXT("context.goal_key_or_reentry"), L.Pawn.Get());
}

bool ShootingLanes(const FLease& L, TArray<FVector>& Lanes, const FEnvQueryInstance* Trace = nullptr)
{
    TArray<AActor*> Members = L.Link->GetLinkedActors(); Members.AddUnique(L.Link->GetCommandSource());
    for (AActor* Member : Members)
    {
        auto* Ally = Cast<ASovAurelionSecurityDrone>(Member);
        auto* AI = IsValid(Ally) ? Cast<ANarrativeNPCController>(Ally->GetController()) : nullptr;
        if (Ally == L.Pawn.Get() || !Ready(Ally, Trace) || !IsValid(AI) || AI->GetPawn() != Ally || AI->IsThreatMemorySuspended()
            || L.Encounter->FindParticipantId(Ally).IsNone()
            || L.Encounter->IsParticipantMassRepresented(L.Encounter->FindParticipantId(Ally))
            || UArsenalStatics::GetAttitude(L.Pawn.Get(), Ally) != ETeamAttitude::Friendly) { continue; }
        const auto* BB = AI->GetBlackboardComponent(); FVector Seen;
        if (!BB || BB->GetValueAsObject(GetDefault<UArsenalSettings>()->BBKey_AttackTarget) != L.Target.Get()
            || !ReadSight(AI, L.Target.Get(), Seen, Trace)) { continue; }
        const FVector Direction = (Ally->GetNavAgentLocation() - L.Observed).GetSafeNormal2D();
        if (!Direction.IsNearlyZero()) { Lanes.Add(Direction); }
    }
    return TraceResult(Trace, !Lanes.IsEmpty(), TEXT("lanes.no_current_friendly_direct_sight_lane"), L.Pawn.Get());
}

float NearestLaneAngle(const FVector& Foot, const FVector& Target, const TArray<FVector>& Lanes)
{
    const FVector Direction = (Foot - Target).GetSafeNormal2D(); float Angle = 180.f;
    for (const FVector& Other : Lanes)
    { Angle = FMath::Min(Angle, static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, Other), -1., 1.))))); }
    return Angle;
}

bool ClearPosition(const FLease& L, const FSovAurelionCrossfireBounds& Bounds, const FVector& Foot, const FEnvQueryInstance* Trace = nullptr, int32 Candidate = INDEX_NONE)
{
    auto* Pawn = L.Pawn.Get(); auto* World = Pawn->GetWorld(); auto* Capsule = Pawn->GetCapsuleComponent();
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    const FVector Start = Pawn->GetNavAgentLocation();
    const auto* Data = Nav ? Nav->GetNavDataForProps(Pawn->GetNavAgentPropertiesRef(), Start) : nullptr;
    if (!Data || !Capsule || !Capsule->IsQueryCollisionEnabled()) { return TraceResult(Trace, false, TEXT("candidate.nav_or_capsule_missing"), Pawn, Candidate); }
    const float Radius = Capsule->GetScaledCapsuleRadius(), Height = Capsule->GetScaledCapsuleHalfHeight();
    const float Range = FVector::Dist2D(Foot, L.Observed);
    if (Trace) { UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d candidate=%d point=%s start=%s range=%.2f radius=%.2f half_height=%.2f"),
        Trace->QueryID, Candidate, *Foot.ToString(), *Start.ToString(), Range, Radius, Height); }
    if (!Bounds.Contains(Foot, Radius) || !Bounds.Contains(Start, Radius) || FMath::Abs(Foot.Z - Start.Z) > 80.f
        || Range < MinimumRange || Range > MaximumRange || FVector::Dist2D(Foot, Start) < 180.f) { return TraceResult(Trace, false, TEXT("candidate.bounds_height_range_or_displacement"), Pawn, Candidate); }
    FPathFindingQuery PathQuery(L.AI.Get(), *Data, Start, Foot, Data->GetDefaultQueryFilter()); PathQuery.SetAllowPartialPaths(false);
    const auto Route = Nav->FindPathSync(Pawn->GetNavAgentPropertiesRef(), PathQuery);
    if (!Route.IsSuccessful() || !Route.Path.IsValid() || Route.Path->IsPartial()) { return TraceResult(Trace, false, TEXT("candidate.no_complete_nav_path"), Pawn, Candidate); }
    const auto& Points = Route.Path->GetPathPoints();
    if (Points.Num() < 2 || Points.Num() > 32 || !Points.Last().Location.Equals(Foot, 10.f)) { return TraceResult(Trace, false, TEXT("candidate.path_point_count_or_endpoint"), Pawn, Candidate); }
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, Height);
    const auto PhysicsQuery = Pawn->GetIgnoreCharacterParams();
    const FCollisionResponseParams Responses(Capsule->GetCollisionResponseToChannels());
    FVector Previous = Pawn->GetActorLocation(); float Length = 0.f;
    for (const auto& Point : Points)
    {
        if (Point.CustomNavLinkId.IsValid() || !Bounds.Contains(Point.Location, Radius)) { return TraceResult(Trace, false, TEXT("candidate.offmesh_link_or_path_bounds"), Pawn, Candidate); }
        const FVector Center = Point.Location + FVector(0,0,Height + 2.f);
        Length += FVector::Distance(Previous, Center);
        FHitResult Hit;
        if (Length > MaximumPathLength || World->SweepSingleByChannel(Hit, Previous, Center, Capsule->GetComponentQuat(),
            Capsule->GetCollisionObjectType(), Shape, PhysicsQuery, Responses))
        {
            if (Trace) { UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d candidate=%d path_length=%.2f sweep_from=%s sweep_to=%s hit=%s component=%s start_penetrating=%d"),
                Trace->QueryID, Candidate, Length, *Previous.ToString(), *Center.ToString(),
                *GetPathNameSafe(Hit.GetActor()), *GetPathNameSafe(Hit.GetComponent()), Hit.bStartPenetrating ? 1 : 0); }
            return TraceResult(Trace, false, Length > MaximumPathLength ? TEXT("candidate.path_length") : TEXT("candidate.capsule_sweep"), Pawn, Candidate);
        }
        Previous = Center;
    }
    // Query the actual ground as well as Recast: dynamic geometry may have disappeared since its build.
    FHitResult Ground;
    if (!World->LineTraceSingleByChannel(Ground, Foot + FVector(0,0,30), Foot - FVector(0,0,60), ECC_WorldStatic, PhysicsQuery)
        || Ground.ImpactNormal.Z < Pawn->GetCharacterMovement()->GetWalkableFloorZ())
    {
        if (Trace) { UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d candidate=%d ground_actor=%s normal_z=%.4f blocking=%d"),
            Trace->QueryID, Candidate, *GetPathNameSafe(Ground.GetActor()), Ground.ImpactNormal.Z, Ground.bBlockingHit ? 1 : 0); }
        return TraceResult(Trace, false, TEXT("candidate.ground_missing_or_unwalkable"), Pawn, Candidate);
    }
    FVector Eyes; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eyes, Rotation);
    FCollisionQueryParams ShotQuery = Pawn->GetIgnoreCharacterParams(); ShotQuery.bTraceComplex = true;
    for (const FVector& Origin : {Foot + FVector(0,0,Height), Foot + Eyes - Start})
    {
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, Origin, L.Observed,
            GetDefault<UArsenalSettings>()->WeaponTraceChannel, ShotQuery)) { continue; }
        AActor* Actor = Hit.GetActor();
        if (Actor == L.Target.Get()) { continue; }
        const auto* Character = Cast<ANarrativeCharacter>(L.Target.Get());
        if (Character && Actor == Character->GetCharacterVisual()) { continue; }
        if (Trace) { UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d candidate=%d shot_origin=%s observed=%s blocker=%s component=%s"),
            Trace->QueryID, Candidate, *Origin.ToString(), *L.Observed.ToString(), *GetPathNameSafe(Actor), *GetPathNameSafe(Hit.GetComponent())); }
        return TraceResult(Trace, false, TEXT("candidate.weapon_line_blocked"), Pawn, Candidate);
    }
    return true;
}
}

bool FSovAurelionCrossfireBounds::IsValid() const
{
    return !EncounterId.IsNone() && !Minimum.ContainsNaN() && !Maximum.ContainsNaN()
        && Maximum.X - Minimum.X > 100.f && Maximum.Y - Minimum.Y > 100.f && Maximum.Z > Minimum.Z;
}
bool FSovAurelionCrossfireBounds::Contains(const FVector& Foot, float Radius) const
{
    return IsValid() && !Foot.ContainsNaN() && Foot.X >= Minimum.X + Radius && Foot.X <= Maximum.X - Radius
        && Foot.Y >= Minimum.Y + Radius && Foot.Y <= Maximum.Y - Radius && Foot.Z >= Minimum.Z && Foot.Z <= Maximum.Z;
}
UEnvQueryItemType_SovCrossfireLease::UEnvQueryItemType_SovCrossfireLease(const FObjectInitializer& Initializer) : Super(Initializer)
{ ValueSize = sizeof(FLease); }
UEnvQueryContext_SovCrossfireLease::UEnvQueryContext_SovCrossfireLease(const FObjectInitializer& Initializer) : Super(Initializer) {}
void UEnvQueryContext_SovCrossfireLease::ProvideContext(FEnvQueryInstance& Query, FEnvQueryContextData& Data) const
{
    FLease L;
    if (!Capture(Query.Owner.Get(), L, &Query)) { return; }
    Data.ValueType = UEnvQueryItemType_SovCrossfireLease::StaticClass(); Data.NumValues = 1;
    Data.RawData.SetNumUninitialized(sizeof(FLease)); FMemory::Memcpy(Data.RawData.GetData(), &L, sizeof(FLease));
}
UEnvQueryGenerator_SovCrossfire::UEnvQueryGenerator_SovCrossfire(const FObjectInitializer& Initializer) : Super(Initializer)
{ ItemType = UEnvQueryItemType_Point::StaticClass(); }
void UEnvQueryGenerator_SovCrossfire::GenerateItems(FEnvQueryInstance& Query) const
{
    const FEnvQueryInstance* Trace = &Query;
    FLease L;
    UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d owner=%s stage=generator bounds_min=%s bounds_max=%s encounter=%s"),
        Query.QueryID, *GetPathNameSafe(Query.Owner.Get()), *Bounds.Minimum.ToString(), *Bounds.Maximum.ToString(), *Bounds.EncounterId.ToString());
    if (!GetLease(Query, Bounds, L)) { return; }
    TArray<FVector> Lanes;
    if (!ShootingLanes(L, Lanes, Trace)) { return; }
    auto* Pawn = L.Pawn.Get(); const FVector Start = Pawn->GetNavAgentLocation();
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Pawn->GetWorld());
    const auto* Data = Nav ? Nav->GetNavDataForProps(Pawn->GetNavAgentPropertiesRef(), Start) : nullptr;
    if (!Data || !Bounds.Contains(Start, Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius()))
    {
        UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d start=%s nav_present=%d"), Query.QueryID, *Start.ToString(), Data ? 1 : 0);
        TraceResult(Trace, false, TEXT("generator.nav_missing_or_start_bounds"), Pawn); return;
    }
    const float Range = FVector::Dist2D(Start, L.Observed);
    UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d start=%s observed=%s range=%.2f lane_count=%d"),
        Query.QueryID, *Start.ToString(), *L.Observed.ToString(), Range, Lanes.Num());
    if (Range < MinimumRange || Range > MaximumRange) { TraceResult(Trace, false, TEXT("generator.range"), Pawn); return; }
    const FVector Radial = (Start - L.Observed).GetSafeNormal2D();
    TArray<FNavLocation> Candidates;
    // Keep the current-radius tier, then seek the near side of intervening cover.
    // Six metres is half the authored E1 low-cover band spacing; it changes only
    // proposal locations. Every original range/path/capsule/shot/angle/lease gate remains.
    // Do not clamp a sub-minimum proposal back into combat range or expand the budget.
    for (float ExtraRange : {0.f, -600.f})
    {
        if (Range + ExtraRange < MinimumRange) { continue; }
        for (float Angle : {-30.f, -15.f, 15.f, 30.f})
        {
            FVector Desired = L.Observed + Radial.RotateAngleAxis(Angle, FVector::UpVector) * FMath::Min(Range + ExtraRange, MaximumRange);
            Desired.Z = Start.Z; FNavLocation Projected;
            if (Nav->ProjectPointToNavigation(Desired, Projected, FVector(60,60,100), Data)
                && FVector::Dist2D(Desired, Projected.Location) <= 60.f
                && Bounds.Contains(Projected.Location, Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius()))
            {
                UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d desired=%s projected=%s projected_index=%d"), Query.QueryID, *Desired.ToString(), *Projected.Location.ToString(), Candidates.Num());
                Candidates.Add(Projected);
            }
            else
            {
                UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d reject=generator.projection_distance_or_bounds desired=%s angle=%.1f extra_range=%.1f"),
                    Query.QueryID, *Desired.ToString(), Angle, ExtraRange);
            }
        }
    }
    UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d projected_count=%d"), Query.QueryID, Candidates.Num());
    if (!SameOwner(L, Trace)) { TraceResult(Trace, false, TEXT("generator.final_owner"), Pawn); return; }
    for (const auto& Candidate : Candidates) { Query.AddItemData<UEnvQueryItemType_Point>(Candidate); }
}
UEnvQueryTest_SovCrossfire::UEnvQueryTest_SovCrossfire(const FObjectInitializer& Initializer) : Super(Initializer)
{
    Cost = EEnvTestCost::High; ValidItemType = UEnvQueryItemType_Point::StaticClass(); SetWorkOnFloatValues(true);
    TestPurpose = EEnvTestPurpose::FilterAndScore; FilterType = EEnvTestFilterType::Minimum;
    FloatValueMin.DefaultValue = 10.f; ScoringFactor.DefaultValue = 1.f;
}
void UEnvQueryTest_SovCrossfire::RunTest(FEnvQueryInstance& Query) const
{
    const FEnvQueryInstance* Trace = &Query;
    UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d owner=%s stage=test item_count=%d"), Query.QueryID, *GetPathNameSafe(Query.Owner.Get()), Query.Items.Num());
    FLease L; TArray<FVector> Lanes; TArray<float> Scores; Scores.Init(-1.f, Query.Items.Num());
    bool bOwns = Query.Items.Num() <= MaximumCandidates && GetLease(Query, Bounds, L) && ShootingLanes(L, Lanes, Trace);
    if (bOwns)
    {
        const float Current = NearestLaneAngle(L.Pawn->GetNavAgentLocation(), L.Observed, Lanes);
        for (int32 Index = 0; Index < Query.Items.Num(); ++Index)
        {
            if (!Query.Items[Index].IsValid()) { continue; }
            const FVector Point = GetItemLocation(Query, Index);
            const float Angle = NearestLaneAngle(Point, L.Observed, Lanes);
            // Stop circling once spacing is useful. More than 90 degrees is not rewarded.
            const float Gain = FMath::Min(Angle, 90.f) - FMath::Min(Current, 90.f);
            if (Angle >= 20.f && Gain >= 10.f && ClearPosition(L, Bounds, Point, Trace, Index)) { Scores[Index] = Gain; }
            UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d candidate=%d point=%s current_angle=%.3f candidate_angle=%.3f gain=%.3f preliminary_score=%.3f result=%s"),
                Query.QueryID, Index, *Point.ToString(), Current, Angle, Gain, Scores[Index],
                Scores[Index] >= 0.f ? TEXT("accepted_pending_final_lease") : Angle < 20.f ? TEXT("lane_angle") : Gain < 10.f ? TEXT("lane_gain") : TEXT("clear_position"));
        }
        bOwns = SameOwner(L, Trace);
        if (bOwns)
        {
            UObject* ActualKey = L.Goal->GetGoalKey();
            bOwns = ActualKey == L.Target.Get() && SameOwner(L, Trace);
        }
    }
    UE_LOG(LogSovCrossfireQuery, VeryVerbose, TEXT("Crossfire id=%d stage=commit item_count=%d owns=%d"), Query.QueryID, Query.Items.Num(), bOwns ? 1 : 0);
    // At most eight candidates were evaluated above. Commit all scores together so
    // a sliced query cannot mix pre-sever acceptance with post-sever refusal.
    for (FEnvQueryInstance::ItemIterator It(this, Query); It.IgnoreTimeLimit(); ++It)
    {
        if (!bOwns || Scores[It.GetIndex()] < 0.f) { It.ForceItemState(EEnvItemStatus::Failed); }
        else { It.SetScore(EEnvTestPurpose::FilterAndScore, EEnvTestFilterType::Minimum, Scores[It.GetIndex()], 10.f, 180.f); }
    }
}
