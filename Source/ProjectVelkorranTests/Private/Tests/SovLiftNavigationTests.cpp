// Copyright Fallen Signal Studios. All Rights Reserved.
#include "World/SovWorldTransitActor.h"
#include "World/SovAurelionNavigationSystem.h"
#include "AI/NavigationSystemBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "NavLinkCustomComponent.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationPath.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS
struct FSovLiftNavigationTestAccess
{
    static void State(ASovWorldTransitActor* Lift, ESovWorldTransitState Value)
    { Lift->State = Value; Lift->UpdateLinks(); }
    static void Dock(ASovWorldTransitActor* Lift, bool bDestination)
    { Lift->bAtDestination = bDestination; Lift->ReconcileEndpoint(); }
};
namespace
{
    struct FLiftNavigationWorld
    {
        UWorld* World = nullptr;
        USovAurelionNavigationSystem* Navigation = nullptr;
        ASovAurelionRecastNavMesh* Data = nullptr;
        FLiftNavigationWorld()
        {
            const UWorld::InitializationValues Init = UWorld::InitializationValues()
                .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
                .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        }
        ~FLiftNavigationWorld()
        {
            if (World)
            {
                if (Navigation) { Navigation->CancelBuild(); }
                World->DestroyWorld(false);
                if (GEngine) { GEngine->DestroyWorldContext(World); }
            }
        }
        UBoxComponent* Box(AActor* Owner, const FVector& Position, const FVector& Extent, bool bFloor)
        {
            if (!Owner) { return nullptr; }
            auto* Component = NewObject<UBoxComponent>(Owner);
            Owner->AddInstanceComponent(Component);
            if (Owner->GetRootComponent()) { Component->SetupAttachment(Owner->GetRootComponent()); }
            else { Owner->SetRootComponent(Component); }
            Component->SetMobility(EComponentMobility::Static);
            Component->SetBoxExtent(Extent, false);
            Component->SetCollisionProfileName(bFloor ? TEXT("BlockAll") : TEXT("NoCollision"));
            Component->SetCanEverAffectNavigation(bFloor);
            Component->SetRelativeLocation(Position);
            Component->RegisterComponent();
            return Component;
        }
        bool InitializeNavigation()
        {
            auto* Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
            if (!Bounds || !Box(Bounds, FVector(0,0,900), FVector(1000,1500,1300), false)) { return false; }
            Bounds->SupportedAgents.Empty(); Bounds->SupportedAgents.Set(0); Bounds->SupportedAgents.MarkInitialized();
            // A real volume supplies persistent bounds to every Build/GatherNavigationBounds pass.
            if (!Bounds->GetComponentsBoundingBox(true).IsInside(FVector(0,0,1900))) { return false; }
            auto* ConfigProperty = FindFProperty<FObjectPropertyBase>(AWorldSettings::StaticClass(), TEXT("NavigationSystemConfig"));
            if (!ConfigProperty) { return false; }
            ConfigProperty->SetObjectPropertyValue_InContainer(World->GetWorldSettings(),
                NewObject<USovAurelionNavigationConfig>(World->GetWorldSettings()));
            FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::GameMode, nullptr, true);
            Navigation = Cast<USovAurelionNavigationSystem>(World->GetNavigationSystem());
            if (!Navigation) { return false; }
            Navigation->Build();
            Data = Cast<ASovAurelionRecastNavMesh>(Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate));
            return Data && Data->IsRegistered();
        }
        void Build() { Navigation->Build(); }
        bool Projects(const FVector& Point) const
        {
            FNavLocation Location;
            return Navigation->ProjectPointToNavigation(Point, Location, FVector(35,35,80), Data)
                && FVector::Dist2D(Location.Location, Point) <= 35.f && FMath::Abs(Location.Location.Z - Point.Z) <= 35.f;
        }
        bool Complete(const FVector& Start, const FVector& End) const
        {
            FPathFindingQuery Query(Data, *Data, Start, End, Data->GetDefaultQueryFilter());
            Query.SetAllowPartialPaths(false);
            const FPathFindingResult Result = Navigation->FindPathSync(Query);
            if (!Result.IsSuccessful() || !Result.Path.IsValid() || Result.Path->IsPartial()) { return false; }
            const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
            return Points.Num() >= 2 && FVector::Dist2D(Points[0].Location, Start) <= 35.f
                && FVector::Dist2D(Points.Last().Location, End) <= 35.f
                && FMath::Abs(Points[0].Location.Z-Start.Z) <= 35.f && FMath::Abs(Points.Last().Location.Z-End.Z) <= 35.f;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovLiftDockedNavigationTest,
    "ProjectVelkorran.World.Transit.LiftDockedNavigationUsesRealRecastAndRetiresBeforeMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovLiftDockedNavigationTest::RunTest(const FString& Parameters)
{
    FLiftNavigationWorld Fixture;
    if (!TestNotNull(TEXT("Isolated physics/navigation world"), Fixture.World)) { return false; }
    // Same physical dimensions/seams as the authored M13 lift, translated near the origin.
    auto* Lower = Fixture.Box(Fixture.World->SpawnActor<AActor>(), FVector(0,-800,-30), FVector(300,500,30), true);
    auto* Upper = Fixture.Box(Fixture.World->SpawnActor<AActor>(), FVector(0,800,1770), FVector(300,500,30), true);
    auto* Lift = Fixture.World->SpawnActor<ASovWorldTransitActor>(FVector(0,0,-30), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Lower landing"), Lower) || !TestNotNull(TEXT("Upper landing"), Upper)
        || !TestNotNull(TEXT("Actual native lift"), Lift)) { return false; }
    Lift->Kind = ESovWorldTransitKind::Lift; Lift->DestinationOffset = FVector(0,0,1800); Lift->TravelSeconds = 8.f;
    Lift->MovingBody->SetBoxExtent(FVector(300,300,30), false);
    TestTrue(TEXT("Navigation-only component initially registered"), Lift->LiftNavigationSurface->IsRegistered());
    TestFalse(TEXT("Initial registration has no published surface"), Lift->LiftNavigationSurface->IsNavigationRelevant());
    if (!TestTrue(TEXT("Actual owned Recast generation initializes"), Fixture.InitializeNavigation())) { return false; }
    TestEqual(TEXT("Actual agent radius"), Fixture.Data->AgentRadius, 34.f);
    TestEqual(TEXT("Actual agent height"), Fixture.Data->AgentHeight, 176.f);
    const FVector LowerStart(0,-500,5), LowerBoard(0,100,5), UpperStart(0,500,1805), UpperBoard(0,-100,1805);
    AddInfo(FString::Printf(TEXT("Native box after post-registration authored resize projects at boarding point: %s"),
        Fixture.Projects(LowerBoard) ? TEXT("true") : TEXT("false")));
    FNavigationSystem::UpdateComponentData(*Lift->MovingBody);
    Fixture.Build();
    // This control proves UE can export a movable simple box after refreshing its actual geometry.
    TestTrue(TEXT("Explicitly refreshed native box exports its actual top"), Fixture.Projects(LowerBoard));
    Lift->RefreshNavigationGeometry();
    Lift->DispatchBeginPlay();
    Fixture.Build();
    TestTrue(TEXT("Registered disabled surface publishes through normal relevance change"), Lift->LiftNavigationSurface->IsNavigationRelevant());
    TestEqual(TEXT("Navigation surface cannot block actors or become a physical floor"),
        Lift->LiftNavigationSurface->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    TestTrue(TEXT("Physical box dimensions retained"), Lift->MovingBody->GetUnscaledBoxExtent().Equals(FVector(300,300,30)));
    TestEqual(TEXT("Physical box still uses the actual colliding profile"), Lift->MovingBody->GetCollisionProfileName(), FName(TEXT("BlockAllDynamic")));
    TestTrue(TEXT("Lower landing to platform complete"), Fixture.Complete(LowerStart, LowerBoard));
    TestTrue(TEXT("Platform to lower landing complete"), Fixture.Complete(LowerBoard, LowerStart));
    TestFalse(TEXT("Empty upper dock has no platform route"), Fixture.Complete(UpperStart, UpperBoard));
    // Do not rebuild yet: this must fail against the previously generated docked tiles.
    FSovLiftNavigationTestAccess::State(Lift, ESovWorldTransitState::Moving);
    TestFalse(TEXT("Departure immediately disconnects stale lower tiles"), Fixture.Complete(LowerStart, LowerBoard));
    Lift->MovingBody->SetRelativeLocation(FVector(0,0,900)); Fixture.Build();
    TestFalse(TEXT("Moving platform exports no walkable midshaft surface"), Fixture.Projects(FVector(0,100,905)));
    TestFalse(TEXT("Vacated lower dock has no route"), Fixture.Complete(LowerStart, LowerBoard));
    FSovLiftNavigationTestAccess::Dock(Lift, true); Fixture.Build();
    TestTrue(TEXT("Upper landing to platform complete"), Fixture.Complete(UpperStart, UpperBoard));
    TestTrue(TEXT("Platform to upper landing complete"), Fixture.Complete(UpperBoard, UpperStart));
    TestFalse(TEXT("Vacated lower dock stays disconnected after arrival"), Fixture.Complete(LowerStart, LowerBoard));
    TArray<uint8> Saved;
    { FMemoryWriter Writer(Saved); FObjectAndNameAsStringProxyArchive Archive(Writer, false); Archive.ArIsSaveGame = true; Lift->Serialize(Archive); }
    FSovLiftNavigationTestAccess::Dock(Lift, false); Fixture.Build();
    { FMemoryReader Reader(Saved); FObjectAndNameAsStringProxyArchive Archive(Reader, true); Archive.ArIsSaveGame = true; Lift->Serialize(Archive); }
    Lift->Load_Implementation(); Fixture.Build();
    TestEqual(TEXT("Saved destination restores the actual settled endpoint"), Lift->GetTransitState(), ESovWorldTransitState::AtDestination);
    TestTrue(TEXT("Real save/load reconciles upper boarding path"), Fixture.Complete(UpperStart, UpperBoard));
    Lift->SetPower(false);
    TestFalse(TEXT("Power loss immediately disconnects existing upper tiles"), Fixture.Complete(UpperStart, UpperBoard));
    Lift->SetPower(true); Fixture.Build();
    Lift->SetLockReason(FText::FromString(TEXT("Fixture lock")));
    TestFalse(TEXT("Lock immediately disconnects upper tiles"), Fixture.Complete(UpperStart, UpperBoard));
    Lift->SetLockReason(FText()); Fixture.Build();
    FSovLiftNavigationTestAccess::State(Lift, ESovWorldTransitState::WaitingForDestination);
    TestFalse(TEXT("Streaming wait exposes no boarding path"), Fixture.Complete(UpperStart, UpperBoard));
    FSovLiftNavigationTestAccess::Dock(Lift, true); Fixture.Build();
    FDamageEvent Damage; Lift->TakeDamage(100.f, Damage, nullptr, nullptr);
    TestFalse(TEXT("Broken lift immediately disconnects its dock"), Fixture.Complete(UpperStart, UpperBoard));
    Fixture.Build(); TestFalse(TEXT("Broken surface is absent after rebuild"), Fixture.Projects(UpperBoard));
    auto* Door = Fixture.World->SpawnActor<ASovWorldTransitActor>(FVector(700,0,0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Default native door"), Door)) { return false; }
    const bool bDoorNav = Door->MovingBody->CanEverAffectNavigation();
    const FVector DoorExtent = Door->MovingBody->GetUnscaledBoxExtent();
    Door->RefreshNavigationGeometry();
    TestEqual(TEXT("Door collision geometry navigation policy unchanged"), Door->MovingBody->CanEverAffectNavigation(), bDoorNav);
    TestTrue(TEXT("Door collision extent unchanged"), Door->MovingBody->GetUnscaledBoxExtent().Equals(DoorExtent));
    TestFalse(TEXT("Door never receives a lift navigation surface"), Door->LiftNavigationSurface->IsNavigationRelevant());
    return true;
}
#endif
