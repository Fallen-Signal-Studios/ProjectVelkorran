// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovNarrativeSerializerTestFixtures.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "NarrativeSave.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
namespace
{
    struct FSerializerWorld
    {
        UWorld* World = nullptr;
        FSerializerWorld()
        {
            const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
            if (!World) { return; }
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        }
        ~FSerializerWorld()
        {
            if (!World) { return; }
            World->DestroyWorld(false);
            if (GEngine) { GEngine->DestroyWorldContext(World); }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrativeIdentityTransformTest,
    "ProjectVelkorran.Campaign.Save.IdentityTransformAndLegacyOmission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrativeIdentityTransformTest::RunTest(const FString& Parameters)
{
    FSerializerWorld Fixture;
    if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
    auto* Serializer = Fixture.World->GetSubsystem<UNarrativeSaveSubsystem>();
    auto* Actor = Fixture.World->SpawnActor<ASovSerializerMovableActor>();
    if (!Serializer || !Actor) { AddError(TEXT("Serializer or actor creation failed")); return false; }
    Actor->SetActorTransform(FTransform::Identity);
    FNarrativeActorRecord Record;
    if (!TestTrue(TEXT("Movable actor capture succeeds"), Serializer->CreateActorRecord(Actor, Record))) { return false; }
    TestTrue(TEXT("Identity is marked as an explicitly captured transform"), Record.bHasTransform && Record.Transform.Equals(FTransform::Identity));

    TStrongObjectPtr<UNarrativeSave> Snapshot(NewObject<UNarrativeSave>());
    Snapshot->RecordMap.Add(Record.ActorGUID, Record);
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Actual Narrative snapshot serializes"), UGameplayStatics::SaveGameToMemory(Snapshot.Get(), Bytes))) { return false; }
    TStrongObjectPtr<UNarrativeSave> Decoded(Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    const auto* Loaded = Decoded.IsValid() ? Decoded->RecordMap.Find(Record.ActorGUID) : nullptr;
    if (!TestTrue(TEXT("Captured identity presence survives disk serialization"), Loaded && Loaded->bHasTransform)) { return false; }

    const FTransform Displaced(FRotator(0.f, 45.f, 0.f), FVector(500.f, 250.f, 100.f), FVector(2.f));
    Actor->SetActorTransform(Displaced); Actor->SavedValue = 99;
    TestTrue(TEXT("Origin-saved actor reload succeeds"), Serializer->LoadActorFromRecord(Actor, *Loaded));
    TestTrue(TEXT("Position, rotation and scale restore to exact identity"), Actor->GetActorTransform().Equals(FTransform::Identity));
    TestEqual(TEXT("Actor state restores together with transform"), Actor->SavedValue, 17);

    FNarrativeActorRecord Legacy = *Loaded;
    Legacy.bHasTransform = false; // This is the default for tagged records saved before the field existed.
    Actor->SetActorTransform(Displaced);
    TestTrue(TEXT("Legacy absent-transform sentinel still loads"), Serializer->LoadActorFromRecord(Actor, Legacy));
    TestTrue(TEXT("Legacy identity omission preserves current placement"), Actor->GetActorTransform().Equals(Displaced));
    Legacy.Transform = FTransform(FVector(25.f, 50.f, 75.f));
    TestTrue(TEXT("Legacy nonidentity transform still loads"), Serializer->LoadActorFromRecord(Actor, Legacy));
    TestTrue(TEXT("Legacy placement is preserved without the new presence field"), Actor->GetActorTransform().Equals(Legacy.Transform));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrativeComponentDefaultsTest,
    "ProjectVelkorran.Campaign.Save.ComponentDefaultsRestoreIntoExistingInstance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrativeComponentDefaultsTest::RunTest(const FString& Parameters)
{
    FSerializerWorld Fixture;
    if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
    auto* Serializer = Fixture.World->GetSubsystem<UNarrativeSaveSubsystem>();
    auto* Actor = Fixture.World->SpawnActor<ASovSerializerMovableActor>();
    if (!Serializer || !Actor) { AddError(TEXT("Serializer or actor creation failed")); return false; }
    auto* Component = NewObject<USovSerializerDefaultComponent>(Actor, TEXT("DefaultState"));
    Actor->AddInstanceComponent(Component); Component->RegisterComponent();
    FNarrativeActorRecord Record;
    if (!TestTrue(TEXT("Actor captures its default-valued savable component"), Serializer->CreateActorRecord(Actor, Record))) { return false; }
    if (!TestEqual(TEXT("Exactly the opted-in component is captured"), Record.SavedComponents.Num(), 1)) { return false; }

    TStrongObjectPtr<UNarrativeSave> Snapshot(NewObject<UNarrativeSave>());
    Snapshot->RecordMap.Add(Record.ActorGUID, Record);
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Component snapshot serializes"), UGameplayStatics::SaveGameToMemory(Snapshot.Get(), Bytes))) { return false; }
    TStrongObjectPtr<UNarrativeSave> Decoded(Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromMemory(Bytes)));
    const auto* Loaded = Decoded.IsValid() ? Decoded->RecordMap.Find(Record.ActorGUID) : nullptr;
    if (!TestNotNull(TEXT("Component record survives serialized snapshot"), Loaded)) { return false; }

    Component->SavedValue = 99; Component->SavedEntries = { 1, 2, 3 }; Component->RuntimeOnlyValue = 73;
    if (!TestTrue(TEXT("Saved component restores into the existing instance"), Serializer->LoadActorFromRecord(Actor, *Loaded))) { return false; }
    TestEqual(TEXT("Captured default scalar replaces later component state"), Component->SavedValue, 17);
    TestTrue(TEXT("Captured empty collection clears later entries"), Component->SavedEntries.IsEmpty());
    TestEqual(TEXT("Full save snapshots still exclude properties without SaveGame"), Component->RuntimeOnlyValue, 73);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrativeStableLookupTest,
    "ProjectVelkorran.Campaign.Save.StableLookupWithoutWorldRecord",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrativeStableLookupTest::RunTest(const FString& Parameters)
{
    FSerializerWorld Fixture;
    if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
    auto* Serializer = Fixture.World->GetSubsystem<UNarrativeSaveSubsystem>();
    auto* SavedActor = Fixture.World->SpawnActor<ASovSerializerMovableActor>();
    auto* LookupActor = Fixture.World->SpawnActor<ASovSerializerStableActor>();
    if (!Serializer || !SavedActor || !LookupActor) { AddError(TEXT("Serializer or actor creation failed")); return false; }
    Serializer->RefreshStableActorIdentity(LookupActor);
    TestTrue(TEXT("Stable-only actor participates in GUID lookup"), Serializer->LookupActorByGUID(LookupActor->Guid) == LookupActor);

    UNarrativeSave* RawSnapshot = nullptr;
    if (!TestTrue(TEXT("World with lookup-only actor captures"), Serializer->CaptureSaveObject(RawSnapshot))) { return false; }
    TStrongObjectPtr<UNarrativeSave> Snapshot(RawSnapshot);
    const auto* Record = Snapshot->RecordMap.Find(SavedActor->Guid);
    TestTrue(TEXT("Savable actor retains a matching GUID record"), Record && Record->ActorGUID == SavedActor->Guid);
    TestFalse(TEXT("Lookup-only actor does not create an invalid world record"), Snapshot->RecordMap.Contains(LookupActor->Guid));
    SavedActor->SavedValue = 123;
    TestTrue(TEXT("Captured world passes actual load preflight and restore"), Serializer->LoadFromSnapshot(Snapshot.Get()));
    TestEqual(TEXT("Savable state restores"), SavedActor->SavedValue, 17);
    TestTrue(TEXT("Lookup-only actor remains available after restore"), Serializer->LookupActorByGUID(LookupActor->Guid) == LookupActor);

    FNarrativeActorRecord Explicit;
    TestTrue(TEXT("Explicit stable-only capture remains available to record owners"), Serializer->CreateActorRecord(LookupActor, Explicit));
    TestTrue(TEXT("Explicit record uses the actor's valid stable identity"), Explicit.ActorGUID == LookupActor->Guid);
    TestFalse(TEXT("Rootless actor has no captured transform"), Explicit.bHasTransform);
    LookupActor->Guid = SavedActor->Guid;
    TestFalse(TEXT("Lookup-only GUID collision still rejects ambiguous capture"), Serializer->CaptureSaveObject(RawSnapshot));
    TestTrue(TEXT("Rejected duplicate returns no partial snapshot"), RawSnapshot == nullptr);
    return true;
}
#endif
