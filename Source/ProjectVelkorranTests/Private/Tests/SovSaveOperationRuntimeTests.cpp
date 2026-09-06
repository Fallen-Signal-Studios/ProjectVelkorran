// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovSaveOperationTestFixtures.h"
#include "Save/SovSaveSubsystem.h"
#include "Platform/SovPlatformServicesAdapter.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "NarrativeSave.h"
#include "Settings/SovGameUserSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

TFunction<void(bool)> USovSaveOperationEnvelope::SerializationBoundary;
void USovSaveOperationEnvelope::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    if (!IsTemplate() && (Ar.IsSaving() || Ar.IsLoading()) && SerializationBoundary)
    { SerializationBoundary(Ar.IsSaving()); }
}
TFunction<void()> USovSaveOperationNarrative::LoadingBoundary;
void USovSaveOperationNarrative::Serialize(FArchive& Ar)
{
    Super::Serialize(Ar);
    if (!IsTemplate() && Ar.IsLoading() && LoadingBoundary) { LoadingBoundary(); }
}

#if WITH_AUTOMATION_TESTS
namespace
{
    class FOperationStorage final : public ISovSaveStorage
    {
    public:
        TMap<FString, TArray<uint8>> Files;
        TArray<FString> Calls;
        TFunction<void(const FString&)> AfterCall;
        static FString Key(const FString& Slot, int32 User) { return FString::FromInt(User) + TEXT(":") + Slot; }
        void Complete(const FString& Operation, const FString& Slot)
        { const FString Call = Operation + TEXT(":") + Slot; Calls.Add(Call); if (AfterCall) { AfterCall(Call); } }
        bool Read(const FString& Slot, int32 User, TArray<uint8>& Bytes) override
        {
            const auto* Found = Files.Find(Key(Slot, User)); const bool bFound = Found != nullptr;
            if (Found) { Bytes = *Found; }
            Complete(TEXT("Read"), Slot); return bFound;
        }
        bool Write(const FString& Slot, int32 User, const TArray<uint8>& Bytes) override
        { Files.Add(Key(Slot, User), Bytes); Complete(TEXT("Write"), Slot); return true; }
        bool Exists(const FString& Slot, int32 User) override
        { const bool bFound = Files.Contains(Key(Slot, User)); Complete(TEXT("Exists"), Slot); return bFound; }
    };
    FSovObservedPlatformAccount Account(const FString& Id = TEXT("SaveOperation.AccountA"))
    {
        FSovObservedPlatformAccount Result;
        Result.StableId = Id; Result.LocalUser = 3; Result.bIdentityKnown = true;
        Result.bRequiresKnownStorageOwner = true; Result.bStorageAccessAuthorized = true;
        return Result;
    }
    struct FSerializationScope
    {
        ~FSerializationScope()
        { USovSaveOperationEnvelope::SerializationBoundary = nullptr; USovSaveOperationNarrative::LoadingBoundary = nullptr; }
    };
}

struct FSovSaveOperationTestAccess
{
    static void Observe(USovSaveSubsystem& Save, const FSovObservedPlatformAccount& Value)
    { Save.ObserveNativePlatformAccount(Value); }
    static void Revoke(USovSaveSubsystem& Save)
    { auto Value = Account(); Value.bStorageAccessAuthorized = false; Observe(Save, Value); }
    static FOperationStorage* Initialize(USovSaveSubsystem& Save)
    {
        auto Storage = MakeUnique<FOperationStorage>(); auto* Result = Storage.Get(); Save.Storage = MoveTemp(Storage);
        Observe(Save, Account()); FString Error;
        if (!Save.SelectPlatformUser(Account().StableId, 3, Error)) { return nullptr; }
        Result->Calls.Reset(); return Result;
    }
    static ESovSaveResult Write(USovSaveSubsystem& Save, USovCampaignSaveGame* Envelope, FString& Error)
    { return Save.WriteEnvelope(Envelope, Error); }
    static FString Bank(const USovSaveSubsystem& Save, int32 Index)
    { return FOperationStorage::Key(Save.BankName(ESovSaveSlotKind::Manual, 0, Index), Save.UserIndex); }
    static USovCampaignSaveGame* Envelope(USovSaveSubsystem& Save, bool bSubclass = false)
    {
        USovCampaignSaveGame* Result = bSubclass ? NewObject<USovSaveOperationEnvelope>(&Save) : NewObject<USovCampaignSaveGame>(&Save);
        Result->Header.AccountNamespace = Save.AccountNamespace;
        Result->Header.MissionId = TEXT("M01_SaveOwnership"); Result->Header.MissionLabel = FText::FromString(TEXT("Ownership"));
        Result->Header.MapPackage = TEXT("/Game/Tests/SaveOwnership");
        Result->Header.MissionDefinition = FSoftObjectPath(TEXT("/Game/Tests/DA_SaveOwnership.DA_SaveOwnership"));
        Result->Header.ActiveProtagonist = FSovGameplayTags::Get().Character_Player_Tarrik;
        UGameplayStatics::SaveGameToMemory(NewObject<UNarrativeSave>(&Save), Result->NarrativePayload);
        NewObject<USovGameUserSettings>(&Save)->CapturePortableSettings(Result->PortableSettings);
        return Result;
    }
    static void StagePending(USovSaveSubsystem& Save)
    {
        Save.PendingSave = Envelope(Save); Save.PendingNarrative = NewObject<UNarrativeSave>(&Save);
        Save.PendingLoadOwner = Save.CaptureOperationOwner(); Save.PendingLoadRequest = FGuid::NewGuid();
        Save.PendingLoadDeadline = FPlatformTime::Seconds() + 100.;
    }
    static void CompletePending(USovSaveSubsystem& Save) { Save.CompletePendingLoad(true, FString()); }
    static bool PendingOwnerCurrent(USovSaveSubsystem& Save) { FString Error; return Save.IsPendingLoadOwnerCurrent(Error); }
    static void StageFailed(USovSaveSubsystem& Save)
    { Save.FailedWrite = Envelope(Save); Save.FailedWriteOwner = Save.CaptureOperationOwner(); Save.bAwaitingFailureDecision = true; }
    static void Tick(USovSaveSubsystem& Save) { Save.Tick(.1f); }
    static int32 Queued(const USovSaveSubsystem& Save) { return Save.PendingAutosaves.Num(); }
    static ESovSaveResult Commit(USovSaveSubsystem& Save, const TArray<uint8>& Remote, const TArray<uint8>& Local, FString& Error)
    { return Save.CommitPlatformSnapshot(Remote, Local, ESovSaveSlotKind::Manual, 0, Error); }
    static UNarrativeSave* Decode(USovSaveSubsystem& Save, USovCampaignSaveGame* Envelope, FString& Error)
    { const auto Owner = Save.CaptureOperationOwner(); return Save.DecodeNarrative(Envelope, Error, &Owner); }
};

namespace
{
    struct FOperationFixture
    {
        TStrongObjectPtr<UGameInstance> Instance { NewObject<UGameInstance>() };
        TStrongObjectPtr<USovSaveSubsystem> Save { NewObject<USovSaveSubsystem>(Instance.Get()) };
        FOperationStorage* Storage = FSovSaveOperationTestAccess::Initialize(*Save);
        bool Seed(bool bSubclass = false)
        {
            if (!Storage) { return false; }
            FString Error;
            TStrongObjectPtr<USovCampaignSaveGame> First(FSovSaveOperationTestAccess::Envelope(*Save, bSubclass));
            TStrongObjectPtr<USovCampaignSaveGame> Second(FSovSaveOperationTestAccess::Envelope(*Save, bSubclass));
            const bool bGood = FSovSaveOperationTestAccess::Write(*Save, First.Get(), Error) == ESovSaveResult::Success
                && FSovSaveOperationTestAccess::Write(*Save, Second.Get(), Error) == ESovSaveResult::Success;
            Storage->Calls.Reset(); return bGood;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveOwnedStorageBoundaryTest,
    "ProjectVelkorran.Campaign.Save.OperationOwnerEveryStorageBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveOwnedStorageBoundaryTest::RunTest(const FString&)
{
    FOperationFixture Baseline;
    if (!TestTrue(TEXT("Two verified banks seed the fault matrix"), Baseline.Seed())) { return false; }
    const auto Disk = Baseline.Storage->Files;
    TStrongObjectPtr<USovCampaignSaveGame> Probe(FSovSaveOperationTestAccess::Envelope(*Baseline.Save)); FString Error;
    if (!TestEqual(TEXT("Uninterrupted third write succeeds"), FSovSaveOperationTestAccess::Write(*Baseline.Save, Probe.Get(), Error), ESovSaveResult::Success)) { return false; }
    const int32 Boundaries = Baseline.Storage->Calls.Num();
    TestTrue(TEXT("Matrix includes bank discovery, reads, target write and readback"), Boundaries >= 7);
    for (int32 Fault = 0; Fault < 3; ++Fault)
    {
        for (int32 Boundary = 1; Boundary <= Boundaries; ++Boundary)
        {
            FOperationFixture Fixture; if (!Fixture.Storage) { return false; } Fixture.Storage->Files = Disk;
            const FString GoodBank = FSovSaveOperationTestAccess::Bank(*Fixture.Save, 1);
            const TArray<uint8> LastGood = Fixture.Storage->Files.FindChecked(GoodBank);
            Fixture.Storage->AfterCall = [&](const FString&)
            {
                if (Fixture.Storage->Calls.Num() != Boundary) { return; }
                if (Fault == 2) { Fixture.Save->SetPlatformSuspended(true); Fixture.Save->SetPlatformSuspended(false); }
                else
                {
                    FSovSaveOperationTestAccess::Revoke(*Fixture.Save);
                    if (Fault == 1) { FSovSaveOperationTestAccess::Observe(*Fixture.Save, Account()); }
                }
            };
            TStrongObjectPtr<USovCampaignSaveGame> Candidate(FSovSaveOperationTestAccess::Envelope(*Fixture.Save));
            const auto Result = FSovSaveOperationTestAccess::Write(*Fixture.Save, Candidate.Get(), Error);
            TestTrue(FString::Printf(TEXT("Fault %d at boundary %d cannot report success"), Fault, Boundary), Result != ESovSaveResult::Success);
            TestEqual(TEXT("No subsequent I/O after invalidated operation"), Fixture.Storage->Calls.Num(), Boundary);
            TestTrue(TEXT("Last good bank remains byte-identical"), Fixture.Storage->Files.FindChecked(GoodBank) == LastGood);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveSerializationOwnerTest,
    "ProjectVelkorran.Campaign.Save.OperationOwnerActualEnvelopeSerialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveSerializationOwnerTest::RunTest(const FString&)
{
    FSerializationScope Reset;
    for (int32 Mode = 0; Mode < 2; ++Mode)
    {
        FOperationFixture Fixture;
        if (!TestTrue(TEXT("Subclass-backed native banks seed successfully"), Fixture.Seed(true))) { return false; }
        TStrongObjectPtr<USovCampaignSaveGame> Candidate(FSovSaveOperationTestAccess::Envelope(*Fixture.Save, true));
        const auto Disk = Fixture.Storage->Files; bool bFired = false;
        USovSaveOperationEnvelope::SerializationBoundary = [&](bool bSaving)
        {
            if (bFired || bSaving != (Mode == 1)) { return; }
            bFired = true; FSovSaveOperationTestAccess::Revoke(*Fixture.Save);
            FSovSaveOperationTestAccess::Observe(*Fixture.Save, Account());
        };
        FString Error;
        TestTrue(TEXT("Actual LoadGameFromMemory/SaveGameToMemory callback cancels old authorization"),
            FSovSaveOperationTestAccess::Write(*Fixture.Save, Candidate.Get(), Error) != ESovSaveResult::Success);
        TestTrue(TEXT("The intended serializer really executed"), bFired);
        TestTrue(TEXT("No bank changed across serializer authorization ABA"), Fixture.Storage->Files.OrderIndependentCompareEqual(Disk));
        for (const auto& Call : Fixture.Storage->Calls) { TestFalse(TEXT("Serialization invalidation forbids later writes"), Call.StartsWith(TEXT("Write:"))); }
        USovSaveOperationEnvelope::SerializationBoundary = nullptr;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveProfileSelectionReentryTest,
    "ProjectVelkorran.Campaign.Save.ProfileHintReentryCannotPublishRevokedSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveProfileSelectionReentryTest::RunTest(const FString&)
{
    for (int32 Boundary = 1; Boundary <= 4; ++Boundary)
    {
        FOperationFixture Fixture; if (!Fixture.Storage) { return false; }
        const FString Original = Fixture.Save->GetAccountNamespace();
        const auto Next = Account(TEXT("SaveOperation.AccountB")); FSovSaveOperationTestAccess::Observe(*Fixture.Save, Next);
        bool bNestedAccepted = false;
        Fixture.Storage->AfterCall = [&](const FString&)
        {
            if (Fixture.Storage->Calls.Num() != Boundary) { return; }
            FString NestedError; bNestedAccepted = Fixture.Save->SelectPlatformUser(Next.StableId, 3, NestedError);
            auto Revoked = Next; Revoked.bStorageAccessAuthorized = false;
            FSovSaveOperationTestAccess::Observe(*Fixture.Save, Revoked);
        };
        FString Error;
        TestFalse(TEXT("Revocation during profile reads/write/readback rejects selection"), Fixture.Save->SelectPlatformUser(Next.StableId, 3, Error));
        TestFalse(TEXT("Profile selection cannot reenter through its storage callback"), bNestedAccepted);
        TestEqual(TEXT("Outgoing profile remains selected"), Fixture.Save->GetAccountNamespace(), Original);
        TestFalse(TEXT("Selection never republishes revoked availability"), Fixture.Save->IsPlatformStorageOwnerAvailable());
        TestEqual(TEXT("No subsequent hint I/O after revocation"), Fixture.Storage->Calls.Num(), Boundary);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSavePendingLoadOwnerTest,
    "ProjectVelkorran.Campaign.Save.PendingLoadSuspensionHoldAndAuthorizationABA",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSavePendingLoadOwnerTest::RunTest(const FString&)
{
    FOperationFixture Fixture; if (!Fixture.Storage) { return false; }
    TStrongObjectPtr<USovSaveOperationCompletion> Probe(NewObject<USovSaveOperationCompletion>());
    Fixture.Save->OnLoadCompleted.AddDynamic(Probe.Get(), &USovSaveOperationCompletion::Completed);
    FSovSaveOperationTestAccess::StagePending(*Fixture.Save);
    Fixture.Save->SetPlatformSuspended(true); Fixture.Save->SetPlatformSuspended(false);
    TestTrue(TEXT("Decoded same-owner pending load survives suspend/resume"), FSovSaveOperationTestAccess::PendingOwnerCurrent(*Fixture.Save));
    FSovSaveOperationTestAccess::CompletePending(*Fixture.Save);
    TestEqual(TEXT("Resumed same-owner completion succeeds"), Probe->Result, ESovSaveResult::Success);
    FSovSaveOperationTestAccess::StagePending(*Fixture.Save);
    FSovSaveOperationTestAccess::Revoke(*Fixture.Save); FSovSaveOperationTestAccess::Observe(*Fixture.Save, Account());
    TestFalse(TEXT("Authorization ABA cannot restore the old pending load token"), FSovSaveOperationTestAccess::PendingOwnerCurrent(*Fixture.Save));
    FSovSaveOperationTestAccess::CompletePending(*Fixture.Save);
    TestEqual(TEXT("Stale ready callback is recovery, never success"), Probe->Result, ESovSaveResult::RecoveryAvailable);
    TestEqual(TEXT("Each pending request publishes one completion"), Probe->Notifications, 2);
    TestEqual(TEXT("Pending restoration does not issue storage I/O"), Fixture.Storage->Calls.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveRetryOwnerTest,
    "ProjectVelkorran.Campaign.Save.DeliberateRetryUsesFreshOriginalOwnerAdmission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveRetryOwnerTest::RunTest(const FString&)
{
    FOperationFixture Fixture; if (!Fixture.Storage) { return false; }
    FSovSaveOperationTestAccess::StageFailed(*Fixture.Save); FString Error;
    FSovSaveOperationTestAccess::Revoke(*Fixture.Save);
    TestEqual(TEXT("Retry cannot write while original owner is unauthorized"), Fixture.Save->RetryFailedWrite(Error), ESovSaveResult::MissingAccount);
    TestEqual(TEXT("Unauthorized retry performs zero I/O"), Fixture.Storage->Calls.Num(), 0);
    FSovSaveOperationTestAccess::Observe(*Fixture.Save, Account());
    TestEqual(TEXT("Explicit retry may acquire fresh authorization for the same retained profile"), Fixture.Save->RetryFailedWrite(Error), ESovSaveResult::Success);
    TestFalse(TEXT("Verified retry releases the failure decision"), Fixture.Save->IsAwaitingFailureDecision());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveRetryDecisionReentryTest,
    "ProjectVelkorran.Campaign.Save.RetryAcknowledgmentReentryKeepsSnapshotAlive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveRetryDecisionReentryTest::RunTest(const FString&)
{
    FOperationFixture Fixture; if (!Fixture.Storage) { return false; }
    FSovSaveOperationTestAccess::StageFailed(*Fixture.Save); bool bFired = false; int32 CallsAtAcknowledgment = 0;
    Fixture.Storage->AfterCall = [&](const FString& Call)
    {
        if (!bFired && Call.StartsWith(TEXT("Write:")))
        { bFired = true; CallsAtAcknowledgment = Fixture.Storage->Calls.Num(); Fixture.Save->AcknowledgeSaveFailure(); }
    };
    FString Error;
    TestEqual(TEXT("A changed recovery decision cannot republish the obsolete retry"), Fixture.Save->RetryFailedWrite(Error), ESovSaveResult::Busy);
    TestTrue(TEXT("Real storage callback changed the pending decision"), bFired);
    TestEqual(TEXT("Canceled retry initiates no subsequent readback"), Fixture.Storage->Calls.Num(), CallsAtAcknowledgment);
    TestFalse(TEXT("Canceled decision does not reacquire failure pause"), Fixture.Save->IsAwaitingFailureDecision());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveQueuedOwnerTest,
    "ProjectVelkorran.Campaign.Save.QueuedAutosaveCannotSurviveOwnerEpochChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveQueuedOwnerTest::RunTest(const FString&)
{
    FOperationFixture Fixture; if (!Fixture.Storage) { return false; }
    Fixture.Save->QueueAutosave(ESovSaveBoundary::ArenaExit, TEXT("OwnershipExit"));
    TestEqual(TEXT("Valid owner can queue a boundary"), FSovSaveOperationTestAccess::Queued(*Fixture.Save), 1);
    FSovSaveOperationTestAccess::Revoke(*Fixture.Save); FSovSaveOperationTestAccess::Observe(*Fixture.Save, Account());
    FSovSaveOperationTestAccess::Tick(*Fixture.Save);
    TestEqual(TEXT("Stale queued work is dropped, not adopted by fresh authorization"), FSovSaveOperationTestAccess::Queued(*Fixture.Save), 0);
    TestEqual(TEXT("Discarding stale queue performs no I/O"), Fixture.Storage->Calls.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveCloudArchiveOwnerTest,
    "ProjectVelkorran.Campaign.Save.CloudArchiveRevocationPreservesNativeBanks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveCloudArchiveOwnerTest::RunTest(const FString&)
{
    FOperationFixture Fixture;
    if (!TestTrue(TEXT("Verified local banks are available"), Fixture.Seed())) { return false; }
    const FString GoodBank = FSovSaveOperationTestAccess::Bank(*Fixture.Save, 1);
    const TArray<uint8> Local = Fixture.Storage->Files.FindChecked(GoodBank);
    TStrongObjectPtr<USovCampaignSaveGame> Remote(FSovSaveOperationTestAccess::Envelope(*Fixture.Save));
    Remote->Header.Generation = 99; Remote->IntegrityChecksum = Remote->CalculateChecksum();
    TArray<uint8> RemoteBytes; UGameplayStatics::SaveGameToMemory(Remote.Get(), RemoteBytes);
    bool bFired = false; int32 CallsAtRevocation = 0;
    Fixture.Storage->AfterCall = [&](const FString& Call)
    {
        if (!bFired && Call.StartsWith(TEXT("Write:")) && Call.Contains(TEXT("_CloudReview_")))
        { bFired = true; CallsAtRevocation = Fixture.Storage->Calls.Num(); FSovSaveOperationTestAccess::Revoke(*Fixture.Save); }
    };
    FString Error;
    TestTrue(TEXT("Revoked cloud archival cannot succeed"), FSovSaveOperationTestAccess::Commit(*Fixture.Save, RemoteBytes, Local, Error) != ESovSaveResult::Success);
    TestTrue(TEXT("Fault occurred inside a real archival storage write"), bFired);
    TestEqual(TEXT("No archive readback or bank write follows revocation"), Fixture.Storage->Calls.Num(), CallsAtRevocation);
    TestTrue(TEXT("Verified local authority remains byte-identical"), Fixture.Storage->Files.FindChecked(GoodBank) == Local);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveStableAccountObservationTest,
    "ProjectVelkorran.Campaign.Save.UnchangedNativeObservationDoesNotCancelWrite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveStableAccountObservationTest::RunTest(const FString&)
{
    FOperationFixture Fixture; if (!Fixture.Storage) { return false; }
    Fixture.Storage->AfterCall = [&](const FString&) { FSovSaveOperationTestAccess::Observe(*Fixture.Save, Account()); };
    TStrongObjectPtr<USovCampaignSaveGame> Candidate(FSovSaveOperationTestAccess::Envelope(*Fixture.Save)); FString Error;
    TestEqual(TEXT("Provider polling without an authorization transition preserves the admitted operation"),
        FSovSaveOperationTestAccess::Write(*Fixture.Save, Candidate.Get(), Error), ESovSaveResult::Success);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveNarrativeDecodeOwnerTest,
    "ProjectVelkorran.Campaign.Save.OperationOwnerConfiguredNarrativeDecode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSaveNarrativeDecodeOwnerTest::RunTest(const FString&)
{
    FSerializationScope Reset;
    FOperationFixture Fixture; if (!Fixture.Storage) { return false; }
    TStrongObjectPtr<USovCampaignSaveGame> Envelope(FSovSaveOperationTestAccess::Envelope(*Fixture.Save));
    TStrongObjectPtr<USovSaveOperationNarrative> Snapshot(NewObject<USovSaveOperationNarrative>());
    if (!TestTrue(TEXT("Configured Narrative subclass serializes normally"),
        UGameplayStatics::SaveGameToMemory(Snapshot.Get(), Envelope->NarrativePayload))) { return false; }
    bool bFired = false;
    USovSaveOperationNarrative::LoadingBoundary = [&]()
    { bFired = true; FSovSaveOperationTestAccess::Revoke(*Fixture.Save); FSovSaveOperationTestAccess::Observe(*Fixture.Save, Account()); };
    FString Error;
    TestNull(TEXT("Revoked configured subclass decode is rejected"), FSovSaveOperationTestAccess::Decode(*Fixture.Save, Envelope.Get(), Error));
    TestTrue(TEXT("Actual Narrative LoadGameFromMemory invoked the fixture callback"), bFired);
    TestTrue(TEXT("Ownership rejection precedes record/asset validation"), Error.Contains(TEXT("ownership")));
    TestEqual(TEXT("No storage access follows configured serializer revocation"), Fixture.Storage->Calls.Num(), 0);
    return true;
}
#endif
