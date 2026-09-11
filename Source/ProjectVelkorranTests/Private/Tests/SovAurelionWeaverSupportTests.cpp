// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAurelionWeaverSupportTestFixtures.h"
#include "Tests/SovBotAttackTestFixtures.h"
#include "AI/SovAurelionRoleActivities.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NavigationSystemBase.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "World/SovAurelionNavigationSystem.h"

#if WITH_AUTOMATION_TESTS
struct FSovAurelionWeaverSupportTestAccess
{
    static bool Find(UBTTask_SovAurelionWeaverSupport& Task, ASovAurelionWeaver* NPC, FVector& Goal, AActor*& Threat)
    { return Task.FindRetreat(NPC, Goal, Threat); }
    static EBTNodeResult::Type Start(UBTTask_SovAurelionWeaverSupport& Task, UBehaviorTreeComponent& Tree)
    { return Task.ExecuteTask(Tree, nullptr); }
    static bool Owns(const UBTTask_SovAurelionWeaverSupport& Task) { return Task.OwnsSupport(); }
    static void Release(UBTTask_SovAurelionWeaverSupport& Task) { Task.ReleaseMove(); }
    static FAIRequestID Id(const UBTTask_SovAurelionWeaverSupport& Task) { return Task.MoveId; }
    static void NextDecision(UBTTask_SovAurelionWeaverSupport& Task) { Task.NextDecisionAt = 0.; }
};
namespace
{
struct FSupportWorld
{
    FEditorScriptExecutionGuard ScriptGuard;
    UWorld* World = nullptr;
    USovAurelionNavigationSystem* Nav = nullptr;
    ASovAurelionSupportTestWeaver* Weaver = nullptr;
    ASovAurelionTestWallRunner* Ally = nullptr;
    ASovBotTestCharacter* Threat = nullptr;
    ANarrativeNPCController* Controller = nullptr;
    UAIPerceptionComponent* Sight = nullptr;
    UBehaviorTreeComponent* Tree = nullptr;
    UBTTask_SovAurelionWeaverSupport* Task = nullptr;
    UBoxComponent* Floor = nullptr;
    FSupportWorld()
    {
        const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!World) { return; }
        if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Weaver = World->SpawnActor<ASovAurelionSupportTestWeaver>(FVector(0,0,62), FRotator::ZeroRotator, Spawn);
        Ally = World->SpawnActor<ASovAurelionTestWallRunner>(FVector(0,350,62), FRotator::ZeroRotator, Spawn);
        Threat = World->SpawnActor<ASovBotTestCharacter>(FVector(350,0,62), FRotator::ZeroRotator, Spawn);
        Controller = World->SpawnActor<ANarrativeNPCController>();
        if (!Weaver || !Ally || !Threat || !Controller) { return; }
        Weaver->InitializeTestRole(); Ally->InitializeTestRole(); Threat->InitializeTestCombat(1);
        Weaver->Hostile = Threat;
        Weaver->GetCapsuleComponent()->SetCanEverAffectNavigation(false);
        Ally->GetCapsuleComponent()->SetCanEverAffectNavigation(false);
        Threat->GetCapsuleComponent()->SetCanEverAffectNavigation(false);
        Controller->Possess(Weaver); Controller->bShareThreatsWithFaction = false;
        Sight = NewObject<UAIPerceptionComponent>(Controller); Controller->AddInstanceComponent(Sight);
        auto* Config = NewObject<UAISenseConfig_Sight>(Sight);
        Config->SightRadius = 1500.f; Config->LoseSightRadius = 1600.f; Config->DetectionByAffiliation.bDetectEnemies = true;
        Sight->ConfigureSense(*Config); Controller->SetPerceptionComponent(*Sight); Sight->RegisterComponent();
        Controller->RefreshThreatMemory();
        Weaver->GetAnchorA()->ConfigureLinkId(TEXT("Test.Support.A"));
        Weaver->GetAnchorB()->ConfigureLinkId(TEXT("Test.Support.B"));
        Weaver->GetAnchorA()->RegisterLinkedActor(Ally); Weaver->GetAnchorB()->RegisterLinkedActor(Ally);
        Weaver->InitializeFreshLinks();
        Tree = NewObject<UBehaviorTreeComponent>(Controller); Controller->AddInstanceComponent(Tree); Tree->RegisterComponent();
        Tree->InitializeComponent();
        Task = NewObject<UBTTask_SovAurelionWeaverSupport>(Tree);
        Floor = Box(FVector(0,0,-30), FVector(2200,2200,30), true);
    }
    ~FSupportWorld()
    {
        if (Task) { FSovAurelionWeaverSupportTestAccess::Release(*Task); }
        if (Nav) { Nav->CancelBuild(); }
        if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
    }
    UBoxComponent* Box(const FVector& Position, const FVector& Extent, bool bNavigation)
    {
        auto* Owner = World->SpawnActor<AActor>();
        auto* Component = NewObject<UBoxComponent>(Owner); Owner->AddInstanceComponent(Component); Owner->SetRootComponent(Component);
        Component->SetMobility(EComponentMobility::Static); Component->SetBoxExtent(Extent, false);
        Component->SetCollisionProfileName(TEXT("BlockAll")); Component->SetCanEverAffectNavigation(bNavigation);
        Component->SetWorldLocation(Position); Component->RegisterComponent(); return Component;
    }
    bool BuildNav()
    {
        auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
        auto* Volume = NewObject<UBoxComponent>(Bounds); Bounds->AddInstanceComponent(Volume);
        Volume->SetupAttachment(Bounds->GetRootComponent()); Volume->SetBoxExtent(FVector(2400,2400,600));
        Volume->SetCollisionProfileName(TEXT("NoCollision")); Volume->SetCanEverAffectNavigation(false); Volume->RegisterComponent();
        Bounds->SupportedAgents.Empty(); Bounds->SupportedAgents.Set(0); Bounds->SupportedAgents.MarkInitialized();
        auto* Property = FindFProperty<FObjectPropertyBase>(AWorldSettings::StaticClass(), TEXT("NavigationSystemConfig"));
        if (!Property) { return false; }
        Property->SetObjectPropertyValue_InContainer(World->GetWorldSettings(), NewObject<USovAurelionNavigationConfig>(World->GetWorldSettings()));
        FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode, nullptr, true);
        Nav = Cast<USovAurelionNavigationSystem>(World->GetNavigationSystem());
        if (!Nav) { return false; }
        Nav->Build();
        return Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr;
    }
    bool Valid() const { return World && Weaver && Ally && Threat && Controller && Sight && Tree && Task && Weaver->HasActiveSupportLink(); }
    void Observe(bool bSeen = true)
    {
        FAIStimulus Stimulus(*GetDefault<UAISense_Sight>(), 1.f, Threat->GetActorLocation(), Weaver->GetActorLocation());
        if (!bSeen) { Stimulus.MarkNoLongerSensed(); }
        Sight->RegisterStimulus(Threat, Stimulus); Sight->ProcessStimuli();
        // Sight notifies on perception changes, not every successful update. This
        // unticked fixture must run the same refresh used by the native controller tick.
        Controller->RefreshThreatMemory();
    }
    bool Find(FVector& Goal, AActor*& Target) { return FSovAurelionWeaverSupportTestAccess::Find(*Task, Weaver, Goal, Target); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeaverRetreatGroundsTest,
    "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WeaverRetreatUsesSightCompleteNavAndRealCapsule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovWeaverRetreatGroundsTest::RunTest(const FString& Parameters)
{
    FSupportWorld F;
    if (!TestTrue(TEXT("Native Weaver/links/controller/perception"), F.Valid())) { return false; }
    FVector Goal; AActor* Target = nullptr;
    F.Observe();
    TestFalse(TEXT("Sight does not invent a route without navigation"), F.Find(Goal, Target));
    if (!TestTrue(TEXT("Real Aurelion Recast builds a supported floor"), F.BuildNav())) { return false; }
    TestNull(TEXT("Goal-free support does not need weapon focus"), F.Controller->GetFocusActor());
    if (!TestTrue(TEXT("Current native sight chooses a complete retreat"), F.Find(Goal, Target))) { return false; }
    TestEqual(TEXT("Selected actor is the actually perceived hostile"), Target, static_cast<AActor*>(F.Threat));
    TestTrue(TEXT("Retreat increases spacing to about nine meters"), FVector::Dist2D(Goal, F.Threat->GetActorLocation()) >= 840.f);
    const FVector SeenGoal = Goal;
    F.Threat->SetActorLocation(FVector(1000,0,62));
    TestTrue(TEXT("A retained sight record uses its observed position, not unreported live coordinates"), F.Find(Goal, Target) && Goal.Equals(SeenGoal, .1f));
    F.Observe();
    const auto UpdatedSight = F.Controller->GetThreatDebugSnapshot();
    const auto* DistantSight = UpdatedSight.FindByPredicate([&F](const FNarrativeThreatMemory& Memory)
    { return Memory.Target.Get() == F.Threat && Memory.Source == ENarrativeThreatSource::Sight; });
    TestTrue(TEXT("Native perception refresh records the actual distant sight position"), DistantSight
        && DistantSight->bDirectObservation && DistantSight->LastKnownPosition.Equals(FVector(1000,0,62), .1f));
    TestFalse(TEXT("A distant observed enemy causes no retreat"), F.Find(Goal, Target));
    F.Threat->SetActorLocation(FVector(350,0,62)); F.Observe();
    const FVector AllyStart = F.Ally->GetActorLocation(); F.Ally->SetActorLocation(FVector(4000,0,62));
    TestFalse(TEXT("No retreat can abandon its live support cohort beyond eighteen meters"), F.Find(Goal, Target));
    F.Ally->SetActorLocation(AllyStart);
    // These real colliders intentionally do not alter the already-built navmesh. A path-only implementation would pass.
    F.Box(FVector(100,0,150), FVector(15,130,150), false);
    F.Box(FVector(-100,0,150), FVector(15,130,150), false);
    F.Box(FVector(0,100,150), FVector(130,15,150), false);
    F.Box(FVector(0,-100,150), FVector(130,15,150), false);
    TestFalse(TEXT("All nav-reachable exits with physically blocked capsule clearance are rejected"), F.Find(Goal, Target));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeaverRetreatOwnershipTest,
    "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WeaverRetreatRetiresOnlyItsMoveAndSeveredSupport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovWeaverRetreatOwnershipTest::RunTest(const FString& Parameters)
{
    FSupportWorld F;
    if (!TestTrue(TEXT("Native support fixture and real navigation"), F.Valid() && F.BuildNav())) { return false; }
    F.Observe();
    const auto Begin = [&]() { FSovAurelionWeaverSupportTestAccess::NextDecision(*F.Task); return FSovAurelionWeaverSupportTestAccess::Start(*F.Task, *F.Tree); };
    auto* ASC = F.Weaver->GetNarrativeAbilitySystemComponent();
    const auto Busy = FNarrativeGameplayTags::Get().State_Busy;
    const float Health = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
    if (!TestTrue(TEXT("Existing support task starts actual native path following"), Begin() == EBTNodeResult::InProgress)) { return false; }
    TestTrue(TEXT("Task owns the exact live request"), FSovAurelionWeaverSupportTestAccess::Owns(*F.Task)
        && F.Controller->GetPathFollowingComponent()->GetCurrentRequestId() == FSovAurelionWeaverSupportTestAccess::Id(*F.Task));
    F.Observe(false);
    TestFalse(TEXT("Actual lost sight retires pursuit despite retained memory"), FSovAurelionWeaverSupportTestAccess::Owns(*F.Task));
    FSovAurelionWeaverSupportTestAccess::Release(*F.Task);
    TestTrue(TEXT("Owned abort stops only its native path"), F.Controller->GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Idle);
    F.Observe();
    if (!TestTrue(TEXT("Fresh sight can begin again"), Begin() == EBTNodeResult::InProgress)) { return false; }
    FAIMoveRequest Replacement(FVector(0,-600,0)); Replacement.SetAllowPartialPath(false);
    const auto NewMove = F.Controller->MoveTo(Replacement);
    if (!TestTrue(TEXT("Another owner obtains a different real request"), NewMove.Code == EPathFollowingRequestResult::RequestSuccessful
        && NewMove.MoveId != FSovAurelionWeaverSupportTestAccess::Id(*F.Task))) { return false; }
    FSovAurelionWeaverSupportTestAccess::Release(*F.Task);
    TestTrue(TEXT("Old task cleanup preserves the successor's movement"), F.Controller->GetPathFollowingComponent()->GetCurrentRequestId() == NewMove.MoveId
        && F.Controller->GetPathFollowingComponent()->GetStatus() != EPathFollowingStatus::Idle);
    F.Controller->StopMovement();
    if (!TestTrue(TEXT("Task can resume after the other move ends"), Begin() == EBTNodeResult::InProgress)) { return false; }
    ASC->AddLooseGameplayTag(Busy);
    TestFalse(TEXT("Independent Busy ownership cancels support movement"), FSovAurelionWeaverSupportTestAccess::Owns(*F.Task));
    FSovAurelionWeaverSupportTestAccess::Release(*F.Task);
    TestEqual(TEXT("Cleanup preserves the other owner's Busy contribution"), ASC->GetTagCount(Busy), 1);
    ASC->RemoveLooseGameplayTag(Busy);
    if (!TestTrue(TEXT("Ready support resumes"), Begin() == EBTNodeResult::InProgress)) { return false; }
    FSovCommandLinkSeverResult Result;
    TestTrue(TEXT("Actual hostile sever succeeds"), F.Weaver->GetAnchorA()->TrySeverCommandLink(F.Threat, Result) == ESovCommandLinkSeverResolution::NewlySevered);
    TestFalse(TEXT("A changed anchor retires the captured support lease"), FSovAurelionWeaverSupportTestAccess::Owns(*F.Task));
    FSovAurelionWeaverSupportTestAccess::Release(*F.Task);
    F.Weaver->GetAnchorB()->TrySeverCommandLink(F.Threat, Result);
    TestTrue(TEXT("Both severed returns the task to existing attack/idle fallback"), Begin() == EBTNodeResult::Failed);
    TestTrue(TEXT("No severed anchor is revived"), F.Weaver->GetAnchorA()->GetCommandLinkState() == ESovCommandLinkState::Severed
        && F.Weaver->GetAnchorB()->GetCommandLinkState() == ESovCommandLinkState::Severed);
    TestEqual(TEXT("Tactics never heal or change combat resources"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), Health);
    TestNull(TEXT("Tactics never take Narrative focus ownership"), F.Controller->GetFocusActor());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeaverRetreatDisconnectedTest,
    "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WeaverRetreatRejectsDisconnectedNavigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovWeaverRetreatDisconnectedTest::RunTest(const FString& Parameters)
{
    FSupportWorld F;
    if (!TestTrue(TEXT("Native support fixture"), F.Valid())) { return false; }
    F.Floor->SetBoxExtent(FVector(110,110,30), false);
    FNavigationSystem::UpdateComponentData(*F.Floor);
    for (const float Angle : {0.f, 40.f, -40.f, 70.f, -70.f})
    {
        FVector Point = FVector(350,0,0) + FVector(-900,0,0).RotateAngleAxis(Angle, FVector::UpVector);
        Point.Z = -30;
        F.Box(Point, FVector(110,110,30), true);
    }
    if (!TestTrue(TEXT("Separate physical islands build real Recast tiles"), F.BuildNav())) { return false; }
    FNavLocation Projected;
    TestTrue(TEXT("Retreat endpoint itself is navigable"), F.Nav->ProjectPointToNavigation(FVector(-550,0,0), Projected, FVector(60,60,100)));
    F.Observe(); FVector Goal; AActor* Target = nullptr;
    TestFalse(TEXT("Navigable endpoint with only a partial/disconnected route never starts a move"), F.Find(Goal, Target));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeaverRetreatPerceptionAdmissionTest,
    "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WeaverRetreatRespectsPerceptionAndEncounterHolds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovWeaverRetreatPerceptionAdmissionTest::RunTest(const FString& Parameters)
{
    FSupportWorld F;
    if (!TestTrue(TEXT("Native support fixture and navigation"), F.Valid() && F.BuildNav())) { return false; }
    FVector Goal; AActor* Target = nullptr;
    F.Controller->ReportThreatObservation(F.Threat, ENarrativeThreatSource::Hearing, F.Threat->GetActorLocation(), 1.f, .5f, 6.f);
    TestFalse(TEXT("Hearing alone is not visual pursuit"), F.Find(Goal, Target));
    F.Observe();
    const auto Cloak = FNarrativeGameplayTags::Get().State_InvisibleToEnemies;
    F.Threat->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(Cloak);
    TestFalse(TEXT("Cloaked target cannot authorize a retreat search"), F.Find(Goal, Target));
    F.Threat->GetNarrativeAbilitySystemComponent()->RemoveLooseGameplayTag(Cloak); F.Observe();
    auto* Director = F.World->SpawnActor<ASovAurelionTraversalTestDirector>();
    if (!TestTrue(TEXT("Actual director registers the Weaver"), Director && Director->RegisterParticipant(TEXT("Weaver"), F.Weaver))) { return false; }
    TestFalse(TEXT("Pre-entry actor readiness cannot bypass the inactive director"), F.Find(Goal, Target));
    const FGuid Attempt = FGuid::NewGuid(); Director->SetAdmissionState(ESovEncounterState::Active, Attempt);
    TestTrue(TEXT("Current encounter admits support retreat"), F.Find(Goal, Target));
    F.Controller->SetThreatMemorySuspended(Director, true);
    TestFalse(TEXT("Native retry/hold suspension forbids new perception-driven movement"), F.Find(Goal, Target));
    F.Controller->SetThreatMemorySuspended(Director, false); F.Observe();
    auto* Duplicate = F.World->SpawnActor<ASovAurelionTraversalTestDirector>();
    Duplicate->Participants = Director->Participants; Duplicate->SetAdmissionState(ESovEncounterState::Active, Attempt);
    TestFalse(TEXT("Duplicate registered owners cannot authorize a retreat"), F.Find(Goal, Target));
    return true;
}
#endif
