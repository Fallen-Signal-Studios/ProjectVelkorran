// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "World/SovDestructibleCover.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include <limits>

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDestructibleCoverStateTest,
    "ProjectVelkorran.World.Destruction.DamageAndSavedObstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovDestructibleCoverStateTest::RunTest(const FString& Parameters)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
        .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Test world"), World)) { return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ASovDestructibleCover* Cover = World->SpawnActor<ASovDestructibleCover>();
    const FDamageEvent Damage;
    Cover->PlacementGuid = FGuid(0x98123212, 0x76354672, 0xABD13579, 0xEE444444);
    Cover->FracturedAsset = NewObject<UGeometryCollection>(Cover);
    TestEqual(TEXT("Protected by default"), Cover->TakeDamage(1000.f, Damage, nullptr, nullptr), 0.f);
    Cover->bDestructionEnabled = true;
    TestEqual(TEXT("Reject NaN"), Cover->TakeDamage(std::numeric_limits<float>::quiet_NaN(), Damage, nullptr, nullptr), 0.f);
    TestEqual(TEXT("Reject healing"), Cover->TakeDamage(-1.f, Damage, nullptr, nullptr), 0.f);
    TestEqual(TEXT("Accumulate first hit"), Cover->TakeDamage(20.f, Damage, nullptr, nullptr), 20.f);
    TestFalse(TEXT("Low damage keeps cover intact"), Cover->IsBroken());
    TArray<uint8> Intact;
    { FMemoryWriter Writer(Intact); FObjectAndNameAsStringProxyArchive Ar(Writer, false); Ar.ArIsSaveGame = true; Ar.ArNoDelta = true; Cover->Serialize(Ar); }
    TestEqual(TEXT("Overkill clamps to remaining health"), Cover->TakeDamage(1000.f, Damage, nullptr, nullptr), 100.f);
    TestTrue(TEXT("Cover breaks"), Cover->IsBroken());
    TestEqual(TEXT("Original collider retired"), Cover->Obstruction->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    TestFalse(TEXT("Intact presentation retired"), Cover->IntactVisual->IsVisible());
    TestFalse(TEXT("Navigation obstruction retired"), Cover->Obstruction->CanEverAffectNavigation());
    TestEqual(TEXT("Repeated damage cannot break twice"), Cover->TakeDamage(1000.f, Damage, nullptr, nullptr), 0.f);
    TArray<uint8> Broken;
    { FMemoryWriter Writer(Broken); FObjectAndNameAsStringProxyArchive Ar(Writer, false); Ar.ArIsSaveGame = true; Ar.ArNoDelta = true; Cover->Serialize(Ar); }
    { FMemoryReader Reader(Intact); FObjectAndNameAsStringProxyArchive Ar(Reader, true); Ar.ArIsSaveGame = true; Ar.ArNoDelta = true; Cover->Serialize(Ar); }
    Cover->Load_Implementation();
    TestFalse(TEXT("Earlier checkpoint restores intact"), Cover->IsBroken());
    TestEqual(TEXT("Partial damage retained"), Cover->RemainingHealth, 100.f);
    TestEqual(TEXT("Movement and shot collision restored"), Cover->Obstruction->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
    TestTrue(TEXT("Navigation restored"), Cover->Obstruction->CanEverAffectNavigation());
    TestTrue(TEXT("Intact mesh restored"), Cover->IntactVisual->IsVisible());
    { FMemoryReader Reader(Broken); FObjectAndNameAsStringProxyArchive Ar(Reader, true); Ar.ArIsSaveGame = true; Ar.ArNoDelta = true; Cover->Serialize(Ar); }
    Cover->Load_Implementation();
    TestTrue(TEXT("Later checkpoint retains destruction"), Cover->IsBroken());
    TestEqual(TEXT("Restored break has no invisible collider"), Cover->Obstruction->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
