// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAurelionEnemyRoleTestFixtures.h"
#include "Tests/SovBotAttackTestFixtures.h"
#include "Tests/SovRuntimeObjectTestFixtures.h"
#include "AI/NarrativeNPCController.h"
#include "AI/SovAurelionElitePolicy.h"
#include "AI/SovAurelionRoleActivities.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "Components/CapsuleComponent.h"
#include "Components/SovAurelionThermalFractureComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "TimerManager.h"
#include "UObject/Script.h"

namespace
{
void InitRole(ASovNPCCharacterBase& NPC)
{
    auto* ASC = NPC.GetNarrativeAbilitySystemComponent();
    ASC->AddAttributeSetSubobject(NPC.GetAttributeSetBase());
    ASC->InitAbilityActorInfo(&NPC, &NPC);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
    NPC.GetCapsuleComponent()->SetCapsuleSize(30.f, 60.f);
    NPC.GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    NPC.GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
    NPC.GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
    NPC.GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}
}
void ASovAurelionTestWallRunner::InitializeTestRole() { InitRole(*this); bEncounterSnapshotReady = true; }
void ASovAurelionTestWeaver::InitializeTestRole() { InitRole(*this); bEncounterSnapshotReady = true; }

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct FRoleWorld
{
    FEditorScriptExecutionGuard ScriptGuard;
    UWorld* World = nullptr;
    ASovAurelionTestWallRunner* Runner = nullptr;
    ASovAurelionWallRoute* Route = nullptr;
    UBoxComponent* Wall = nullptr;
    ANarrativeNPCController* Controller = nullptr;
    FRoleWorld()
    {
        const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
        if (!World) { return; }
        if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        World->InitializeActorsForPlay(FURL());
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Runner = World->SpawnActor<ASovAurelionTestWallRunner>(ASovAurelionTestWallRunner::StaticClass(), FVector(0.,0.,62.), FRotator::ZeroRotator, Spawn);
        Route = World->SpawnActor<ASovAurelionWallRoute>();
        Controller = World->SpawnActor<ANarrativeNPCController>();
        if (!Runner || !Route || !Controller) { return; }
        Runner->InitializeTestRole(); Controller->Possess(Runner);
        // Same capsule-centre clearance as the actual authored wall_route_spec.
        Route->LocalPoints = {FVector(0.,0.,62.), FVector(0.,0.,302.), FVector(250.,0.,302.)};
        Route->RouteId = TEXT("Test.WallRoute");
        Route->Speed = 300.f; Route->MaximumDuration = 4.f;
        Runner->GetWallTraversal()->Route = Route;
        Box(FVector(0.,0.,-10.), FVector(600.,600.,10.));
        Wall = Box(FVector(0.,100.,150.), FVector(500.,10.,150.));
        Box(FVector(250.,0.,230.), FVector(85.,85.,10.));
    }
    UBoxComponent* Box(const FVector& Location, const FVector& Extent)
    {
        auto* Actor = World->SpawnActor<AActor>();
        auto* Component = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Component); Actor->AddInstanceComponent(Component);
        Component->SetBoxExtent(Extent); Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Component->SetCollisionObjectType(ECC_WorldStatic); Component->SetCollisionResponseToAllChannels(ECR_Block);
        Component->RegisterComponent(); Actor->SetActorLocation(Location);
        return Component;
    }
    bool Valid() const { return World && Runner && Route && Controller && Wall; }
    FString DescribeAdmission() const
    {
        const auto* ASC = Runner->GetNarrativeAbilitySystemComponent();
        const auto* Capsule = Runner->GetCapsuleComponent();
        FString Result = FString::Printf(TEXT("Wall fixture ready=%d alive=%d registered=%d avatar=%d controller=%d suspension=%d busy=%d movement=%d position=%s capsule=%.2f/%.2f"),
            Runner->IsEncounterSnapshotReady(), Runner->IsAlive(), Runner->GetWallTraversal()->IsRegistered(), ASC->GetAvatarActor() == Runner,
            Runner->GetController() == Controller && Controller->GetPawn() == Runner, Controller->IsThreatMemorySuspended(),
            ASC->GetTagCount(FNarrativeGameplayTags::Get().State_Busy), int32(Runner->GetCharacterMovement()->MovementMode),
            *Runner->GetActorLocation().ToString(), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
        const auto Points = Route->GetWorldPoints();
        const auto Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
        FCollisionQueryParams Query(SCENE_QUERY_STAT(AurelionWallFixtureDiagnostic), false, Runner); Query.AddIgnoredActor(Route);
        FCollisionResponseParams Response(Capsule->GetCollisionResponseToChannels()); FVector Previous = Runner->GetActorLocation();
        for (int32 Index = 0; Index < Points.Num(); ++Index)
        {
            FHitResult Hit;
            if (World->SweepSingleByChannel(Hit, Previous, Points[Index], Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(), Shape, Query, Response))
            {
                Result += FString::Printf(TEXT("; sweep[%d] blocked by %s initial=%d time=%.4f normal=%s location=%s"),
                    Index, *GetNameSafe(Hit.GetActor()), Hit.bStartPenetrating, Hit.Time, *Hit.ImpactNormal.ToString(), *Hit.Location.ToString());
            }
            Previous = Points[Index];
        }
        return Result;
    }
    ~FRoleWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionVisualWallContactTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.VisualContactDoesNotChangeMovementCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionVisualWallContactTest::RunTest(const FString& Parameters)
{
    FRoleWorld F; if (!TestTrue(TEXT("Fixture ready"), F.Valid())) { return false; }
    auto* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Contact test mesh"), Mesh)) { return false; }
    auto* Owner = F.World->SpawnActor<AActor>();
    auto* Surface = NewObject<UInstancedStaticMeshComponent>(Owner);
    Owner->SetRootComponent(Surface); Owner->AddInstanceComponent(Surface);
    Surface->SetStaticMesh(Mesh); Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Surface->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.,80.,200.), FVector(3.,.2,3.)));
    Surface->RegisterComponent();
    F.Route->PresentationSurfaces.Add(Surface); F.Route->RebuildPresentationSurfaces();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VisualContactTest), false, F.Runner);
    FHitResult Physical;
    TestTrue(TEXT("Native wall remains the visibility hit"), F.World->LineTraceSingleByChannel(Physical, FVector(0.,0.,200.), FVector(0.,180.,200.), ECC_Visibility, Query));
    TestTrue(TEXT("Cosmetic contact never replaces world collision"), Physical.GetComponent()==F.Wall);
    FHitResult ObjectHit;
    TestTrue(TEXT("Object-type traces also retain native collision"), F.World->LineTraceSingleByObjectType(ObjectHit,
        FVector(0.,0.,200.), FVector(0.,180.,200.), FCollisionObjectQueryParams::AllObjects, Query)
        && ObjectHit.GetComponent()==F.Wall);
    TArray<UPrimitiveComponent*> RoutePrimitives;
    F.Route->GetComponents(RoutePrimitives);
    TestEqual(TEXT("Contact preparation creates no world physics components"), RoutePrimitives.Num(), 0);
    FHitResult Contact;
    TestTrue(TEXT("Authored projecting face supplies cosmetic contact"), F.Route->ResolvePresentationContact(Physical,FVector(0.,1.,0.),Contact));
    TestTrue(TEXT("Contact reaches visible face, not native plane"), FMath::Abs(Contact.ImpactPoint.Y-70.)<.1);
    TestTrue(TEXT("Visual source remains noncolliding"), Surface->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
    Surface->UpdateInstanceTransform(0,FTransform(FRotator::ZeroRotator,FVector(0.,120.,200.),FVector(3.,.2,3.)),false,true,true);
    TestTrue(TEXT("Changed instances immediately supply current contact"), F.Route->ResolvePresentationContact(Physical,FVector(0.,1.,0.),Contact));
    F.Route->RebuildPresentationSurfaces();
    TestTrue(TEXT("Recessed visual surface also supplies contact"), F.Route->ResolvePresentationContact(Physical,FVector(0.,1.,0.),Contact));
    TestTrue(TEXT("Recess lies behind native plane"), FMath::Abs(Contact.ImpactPoint.Y-110.)<.1);
    Surface->SetVisibility(false);
    TestFalse(TEXT("Hidden source cannot retain cosmetic contact"), F.Route->ResolvePresentationContact(Physical,FVector(0.,1.,0.),Contact));
    F.Route->PresentationSurfaces.Reset(); F.Route->RebuildPresentationSurfaces();
    TestFalse(TEXT("Unbound route retains physical fallback"), F.Route->ResolvePresentationContact(Physical,FVector(0.,1.,0.),Contact));
    TestTrue(TEXT("Native collision remains intact after rebuild"), F.World->LineTraceSingleByChannel(Physical,FVector(0.,0.,200.),FVector(0.,180.,200.),ECC_Visibility,Query) && Physical.GetComponent()==F.Wall);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionAuthoredContactTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.AuthoredCrucibleClimbSurfaceContact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionAuthoredContactTest::RunTest(const FString& Parameters)
{
    FRoleWorld F; if (!TestTrue(TEXT("Fixture ready"),F.Valid())) { return false; }
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Aurelion/Environment/ArchitectureKit/Meshes/SM_Aurelion_KIT_Z08WallAssembly.SM_Aurelion_KIT_Z08WallAssembly"));
    if (!TestNotNull(TEXT("Saved Crucible assembly"),Mesh)) { return false; }
#if WITH_EDITOR
    TArray<UStaticMesh*> PendingMeshes{Mesh};
    FStaticMeshCompilingManager::Get().FinishCompilation(PendingMeshes);
#endif
    auto* Wall=F.Box(FVector(1416.,21930.,-1050.),FVector(10.,220.,150.));
    auto* Owner=F.World->SpawnActor<AActor>();
    auto* Surface=NewObject<UInstancedStaticMeshComponent>(Owner);
    Owner->SetRootComponent(Surface);Owner->AddInstanceComponent(Surface);
    Surface->SetStaticMesh(Mesh);Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Surface->AddInstance(FTransform(FVector(0.,20800.,-1200.)));Surface->RegisterComponent();
    F.Route->PresentationSurfaces.Add(Surface);F.Route->RebuildPresentationSurfaces();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AuthoredContactTest),false,F.Runner);FHitResult Physical,Contact;
    TestTrue(TEXT("Actual climb wall's native plane is preserved"),F.World->LineTraceSingleByChannel(Physical,FVector(1600.,21930.,-1033.),FVector(1350.,21930.,-1033.),ECC_Visibility,Query) && Physical.GetComponent()==Wall);
    TestTrue(TEXT("Saved production mesh supplies climb contact"),F.Route->ResolvePresentationContact(Physical,FVector(-1.,0.,0.),Contact));
    TestTrue(TEXT("Feet reach the restored stone face"),FMath::Abs(Contact.ImpactPoint.X-1426.)<.2);
    TestTrue(TEXT("Stone normal faces the approaching runner"),Contact.ImpactNormal.X>.99);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionRoleComponentsTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.NativeComponentOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionRoleComponentsTest::RunTest(const FString& Parameters)
{
    const auto* Elite = GetDefault<ASovAurelionElite>();
    const auto* Weaver = GetDefault<ASovAurelionWeaver>();
    const auto* Runner = GetDefault<ASovAurelionWallRunner>();
    const auto* Drone = GetDefault<ASovAurelionSecurityDrone>();
    TestNotNull(TEXT("Security drone owns the stable native formation link"), Drone->GetFormationLink());
    TestEqual(TEXT("Formation subobject keeps its authored component identity"), Drone->GetFormationLink()->GetFName(), FName(TEXT("AurelionFormation")));
    TestTrue(TEXT("Formation membership includes its actual command source"), Drone->GetFormationLink()->IncludesOwnerAsParticipant());
    TestNotNull(TEXT("Elite retains native status owner"), Elite->GetStatusComponent());
    TestNotNull(TEXT("Elite has weak-point owner"), Elite->GetCoreWeakPoints());
    TestNotNull(TEXT("Elite has the actual native Thermal Fracture owner"), Elite->GetThermalFracture());
    TestNotNull(TEXT("Thermal elite retains actual native Poise lifecycle"), Elite->GetElitePoise());
    TestFalse(TEXT("An unbound elite never fabricates a fracture receipt"), Elite->GetThermalFracture()->GetFractureReceipt().IsComplete());
    TestFalse(TEXT("Empty core matcher requires real mesh authoring"), Elite->GetCoreWeakPoints()->HasValidWeakPointConfiguration());
    FSovWeakPointZone CoreZone; CoreZone.ZoneId = TEXT("Core");
    TestFalse(TEXT("Ordinary damage cannot break the Core before this attempt's Thermal Fracture"),
        Elite->GetCoreWeakPoints()->CanBreakWeakPoint(CoreZone, FSovDamageResult()));
    TestFalse(TEXT("An unbound elite reports no current fracture"), Elite->GetThermalFracture()->HasCompletedCurrentFracture());
    TArray<USovCommandLinkComponent*> Links; Weaver->GetComponents(Links);
    TestEqual(TEXT("Weaver owns exactly two native link components"), Links.Num(), 2);
    TestTrue(TEXT("Two anchors are distinct native subobjects"), Weaver->GetAnchorA() != Weaver->GetAnchorB());
    TestFalse(TEXT("Missing authored membership cannot start support"), Weaver->HasActiveSupportLink());
    TestTrue(TEXT("Traversal tick supports surface presentation"), Runner->GetWallTraversal()->PrimaryComponentTick.bCanEverTick);
    TestFalse(TEXT("Presentation does not autonomously acquire traversal"), Runner->GetWallTraversal()->IsTraversing());
    TestTrue(TEXT("Traversal task has per-AI state"), GetDefault<UBTTask_SovAurelionTraverseWall>()->HasInstance());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionWallRoutePhysicalTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WallRouteRequiresGeometryAndActuallyClimbs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionWallRoutePhysicalTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!TestTrue(TEXT("Real initialized NPC, controller and collision world"), F.Valid())) { return false; }
    auto* Traversal = F.Runner->GetWallTraversal();
    FString Error;
    TestTrue(TEXT("Authored wall route has actual rise and a landing"), F.Route->ValidateRoute(Error));
    F.Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TestFalse(TEXT("An air path without a wall is not wall running"), Traversal->CanBeginTraversal());
    F.Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    auto* Obstruction = F.Box(FVector(0.,0.,170.), FVector(40.,40.,20.));
    TestFalse(TEXT("Capsule obstruction rejects route before movement"), Traversal->CanBeginTraversal());
    Obstruction->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (!TestTrue(TEXT("Clear swept path beside physical wall is admitted"), Traversal->CanBeginTraversal())) { AddError(F.DescribeAdmission()); return false; }
    UObject* TaskOwner = NewObject<USovRuntimeTestIdentity>(F.World);
    const uint64 Lease = Traversal->BeginTraversal(TaskOwner);
    TestTrue(TEXT("Authority obtains a nonzero traversal lease"), Lease != 0);
    TestEqual(TEXT("Traversal owns actual movement mode"), static_cast<int32>(F.Runner->GetCharacterMovement()->MovementMode), static_cast<int32>(MOVE_Flying));
    ESovAurelionTraversalResult Result = ESovAurelionTraversalResult::Running;
    float Highest = F.Runner->GetActorLocation().Z;
    for (int32 Step = 0; Step < 80 && Result == ESovAurelionTraversalResult::Running; ++Step)
    {
        Result = Traversal->AdvanceTraversal(TaskOwner, Lease, .05f);
        Highest = FMath::Max(Highest, static_cast<float>(F.Runner->GetActorLocation().Z));
    }
    TestTrue(TEXT("Capsule actually climbed the authored 240cm wall"), Highest >= 301.f);
    TestTrue(TEXT("Capsule reached real elevated flank landing"), F.Runner->GetActorLocation().Equals(FVector(250.,0.,302.), 2.f));
    TestTrue(TEXT("Native swept traversal completed"), Result == ESovAurelionTraversalResult::Completed);
    TestEqual(TEXT("Successful landing restores walking"), static_cast<int32>(F.Runner->GetCharacterMovement()->MovementMode), static_cast<int32>(MOVE_Walking));
    TestFalse(TEXT("Its busy contribution is released"), F.Runner->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
    TestFalse(TEXT("The same entry route is not repeatedly executed by the BT"), Traversal->CanBeginTraversal());
    TestFalse(TEXT("A delayed abort cannot cancel the completed lease"), Traversal->CancelTraversal(TaskOwner, Lease));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionWallSurfacePresentationTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WallSurfacePoseAndRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionWallSurfacePresentationTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    auto* Mesh = F.Runner->GetMesh();
    auto* Asset = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Parasites_Pack/Mesh/SK_Parasite_Spider.SK_Parasite_Spider"));
    auto* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Aurelion/Enemies/Animation/AM_EclipseWallRun.AM_EclipseWallRun"));
    if (!TestNotNull(TEXT("Authored spider mesh"), Asset) || !TestNotNull(TEXT("Authored wall gait"), Montage)) { return false; }
    Mesh->SetSkeletalMesh(Asset);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetRelativeLocation(FVector(0.,0.,-60.));
    Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass());
    const FTransform Ground = Mesh->GetRelativeTransform();
    auto* Traversal = F.Runner->GetWallTraversal();
    Traversal->WallRunMontage = Montage;
    const FVector BeforeTick = F.Runner->GetActorLocation();
    Traversal->TickComponent(.05f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Presentation tick does not acquire a movement lease"), Traversal->IsTraversing());
    TestTrue(TEXT("Presentation tick cannot move the capsule"), F.Runner->GetActorLocation().Equals(BeforeTick));
    UObject* Task = NewObject<USovRuntimeTestIdentity>(F.World);
    const uint64 Lease = Traversal->BeginTraversal(Task);
    if (!TestTrue(TEXT("Physical wall admits normal traversal"), Lease != 0)) { return false; }
    double BestAlignment = 0.;
    bool PlayedGait = false;
    bool PlayedGaitOnLedge = false;
    for (int32 Step=0; Step<100; ++Step)
    {
        if (Traversal->IsTraversing()) { Traversal->AdvanceTraversal(Task, Lease, .05f); }
        Traversal->TickComponent(.05f, LEVELTICK_All, nullptr);
        BestAlignment = FMath::Max(BestAlignment, FMath::Abs(Mesh->GetUpVector().Y));
        PlayedGait |= Mesh->GetAnimInstance() && Mesh->GetAnimInstance()->Montage_IsPlaying(Montage);
        PlayedGaitOnLedge |= Traversal->IsTraversing() && F.Runner->GetActorLocation().X > 50.
            && Mesh->GetAnimInstance() && Mesh->GetAnimInstance()->Montage_IsPlaying(Montage);
    }
    TestTrue(TEXT("Spider body up follows the physical wall normal"), BestAlignment > .95);
    TestTrue(TEXT("Traversal plays its authored gait"), PlayedGait);
    TestTrue(TEXT("The ledge transition keeps gait while capsule motion has no AnimBP velocity"), PlayedGaitOnLedge);
    TestTrue(TEXT("Landing restores the original mesh transform"), Mesh->GetRelativeTransform().Equals(Ground, .01));
    TestFalse(TEXT("Landing stops only the wall gait"), Mesh->GetAnimInstance()->Montage_IsPlaying(Montage));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionWallRouteAbortTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WallRouteInterruptionPreservesOtherOwners",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionWallRouteAbortTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    auto* Traversal = F.Runner->GetWallTraversal();
    auto* ASC = F.Runner->GetNarrativeAbilitySystemComponent();
    UObject* TaskOwner = NewObject<USovRuntimeTestIdentity>(F.World);
    UObject* OtherTask = NewObject<USovRuntimeTestIdentity>(F.World);
    const uint64 Lease = Traversal->BeginTraversal(TaskOwner);
    if (!TestTrue(TEXT("Valid route starts"), Lease != 0)) { AddError(F.DescribeAdmission()); return false; }
    Traversal->AdvanceTraversal(TaskOwner, Lease, .1f);
    TestFalse(TEXT("A different task cannot abort this traversal"), Traversal->CancelTraversal(OtherTask, Lease));
    F.Runner->GetCharacterMovement()->SetMovementMode(MOVE_None); // Encounter staging owns this newer state.
    ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy);
    TestFalse(TEXT("External staging retires traversal without another BT tick"), Traversal->IsTraversing());
    TestTrue(TEXT("External staging records a native cancellation"), Traversal->GetLastResult() == ESovAurelionTraversalResult::Cancelled);
    TestEqual(TEXT("Newer staging movement mode is preserved"), static_cast<int32>(F.Runner->GetCharacterMovement()->MovementMode), static_cast<int32>(MOVE_None));
    TestEqual(TEXT("Only traversal's busy count is removed"), ASC->GetTagCount(FNarrativeGameplayTags::Get().State_Busy), 1);
    TestFalse(TEXT("Cancelled lease cannot continue moving"), Traversal->AdvanceTraversal(TaskOwner, Lease, .05f) == ESovAurelionTraversalResult::Running);
    TestFalse(TEXT("Cancelled route does not mark completion"), Traversal->HasCompletedRoute());

    // A later lease must also retire safely if the entire tag count is externally reset.
    const FGameplayTag Busy = FNarrativeGameplayTags::Get().State_Busy;
    ASC->RemoveLooseGameplayTag(Busy);
    F.Runner->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    const uint64 ResetLease = Traversal->BeginTraversal(TaskOwner);
    if (!TestTrue(TEXT("Another physical lease starts after independent staging releases"), ResetLease != 0 && ResetLease != Lease)) { return false; }
    bool bReplacementBusyAdded = false;
    F.Runner->OnTestMovementModeChanged = [&]()
    {
        if (!bReplacementBusyAdded && F.Runner->GetCharacterMovement()->MovementMode == MOVE_Falling)
        { bReplacementBusyAdded = true; ASC->AddLooseGameplayTag(Busy); }
    };
    ASC->SetLooseGameplayTagCount(Busy, 0);
    F.Runner->OnTestMovementModeChanged = nullptr;
    TestFalse(TEXT("A zero-count reset cancels without a BT tick"), Traversal->IsTraversing());
    TestTrue(TEXT("Real movement teardown installed a new independent Busy contribution"), bReplacementBusyAdded);
    TestEqual(TEXT("Zero-count cleanup cannot consume the replacement contribution"), ASC->GetTagCount(Busy), 1);
    TestFalse(TEXT("The retired reset lease cannot remove the replacement count"), Traversal->CancelTraversal(TaskOwner, ResetLease));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionWallEncounterAdmissionTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WallTraversalRequiresActiveUniqueEncounter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionWallEncounterAdmissionTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    auto* Traversal = F.Runner->GetWallTraversal();
    auto* ASC = F.Runner->GetNarrativeAbilitySystemComponent();
    auto* Director = F.World->SpawnActor<ASovAurelionTraversalTestDirector>();
    if (!TestNotNull(TEXT("Native encounter director"), Director)) { return false; }
    Director->EncounterId = TEXT("Test.WallEntry");
    if (!TestTrue(TEXT("Register before the readiness hold has run"), Director->RegisterParticipant(TEXT("Runner"), F.Runner))) { return false; }
    TestEqual(TEXT("This is the uncovered startup window, with no Busy hold yet"), ASC->GetTagCount(FNarrativeGameplayTags::Get().State_Busy), 0);
    TestFalse(TEXT("Ready registered actor cannot traverse while encounter is inactive"), Traversal->CanBeginTraversal());
    Director->SetAdmissionState(ESovEncounterState::Active, FGuid());
    TestFalse(TEXT("Active state without an attempt is not combat admission"), Traversal->CanBeginTraversal());
    const FGuid FirstAttempt = FGuid::NewGuid();
    Director->SetAdmissionState(ESovEncounterState::Active, FirstAttempt);
    TestTrue(TEXT("One active attempt admits the physical route"), Traversal->CanBeginTraversal());
    auto* Duplicate = F.World->SpawnActor<ASovAurelionTraversalTestDirector>();
    if (!Duplicate) { return false; }
    // Serialized duplicate rosters cannot use RegisterParticipant's public duplicate rejection.
    Duplicate->Participants = Director->Participants;
    Duplicate->SetAdmissionState(ESovEncounterState::Active, FGuid::NewGuid());
    TestFalse(TEXT("Two active registering owners fail closed"), Traversal->CanBeginTraversal());
    Duplicate->Participants.Reset();
    UObject* TaskOwner = NewObject<USovRuntimeTestIdentity>(F.World);
    const uint64 Lease = Traversal->BeginTraversal(TaskOwner);
    if (!TestTrue(TEXT("Current encounter obtains a real movement lease"), Lease != 0)) { return false; }
    Director->SetAdmissionState(ESovEncounterState::Active, FGuid::NewGuid());
    TestTrue(TEXT("An old task cannot continue under a successor attempt"), Traversal->AdvanceTraversal(TaskOwner, Lease, .05f) == ESovAurelionTraversalResult::Cancelled);
    TestFalse(TEXT("Attempt retirement removes only its traversal Busy contribution"), ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
    F.Runner->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    for (const auto State : {ESovEncounterState::Inactive, ESovEncounterState::Restoring, ESovEncounterState::Failed, ESovEncounterState::Succeeded})
    {
        Director->SetAdmissionState(State, FirstAttempt);
        TestFalse(TEXT("Non-active encounter phases cannot start an entry route"), Traversal->CanBeginTraversal());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionWallNativeHoldTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WallTraversalNativeHoldRetiresLeaseBeforeBrainPause",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionWallNativeHoldTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    auto* Traversal = F.Runner->GetWallTraversal();
    auto* ASC = F.Runner->GetNarrativeAbilitySystemComponent();
    const FGameplayTag Busy = FNarrativeGameplayTags::Get().State_Busy;
    auto* Brain = NewObject<USovAurelionTraversalTestBrain>(F.Controller);
    F.Controller->AddInstanceComponent(Brain); Brain->RegisterComponent(); F.Controller->BrainComponent = Brain;
    UObject* TaskOwner = NewObject<USovRuntimeTestIdentity>(F.World);
    const uint64 Lease = Traversal->BeginTraversal(TaskOwner);
    if (!TestTrue(TEXT("Existing physical task has a real lease before late encounter ownership"), Lease != 0)) { return false; }
    Traversal->AdvanceTraversal(TaskOwner, Lease, .05f);
    const FVector InterruptedPosition = F.Runner->GetActorLocation();
    auto* Director = F.World->SpawnActor<ASovEncounterDirector>();
    if (!Director) { return false; }
    Director->EncounterId = TEXT("Test.LateWallHold"); Director->bHoldParticipantsBeforeEntry = true;
    if (!TestTrue(TEXT("Native participant registration installs its actual hold"), Director->RegisterParticipant(TEXT("Runner"), F.Runner))) { return false; }
    // Deliberately do not AdvanceTraversal: PauseLogic prevents the real task from ticking.
    TestTrue(TEXT("The existing controller brain is now paused by the director"), Brain->IsPaused());
    TestEqual(TEXT("The native director paused once"), Brain->PauseCount, 1);
    TestFalse(TEXT("Traversal retires synchronously before another task tick"), Traversal->IsTraversing());
    TestTrue(TEXT("Native traversal cancellation is recorded"), Traversal->GetLastResult() == ESovAurelionTraversalResult::Cancelled);
    TestEqual(TEXT("Exactly the director's Busy count remains"), ASC->GetTagCount(Busy), 1);
    TestTrue(TEXT("Native hold is now quiescent and remains exclusively owned"), Director->IsOwnedPreEntryHold(F.Runner));
    TestTrue(TEXT("The actual hold no longer reports a startup failure"), Director->PreEntryHoldError.IsEmpty());
    TestTrue(TEXT("Cancelling never teleports the partially climbed actor"), F.Runner->GetActorLocation().Equals(InterruptedPosition));
    TestEqual(TEXT("Interrupted capsule may fall from its real position"), static_cast<int32>(F.Runner->GetCharacterMovement()->MovementMode), static_cast<int32>(MOVE_Falling));
    TestFalse(TEXT("Interruption cannot manufacture route completion"), Traversal->HasCompletedRoute());
    TestFalse(TEXT("The paused old task cannot release the director hold"), Traversal->CancelTraversal(TaskOwner, Lease));
    ASC->AddLooseGameplayTag(Busy);
    TestEqual(TEXT("Retired callback leaves subsequent independent Busy contributions alone"), ASC->GetTagCount(Busy), 2);
    ASC->RemoveLooseGameplayTag(Busy);
    TestEqual(TEXT("Independent release preserves the director contribution"), ASC->GetTagCount(Busy), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionWeaverSeverRestoreTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WeaverSeverAndRestoreKeepExactContributions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionWeaverSeverRestoreTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Weaver = F.World->SpawnActor<ASovAurelionTestWeaver>(ASovAurelionTestWeaver::StaticClass(), FVector(700.,0.,60.), FRotator::ZeroRotator, Spawn);
    auto* Severer = F.World->SpawnActor<ASovBotTestCharacter>(ASovBotTestCharacter::StaticClass(), FVector(800.,0.,60.), FRotator::ZeroRotator, Spawn);
    if (!Weaver || !Severer) { return false; }
    Weaver->InitializeTestRole(); Severer->InitializeTestCombat(1);
    auto* ASC = F.Runner->GetNarrativeAbilitySystemComponent();
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetArmorAttribute(), 10.f);
    const float InitialHealth = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
    auto* A = Weaver->GetAnchorA(); auto* B = Weaver->GetAnchorB();
    TestTrue(TEXT("Anchor A gets explicit unique identity"), A->ConfigureLinkId(TEXT("Test.Weaver.A")));
    TestTrue(TEXT("Anchor B gets explicit unique identity"), B->ConfigureLinkId(TEXT("Test.Weaver.B")));
    TestTrue(TEXT("Anchor A registers actual participant"), A->RegisterLinkedActor(F.Runner));
    TestTrue(TEXT("Anchor B registers actual participant"), B->RegisterLinkedActor(F.Runner));
    TestTrue(TEXT("Fresh support starts through native link owner"), Weaver->InitializeFreshLinks());
    TestEqual(TEXT("Two independent wards each contribute15 armor"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()), 40.f);
    const auto SavedA = A->CaptureCommandLinkState();
    const auto SavedB = B->CaptureCommandLinkState();
    Weaver->InitializeFreshLinks();
    TestEqual(TEXT("Repeated support task cannot stack duplicate effects"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()), 40.f);
    FSovCommandLinkSeverResult Sever;
    TestTrue(TEXT("Actual hostile sever retires one link"), A->TrySeverCommandLink(Severer, Sever) == ESovCommandLinkSeverResolution::NewlySevered);
    TestEqual(TEXT("Sever removes only the selected link modifier"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()), 25.f);
    Weaver->InitializeFreshLinks();
    TestTrue(TEXT("Support task never reactivates a severed anchor"), A->GetCommandLinkState() == ESovCommandLinkState::Severed);
    TestTrue(TEXT("Second native sever succeeds independently"), B->TrySeverCommandLink(Severer, Sever) == ESovCommandLinkSeverResolution::NewlySevered);
    TestEqual(TEXT("Both severed restores original armor"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()), 10.f);
    TestFalse(TEXT("Both severed means support ends, not automatic reset"), Weaver->InitializeFreshLinks());
    const TArray<AActor*> Members = {F.Runner};
    TestTrue(TEXT("Encounter restoration accepts exact active snapshot A"), A->RestoreCommandLinkState(SavedA, Weaver, Members));
    TestTrue(TEXT("Encounter restoration accepts exact active snapshot B"), B->RestoreCommandLinkState(SavedB, Weaver, Members));
    TestEqual(TEXT("Restored support has exactly two modifiers"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()), 40.f);
    TestTrue(TEXT("A second restore is still idempotent"), A->RestoreCommandLinkState(SavedA, Weaver, Members));
    TestEqual(TEXT("No duplicate modifier from repeated restore"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()), 40.f);
    TestEqual(TEXT("Link support never heals or refills resources"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), InitialHealth);
    Weaver->Destroy();
    TestEqual(TEXT("Source destruction removes owned wards"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()), 10.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionWallRouteReentryTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WallAcquisitionCallbacksCannotOverwriteStaging",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionWallRouteReentryTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    auto* ASC = F.Runner->GetNarrativeAbilitySystemComponent();
    auto* Traversal = F.Runner->GetWallTraversal();
    bool bStaged = false;
    if (!TestTrue(TEXT("Physical route admits before the callback is installed"), Traversal->CanBeginTraversal())) { AddError(F.DescribeAdmission()); return false; }
    const FGameplayTag Busy = FNarrativeGameplayTags::Get().State_Busy;
    const FDelegateHandle Callback = ASC->RegisterGameplayTagEvent(Busy, EGameplayTagEventType::NewOrRemoved).AddLambda(
        [&](const FGameplayTag, int32 NewCount)
        {
            if (!bStaged && NewCount > 0)
            {
                bStaged = true;
                F.Runner->GetCharacterMovement()->SetMovementMode(MOVE_None);
                ASC->AddLooseGameplayTag(Busy);
            }
        });
    const uint64 Lease = Traversal->BeginTraversal(NewObject<USovRuntimeTestIdentity>(F.World));
    ASC->RegisterGameplayTagEvent(Busy, EGameplayTagEventType::NewOrRemoved).Remove(Callback);
    TestTrue(TEXT("Real tag callback stages the actor during acquisition"), bStaged);
    TestEqual(TEXT("Acquisition returns no usable lease after staging callback"), Lease, uint64(0));
    TestEqual(TEXT("Acquisition never overwrites the new stage with flying"), static_cast<int32>(F.Runner->GetCharacterMovement()->MovementMode), static_cast<int32>(MOVE_None));
    TestEqual(TEXT("Only the acquired busy contribution is retired"), ASC->GetTagCount(Busy), 1);
    TestFalse(TEXT("Failed acquisition is no longer traversing"), Traversal->IsTraversing());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionWallRouteAvatarTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.WallAvatarReplacementRetiresOnlyOldMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionWallRouteAvatarTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    auto* Traversal = F.Runner->GetWallTraversal();
    auto* ASC = F.Runner->GetNarrativeAbilitySystemComponent();
    UObject* Owner = NewObject<USovRuntimeTestIdentity>(F.World);
    const uint64 Lease = Traversal->BeginTraversal(Owner);
    if (!TestTrue(TEXT("Real traversal acquired before replacement"), Lease != 0)) { AddError(F.DescribeAdmission()); return false; }
    Traversal->AdvanceTraversal(Owner, Lease, .1f);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Replacement = F.World->SpawnActor<ASovAurelionTestWallRunner>(ASovAurelionTestWallRunner::StaticClass(), FVector(1500.,0.,60.), FRotator::ZeroRotator, Spawn);
    if (!Replacement) { return false; }
    Replacement->InitializeTestRole();
    Replacement->GetCharacterMovement()->SetMovementMode(MOVE_None);
    ASC->InitAbilityActorInfo(F.Runner, Replacement);
    TestTrue(TEXT("The actual ASC avatar was replaced"), ASC->GetAvatarActor() == Replacement);
    TestTrue(TEXT("Actor-info replacement cancels the old route"), Traversal->AdvanceTraversal(Owner, Lease, .05f) == ESovAurelionTraversalResult::Cancelled);
    TestEqual(TEXT("The old airborne capsule falls instead of staying suspended"), static_cast<int32>(F.Runner->GetCharacterMovement()->MovementMode), static_cast<int32>(MOVE_Falling));
    TestEqual(TEXT("Replacement movement state remains independently owned"), static_cast<int32>(Replacement->GetCharacterMovement()->MovementMode), static_cast<int32>(MOVE_None));
    TestFalse(TEXT("Old traversal's busy contribution is gone"), ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
    TestFalse(TEXT("Stale traversal cannot move the replacement"), Traversal->IsTraversing());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionFormationReadinessTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.FormationWaitsForMembersAndNeverRearms",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionFormationReadinessTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Member = F.World->SpawnActor<ASovAurelionTestWeaver>(ASovAurelionTestWeaver::StaticClass(), FVector(700.,0.,60.), FRotator::ZeroRotator, Spawn);
    auto* Severer = F.World->SpawnActor<ASovBotTestCharacter>(ASovBotTestCharacter::StaticClass(), FVector(900.,0.,60.), FRotator::ZeroRotator, Spawn);
    if (!Member || !Severer) { return false; }
    Severer->InitializeTestCombat(1);
    auto* Link = NewObject<USovAurelionFreshCommandLink>(F.Runner);
    F.Runner->AddInstanceComponent(Link); Link->RegisterComponent();
    TestTrue(TEXT("Explicit formation identity configured"), Link->ConfigureLinkId(TEXT("Test.Formation")));
    TestTrue(TEXT("Actual member registered"), Link->RegisterLinkedActor(Member));
    TestFalse(TEXT("Formation cannot publish before all members finish readiness"), Link->InitializeFreshLink());
    TestFalse(TEXT("Rejected bootstrap never creates a live identity"), Link->GetLinkInstanceId().IsValid());
    Member->InitializeTestRole();
    const float Armor = Member->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute());
    TestTrue(TEXT("Ready formation activates normally"), Link->InitializeFreshLink());
    const FGuid Instance = Link->GetLinkInstanceId();
    TestTrue(TEXT("Ready source is the actual linked pawn"), Link->GetCommandSource() == F.Runner);
    TestTrue(TEXT("Repeating readiness keeps the same instance"), Link->InitializeFreshLink() && Link->GetLinkInstanceId() == Instance);
    TestEqual(TEXT("Plain security formation does not invent a Weaver armor buff"), Member->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()), Armor);
    FSovCommandLinkSeverResult Sever;
    TestTrue(TEXT("Formation can be severed through the real link API"), Link->TrySeverCommandLink(Severer, Sever) == ESovCommandLinkSeverResolution::NewlySevered);
    TestFalse(TEXT("Readiness cannot rearm the severed instance"), Link->InitializeFreshLink());
    TestTrue(TEXT("Severed instance remains exact and retired"), Link->GetLinkInstanceId() == Instance && Link->GetCommandLinkState() == ESovCommandLinkState::Severed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionSlowFormationBootstrapTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.FormationBootstrapSurvivesSlowMemberReadiness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionSlowFormationBootstrapTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Member = F.World->SpawnActor<ASovAurelionTestWeaver>(ASovAurelionTestWeaver::StaticClass(), FVector(700.,0.,60.), FRotator::ZeroRotator, Spawn);
    if (!Member) { return false; }
    F.Runner->GetCharacterMovement()->DisableMovement();
    Member->GetCharacterMovement()->DisableMovement();
    auto* Link = NewObject<USovAurelionBootstrapTestLink>(F.Runner);
    Link->bAutoInitializeFreshLink = true;
    F.Runner->AddInstanceComponent(Link); Link->RegisterComponent();
    Link->ConfigureLinkId(TEXT("Test.SlowFormation"));
    Link->RegisterLinkedActor(Member);
    Link->StartBootstrapForTest();
    const double Started = F.World->GetTimeSeconds();
    int32 TimerTicks = 0;
    FTimerHandle ClockProbe;
    F.World->GetTimerManager().SetTimer(ClockProbe, FTimerDelegate::CreateLambda([&TimerTicks]() { ++TimerTicks; }), .1f, true);
    uint64 Frame = GFrameCounter;
    const auto TickFrame = [&F, &Frame]()
    {
        // TimerManager runs once per engine frame, even if an isolated test
        // advances UWorld repeatedly within one automation callback.
        TGuardValue<uint64> ScopedFrame(GFrameCounter, ++Frame);
        F.World->Tick(LEVELTICK_TimeOnly, .1f);
        // TimeOnly deliberately skips timers and actor simulation in UWorld.
        F.World->GetTimerManager().Tick(.1f);
    };
    for (int32 Index=0; Index<320; ++Index) { TickFrame(); }
    F.World->GetTimerManager().ClearTimer(ClockProbe);
    if (!TestTrue(TEXT("Fixture delivered at least 310 actual timer ticks"), TimerTicks >= 310)) { return false; }
    TestTrue(TEXT("The real game clock passed the old 30-second bootstrap cutoff"), F.World->GetTimeSeconds()-Started > 31.);
    TestFalse(TEXT("An unready member cannot create an active formation"), Link->GetLinkInstanceId().IsValid());
    Member->InitializeTestRole();
    Member->GetCharacterMovement()->DisableMovement();
    for (int32 Index=0; Index<3; ++Index) { TickFrame(); }
    TestTrue(TEXT("Native polling activates when the late member becomes ready, without a manual activation call"), Link->IsCommandLinkActive());
    TestTrue(TEXT("The initialized command source is the real owner"), Link->GetCommandSource()==F.Runner);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionElitePoiseInitializationTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.ElitePoiseUsesOrdinaryStartupEffect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionElitePoiseInitializationTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    auto* ASC = F.Runner->GetNarrativeAbilitySystemComponent();
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 37.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetArmorAttribute(), 11.f);
    const auto Spec = ASC->MakeOutgoingSpec(USovAurelionElitePoiseAttributes::StaticClass(), 1.f, ASC->MakeEffectContext());
    if (!TestTrue(TEXT("Actual startup effect builds through GAS"), Spec.IsValid())) { return false; }
    ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    TestEqual(TEXT("Elite config gives a real Poise maximum"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxPoiseAttribute()), 100.f);
    TestEqual(TEXT("Elite config initializes current Poise"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute()), 100.f);
    TestEqual(TEXT("Poise setup does not refill health"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 37.f);
    TestEqual(TEXT("Poise setup does not overwrite armor"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()), 11.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionRoleConfigurationRestoreTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.NativeSavedConfigurationSurvivesReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionRoleConfigurationRestoreTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    // Exact same SaveGame archive flags as Narrative's component record writer/reader.
    const auto Write = [](UActorComponent* Component)
    {
        INarrativeSavableComponent::Execute_PrepareForSave(Component);
        TArray<uint8> Bytes; FMemoryWriter Writer(Bytes);
        FObjectAndNameAsStringProxyArchive Archive(Writer, true); Archive.ArIsSaveGame = true; Archive.ArNoDelta = true;
        Component->Serialize(Archive); return Bytes;
    };
    const auto Read = [](UActorComponent* Component, const TArray<uint8>& Bytes)
    {
        FMemoryReader Reader(Bytes); FObjectAndNameAsStringProxyArchive Archive(Reader, true); Archive.ArIsSaveGame = true;
        Component->Serialize(Archive); INarrativeSavableComponent::Execute_Load(Component);
        return !Archive.IsError() && Cast<INarrativeSavableComponent>(Component)->WasSaveRecordLoadAccepted();
    };
    const auto RouteBytes = Write(F.Runner->GetWallTraversal());
    auto* NewTraversal = NewObject<USovAurelionWallTraversalComponent>(F.Runner);
    F.Runner->AddInstanceComponent(NewTraversal); NewTraversal->RegisterComponent();
    TestTrue(TEXT("Replacement resolves the exact saved route identity"), Read(NewTraversal, RouteBytes));
    TestTrue(TEXT("Route binding points to the live authored route, not an old pawn"), NewTraversal->Route == F.Route);
    auto* DuplicateRoute = F.World->SpawnActor<ASovAurelionWallRoute>(); DuplicateRoute->RouteId = F.Route->RouteId;
    TestFalse(TEXT("Ambiguous route identity rejects restoration"), Read(NewTraversal, RouteBytes));
    TestNull(TEXT("Rejected route restore never selects an arbitrary route"), NewTraversal->Route.Get());
    DuplicateRoute->Destroy();

    auto* Link = NewObject<USovAurelionFreshCommandLink>(F.Runner);
    F.Runner->AddInstanceComponent(Link); Link->RegisterComponent();
    TestTrue(TEXT("Instance-authored formation ID accepted"), Link->ConfigureLinkId(TEXT("Test.Authored.Formation")));
    TestTrue(TEXT("Actual owner-only formation establishes a live instance"), Link->InitializeFreshLink());
    const auto State = Link->CaptureCommandLinkState();
    const auto LinkBytes = Write(Link); Link->DestroyComponent();
    auto* ReplacementLink = NewObject<USovAurelionFreshCommandLink>(F.Runner);
    F.Runner->AddInstanceComponent(ReplacementLink); ReplacementLink->RegisterComponent();
    TestTrue(TEXT("Replacement receives its original authored link configuration"), Read(ReplacementLink, LinkBytes));
    TestEqual(TEXT("Saved ID restores before director applies link state"), ReplacementLink->GetLinkId(), FName(TEXT("Test.Authored.Formation")));
    TestFalse(TEXT("Configuration loading itself never activates a link"), ReplacementLink->IsCommandLinkActive());
    TestFalse(TEXT("Fresh bootstrap cannot race a pending director restoration"), ReplacementLink->InitializeFreshLink());
    TestTrue(TEXT("Director's exact link snapshot can restore normally"), ReplacementLink->RestoreCommandLinkState(State, F.Runner, {}));
    TestTrue(TEXT("Restored live instance identity is unchanged"), ReplacementLink->GetLinkInstanceId() == State.LinkInstanceId);

    auto* Thermal = NewObject<USovAurelionEliteThermalFracture>(F.Runner);
    F.Runner->AddInstanceComponent(Thermal); Thermal->RegisterComponent();
    AActor* Anchor = F.World->SpawnActor<AActor>(); Anchor->Tags.Add(TEXT("Test.CleanFrost"));
    Thermal->FrostAnchor = Anchor; Thermal->FrostAnchorId = TEXT("Test.CleanFrost");
    const auto ThermalBytes = Write(Thermal);
    auto* ReplacementThermal = NewObject<USovAurelionEliteThermalFracture>(F.Runner);
    F.Runner->AddInstanceComponent(ReplacementThermal); ReplacementThermal->RegisterComponent();
    TestTrue(TEXT("Replacement resolves the exact tagged physical frost anchor"), Read(ReplacementThermal, ThermalBytes));
    TestTrue(TEXT("Thermal anchor binding survives pawn reconstruction"), ReplacementThermal->FrostAnchor == Anchor);
    TestFalse(TEXT("Configuration restoration never fabricates thermal proof"), ReplacementThermal->GetFractureReceipt().IsComplete());
    AActor* DuplicateAnchor = F.World->SpawnActor<AActor>(); DuplicateAnchor->Tags.Add(TEXT("Test.CleanFrost"));
    TestFalse(TEXT("Duplicate frost anchor identity fails closed"), Read(ReplacementThermal, ThermalBytes));

    UObject* First = NewObject<USovRuntimeTestIdentity>(F.World); UObject* Second = NewObject<USovRuntimeTestIdentity>(F.World);
    F.Controller->SetThreatMemorySuspended(First, true);
    TestTrue(TEXT("Exact one-owner threat lease can be proven"), F.Controller->IsThreatMemorySuspendedOnlyBy(First));
    F.Controller->SetThreatMemorySuspended(Second, true);
    TestFalse(TEXT("A second owner's suspension is never bypassed"), F.Controller->IsThreatMemorySuspendedOnlyBy(First));
    F.Controller->SetThreatMemorySuspended(First, false);
    TestTrue(TEXT("Remaining owner's exact lease is retained"), F.Controller->IsThreatMemorySuspendedOnlyBy(Second));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionEliteDurabilityTest, "ProjectVelkorran.Campaign.Aurelion.EnemyRoles.EliteDurabilityScalesBothBossPhases",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionEliteDurabilityTest::RunTest(const FString& Parameters)
{
    FRoleWorld F;
    if (!F.Valid()) { return false; }
    auto* ASC = F.Runner->GetNarrativeAbilitySystemComponent();
    // The seeded enforcer pool the elite inherited, and an existing armour value it must not discard.
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 53.2f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 53.2f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetArmorAttribute(), 11.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), 100.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 100.f);

    const auto Link = ASC->MakeOutgoingSpec(USovAurelionEliteDurability::StaticClass(), 1.f, ASC->MakeEffectContext());
    if (!TestTrue(TEXT("Actual link-phase durability builds through GAS"), Link.IsValid())) { return false; }
    ASC->ApplyGameplayEffectSpecToSelf(*Link.Data.Get());
    TestEqual(TEXT("Link phase raises the boss health pool"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute()), SovAurelionElitePolicy::LinkPhaseHealth);
    TestEqual(TEXT("Link phase fills that pool"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), SovAurelionElitePolicy::LinkPhaseHealth);
    TestEqual(TEXT("Armour is added to what the definition already supplied, never overwritten"),
        ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()), 11.f + SovAurelionElitePolicy::LinkPhaseArmor);
    TestEqual(TEXT("Durability does not disturb poise"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute()), 100.f);

    // The real fight: the same elite re-arms at the phase boundary, which is also its full restore.
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 40.f);
    const auto Crucible = ASC->MakeOutgoingSpec(USovAurelionEliteCrucibleDurability::StaticClass(), 1.f, ASC->MakeEffectContext());
    if (!TestTrue(TEXT("Actual crucible durability builds through GAS"), Crucible.IsValid())) { return false; }
    ASC->ApplyGameplayEffectSpecToSelf(*Crucible.Data.Get());
    TestEqual(TEXT("The crucible boss carries the larger pool"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute()), SovAurelionElitePolicy::CruciblePhaseHealth);
    TestEqual(TEXT("Hardening restores the elite for its real fight"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), SovAurelionElitePolicy::CruciblePhaseHealth);
    TestEqual(TEXT("Crucible armour stacks on the link-phase plating"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetArmorAttribute()),
        11.f + SovAurelionElitePolicy::LinkPhaseArmor + SovAurelionElitePolicy::CruciblePhaseArmor);
    TestTrue(TEXT("The second fight is strictly harder than the first"), SovAurelionElitePolicy::CruciblePhaseHealth > SovAurelionElitePolicy::LinkPhaseHealth);
    // The measured starting point this scaling answers: an ordinary trash-mob pool on the mission's boss.
    TestTrue(TEXT("Both phases far exceed the measured baseline"), SovAurelionElitePolicy::LinkPhaseHealth > SovAurelionElitePolicy::BaselineHealth * 3.f);
    return true;
}
#endif
