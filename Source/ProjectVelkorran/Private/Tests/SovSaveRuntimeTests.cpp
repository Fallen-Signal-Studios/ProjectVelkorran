// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovSaveRuntimeTestFixtures.h"
#include "Save/SovSaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Settings/SovGameUserSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/StrongObjectPtr.h"

TArray<FName> USovSavePhaseProbeComponent::RestoreOrder;

#if WITH_AUTOMATION_TESTS
namespace
{
    class FMemorySaveStorage final : public ISovSaveStorage
    {
    public:
        TMap<FString, TArray<uint8>> Slots;
        bool bFailWrite = false;
        bool bTornWrite = false;
        bool Read(const FString& Slot, int32 User, TArray<uint8>& Bytes) override
        { const auto* Found = Slots.Find(FString::FromInt(User) + Slot); if (!Found) { return false; } Bytes = *Found; return true; }
        bool Write(const FString& Slot, int32 User, const TArray<uint8>& Bytes) override
        {
            if (bFailWrite) { return false; }
            auto& Stored = Slots.FindOrAdd(FString::FromInt(User) + Slot); Stored = Bytes;
            if (bTornWrite) { Stored.SetNum(Stored.Num() / 2); return false; }
            return true;
        }
        bool Exists(const FString& Slot, int32 User) override { return Slots.Contains(FString::FromInt(User) + Slot); }
    };
}
struct FSovSaveTestAccess
{
    static FMemorySaveStorage* Initialize(USovSaveSubsystem& S)
    {
        S.AccountNamespace = TEXT("test-account-hash");
        auto Memory = MakeUnique<FMemorySaveStorage>(); auto* Result = Memory.Get(); S.Storage = MoveTemp(Memory); return Result;
    }
    static void SetLive(UNarrativeSaveSubsystem& N, UNarrativeSave* Save) { N.NarrativeSaveGame = Save; }
    static ESovSaveResult Write(USovSaveSubsystem& S, USovCampaignSaveGame* Save, FString& Error) { return S.WriteEnvelope(Save, Error); }
    static USovCampaignSaveGame* Read(USovSaveSubsystem& S, ESovSaveSlotKind Kind, int32 Index, bool& Bad)
    { int32 Bank; FString Error; return S.ReadBest(Kind, Index, Bank, Bad, Error); }
    static bool Validate(USovSaveSubsystem& S, USovCampaignSaveGame* Save, FString& Error) { return S.ValidateEnvelope(Save, false, Error); }
    static FString Name(USovSaveSubsystem& S, ESovSaveSlotKind Kind, int32 Index, int32 Bank) { return S.BankName(Kind, Index, Bank); }
    static void SetAccount(USovSaveSubsystem& S, const FString& Namespace) { S.AccountNamespace = Namespace; }
    static USovCampaignSaveGame* Envelope(USovSaveSubsystem& S)
    {
        auto* Save = NewObject<USovCampaignSaveGame>(&S);
        Save->Header.AccountNamespace = S.AccountNamespace;
        Save->Header.ActiveProtagonist = FSovGameplayTags::Get().Character_Player_Tarrik;
        Save->Header.MissionId = TEXT("M01_Mantle"); Save->Header.MissionLabel = FText::FromString(TEXT("Mantle"));
        Save->Header.MapPackage = TEXT("/Game/Missions/M01");
        Save->Header.MissionDefinition = FSoftObjectPath(TEXT("/Game/Missions/DA_M01.DA_M01"));
        Save->Header.TimestampUtc = FDateTime(2026, 9, 4);
        auto* Narrative = NewObject<USovSaveRuntimeSubclass>(&S);
        Narrative->CreatorMarker = TEXT("Configured subclass survives");
        UGameplayStatics::SaveGameToMemory(Narrative, Save->NarrativePayload);
        auto* Settings = NewObject<USovGameUserSettings>(&S);
        Settings->CapturePortableSettings(Save->PortableSettings);
        return Save;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveBankFailureTest, "ProjectVelkorran.Campaign.Save.VerifiedBanksAndWriteFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveBankFailureTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<USovSaveSubsystem> S(NewObject<USovSaveSubsystem>());
    auto* Storage = FSovSaveTestAccess::Initialize(*S); FString Error;
    TStrongObjectPtr<USovCampaignSaveGame> First(FSovSaveTestAccess::Envelope(*S));
    TestEqual(TEXT("First platform write/readback succeeds"), FSovSaveTestAccess::Write(*S, First.Get(), Error), ESovSaveResult::Success);
    auto* Decoded = Cast<USovSaveRuntimeSubclass>(UGameplayStatics::LoadGameFromMemory(First->NarrativePayload));
    TestTrue(TEXT("Configured Narrative save subclass and extra data survive envelope"), Decoded && Decoded->CreatorMarker == TEXT("Configured subclass survives"));
    const auto FirstBytes = Storage->Slots;
    TStrongObjectPtr<USovCampaignSaveGame> Second(FSovSaveTestAccess::Envelope(*S));
    Storage->bFailWrite = true;
    TestEqual(TEXT("Storage denied is not success"), FSovSaveTestAccess::Write(*S, Second.Get(), Error), ESovSaveResult::WriteFailed);
    bool Bad = false; auto* Recovered = FSovSaveTestAccess::Read(*S, ESovSaveSlotKind::Manual, 0, Bad);
    TestTrue(TEXT("Denied write leaves first generation available"), Recovered && Recovered->Header.Generation == 1 && Storage->Slots.Num() == FirstBytes.Num());
    Storage->bFailWrite = false; Storage->bTornWrite = true;
    TestEqual(TEXT("Interrupted write is not success"), FSovSaveTestAccess::Write(*S, Second.Get(), Error), ESovSaveResult::WriteFailed);
    Recovered = FSovSaveTestAccess::Read(*S, ESovSaveSlotKind::Manual, 0, Bad);
    TestTrue(TEXT("Corrupt newest bank falls back to good previous bank"), Bad && Recovered && Recovered->Header.Generation == 1);
    Storage->bTornWrite = false;
    TestEqual(TEXT("Later successful save can recover"), FSovSaveTestAccess::Write(*S, Second.Get(), Error), ESovSaveResult::Success);
    TestTrue(TEXT("Corrupt source bytes retained in recovery slot"), Storage->Slots.Num() == 3);
    Recovered = FSovSaveTestAccess::Read(*S, ESovSaveSlotKind::Manual, 0, Bad);
    TestTrue(TEXT("Verified new generation selected"), Recovered && Recovered->Header.Generation == 2);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveSchemaTest, "ProjectVelkorran.Campaign.Save.SchemaIntegrityAndAccount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveSchemaTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<USovSaveSubsystem> S(NewObject<USovSaveSubsystem>()); FSovSaveTestAccess::Initialize(*S);
    TStrongObjectPtr<USovCampaignSaveGame> Save(FSovSaveTestAccess::Envelope(*S)); FString Error;
    Save->Header.Generation = 1; Save->IntegrityChecksum = Save->CalculateChecksum();
    TestTrue(TEXT("Schema 1.0 fixture accepted"), FSovSaveTestAccess::Validate(*S, Save.Get(), Error));
    Save->Header.Build = TEXT("Future patch build"); Save->IntegrityChecksum = Save->CalculateChecksum();
    TestTrue(TEXT("Patch version change preserves schema compatibility"), FSovSaveTestAccess::Validate(*S, Save.Get(), Error));
    Save->Header.SchemaMajor = 0; Save->IntegrityChecksum = Save->CalculateChecksum();
    TestFalse(TEXT("Legacy prototype schema rejected"), FSovSaveTestAccess::Validate(*S, Save.Get(), Error));
    Save->Header.SchemaMajor = 1; Save->Header.SchemaMinor = 1; Save->IntegrityChecksum = Save->CalculateChecksum();
    TestFalse(TEXT("Unknown newer required schema fails closed"), FSovSaveTestAccess::Validate(*S, Save.Get(), Error));
    Save->Header.SchemaMinor = 0; Save->IntegrityChecksum = Save->CalculateChecksum();
    FSovSaveTestAccess::SetAccount(*S, TEXT("another-account"));
    TestFalse(TEXT("Other account's save rejected"), FSovSaveTestAccess::Validate(*S, Save.Get(), Error));
    FSovSaveTestAccess::SetAccount(*S, TEXT("test-account-hash"));
    Save->NarrativePayload[0] ^= 1;
    TestFalse(TEXT("Payload mutation invalidates checksum"), FSovSaveTestAccess::Validate(*S, Save.Get(), Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrativeCaptureTransactionTest, "ProjectVelkorran.Campaign.Save.NarrativeCaptureRetainsLastGood",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrativeCaptureTransactionTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) { AddError(TEXT("World creation failed")); return false; }
    if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
    World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
    auto* Narrative = World->GetSubsystem<UNarrativeSaveSubsystem>();
    auto* Actor = World->SpawnActor<ASovSaveRuntimeActor>();
    auto* Live = NewObject<USovSaveRuntimeSubclass>(Narrative); Live->CreatorMarker = TEXT("Keep subclass");
    FSovSaveTestAccess::SetLive(*Narrative, Live);
    UNarrativeSave* Candidate = nullptr;
    TestTrue(TEXT("World capture succeeds"), Narrative->CaptureSaveObject(Candidate));
    TestTrue(TEXT("Candidate preserves configured subclass"), Candidate && Cast<USovSaveRuntimeSubclass>(Candidate)
        && CastChecked<USovSaveRuntimeSubclass>(Candidate)->CreatorMarker == TEXT("Keep subclass"));
    TestTrue(TEXT("Capture does not replace live snapshot"), Narrative->GetSaveObject() == Live);
    const int32 PriorCount = Live->RecordMap.Num(); Actor->bRejectSerialization = true;
    TestFalse(TEXT("Actor archive failure rejects full candidate"), Narrative->CaptureSaveObject(Candidate));
    TestTrue(TEXT("Failed capture returns no partial candidate and leaves live records"), !Candidate && Narrative->GetSaveObject() == Live && Live->RecordMap.Num() == PriorCount);
    World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrativeRestorePhaseTest, "ProjectVelkorran.Campaign.Save.ComponentOrderAndPreflight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrativeRestorePhaseTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) { AddError(TEXT("World creation failed")); return false; }
    if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
    World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
    auto* Narrative = World->GetSubsystem<UNarrativeSaveSubsystem>();
    auto* Actor = World->SpawnActor<ASovSaveRuntimeActor>();
    auto* Late = NewObject<USovSavePhaseProbeComponent>(Actor, TEXT("Presentation"));
    Late->Phase = ENarrativeRestorePhase::Presentation; Actor->AddInstanceComponent(Late); Late->RegisterComponent();
    auto* Early = NewObject<USovSavePhaseProbeComponent>(Actor, TEXT("Canon"));
    Early->Phase = ENarrativeRestorePhase::World; Actor->AddInstanceComponent(Early); Early->RegisterComponent();
    FNarrativeActorRecord Record;
    TestTrue(TEXT("Actual actor/components capture"), Narrative->CreateActorRecord(Actor, Record));
    USovSavePhaseProbeComponent::RestoreOrder.Reset();
    TestTrue(TEXT("Actual phased actor restore"), Narrative->LoadActorFromRecord(Actor, Record));
    TestTrue(TEXT("Canon restores before presentation regardless of component creation order"),
        USovSavePhaseProbeComponent::RestoreOrder.Num()==2 && USovSavePhaseProbeComponent::RestoreOrder[0]==TEXT("Canon")
        && USovSavePhaseProbeComponent::RestoreOrder[1]==TEXT("Presentation"));
    Actor->SavedValue = 92;
    const FNarrativeSaveComponent Duplicate = Record.SavedComponents[0]; Record.SavedComponents.Add(Duplicate);
    TestFalse(TEXT("Duplicate required component rejects the record before mutation"), Narrative->LoadActorFromRecord(Actor, Record));
    TestEqual(TEXT("Rejected record did not deserialize actor state"), Actor->SavedValue, 92);
    Record.SavedComponents.Pop(); Record.SavedComponents[0].RestorePhase = static_cast<ENarrativeRestorePhase>(255);
    TestFalse(TEXT("Unknown restore phase fails closed"), Narrative->LoadActorFromRecord(Actor, Record));
    World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); }
    return true;
}
#endif
