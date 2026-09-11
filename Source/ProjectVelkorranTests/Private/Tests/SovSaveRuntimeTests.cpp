// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovSaveRuntimeTestFixtures.h"
#include "Tests/SovLifecycleTestFixtures.h"
#include "UI/SovAurelionPauseMenu.h"
#include "UI/SovAccessibilitySettingsMenu.h"
#include "Framework/SovPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "CommonGameViewportClient.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
#include "ICommonInputModule.h"
#include "HAL/PlatformProperties.h"
#include "Save/SovSaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Settings/SovGameUserSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/StrongObjectPtr.h"

TArray<FName> USovSavePhaseProbeComponent::RestoreOrder;

void USovSaveLoadCompletionProbe::OnCompleted(ESovSaveResult Result, const FSovSaveSlotHeader& Header, const FString& Message)
{
    ++Notifications; LastResult = Result; LastMessage = Message;
    bObservedReleasedOwnership = Subsystem && !Subsystem->IsLoadPending();
}

#if WITH_AUTOMATION_TESTS
namespace
{
    class FMemorySaveStorage final : public ISovSaveStorage
    {
    public:
        TMap<FString, TArray<uint8>> Slots;
        bool bFailWrite = false;
        bool bTornWrite = false;
        bool bAcknowledgeTornWrite = false;
        bool Read(const FString& Slot, int32 User, TArray<uint8>& Bytes) override
        { const auto* Found = Slots.Find(FString::FromInt(User) + Slot); if (!Found) { return false; } Bytes = *Found; return true; }
        bool Write(const FString& Slot, int32 User, const TArray<uint8>& Bytes) override
        {
            if (bFailWrite) { return false; }
            auto& Stored = Slots.FindOrAdd(FString::FromInt(User) + Slot); Stored = Bytes;
            if (bTornWrite) { Stored.SetNum(Stored.Num() / 2); return bAcknowledgeTornWrite; }
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
    static void StageLoad(USovSaveSubsystem& S, const FGuid& Request, bool bFailed, double Deadline)
    {
        S.PendingSave = Envelope(S); S.PendingNarrative = NewObject<UNarrativeSave>(&S);
        S.PendingLoadOwner = S.CaptureOperationOwner();
        S.PendingLoadRequest = Request; S.bPendingLoadFailed = bFailed; S.PendingLoadDeadline = Deadline;
        S.PendingLoadError = bFailed ? TEXT("Required destination participant is unavailable.") : FString();
    }
    static bool Matches(USovSaveSubsystem& S, const FString& Options) { return S.MatchesPendingLoadRequest(Options); }
    static void Tick(USovSaveSubsystem& S) { S.Tick(0.1f); }
    static bool IsFailed(const USovSaveSubsystem& S) { return S.bPendingLoadFailed; }
    static void Complete(USovSaveSubsystem& S, bool bSucceeded) { S.CompletePendingLoad(bSucceeded, TEXT("Test completion")); }
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveLoadRequestTest, "ProjectVelkorran.Campaign.Save.ExactLoadRequestAndTerminalFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveLoadRequestTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> S(NewObject<USovSaveSubsystem>(Instance.Get()));
    FSovSaveTestAccess::Initialize(*S);
    TStrongObjectPtr<USovSaveLoadCompletionProbe> Probe(NewObject<USovSaveLoadCompletionProbe>());
    Probe->Subsystem = S.Get(); S->OnLoadCompleted.AddDynamic(Probe.Get(), &USovSaveLoadCompletionProbe::OnCompleted);
    const FGuid Old = FGuid::NewGuid(), Current = FGuid::NewGuid();
    const auto Options = [](const FGuid& Request)
    { return TEXT("?SovCampaignSlotLoad=1?SovCampaignLoadRequest=") + Request.ToString(EGuidFormats::Digits); };
    FSovSaveTestAccess::StageLoad(*S, Current, true, FPlatformTime::Seconds() + 60.0);
    TestFalse(TEXT("Missing request token cannot consume a load"), FSovSaveTestAccess::Matches(*S, TEXT("?SovCampaignSlotLoad=1")));
    TestFalse(TEXT("Old request cannot consume a newer same-map load"), FSovSaveTestAccess::Matches(*S, Options(Old)));
    TestTrue(TEXT("Only exact request matches"), FSovSaveTestAccess::Matches(*S, Options(Current)));
    FSovSaveTestAccess::Tick(*S);
    TestEqual(TEXT("Initialization rejection publishes a terminal result without waiting for timeout"), Probe->Notifications, 1);
    TestEqual(TEXT("Failure is recovery, never success"), Probe->LastResult, ESovSaveResult::RecoveryAvailable);
    TestTrue(TEXT("Actual rejection reason preserved"), Probe->LastMessage.Contains(TEXT("participant")));
    TestTrue(TEXT("Ownership released before notification; failed world remains blocked"), Probe->bObservedReleasedOwnership && FSovSaveTestAccess::IsFailed(*S));
    TestFalse(TEXT("Late callback cannot resurrect completed request"), FSovSaveTestAccess::Matches(*S, Options(Current)));
    FSovSaveTestAccess::Tick(*S); FSovSaveTestAccess::Complete(*S, true);
    TestEqual(TEXT("Repeated tick/late completion cannot send duplicate success"), Probe->Notifications, 1);
    FSovSaveTestAccess::StageLoad(*S, FGuid::NewGuid(), false, FPlatformTime::Seconds() - 1.0);
    FSovSaveTestAccess::Tick(*S);
    TestEqual(TEXT("Timed-out replacement also terminates exactly once"), Probe->Notifications, 2);
    TestTrue(TEXT("Timeout is actionable"), Probe->LastMessage.Contains(TEXT("timed out")));
    FSovSaveTestAccess::StageLoad(*S, FGuid::NewGuid(), false, FPlatformTime::Seconds() + 60.0);
    FSovSaveTestAccess::Complete(*S, true);
    TestEqual(TEXT("Successful later recovery completes"), Probe->LastResult, ESovSaveResult::Success);
    TestFalse(TEXT("Successful recovery clears prior failure"), FSovSaveTestAccess::IsFailed(*S));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveBankFailureTest, "ProjectVelkorran.Campaign.Save.VerifiedBanksAndWriteFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveBankFailureTest::RunTest(const FString& Parameters)
{
    // The deliberately truncated bank is decoded during fallback, repair preflight,
    // and recovery preservation. Keep these expected field-specific diagnostics counted.
    AddExpectedMessage(TEXT("Failed loading tagged StructProperty /Script/ProjectVelkorran.SovCampaignSaveGame:Header. Read [0-9]+B, expected [0-9]+B. Package: FMemoryReader"),
        ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 3);
    AddExpectedMessage(TEXT("Failed loading tagged TextProperty /Script/ProjectVelkorran.SovSaveSlotHeader:MissionLabel. Read 0B, expected [0-9]+B. Package: FMemoryReader"),
        ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 3);
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> S(NewObject<USovSaveSubsystem>(Instance.Get()));
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
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> S(NewObject<USovSaveSubsystem>(Instance.Get())); FSovSaveTestAccess::Initialize(*S);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveRepeatedFaultRecoveryTest, "ProjectVelkorran.Campaign.Save.RepeatedStorageFaultsAcrossSubsystemRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveRepeatedFaultRecoveryTest::RunTest(const FString& Parameters)
{
    // Fifty torn-write cycles each decode the invalid bank three times. Only the
    // known truncated header/text diagnostics are expected; every recovery assertion remains active.
    AddExpectedMessage(TEXT("Failed loading tagged StructProperty /Script/ProjectVelkorran.SovCampaignSaveGame:Header. Read [0-9]+B, expected [0-9]+B. Package: FMemoryReader"),
        ELogVerbosity::Error, EAutomationExpectedMessageFlags::Contains, 150);
    AddExpectedMessagePlain(TEXT("Type mismatch in MissionLabel of SovSaveSlotHeader - Previous (None) Current(TextProperty) in package: FMemoryReader"),
        ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 150);
    TMap<FString, TArray<uint8>> Disk;
    int64 LastGoodGeneration = 0;
    for (int32 Cycle = 0; Cycle < 100; ++Cycle)
    {
        // Recreate the subsystem over retained physical bytes, not a retained live save UObject.
        TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
        TStrongObjectPtr<USovSaveSubsystem> S(NewObject<USovSaveSubsystem>(Instance.Get()));
        auto* Storage = FSovSaveTestAccess::Initialize(*S); Storage->Slots = Disk;
        TStrongObjectPtr<USovCampaignSaveGame> Save(FSovSaveTestAccess::Envelope(*S));
        Save->Header.PlaySeconds = Cycle;
        FString Error; bool Bad = false;
        if (Cycle > 0)
        {
            Storage->bFailWrite = Cycle % 2 == 0;
            Storage->bTornWrite = !Storage->bFailWrite;
            Storage->bAcknowledgeTornWrite = true;
            TestEqual(TEXT("A denied or falsely acknowledged torn write never reports success"),
                FSovSaveTestAccess::Write(*S, Save.Get(), Error),
                Storage->bFailWrite ? ESovSaveResult::WriteFailed : ESovSaveResult::ReadbackFailed);
            auto* Prior = FSovSaveTestAccess::Read(*S, ESovSaveSlotKind::Manual, 0, Bad);
            if (!TestTrue(TEXT("Last verified generation survives every injected fault"), Prior && Prior->Header.Generation == LastGoodGeneration)) { return false; }
        }
        Storage->bFailWrite = false; Storage->bTornWrite = false;
        if (!TestEqual(TEXT("A later verified retry can recover"), FSovSaveTestAccess::Write(*S, Save.Get(), Error), ESovSaveResult::Success)) { return false; }
        auto* Recovered = FSovSaveTestAccess::Read(*S, ESovSaveSlotKind::Manual, 0, Bad);
        if (!TestTrue(TEXT("Verified generation and data survive restart/recovery"), Recovered && !Bad
            && Recovered->Header.Generation == LastGoodGeneration + 1 && Recovered->Header.PlaySeconds == Cycle)) { return false; }
        ++LastGoodGeneration; Disk = Storage->Slots;
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNarrativeCaptureTransactionTest, "ProjectVelkorran.Campaign.Save.NarrativeCaptureRetainsLastGood",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovNarrativeCaptureTransactionTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitialization);
    if (!World) { AddError(TEXT("World creation failed")); return false; }
    if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
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
    const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
        .CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitialization);
    if (!World) { AddError(TEXT("World creation failed")); return false; }
    if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSavePersistentChecksumTest,
    "ProjectVelkorran.Campaign.Save.PersistentEnvelopeChecksumSurvivesTextRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSavePersistentChecksumTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> S(NewObject<USovSaveSubsystem>(Instance.Get()));
    FSovSaveTestAccess::Initialize(*S);
    const FText Labels[] = {
        FText::FromString(TEXT("Runtime mission label")),
        FText::AsCultureInvariant(TEXT("Invariant mission label")),
        NSLOCTEXT("SovSaveTests", "AuthoredMission", "Authored mission label"),
        FText::Format(NSLOCTEXT("SovSaveTests", "FormattedMission", "Mission {0}"), FText::AsNumber(2))
    };
    for (const FText& Label : Labels)
    {
        TStrongObjectPtr<USovCampaignSaveGame> Original(FSovSaveTestAccess::Envelope(*S));
        Original->Header.MissionLabel = Label;
        Original->Header.Generation = 1;
        Original->IntegrityChecksum = Original->CalculateChecksum();
        TestEqual(TEXT("Repeated checksum calculation is stable before serialization"), Original->CalculateChecksum(), Original->IntegrityChecksum);
        TArray<uint8> Bytes;
        if (!TestTrue(TEXT("Actual native envelope serialization succeeds"), UGameplayStatics::SaveGameToMemory(Original.Get(), Bytes))) { return false; }
        TStrongObjectPtr<USovCampaignSaveGame> Restored(Cast<USovCampaignSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)));
        if (!TestNotNull(TEXT("Native envelope deserializes"), Restored.Get())) { return false; }
        TestEqual(TEXT("Persistent serialization preserves the displayed mission label"), Restored->Header.MissionLabel.ToString(), Label.ToString());
        TestTrue(TEXT("Construction flags cannot invalidate intact persisted data"), Restored->HasValidIntegrity());
        TestEqual(TEXT("Serialization leaves original checksum valid"), Original->CalculateChecksum(), Original->IntegrityChecksum);
        FString Error;
        TestTrue(TEXT("The real save validator accepts the intact roundtrip"), FSovSaveTestAccess::Validate(*S, Restored.Get(), Error));
        Restored->Header.MissionLabel = FText::AsCultureInvariant(TEXT("Tampered label"));
        TestFalse(TEXT("Mission label changes remain protected by integrity"), Restored->HasValidIntegrity());
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveLegacyChecksumCompatibilityTest,
    "ProjectVelkorran.Campaign.Save.ExistingStableLabelBankRemainsReadable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveLegacyChecksumCompatibilityTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> S(NewObject<USovSaveSubsystem>(Instance.Get()));
    auto* Storage = FSovSaveTestAccess::Initialize(*S);
    TStrongObjectPtr<USovCampaignSaveGame> Existing(FSovSaveTestAccess::Envelope(*S));
    Existing->Header.Generation = 7;
    Existing->Header.MissionLabel = NSLOCTEXT("SovSaveTests", "LegacyMission", "Existing mission label");
    // Encode the original schema-1 checksum exactly as released before the fix.
    TArray<uint8> HeaderBytes;
    FMemoryWriter Writer(HeaderBytes);
    FObjectAndNameAsStringProxyArchive Ar(Writer, false);
    FSovSaveSlotHeader LegacyHeader = Existing->Header;
    FSovSaveSlotHeader::StaticStruct()->SerializeItem(Ar, &LegacyHeader, nullptr);
    for (const FSoftObjectPath& Asset : Existing->RequiredAssets)
    { FString Path = Asset.ToString(); Ar << Path; }
    uint32 LegacyCRC = FCrc::MemCrc32(HeaderBytes.GetData(), HeaderBytes.Num());
    LegacyCRC = FCrc::MemCrc32(Existing->NarrativePayload.GetData(), Existing->NarrativePayload.Num(), LegacyCRC);
    Existing->IntegrityChecksum = FCrc::MemCrc32(Existing->PortableSettings.GetData(), Existing->PortableSettings.Num(), LegacyCRC);
    TestTrue(TEXT("Fixture exercises legacy rather than canonical checksum"), Existing->IntegrityChecksum != Existing->CalculateChecksum());
    TArray<uint8> BankBytes;
    TestTrue(TEXT("Actual old-format bank serializes"), UGameplayStatics::SaveGameToMemory(Existing.Get(), BankBytes));
    Storage->Write(FSovSaveTestAccess::Name(*S, ESovSaveSlotKind::Manual, 0, 0), 0, BankBytes);
    bool bDamaged = false;
    TStrongObjectPtr<USovCampaignSaveGame> Restored(FSovSaveTestAccess::Read(*S, ESovSaveSlotKind::Manual, 0, bDamaged));
    if (!TestNotNull(TEXT("Existing valid bank remains readable after checksum repair"), Restored.Get())) { return false; }
    TestFalse(TEXT("Stable old-format bank is not marked corrupt"), bDamaged);
    TestEqual(TEXT("Previously verified generation is retained"), Restored->Header.Generation, static_cast<int64>(7));
    FString Error;
    TestEqual(TEXT("Next write upgrades checksum through normal alternating-bank transaction"), FSovSaveTestAccess::Write(*S, Restored.Get(), Error), ESovSaveResult::Success);
    TestEqual(TEXT("New write advances exactly one generation"), Restored->Header.Generation, static_cast<int64>(8));
    TestEqual(TEXT("New bank receives canonical checksum"), Restored->IntegrityChecksum, Restored->CalculateChecksum());
    Restored->Header.MissionLabel = FText::AsCultureInvariant(TEXT("Tampered historical label"));
    TestFalse(TEXT("Compatibility cannot accept changed label contents"), Restored->HasValidIntegrity());
    return true;
}


struct FSovAurelionPauseMenuTestAccess
{
    static void Bind(USovAurelionPauseMenu* Menu, USovSaveSubsystem* Save)
    { Menu->BoundSave = Save; Menu->RefreshCheckpoint(); }
    static void LoadClick(USovAurelionPauseMenu* Menu) { Menu->LoadButton->OnClicked.Broadcast(); }
    static void ResumeClick(USovAurelionPauseMenu* Menu) { Menu->ResumeButton->OnClicked.Broadcast(); }
    static bool Back(USovAurelionPauseMenu* Menu) { return Menu->NativeOnHandleBackAction(); }
    static bool OffersLoad(const USovAurelionPauseMenu* Menu) { return Menu->bHasCheckpoint && Menu->LoadButton->GetIsEnabled(); }
    static bool OffersRecovery(const USovAurelionPauseMenu* Menu) { return Menu->bAcceptRecovery; }
    static FString Message(const USovAurelionPauseMenu* Menu) { return Menu->Message->GetText().ToString(); }
    static bool Bound(const USovAurelionPauseMenu* Menu) { return Menu->BoundSave.IsValid(); }
};
namespace
{
    struct FAurelionPauseWorld
    {
        TStrongObjectPtr<UGameInstance> Instance { NewObject<UGameInstance>() };
        TStrongObjectPtr<ULocalPlayer> LocalPlayer { NewObject<ULocalPlayer>(GEngine) };
        TStrongObjectPtr<UCommonGameViewportClient> Viewport { NewObject<UCommonGameViewportClient>(GEngine) };
        TStrongObjectPtr<USovSaveSubsystem> Save { NewObject<USovSaveSubsystem>(Instance.Get()) };
        TStrongObjectPtr<USovAurelionPauseMenu> Menu;
        UWorld* World = nullptr;
        ASovPlayerController* PC = nullptr;
        FMemorySaveStorage* Storage = nullptr;
        FAurelionPauseWorld()
        {
            ICommonInputModule::GetSettings().LoadData();
            const auto IVS = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
                .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS, true);
            World->SetGameInstance(Instance.Get());
            if (GEngine)
            {
                auto& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
                Context.SetCurrentWorld(World); Context.OwningGameInstance = Instance.Get(); Context.GameViewport = Viewport.Get();
                Instance->OnWorldChanged(nullptr, World);
            }
            World->InitWorld(IVS); World->UpdateWorldComponents(!FPlatformProperties::RequiresCookedData(), false);
            FURL URL; URL.AddOption(*(TEXT("game=") + ASovLifecycleTestGameMode::StaticClass()->GetPathName()));
            if (!World->SetGameMode(URL)) { return; }
            World->InitializeActorsForPlay(URL);
            Viewport->Init(*Instance->GetWorldContext(), Instance.Get(), false);
            PC = World->SpawnActor<ASovPlayerController>();
            if (!PC) { return; }
            PC->Player = LocalPlayer.Get(); LocalPlayer->PlayerController = PC; PC->SetAsLocalPlayerController(); World->AddController(PC);
            Instance->AddLocalPlayer(LocalPlayer.Get(), IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser());
            if (Instance->GetWorld() != World || LocalPlayer->GetGameInstance() != Instance.Get()
                || LocalPlayer->GetLocalPlayerIndex() != 0 || LocalPlayer->GetPlatformUserId() == PLATFORMUSERID_NONE) { return; }
            Storage = FSovSaveTestAccess::Initialize(*Save);
            Menu.Reset(NewObject<USovAurelionPauseMenu>(PC));
            Menu->SetOwningPlayer(PC); Menu->Initialize(); Menu->TakeWidget();
        }
        void Open()
        {
            Menu->ActivateWidget();
            // Only save-subsystem discovery is supplied by this isolated in-memory fixture.
            // The real widget, buttons, pause controller, storage, header reader and LoadSlot all execute unchanged.
            FSovAurelionPauseMenuTestAccess::Bind(Menu.Get(), Save.Get());
        }
        ~FAurelionPauseWorld()
        {
            if (Menu.IsValid()) { Menu->DeactivateWidget(); static_cast<UWidget*>(Menu.Get())->ReleaseSlateResources(true); Menu.Reset(); }
            LocalPlayer->PlayerController = nullptr;
            Instance->RemoveLocalPlayer(LocalPlayer.Get());
            World->DestroyWorld(false);
            Instance->OnWorldChanged(World, nullptr);
            if (GEngine) { GEngine->DestroyWorldContext(World); }
        }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionNativePauseOwnership,
    "ProjectVelkorran.UI.AurelionPause.RealButtonsPreserveForeignPause",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionNativePauseOwnership::RunTest(const FString&)
{
    FAurelionPauseWorld F;
    if (!TestNotNull(TEXT("Real project controller and menu"), F.PC) || !F.Menu.IsValid()) { return false; }
    F.Open();
    TestTrue(TEXT("Actual native tree has a safe-area root"), F.Menu->WidgetTree && Cast<USafeZone>(F.Menu->WidgetTree->RootWidget));
    TestTrue(TEXT("Ordinary menu activation pauses the actual game world"), F.World->IsPaused());
    TestFalse(TEXT("Empty native profile cannot expose legacy slots"), FSovAurelionPauseMenuTestAccess::OffersLoad(F.Menu.Get()));
    TestTrue(TEXT("Independent save failure acquires its own pause"), F.PC->AcquireSystemPause(TEXT("SaveFailure")));
    FSovAurelionPauseMenuTestAccess::ResumeClick(F.Menu.Get());
    TestFalse(TEXT("Actual Resume button deactivates the menu"), F.Menu->IsActivated());
    TestTrue(TEXT("Resume preserves the foreign save-failure pause"), F.World->IsPaused());
    TestFalse(TEXT("Retirement releases its save reference"), FSovAurelionPauseMenuTestAccess::Bound(F.Menu.Get()));
    F.PC->ReleaseSystemPause(TEXT("SaveFailure"));
    TestFalse(TEXT("Last real pause owner resumes simulation"), F.World->IsPaused());
    F.Open(); TestTrue(TEXT("Second activation acquires a fresh pause"), F.World->IsPaused());
    TestTrue(TEXT("CommonUI Back is handled"), FSovAurelionPauseMenuTestAccess::Back(F.Menu.Get()));
    TestFalse(TEXT("Back releases this menu's pause"), F.World->IsPaused());
    FSovAurelionPauseMenuTestAccess::LoadClick(F.Menu.Get());
    TestFalse(TEXT("Retired control cannot start travel"), F.Save->IsLoadPending());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionCheckpointNativeMenuRecovery,
    "ProjectVelkorran.UI.AurelionPause.NativeBankPreviewAndExplicitRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionCheckpointNativeMenuRecovery::RunTest(const FString&)
{
    FAurelionPauseWorld F;
    if (!TestNotNull(TEXT("Real project controller"), F.PC) || !F.Storage || !F.Menu.IsValid()) { return false; }
    FString Error;
    TStrongObjectPtr<USovCampaignSaveGame> Envelope(FSovSaveTestAccess::Envelope(*F.Save));
    Envelope->Header.Kind = ESovSaveSlotKind::Checkpoint; Envelope->Header.SlotIndex = 0;
    if (!TestEqual(TEXT("An unrelated native campaign checkpoint is valid storage"),
        FSovSaveTestAccess::Write(*F.Save, Envelope.Get(), Error), ESovSaveResult::Success)) { return false; }
    F.Open();
    TestFalse(TEXT("A different campaign checkpoint is not offered as Aurelion"), FSovAurelionPauseMenuTestAccess::OffersLoad(F.Menu.Get()));
    Envelope->Header.MissionId = TEXT("M12_FireAndFrost"); Envelope->Header.MissionLabel = FText::FromString(TEXT("Fire and Frost"));
    Envelope->Header.MapPackage = TEXT("/Game/Aurelion/Maps/L_Aurelion_M12");
    // Deliberately unavailable definition: accepting recovery must still obey native required-asset validation.
    Envelope->Header.MissionDefinition = FSoftObjectPath(TEXT("/Game/Tests/DA_MissingPauseDefinition.DA_MissingPauseDefinition"));
    if (!TestEqual(TEXT("Aurelion checkpoint writes through actual envelope storage"),
        FSovSaveTestAccess::Write(*F.Save, Envelope.Get(), Error), ESovSaveResult::Success)) { return false; }
    // The second alternating write used B; corrupt only A so the actual loader offers verified B.
    const TArray<uint8> Broken { 0, 1, 2, 3 };
    F.Storage->Write(FSovSaveTestAccess::Name(*F.Save, ESovSaveSlotKind::Checkpoint, 0, 0), 0, Broken);
    const auto Before = F.Storage->Slots;
    FSovAurelionPauseMenuTestAccess::Bind(F.Menu.Get(), F.Save.Get());
    TestTrue(TEXT("The exact Aurelion checkpoint is visible"), FSovAurelionPauseMenuTestAccess::OffersLoad(F.Menu.Get()));
    TestTrue(TEXT("Preview displays the stored mission"), FSovAurelionPauseMenuTestAccess::Message(F.Menu.Get()).Contains(TEXT("Fire and Frost")));
    TStrongObjectPtr<USovSaveLoadCompletionProbe> Probe(NewObject<USovSaveLoadCompletionProbe>());
    Probe->Subsystem = F.Save.Get(); F.Save->OnLoadCompleted.AddDynamic(Probe.Get(), &USovSaveLoadCompletionProbe::OnCompleted);
    FSovAurelionPauseMenuTestAccess::LoadClick(F.Menu.Get());
    TestEqual(TEXT("Actual button dispatch reaches the native damaged-bank branch once"), Probe->Notifications, 1);
    TestEqual(TEXT("Native result is recovery offered, never fabricated success"), Probe->LastResult, ESovSaveResult::RecoveryAvailable);
    TestTrue(TEXT("Menu requires another explicit recovery click"), F.Menu->IsActivated() && FSovAurelionPauseMenuTestAccess::OffersRecovery(F.Menu.Get()));
    TestFalse(TEXT("First click never initiates damaged-bank travel"), F.Save->IsLoadPending());
    TestTrue(TEXT("Recovery offer includes its actual stored timestamp"), FSovAurelionPauseMenuTestAccess::Message(F.Menu.Get()).Contains(Probe->LastMessage)
        && FSovAurelionPauseMenuTestAccess::Message(F.Menu.Get()).Contains(TEXT("UTC")));
    AddExpectedError(TEXT("Failed to find object"), EAutomationExpectedErrorFlags::Contains, 0);
    FSovAurelionPauseMenuTestAccess::LoadClick(F.Menu.Get());
    TestFalse(TEXT("Explicit acceptance cannot bypass missing required content"), F.Save->IsLoadPending());
    TestTrue(TEXT("Native validation failure keeps an actionable menu"), F.Menu->IsActivated());
    TestFalse(TEXT("Acceptance is one click, not a persistent recovery bypass"), FSovAurelionPauseMenuTestAccess::OffersRecovery(F.Menu.Get()));
    TestTrue(TEXT("Header inspection/rejected load preserve both banks byte-for-byte"), F.Storage->Slots.OrderIndependentCompareEqual(Before));
    F.Save->OnLoadCompleted.RemoveDynamic(Probe.Get(), &USovSaveLoadCompletionProbe::OnCompleted);
    return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignMalformedSavePreambleRecovery,
    "ProjectVelkorran.Campaign.Save.MalformedPreambleFallsBackAndPreservesRepairBytes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignMalformedSavePreambleRecovery::RunTest(const FString&)
{
    // Each case starts from a real verified bank and mutates only its alternate.
    // No malformed preamble is passed directly to the unsafe engine legacy decoder.
    for (int32 Case = 0; Case < 5; ++Case)
    {
        TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
        TStrongObjectPtr<USovSaveSubsystem> S(NewObject<USovSaveSubsystem>(Instance.Get()));
        auto* Storage = FSovSaveTestAccess::Initialize(*S);
        TStrongObjectPtr<USovCampaignSaveGame> Save(FSovSaveTestAccess::Envelope(*S));
        Save->Header.Kind = ESovSaveSlotKind::Checkpoint;
        FString Error;
        if (!TestEqual(TEXT("Real initial envelope writes and verifies"), FSovSaveTestAccess::Write(*S, Save.Get(), Error), ESovSaveResult::Success)) { return false; }
        const FString GoodName = FSovSaveTestAccess::Name(*S, ESovSaveSlotKind::Checkpoint, 0, 0);
        const FString BadName = FSovSaveTestAccess::Name(*S, ESovSaveSlotKind::Checkpoint, 0, 1);
        TArray<uint8> Good;
        if (!TestTrue(TEXT("Read exact native serialized bytes"), Storage->Read(GoodName, 0, Good))
            || !TestTrue(TEXT("Native envelope contains its complete preamble"), Good.Num() >= 8)) { return false; }
        TArray<uint8> Broken = Good;
        switch (Case)
        {
        case 0: Broken = { 0, 1, 2, 3 }; break;
        case 1: Broken.Reset(); break;
        case 2: Broken.SetNum(7); break;
        case 3: Broken[0] ^= 1; break;
        case 4:
        {
            int32 Version = 0; FMemory::Memcpy(&Version, Good.GetData() + sizeof(int32), sizeof(Version));
            ++Version; FMemory::Memcpy(Broken.GetData() + sizeof(int32), &Version, sizeof(Version));
            break;
        }
        }
        Storage->Write(BadName, 0, Broken);
        const auto Before = Storage->Slots;
        bool bDamaged = false;
        TStrongObjectPtr<USovCampaignSaveGame> Loaded(FSovSaveTestAccess::Read(*S, ESovSaveSlotKind::Checkpoint, 0, bDamaged));
        if (!TestNotNull(FString::Printf(TEXT("Malformed preamble case%d falls back to the real valid bank"), Case), Loaded.Get())) { return false; }
        TestTrue(TEXT("Rejected alternate is reported damaged"), bDamaged);
        TestEqual(TEXT("Fallback preserves the verified generation"), Loaded->Header.Generation, int64(1));
        const auto Headers = S->ListSlots();
        TestEqual(TEXT("The public menu header reader still exposes one valid checkpoint"), Headers.Num(), 1);
        TestTrue(TEXT("All reads preserve both exact physical byte arrays"), Storage->Slots.OrderIndependentCompareEqual(Before));
        Save->Header.PlaySeconds = 42.;
        if (!TestEqual(TEXT("Normal later write repairs the alternate via existing writer"), FSovSaveTestAccess::Write(*S, Save.Get(), Error), ESovSaveResult::Success)) { return false; }
        TArray<uint8> RetainedGood; Storage->Read(GoodName, 0, RetainedGood);
        TestTrue(TEXT("Repair never overwrites the last good bank"), RetainedGood == Good);
        int32 Preserved = 0;
        for (const auto& File : Storage->Slots)
        {
            if (File.Key.StartsWith(TEXT("0") + BadName + TEXT("_Recovery_")))
            { ++Preserved; TestTrue(TEXT("Support recovery file retains original malformed bytes including empty input"), File.Value == Broken); }
        }
        TestEqual(TEXT("Exactly one preservation copy precedes the repair"), Preserved, 1);
        Loaded.Reset(FSovSaveTestAccess::Read(*S, ESovSaveSlotKind::Checkpoint, 0, bDamaged));
        TestTrue(TEXT("Repaired newest generation is real and no longer damaged"), Loaded.IsValid() && !bDamaged
            && Loaded->Header.Generation == 2 && Loaded->Header.PlaySeconds == 42.);
    }
    return true;
}

#endif
