// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAurelionCrossfireTestFixtures.h"
#include "Tests/SovAurelionEnemyRoleTestFixtures.h"
#include "Tests/SovBotAttackTestFixtures.h"
#include "Tests/SovNPCGoalKeyLifetimeTestFixtures.h"
#include "AI/SovAurelionCrossfireQuery.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NavigationSystemBase.h"
#include "ArsenalSettings.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryOption.h"
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
#include "UObject/Script.h"
#include "UObject/UnrealType.h"
#include "World/SovAurelionNavigationSystem.h"

void ASovAurelionCrossfireTestDrone::InitializeTestRole()
{
    auto* ASC = GetNarrativeAbilitySystemComponent(); ASC->AddAttributeSetSubobject(GetAttributeSetBase());
    ASC->InitAbilityActorInfo(this, this);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
    GetCapsuleComponent()->SetCapsuleSize(30.f,60.f); GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn); GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
    GetCapsuleComponent()->SetCanEverAffectNavigation(false); GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    bEncounterSnapshotReady = true;
}

#if WITH_AUTOMATION_TESTS
namespace
{
struct FCrossfireWorld
{
    FEditorScriptExecutionGuard ScriptGuard;
    UWorld* World = nullptr; USovAurelionNavigationSystem* Nav = nullptr;
    ASovAurelionCrossfireTestDrone* Drone = nullptr; ASovAurelionCrossfireTestDrone* Commander = nullptr;
    ASovBotTestCharacter* Target = nullptr; ASovAurelionTraversalTestDirector* Director = nullptr;
    ANarrativeNPCController* AI = nullptr; ANarrativeNPCController* AllyAI = nullptr;
    USovNPCGoalKeyLifetimeTestGoal* Goal = nullptr; UEnvQuery* Query = nullptr;
    UBoxComponent* Floor = nullptr; UEnvQueryManager* Manager = nullptr;
    FSovAurelionCrossfireBounds Bounds;
    FCrossfireWorld()
    {
        const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!World) { return; }
        if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        World->InitializeActorsForPlay(FURL());
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Drone = World->SpawnActor<ASovAurelionCrossfireTestDrone>(FVector(-1200,0,62), FRotator::ZeroRotator, Spawn);
        Commander = World->SpawnActor<ASovAurelionCrossfireTestDrone>(FVector(-1300,180,62), FRotator::ZeroRotator, Spawn);
        Target = World->SpawnActor<ASovBotTestCharacter>(FVector(0,0,62), FRotator::ZeroRotator, Spawn);
        Director = World->SpawnActor<ASovAurelionTraversalTestDirector>();
        if (!Drone || !Commander || !Target || !Director) { return; }
        Drone->InitializeTestRole(); Commander->InitializeTestRole(); Target->InitializeTestCombat(1);
        Drone->Hostile = Target; Commander->Hostile = Target; Target->GetCapsuleComponent()->SetCanEverAffectNavigation(false);
        Target->GetCapsuleComponent()->SetCollisionResponseToChannel(GetDefault<UArsenalSettings>()->WeaponTraceChannel, ECR_Block);
        AI = Controller(Drone); AllyAI = Controller(Commander);
        if (!AI || !AllyAI) { return; }
        Goal = SelectGoal(AI); if (!Goal || !SelectGoal(AllyAI)) { return; }
        Director->EncounterId = TEXT("Test.Crossfire");
        FSovEncounterParticipant A; A.ParticipantId = TEXT("Drone"); A.Character = Drone; Director->Participants.Add(A);
        A.ParticipantId = TEXT("Commander"); A.Character = Commander; Director->Participants.Add(A);
        Director->SetAdmissionState(ESovEncounterState::Active, FGuid::NewGuid());
        Commander->GetFormationLink()->ConfigureLinkId(TEXT("Test.Formation"));
        Commander->GetFormationLink()->RegisterLinkedActor(Drone); Commander->GetFormationLink()->InitializeFreshLink();
        Bounds.EncounterId = Director->EncounterId; Bounds.Minimum = FVector(-2100,-2100,-25); Bounds.Maximum = FVector(2100,2100,225);
        Floor = Box(FVector(0,0,-30), FVector(2200,2200,30), true);
        Manager = UEnvQueryManager::GetCurrent(World);
        Query = NewObject<UEnvQuery>(World);
        auto* Option = NewObject<UEnvQueryOption>(Query);
        auto* Generator = NewObject<UEnvQueryGenerator_SovCrossfire>(Option); Generator->Bounds = Bounds;
        auto* Test = NewObject<UEnvQueryTest_SovCrossfire>(Option); Test->Bounds = Bounds;
        Option->Generator = Generator; Option->Tests.Add(Test); Query->GetOptionsMutable().Add(Option);
    }
    ~FCrossfireWorld()
    {
        for (auto* Controller : {AI, AllyAI})
        { if (IsValid(Controller)) { Controller->GetActivityComponent()->StopCurrentActivity(); Controller->GetActivityComponent()->RemoveAllGoals(); } }
        if (Nav) { Nav->CancelBuild(); }
        if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
    }
    ANarrativeNPCController* Controller(ASovAurelionCrossfireTestDrone* Pawn)
    {
        auto* C = World->SpawnActor<ANarrativeNPCController>(); if (!C) { return nullptr; }
        C->Possess(Pawn); C->bShareThreatsWithFaction = false; C->DispatchBeginPlay();
        auto* Data = NewObject<UBlackboardData>(C);
        FBlackboardEntry Entry; Entry.EntryName = GetDefault<UArsenalSettings>()->BBKey_AttackTarget;
        Entry.KeyType = NewObject<UBlackboardKeyType_Object>(Data); Data->Keys.Add(Entry);
        Entry.EntryName = GetDefault<UArsenalSettings>()->BBKey_TargetLocation; Entry.KeyType = NewObject<UBlackboardKeyType_Vector>(Data); Data->Keys.Add(Entry);
        UBlackboardComponent* BB = nullptr; if (!C->UseBlackboard(Data, BB)) { return nullptr; }
        auto* Sight = NewObject<UAIPerceptionComponent>(C); C->AddInstanceComponent(Sight);
        auto* Config = NewObject<UAISenseConfig_Sight>(Sight); Config->SightRadius = 3000.f; Config->LoseSightRadius = 3500.f;
        Config->DetectionByAffiliation.bDetectEnemies = true; Sight->ConfigureSense(*Config); C->SetPerceptionComponent(*Sight); Sight->RegisterComponent();
        C->RefreshThreatMemory(); Observe(C, true); BB->SetValueAsObject(GetDefault<UArsenalSettings>()->BBKey_AttackTarget, Target);
        return C;
    }
    USovNPCGoalKeyLifetimeTestGoal* SelectGoal(ANarrativeNPCController* C)
    {
        auto* Activities = C->GetActivityComponent();
        auto* Activity = Cast<USovNPCGoalKeyLifetimeTestActivity>(Activities->AddActivity(USovNPCGoalKeyLifetimeTestActivity::StaticClass(), false));
        if (!Activity) { return nullptr; }
        Activity->Support(USovNPCGoalKeyLifetimeTestGoal::StaticClass());
        auto* NewGoal = NewObject<USovNPCGoalKeyLifetimeTestGoal>(Activities); NewGoal->GoalKey = Target; NewGoal->DefaultScore = 1.f;
        return Activities->AddGoal(NewGoal, true) == NewGoal && Activities->GetCurrentActivity() == Activity
            && Activities->GetCurrentActivityGoal() == NewGoal ? NewGoal : nullptr;
    }
    void Observe(ANarrativeNPCController* C, bool Seen)
    {
        FAIStimulus Stimulus(*GetDefault<UAISense_Sight>(), 1.f, Target->GetActorLocation(), C->GetPawn()->GetActorLocation());
        if (!Seen) { Stimulus.MarkNoLongerSensed(); }
        C->GetPerceptionComponent()->RegisterStimulus(Target, Stimulus); C->GetPerceptionComponent()->ProcessStimuli(); C->RefreshThreatMemory();
    }
    UBoxComponent* Box(const FVector& Location, const FVector& Extent, bool Navigation)
    {
        auto* A = World->SpawnActor<AActor>(); auto* C = NewObject<UBoxComponent>(A); A->AddInstanceComponent(C); A->SetRootComponent(C);
        C->SetMobility(EComponentMobility::Static); C->SetBoxExtent(Extent, false); C->SetCollisionProfileName(TEXT("BlockAll"));
        C->SetCanEverAffectNavigation(Navigation); C->SetWorldLocation(Location); C->RegisterComponent(); return C;
    }
    bool BuildNav()
    {
        if (!Nav)
        {
            auto* Volume = World->SpawnActor<ANavMeshBoundsVolume>(); auto* Box = NewObject<UBoxComponent>(Volume); Volume->AddInstanceComponent(Box);
            Box->SetupAttachment(Volume->GetRootComponent()); Box->SetBoxExtent(FVector(2400,2400,600)); Box->SetCollisionProfileName(TEXT("NoCollision"));
            Box->SetCanEverAffectNavigation(false); Box->RegisterComponent(); Volume->SupportedAgents.Empty(); Volume->SupportedAgents.Set(0); Volume->SupportedAgents.MarkInitialized();
            auto* Property = FindFProperty<FObjectPropertyBase>(AWorldSettings::StaticClass(), TEXT("NavigationSystemConfig"));
            if (!Property) { return false; }
            Property->SetObjectPropertyValue_InContainer(World->GetWorldSettings(), NewObject<USovAurelionNavigationConfig>(World->GetWorldSettings()));
            FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode, nullptr, true);
            Nav = Cast<USovAurelionNavigationSystem>(World->GetNavigationSystem());
        }
        if (!Nav) { return false; } Nav->Build(); return Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr;
    }
    bool Valid() const { return World && Drone && Commander && AI && AllyAI && Goal && Query && Manager && Commander->GetFormationLink()->IsCommandLinkActive(); }
    TSharedPtr<FEnvQueryResult> Run() { return Manager->RunInstantQuery(FEnvQueryRequest(Query, AI), EEnvQueryRunMode::AllMatching); }
    static bool Found(const TSharedPtr<FEnvQueryResult>& R) { return R.IsValid() && R->IsSuccessful() && !R->Items.IsEmpty(); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCrossfireGroundsTest, "ProjectVelkorran.Campaign.Aurelion.Crossfire.RealNavCapsuleAndWeaponOcclusion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCrossfireGroundsTest::RunTest(const FString& Parameters)
{
    FCrossfireWorld F;
    if (!TestTrue(TEXT("Real registered goals, direct sight, active formation and native EQS"), F.Valid())) { return false; }
    TestFalse(TEXT("Sight cannot manufacture navigation"), F.Found(F.Run()));
    if (!TestTrue(TEXT("Actual Recast supports the floor"), F.BuildNav())) { return false; }
    auto Result = F.Run();
    if (!TestTrue(TEXT("Native EQS finds linked clear lateral positions"), F.Found(Result))) { return false; }
    TestTrue(TEXT("The complete query stays bounded to eight samples"), Result->Items.Num() <= 8);
    for (int32 I = 0; I < Result->Items.Num(); ++I)
    {
        const auto Point = Result->GetItemAsLocation(I);
        TestTrue(TEXT("Result remains on the authored combat floor with capsule margin"), F.Bounds.Contains(Point,30.f));
        TestTrue(TEXT("Useful crossfire leaves the current shooting line laterally"), FMath::Abs(Point.Y) >= 300.f);
    }
    // These collision-only barriers are deliberately absent from Recast. A nav-only test would pass.
    auto* North = F.Box(FVector(-1200,120,150), FVector(550,8,150), false);
    auto* South = F.Box(FVector(-1200,-120,150), FVector(550,8,150), false);
    TestFalse(TEXT("Actual capsule sweeps refuse both blocked lateral routes despite unchanged Recast"), F.Found(F.Run()));
    North->DestroyComponent(); South->DestroyComponent();
    TestTrue(TEXT("Removing physical route blockers recovers real candidates"), F.Found(F.Run()));
    // All targets are still currently perceived. A separate weapon-channel occluder refuses a firing lane.
    F.Box(FVector(-100,0,150), FVector(8,250,150), false);
    TestFalse(TEXT("Clear paths do not qualify a shot through an actual target-side wall"), F.Found(F.Run()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCrossfireOwnershipTest, "ProjectVelkorran.Campaign.Aurelion.Crossfire.SlicedQueryRetiresItsOriginalContext",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCrossfireOwnershipTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 7; ++Case)
    {
        FCrossfireWorld F; if (!F.Valid() || !F.BuildNav()) { AddError(TEXT("Native fixture failed")); return false; }
        auto Pending = F.Manager->PrepareQueryInstance(FEnvQueryRequest(F.Query,F.AI), EEnvQueryRunMode::AllMatching);
        if (!TestTrue(TEXT("Real pending query prepared"), Pending.IsValid())) { return false; }
        Pending->ExecuteOneStep(UEnvQueryTypes::UnlimitedStepTime); // Stock EQS generator only; its context is now cached.
        if (!TestTrue(TEXT("Generator produced candidates before the native test"), !Pending->IsFinished() && Pending->Items.Num() > 0)) { return false; }
        switch (Case)
        {
        case 0: F.Observe(F.AI,false); break;
        case 1:
        {
            FSovCommandLinkSeverResult Sever;
            F.Commander->GetFormationLink()->TrySeverCommandLink(F.Target,Sever);
            TestFalse(TEXT("Actual native formation sever committed"),F.Commander->GetFormationLink()->IsCommandLinkActive());
            break;
        }
        case 2: F.Director->SetAdmissionState(ESovEncounterState::Active,FGuid::NewGuid()); break;
        case 3: F.AI->GetActivityComponent()->StopCurrentActivity(); break;
        case 4:
            F.Drone->GetNarrativeAbilitySystemComponent()->ClearActorInfo();
            F.Drone->GetNarrativeAbilitySystemComponent()->InitAbilityActorInfo(F.Drone,F.Drone); break;
        case 5: F.Goal->GoalKey = F.Commander; break;
        case 6: F.AI->SetPawn(F.Commander); break;
        }
        F.Manager->RunInstantQuery(Pending);
        TestFalse(FString::Printf(TEXT("Cached query cannot outlive loss/replacement case %d"),Case), Pending->IsSuccessful() && !Pending->Items.IsEmpty());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCrossfireReadOnlyTest, "ProjectVelkorran.Campaign.Aurelion.Crossfire.PreservesForeignMoveAndFallbackState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCrossfireReadOnlyTest::RunTest(const FString& Parameters)
{
    FCrossfireWorld F; if (!F.Valid() || !F.BuildNav()) { return false; }
    auto* BB = F.AI->GetBlackboardComponent(); const FName LocationKey = GetDefault<UArsenalSettings>()->BBKey_TargetLocation;
    const FVector ExistingDestination(-1600,-300,2); BB->SetValueAsVector(LocationKey,ExistingDestination); F.AI->SetFocus(F.Target);
    FAIMoveRequest Move(ExistingDestination); Move.SetAllowPartialPath(false);
    const auto Request = F.AI->MoveTo(Move);
    if (!TestTrue(TEXT("An independently owned real path request is moving"), Request.Code == EPathFollowingRequestResult::RequestSuccessful)) { return false; }
    const auto Activity = F.AI->GetActivityComponent()->GetCurrentActivity(); const auto Transform = F.Drone->GetActorTransform();
    TestTrue(TEXT("Reading the query still finds a position"), F.Found(F.Run()));
    F.Drone->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy);
    TestFalse(TEXT("Native hold makes the optional query fail to stock fallback"), F.Found(F.Run()));
    TestTrue(TEXT("The query never cancels or replaces a foreign native MoveId"), F.AI->GetPathFollowingComponent()->GetCurrentRequestId() == Request.MoveId);
    TestTrue(TEXT("Location, focus, activity and pawn transform remain owned by their original writers"),
        BB->GetValueAsVector(LocationKey) == ExistingDestination && F.AI->GetFocusActor() == F.Target
        && F.AI->GetActivityComponent()->GetCurrentActivity() == Activity && F.Drone->GetActorTransform().Equals(Transform));
    int32 Callbacks = 0; bool Aborted = false;
    const int32 QueryId = F.Manager->RunQuery(FEnvQueryRequest(F.Query,F.AI), EEnvQueryRunMode::SingleResult,
        FQueryFinishedSignature::CreateLambda([&](TSharedPtr<FEnvQueryResult> Result) { ++Callbacks; Aborted = Result->IsAborted(); }));
    TestTrue(TEXT("Native query cancellation remains the stock task's mechanism"), F.Manager->AbortQuery(QueryId));
    F.Manager->Tick(.016f);
    TestTrue(TEXT("An aborted native request reports once and cannot publish a location"), Callbacks == 1 && Aborted
        && BB->GetValueAsVector(LocationKey) == ExistingDestination && F.AI->GetPathFollowingComponent()->GetCurrentRequestId() == Request.MoveId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCrossfireDisconnectedTest, "ProjectVelkorran.Campaign.Aurelion.Crossfire.DisconnectedLandingAndEncounterBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCrossfireDisconnectedTest::RunTest(const FString& Parameters)
{
    FCrossfireWorld F; if (!F.Valid()) { return false; }
    // Each lateral destination has real supported navmesh, but the shooter island is disconnected.
    F.Floor->DestroyComponent();
    F.Box(FVector(-1200,0,-30),FVector(300,100,30),true);
    F.Box(FVector(-1100,500,-30),FVector(500,250,30),true);
    F.Box(FVector(-1100,-500,-30),FVector(500,250,30),true);
    if (!TestTrue(TEXT("Real disconnected Recast islands build"),F.BuildNav())) { return false; }
    auto Pending = F.Manager->PrepareQueryInstance(FEnvQueryRequest(F.Query,F.AI),EEnvQueryRunMode::AllMatching);
    if (!Pending.IsValid()) { return false; }
    Pending->ExecuteOneStep(UEnvQueryTypes::UnlimitedStepTime);
    if (!TestTrue(TEXT("Supported remote candidates were generated"),!Pending->IsFinished() && Pending->Items.Num()>0)) { return false; }
    F.Manager->RunInstantQuery(Pending);
    TestFalse(TEXT("A supported destination never admits a partial or disconnected route"),Pending->IsSuccessful() && !Pending->Items.IsEmpty());
    F.Director->EncounterId = TEXT("AnotherEncounter");
    TestFalse(TEXT("The same physical floor cannot grant another encounter this authored query"),F.Found(F.Run()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCrossfireInwardCoverTest, "ProjectVelkorran.Campaign.Aurelion.Crossfire.InwardCandidatesRespectCoverAndRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCrossfireInwardCoverTest::RunTest(const FString& Parameters)
{
    FCrossfireWorld F;
    if (!TestTrue(TEXT("Real formation, goals, target and EQS fixture"), F.Valid())) { return false; }
    F.Drone->SetActorLocation(FVector(-1800,0,62),false,nullptr,ETeleportType::TeleportPhysics);
    F.Commander->SetActorLocation(FVector(-1900,180,62),false,nullptr,ETeleportType::TeleportPhysics);
    F.Observe(F.AI,true); F.Observe(F.AllyAI,true);
    // Real Recast must route around this opaque wall. Every same-radius and old
    // outward angular proposal is on its far side and has a blocked target ray;
    // a nearer point can get around an end and stand on the clear target side.
    auto* Cover = F.Box(FVector(-1300,0,150),FVector(15,800,150),true);
    TestFalse(TEXT("An inward proposal never manufactures missing navigation"),F.Found(F.Run()));
    if (!TestTrue(TEXT("Actual navigation incorporates the cover wall"),F.BuildNav())) { return false; }
    const auto Transform = F.Drone->GetActorTransform();
    const auto MoveId = F.AI->GetPathFollowingComponent()->GetCurrentRequestId();
    const auto Result = F.Run();
    if (!TestTrue(TEXT("A linked midrange shooter finds a supported clear point around actual cover"),F.Found(Result))) { return false; }
    TestTrue(TEXT("The total proposal budget remains at most eight"),Result->Items.Num() <= 8);
    for (int32 Index=0; Index<Result->Items.Num(); ++Index)
    {
        const FVector Point = Result->GetItemAsLocation(Index);
        // Assert the geometry outcome, not the generator's angle/radius formula.
        TestTrue(TEXT("Accepted point has crossed to the clear side with capsule margin"),Point.X > -1255.f);
        TestTrue(TEXT("Accepted point remains on the authored floor and in the original combat range"),
            F.Bounds.Contains(Point,30.f) && FVector::Dist2D(Point,F.Target->GetActorLocation()) >= 1000.f
            && FVector::Dist2D(Point,F.Target->GetActorLocation()) <= 3000.f);
        FHitResult Shot;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CrossfireInwardRegression),true,F.Drone);
        const bool Hit = F.World->LineTraceSingleByChannel(Shot,Point+FVector(0,0,60),F.Target->GetActorLocation(),
            GetDefault<UArsenalSettings>()->WeaponTraceChannel,Params);
        TestTrue(TEXT("Independent actual shot query reaches the target, not the cover wall"),Hit && Shot.GetActor()==F.Target && Shot.GetComponent()!=Cover);
    }
    TestTrue(TEXT("Finding an inward point still does not move the pawn or replace a path owner"),
        F.Drone->GetActorTransform().Equals(Transform) && F.AI->GetPathFollowingComponent()->GetCurrentRequestId()==MoveId);
    // This second physical wall is intentionally absent from Recast. Every accepted
    // candidate still has a complete route, but none may shoot through the blocker.
    auto* ShotBlocker = F.Box(FVector(-500,0,150),FVector(8,350,150),false);
    TestFalse(TEXT("A closer navigable point cannot ignore a real target-side shot blocker"),F.Found(F.Run()));
    ShotBlocker->DestroyComponent();
    TestTrue(TEXT("Removing that physical shot blocker restores useful candidates"),F.Found(F.Run()));
    // Retain exact early/late refusals observed in the played hall. Prepare the
    // actual context first so these cases cannot pass because sight/ownership failed.
    for (const FVector TargetLocation : {FVector(1400,0,62),FVector(-1800,500,62)})
    {
        F.Target->SetActorLocation(TargetLocation,false,nullptr,ETeleportType::TeleportPhysics);
        F.Observe(F.AI,true); F.Observe(F.AllyAI,true);
        auto Pending = F.Manager->PrepareQueryInstance(FEnvQueryRequest(F.Query,F.AI),EEnvQueryRunMode::AllMatching);
        if (!TestTrue(TEXT("Real range-refusal query instance exists"),Pending.IsValid())) { return false; }
        FEnvQueryContextData Context;
        const bool HasContext = Pending->PrepareContext(UEnvQueryContext_SovCrossfireLease::StaticClass(),Context);
        if (!TestTrue(TEXT("Out-of-range target still has the exact direct-sight activity/formation lease"),HasContext && Context.NumValues==1)) { return false; }
        const double Range = FVector::Dist2D(F.Drone->GetNavAgentLocation(),F.Target->GetActorLocation());
        TestTrue(TEXT("Fixture is actually outside the unchanged ten-to-thirty-metre range"),Range < 1000. || Range > 3000.);
        Pending->ExecuteOneStep(UEnvQueryTypes::UnlimitedStepTime);
        TestTrue(TEXT("No inward proposal is generated for an out-of-range original shooter"),Pending->Items.IsEmpty());
        F.Manager->RunInstantQuery(Pending);
        TestFalse(TEXT("Original near/far range refusal remains unsuccessful"),Pending->IsSuccessful());
    }
    return true;
}
#endif
