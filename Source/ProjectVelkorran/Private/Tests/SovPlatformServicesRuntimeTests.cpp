// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Platform/SovPlatformServicesSubsystem.h"
#include "Platform/SovPlatformServicesAdapter.h"
#include "Tests/SovPlatformServicesTestFixtures.h"
#include "Save/SovSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryWriter.h"
#include "Settings/SovGameUserSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
namespace
{
    class FSovCloudTestStorage final : public ISovSaveStorage
    {
    public:
        TMap<FString, TArray<uint8>> Files;
        bool bFailArchive = false;
        bool bTornBank = false;
        int32 CountCampaignFiles() const
        { int32 Count = 0; for (const auto& Pair : Files) { Count += Pair.Key.Contains(TEXT("SovCampaign_")) ? 1 : 0; } return Count; }
        bool Read(const FString& Slot, int32 User, TArray<uint8>& Bytes) override
        { if (const auto* Data = Files.Find(FString::FromInt(User) + Slot)) { Bytes = *Data; return true; } return false; }
        bool Exists(const FString& Slot, int32 User) override { return Files.Contains(FString::FromInt(User) + Slot); }
        bool Write(const FString& Slot, int32 User, const TArray<uint8>& Bytes) override
        {
            const bool Archive = Slot.Contains(TEXT("_CloudReview_"));
            if (Archive && bFailArchive) { return false; }
            auto& Data = Files.FindOrAdd(FString::FromInt(User) + Slot); Data = Bytes;
            if (!Archive && bTornBank) { Data.SetNum(Data.Num() / 2); }
            return true;
        }
    };
    class FSovCloudTestAdapter final : public ISovPlatformServicesAdapter
    {
    public:
        FSovObservedPlatformAccount Account;
        FAccountChanged Changed;
        TArray<FReadComplete> Reads;
        TArray<FWriteComplete> Writes;
        TArray<TArray<uint8>> Uploaded;
        TArray<FGuid> Cancelled;
        void Start(FAccountChanged Callback) override { Changed = MoveTemp(Callback); }
        void Stop() override { Changed = nullptr; }
        FSovObservedPlatformAccount GetAccount() override { return Account; }
        bool ReadLatest(FGuid, const FString&, FReadComplete Callback) override { Reads.Add(MoveTemp(Callback)); return true; }
        bool WriteRevision(FGuid, const FString&, const TArray<uint8>& Bytes, FWriteComplete Callback) override
        { Uploaded.Add(Bytes); Writes.Add(MoveTemp(Callback)); return true; }
        void Cancel(FGuid Request) override { Cancelled.Add(Request); }
        void ChangeTo(const FString& Id, bool SignedIn)
        { Account.StableId = Id; Account.bIdentityKnown = true; Account.bSignedIn = SignedIn; Account.bCloudAvailable = SignedIn; if (Changed) { Changed(); } }
    };
}
struct FSovPlatformServicesTestAccess
{
    static FSovCloudTestStorage* Initialize(USovPlatformServicesSubsystem& Service, USovSaveSubsystem& Save,
        const TSharedPtr<FSovCloudTestAdapter>& Adapter)
    {
        auto Storage = MakeUnique<FSovCloudTestStorage>(); auto* Result = Storage.Get(); Save.Storage = MoveTemp(Storage);
        Service.Saves = &Save; Service.Adapter = Adapter;
        Adapter->Start([&Service]() { Service.ObserveAccount(); });
        Adapter->ChangeTo(TEXT("TestProvider|AccountA"), true);
        return Result;
    }
    static USovCampaignSaveGame* Envelope(USovSaveSubsystem& Save, const FString& Marker)
    {
        auto* Envelope = NewObject<USovCampaignSaveGame>(&Save);
        Envelope->Header.AccountNamespace = Save.AccountNamespace;
        Envelope->Header.Generation = 1; Envelope->Header.TimestampUtc = FDateTime(2026, 9, 5);
        Envelope->Header.MissionId = TEXT("CloudTestMission"); Envelope->Header.BoundaryId = FName(*Marker);
        Envelope->Header.MapPackage = TEXT("/Game/Tests/CloudMissingMap");
        Envelope->Header.MissionDefinition = FSoftObjectPath(TEXT("/Game/Tests/CloudMissingMission.CloudMissingMission"));
        Envelope->Header.ActiveProtagonist = FSovGameplayTags::Get().Character_Player_Tarrik;
        auto* Narrative = NewObject<UNarrativeSave>(&Save);
        UGameplayStatics::SaveGameToMemory(Narrative, Envelope->NarrativePayload);
        NewObject<USovGameUserSettings>(&Save)->CapturePortableSettings(Envelope->PortableSettings);
        Envelope->IntegrityChecksum = Envelope->CalculateChecksum(); return Envelope;
    }
    static bool Write(USovSaveSubsystem& Save, USovCampaignSaveGame* Envelope)
    { FString Error; return Save.WriteEnvelope(Envelope, Error) == ESovSaveResult::Success; }
    static ESovSaveResult Commit(USovSaveSubsystem& Save, const TArray<uint8>& Remote, const TArray<uint8>& Local, FString& Error)
    { return Save.CommitPlatformSnapshot(Remote, Local, ESovSaveSlotKind::Manual, 0, Error); }
    static void SetPendingLoad(USovSaveSubsystem& Save, USovCampaignSaveGame* Envelope) { Save.PendingSave = Envelope; }
    static void SetFailure(USovSaveSubsystem& Save, bool Value) { Save.bAwaitingFailureDecision = Value; }
    static bool Failure(const USovSaveSubsystem& Save) { return Save.bAwaitingFailureDecision; }
    static bool RestoreHint(USovSaveSubsystem& Save, int32 User) { return Save.RestorePlatformProfileHint(User); }
    static bool PersistHint(USovSaveSubsystem& Save, const FString& Namespace, int32 User, FString& Error)
    { return Save.PersistPlatformProfileHint(Namespace, User, Error); }
    static FSovCloudTestStorage* CopyStorage(USovSaveSubsystem& Save, const FSovCloudTestStorage& Existing)
    { auto Copy = MakeUnique<FSovCloudTestStorage>(); Copy->Files = Existing.Files; auto* Result = Copy.Get(); Save.Storage = MoveTemp(Copy); return Result; }
    static void BindUnknown(USovPlatformServicesSubsystem& Service, USovSaveSubsystem& Save, const TSharedPtr<FSovCloudTestAdapter>& Adapter)
    { Service.Saves = &Save; Service.Adapter = Adapter; Service.ObserveAccount(); }
    static void Expire(USovPlatformServicesSubsystem& Service)
    { Service.Deadline = FPlatformTime::Seconds() - 1.; Service.Tick(0.1f); }
    static void SaveTick(USovSaveSubsystem& Save, float Seconds) { Save.Tick(Seconds); }
    static double PrepareSuspendDeadline(USovSaveSubsystem& Save)
    { Save.PendingLoadDeadline = FPlatformTime::Seconds() + 100.; return Save.PendingLoadDeadline; }
    static void SimulateSuspendedTime(USovSaveSubsystem& Save, double Seconds) { Save.PlatformSuspendedAt -= Seconds; }
    static double SaveDeadline(const USovSaveSubsystem& Save) { return Save.PendingLoadDeadline; }
    static bool DiscardResumeDelta(const USovSaveSubsystem& Save) { return Save.bDiscardPlatformResumeDelta; }
    static void RequireNativeOwner(USovSaveSubsystem& Save)
    { Save.bRequiresNativePlatformAuthorization = true; Save.bPlatformStorageOwnerAvailable = false; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovConsoleStorageOwnerRuntime, "ProjectVelkorran.Campaign.PlatformServices.ConsoleOwnerRoutingAndSignout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovConsoleStorageOwnerRuntime::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> Save(NewObject<USovSaveSubsystem>(Instance.Get()));
    TStrongObjectPtr<USovPlatformServicesSubsystem> Service(NewObject<USovPlatformServicesSubsystem>(Instance.Get()));
    FString Error; FSovCloudTestStorage EmptyStorage;
    auto* UnselectedStorage = FSovPlatformServicesTestAccess::CopyStorage(*Save, EmptyStorage);
    FSovPlatformServicesTestAccess::RequireNativeOwner(*Save);
    TestFalse(TEXT("Public Blueprint selection cannot authorize an invented console user before provider readiness"),
        Save->SelectPlatformUser(TEXT("Offline.LocalProfile.0"), 0, Error));
    TestEqual(TEXT("Unauthenticated selection never touches profile-hint storage"), UnselectedStorage->Files.Num(), 0);
    auto Adapter = MakeShared<FSovCloudTestAdapter>();
    Adapter->Account.LocalUser = 3; Adapter->Account.bRequiresKnownStorageOwner = true;
    Adapter->Account.bStorageAccessAuthorized = true; Adapter->Account.bUsesPlatformManagedCloud = true;
    auto* Storage = FSovPlatformServicesTestAccess::Initialize(*Service, *Save, Adapter);
    const FString Original = Save->GetAccountNamespace();
    const int32 BeforeForgedSelection = Storage->Files.Num();
    TestFalse(TEXT("A caller cannot select a different ID despite an authorized provider account"),
        Save->SelectPlatformUser(TEXT("InventedOwner"), 3, Error));
    TestFalse(TEXT("A caller cannot route the authorized ID into user-zero storage"),
        Save->SelectPlatformUser(TEXT("TestProvider|AccountA"), 0, Error));
    TestEqual(TEXT("Rejected owner/index pairs perform no profile writes"), Storage->Files.Num(), BeforeForgedSelection);
    TestEqual(TEXT("The actual platform user is retained instead of player zero"), Save->GetLocalSaveUserIndex(), 3);
    TestTrue(TEXT("Confirmed owner can write a real native bank"),
        FSovPlatformServicesTestAccess::Write(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("console-owner"))));
    for (const auto& Pair : Storage->Files)
    { TestTrue(TEXT("Every native bank and profile operation uses user-three storage"), Pair.Key.StartsWith(TEXT("3Sov"))); }
    TestTrue(TEXT("Manual cloud is explicitly platform-managed"), Service->IsCloudManagedByPlatform());
    TestFalse(TEXT("A provider cloud interface cannot accidentally enable generic console cloud"), Service->IsCloudAvailable());
    TestFalse(TEXT("Console opt-in is rejected before any generic provider I/O"), Service->SetCloudEnabled(true, Error));
    Adapter->Account.bSignedIn = false; Adapter->Account.bCloudAvailable = false;
    Service->RefreshPlatformAccount();
    TestTrue(TEXT("A confirmed offline local profile retains native save access"), Save->IsPlatformStorageOwnerAvailable());
    FSovPlatformServicesTestAccess::SetPendingLoad(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("pending")));
    Adapter->Account.bIdentityKnown = false; Adapter->Account.bStorageAccessAuthorized = false;
    Service->RefreshPlatformAccount();
    TestFalse(TEXT("Unknown console identity immediately fences native storage"), Save->IsPlatformStorageOwnerAvailable());
    TestFalse(TEXT("Public selection cannot restore a revoked console authorization latch"),
        Save->SelectPlatformUser(TEXT("TestProvider|AccountA"), 3, Error));
    TestEqual(TEXT("Unknown identity never relabels outgoing campaign"), Save->GetAccountNamespace(), Original);
    TestTrue(TEXT("In-flight restore is retained for the original owner"), Save->IsLoadPending());
    TestFalse(TEXT("Native bank writer also enforces the signout fence"),
        FSovPlatformServicesTestAccess::Write(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("forbidden"))));
    Adapter->Account.bIdentityKnown = true; Adapter->Account.bStorageAccessAuthorized = true;
    Service->RefreshPlatformAccount();
    TestTrue(TEXT("Returning original offline owner restores access without cancelling load"), Save->IsPlatformStorageOwnerAvailable());
    Adapter->Account.LocalUser = 1; Adapter->ChangeTo(TEXT("TestProvider|AccountB"), true);
    TestFalse(TEXT("Different platform owner cannot reuse outgoing storage during a load"), Save->IsPlatformStorageOwnerAvailable());
    TestEqual(TEXT("Different owner preserves original user compartment"), Save->GetLocalSaveUserIndex(), 3);
    FSovPlatformServicesTestAccess::SetPendingLoad(*Save, nullptr); Service->RefreshPlatformAccount();
    TestEqual(TEXT("Front end may adopt a confirmed replacement platform user"), Save->GetLocalSaveUserIndex(), 1);
    Adapter->Account.bStorageAccessAuthorized = false; Service->RefreshPlatformAccount();
    TestFalse(TEXT("With no transaction pending, public selection still cannot restore revoked native authorization"),
        Save->SelectPlatformUser(TEXT("TestProvider|AccountB"), 1, Error));
    TestFalse(TEXT("Rejected reauthorization leaves native storage fenced"), Save->IsPlatformStorageOwnerAvailable());
    TestEqual(TEXT("No generic cloud I/O performed on a managed platform"), Adapter->Reads.Num() + Adapter->Writes.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlatformRevocationReentryRuntime, "ProjectVelkorran.Campaign.PlatformServices.RevokeBeforeCloudCancellationCallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlatformRevocationReentryRuntime::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> Save(NewObject<USovSaveSubsystem>(Instance.Get()));
    TStrongObjectPtr<USovPlatformServicesSubsystem> Service(NewObject<USovPlatformServicesSubsystem>(Instance.Get()));
    TStrongObjectPtr<USovCloudReentryProbe> Listener(NewObject<USovCloudReentryProbe>());
    auto Adapter = MakeShared<FSovCloudTestAdapter>(); auto* Storage = FSovPlatformServicesTestAccess::Initialize(*Service, *Save, Adapter);
    TestTrue(TEXT("Initial outgoing-owner bank is written"),
        FSovPlatformServicesTestAccess::Write(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("original"))));
    FString Error; Service->SetCloudEnabled(true, Error);
    TestTrue(TEXT("Create an active review that publishes cancellation on owner change"), Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error));
    const int32 BeforeBanks = Storage->CountCampaignFiles(); bool bCallbackRan = false, bWriteSucceeded = false, bOwnerAvailable = true;
    Listener->Service = Service.Get();
    Listener->CancelAction = [&]()
    {
        bCallbackRan = true; bOwnerAvailable = Save->IsPlatformStorageOwnerAvailable();
        bWriteSucceeded = FSovPlatformServicesTestAccess::Write(*Save,
            FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("forbidden-cancellation-write")));
    };
    Service->OnCloudReviewChanged.AddDynamic(Listener.Get(), &USovCloudReentryProbe::OnChanged);
    Adapter->Account.LocalUser = 2; Adapter->ChangeTo(TEXT("TestProvider|Replacement"), true);
    TestTrue(TEXT("The synchronous cancellation listener ran"), bCallbackRan);
    TestFalse(TEXT("Outgoing save access is revoked before listener code executes"), bOwnerAvailable);
    TestFalse(TEXT("Reentrant native-bank write is rejected"), bWriteSucceeded);
    TestEqual(TEXT("No outgoing campaign bank is mutated by cancellation callback"), Storage->CountCampaignFiles(), BeforeBanks);
    TestEqual(TEXT("After callbacks the front end may adopt the confirmed replacement user"), Save->GetLocalSaveUserIndex(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSaveSuspensionRuntime, "ProjectVelkorran.Campaign.PlatformServices.SuspendHoldsStorageAndWatchdogs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovSaveSuspensionRuntime::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> Save(NewObject<USovSaveSubsystem>(Instance.Get()));
    TStrongObjectPtr<USovPlatformServicesSubsystem> Service(NewObject<USovPlatformServicesSubsystem>(Instance.Get()));
    auto Adapter = MakeShared<FSovCloudTestAdapter>(); auto* Storage = FSovPlatformServicesTestAccess::Initialize(*Service, *Save, Adapter);
    const double Before = FSovPlatformServicesTestAccess::PrepareSuspendDeadline(*Save);
    FSovPlatformServicesTestAccess::SetPendingLoad(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("suspended-load")));
    const int32 OriginalFiles = Storage->Files.Num(); Save->SetPlatformSuspended(true);
    FSovPlatformServicesTestAccess::SimulateSuspendedTime(*Save, 3600.);
    Save->SetPlatformSuspended(true); // Duplicate platform delegates cannot restart the suspension clock.
    TestFalse(TEXT("No write dispatch during suspension"),
        FSovPlatformServicesTestAccess::Write(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("suspended"))));
    TestEqual(TEXT("Suspension does not emit a save or autosave"), Storage->Files.Num(), OriginalFiles);
    FSovPlatformServicesTestAccess::SaveTick(*Save, 3600.f);
    TestTrue(TEXT("Suspension retains the owned pending load"), Save->IsLoadPending());
    TestEqual(TEXT("Suspended ticker does not change pending load deadline"), FSovPlatformServicesTestAccess::SaveDeadline(*Save), Before);
    Save->SetPlatformSuspended(false);
    const double After = FSovPlatformServicesTestAccess::SaveDeadline(*Save);
    TestTrue(TEXT("Load watchdog resumes with its remaining foreground budget"), After >= Before + 3600.);
    Save->SetPlatformSuspended(false);
    TestEqual(TEXT("Repeated resume does not extend a deadline again"), FSovPlatformServicesTestAccess::SaveDeadline(*Save), After);
    TestTrue(TEXT("First foreground delta is explicitly discarded"), FSovPlatformServicesTestAccess::DiscardResumeDelta(*Save));
    FSovPlatformServicesTestAccess::SaveTick(*Save, 3600.f);
    TestFalse(TEXT("Discard is consumed exactly once"), FSovPlatformServicesTestAccess::DiscardResumeDelta(*Save));
    FSovPlatformServicesTestAccess::SetPendingLoad(*Save, nullptr);
    TestTrue(TEXT("Confirmed local owner can write again after foreground revalidation"),
        FSovPlatformServicesTestAccess::Write(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("resumed"))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlatformAccountRuntime, "ProjectVelkorran.Campaign.PlatformServices.AccountChangePreservesTransactions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlatformAccountRuntime::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> Save(NewObject<USovSaveSubsystem>(Instance.Get()));
    TStrongObjectPtr<USovPlatformServicesSubsystem> Service(NewObject<USovPlatformServicesSubsystem>(Instance.Get()));
    auto Adapter = MakeShared<FSovCloudTestAdapter>(); FSovPlatformServicesTestAccess::Initialize(*Service, *Save, Adapter);
    const FString Original = Save->GetAccountNamespace(); FString Error;
    TestTrue(TEXT("Explicit opt-in admitted for selected signed-in account"), Service->SetCloudEnabled(true, Error));
    Adapter->ChangeTo(TEXT("TestProvider|AccountA"), false);
    TestEqual(TEXT("NotLoggedIn/network loss does not change local ownership"), Save->GetAccountNamespace(), Original);
    TestFalse(TEXT("Network loss revokes optional cloud"), Service->IsCloudEnabled());
    TestTrue(TEXT("Same owner remains able to write real native save banks offline"),
        FSovPlatformServicesTestAccess::Write(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("offline-write"))));
    Adapter->ChangeTo(TEXT("TestProvider|AccountA"), true); Service->SetCloudEnabled(true, Error);
    FSovPlatformServicesTestAccess::SetPendingLoad(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("pending")));
    Adapter->ChangeTo(TEXT("TestProvider|AccountB"), true);
    TestEqual(TEXT("Account callback cannot relabel pending load"), Save->GetAccountNamespace(), Original);
    TestTrue(TEXT("Pending load remains owned and can finish"), Save->IsLoadPending());
    TestEqual(TEXT("Replacement account cannot write outgoing campaign's native bank"), Save->SaveManual(0, Error), ESovSaveResult::Busy);
    TestTrue(TEXT("New account selection deferred"), Service->IsAccountSelectionDeferred());
    TestFalse(TEXT("Account switch revokes cloud opt-in"), Service->IsCloudEnabled());
    FSovPlatformServicesTestAccess::SetPendingLoad(*Save, nullptr);
    TestEqual(TEXT("Outgoing campaign disk access is fenced after pending load ownership releases"), Save->SaveManual(0, Error), ESovSaveResult::MissingAccount);
    FSovPlatformServicesTestAccess::SetFailure(*Save, true); Service->RefreshPlatformAccount();
    TestEqual(TEXT("Paused failed-write owner is unchanged"), Save->GetAccountNamespace(), Original);
    TestTrue(TEXT("Account observation never acknowledges failed write"), FSovPlatformServicesTestAccess::Failure(*Save));
    FSovPlatformServicesTestAccess::SetFailure(*Save, false); Service->RefreshPlatformAccount();
    TestNotEqual(TEXT("Frontend adopts actual observed account after transaction ends"), Save->GetAccountNamespace(), Original);
    TestFalse(TEXT("Deferred status clears"), Service->IsAccountSelectionDeferred());
    Adapter->ChangeTo(TEXT("Offline.LocalProfile.0"), false);
    TestFalse(TEXT("Offline has no fake cloud"), Service->SetCloudEnabled(true, Error));
    TestFalse(TEXT("Offline does not stage network work"), Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error));
    TestEqual(TEXT("No login, network read or upload during observation"), Adapter->Reads.Num() + Adapter->Writes.Num(), 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCloudOperationRuntime, "ProjectVelkorran.Campaign.PlatformServices.ExactReviewCancelAndProviderCallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCloudOperationRuntime::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> Save(NewObject<USovSaveSubsystem>(Instance.Get()));
    TStrongObjectPtr<USovPlatformServicesSubsystem> Service(NewObject<USovPlatformServicesSubsystem>(Instance.Get()));
    auto Adapter = MakeShared<FSovCloudTestAdapter>(); FSovPlatformServicesTestAccess::Initialize(*Service, *Save, Adapter);
    TestTrue(TEXT("Real native serializer/bank write"), FSovPlatformServicesTestAccess::Write(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("local"))));
    FString Error; Service->SetCloudEnabled(true, Error);
    TestTrue(TEXT("First explicit compare"), Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error));
    const FGuid Old = Service->GetCloudReview().RequestId; Service->CancelCloudOperation();
    TestTrue(TEXT("New review after cancellation"), Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error));
    const FGuid Current = Service->GetCloudReview().RequestId;
    Adapter->Reads[0](true, false, {}, {});
    TestEqual(TEXT("Late callback cannot complete newer request"), Service->GetCloudReview().Phase, ESovCloudPhase::Reading);
    Adapter->Reads[1](true, false, {}, {});
    TestEqual(TEXT("Exact callback stages review"), Service->GetCloudReview().Phase, ESovCloudPhase::AwaitingChoice);
    TestFalse(TEXT("Old UI choice rejected"), Service->ResolveCloudReview(Old, ESovCloudChoice::KeepLocal, Error));
    TestTrue(TEXT("Explicit current KeepLocal reaches platform adapter"), Service->ResolveCloudReview(Current, ESovCloudChoice::KeepLocal, Error));
    TestEqual(TEXT("No success before provider verified completion"), Service->GetCloudReview().Phase, ESovCloudPhase::Writing);
    TestEqual(TEXT("One immutable revision requested"), Adapter->Writes.Num(), 1);
    FSovSaveSlotHeader Header; bool Exists; TArray<uint8> Local;
    Save->ExportPlatformSnapshot(ESovSaveSlotKind::Manual, 0, Local, Header, Exists, Error);
    TestTrue(TEXT("Transport uploads exact verified local bank bytes"), Adapter->Uploaded[0] == Local);
    Adapter->Writes[0](false, TEXT("Injected provider/readback failure"));
    TestEqual(TEXT("Provider failure never becomes save success"), Service->GetCloudReview().Phase, ESovCloudPhase::Failed);
    TestTrue(TEXT("Retry compare"), Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error));
    FSovPlatformServicesTestAccess::Expire(*Service);
    TestEqual(TEXT("Timeout is terminal visible failure"), Service->GetCloudReview().Phase, ESovCloudPhase::Failed);
    Adapter->Reads[2](true, true, Local, {});
    TestEqual(TEXT("Late post-timeout read cannot publish review"), Service->GetCloudReview().Phase, ESovCloudPhase::Failed);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovOfflineProfileRestartRuntime, "ProjectVelkorran.Campaign.PlatformServices.HashOnlyOfflineProfileRestart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovOfflineProfileRestartRuntime::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> FirstInstance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> FirstSave(NewObject<USovSaveSubsystem>(FirstInstance.Get()));
    TStrongObjectPtr<USovPlatformServicesSubsystem> FirstService(NewObject<USovPlatformServicesSubsystem>(FirstInstance.Get()));
    auto FirstAdapter = MakeShared<FSovCloudTestAdapter>(); auto* FirstStorage = FSovPlatformServicesTestAccess::Initialize(*FirstService, *FirstSave, FirstAdapter);
    const FString Original = FirstSave->GetAccountNamespace();
    TestTrue(TEXT("Signed-in session writes native campaign"), FSovPlatformServicesTestAccess::Write(*FirstSave, FSovPlatformServicesTestAccess::Envelope(*FirstSave, TEXT("restart"))));
    TStrongObjectPtr<UGameInstance> RestartInstance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> RestartSave(NewObject<USovSaveSubsystem>(RestartInstance.Get()));
    auto* RestartStorage = FSovPlatformServicesTestAccess::CopyStorage(*RestartSave, *FirstStorage);
    TestTrue(TEXT("New subsystem restores actual stored hint bytes without an online identity"), FSovPlatformServicesTestAccess::RestoreHint(*RestartSave, 0));
    TestEqual(TEXT("Offline restart selects previous hash namespace"), RestartSave->GetAccountNamespace(), Original);
    TStrongObjectPtr<USovPlatformServicesSubsystem> RestartService(NewObject<USovPlatformServicesSubsystem>(RestartInstance.Get()));
    auto UnknownAdapter = MakeShared<FSovCloudTestAdapter>();
    FSovPlatformServicesTestAccess::BindUnknown(*RestartService, *RestartSave, UnknownAdapter);
    TestEqual(TEXT("Unknown startup identity cannot replace restored profile with empty offline profile"), RestartSave->GetAccountNamespace(), Original);
    TArray<uint8> Bytes; FSovSaveSlotHeader Header; bool Exists = false; FString Error;
    TestTrue(TEXT("Previous actual native bank is discoverable offline"), RestartSave->ExportPlatformSnapshot(ESovSaveSlotKind::Manual, 0, Bytes, Header, Exists, Error) && Exists);
    TestTrue(TEXT("Restarted campaign owner can write local bank while identity remains unknown"),
        FSovPlatformServicesTestAccess::Write(*RestartSave, FSovPlatformServicesTestAccess::Envelope(*RestartSave, TEXT("offline-restart-write"))));
    TestFalse(TEXT("Hint never crosses local-user partition"), FSovPlatformServicesTestAccess::RestoreHint(*RestartSave, 1));
    TestFalse(TEXT("Malformed namespace cannot be persisted"), FSovPlatformServicesTestAccess::PersistHint(*RestartSave, TEXT("not-a-hash"), 0, Error));
    int32 HintCount = 0;
    for (auto& Pair : RestartStorage->Files)
    {
        if (!Pair.Key.Contains(TEXT("SovAccount_"))) { continue; }
        ++HintCount;
        TestEqual(TEXT("Hint has only fixed schema/index/generation and two hashes/CRC"), Pair.Value.Num(), 88);
        // Recompute the CRC after forging schema, so schema validation itself must reject it.
        Pair.Value[4] = 99;
        uint32 CRC = FCrc::MemCrc32(Pair.Value.GetData(), Pair.Value.Num() - sizeof(uint32));
        FMemoryWriter Writer(Pair.Value, true); Writer.Seek(Pair.Value.Num() - sizeof(uint32)); Writer << CRC;
    }
    TestEqual(TEXT("Profile selection created one initial verified hint bank"), HintCount, 1);
    TestFalse(TEXT("Unsupported hint schema rejected even with recomputed CRC"), FSovPlatformServicesTestAccess::RestoreHint(*RestartSave, 0));
    TestEqual(TEXT("Rejected hint never mutates already selected owner"), RestartSave->GetAccountNamespace(), Original);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCloudListenerReentryRuntime, "ProjectVelkorran.Campaign.PlatformServices.ListenerReentryBeforeDispatchAndAfterCompletion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCloudListenerReentryRuntime::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> Save(NewObject<USovSaveSubsystem>(Instance.Get()));
    TStrongObjectPtr<USovPlatformServicesSubsystem> Service(NewObject<USovPlatformServicesSubsystem>(Instance.Get()));
    TStrongObjectPtr<USovCloudReentryProbe> Listener(NewObject<USovCloudReentryProbe>());
    auto Adapter = MakeShared<FSovCloudTestAdapter>(); FSovPlatformServicesTestAccess::Initialize(*Service, *Save, Adapter);
    FSovPlatformServicesTestAccess::Write(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("reentry")));
    Listener->Service = Service.Get(); Service->OnCloudReviewChanged.AddDynamic(Listener.Get(), &USovCloudReentryProbe::OnChanged);
    FString Error; Service->SetCloudEnabled(true, Error);
    Listener->bCancelReading = true;
    TestFalse(TEXT("Reading listener may cancel before provider dispatch"), Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error));
    TestEqual(TEXT("Cancelled Reading broadcast never starts provider read"), Adapter->Reads.Num(), 0);
    TestTrue(TEXT("Second review admitted"), Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error));
    Adapter->Reads[0](true, false, {}, {});
    Listener->bCancelWriting = true;
    TestFalse(TEXT("Writing listener may cancel before upload dispatch"), Service->ResolveCloudReview(Service->GetCloudReview().RequestId, ESovCloudChoice::KeepLocal, Error));
    TestEqual(TEXT("Cancelled Writing broadcast never starts upload"), Adapter->Writes.Num(), 0);
    Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error); Adapter->Reads[1](true, false, {}, {});
    Service->ResolveCloudReview(Service->GetCloudReview().RequestId, ESovCloudChoice::KeepLocal, Error);
    Listener->bRestartOnComplete = true; Adapter->Writes[0](true, {});
    TestTrue(TEXT("Completed listener can start successor review"), Listener->bRestartAccepted);
    TestEqual(TEXT("Old terminal callback does not overwrite successor phase"), Service->GetCloudReview().Phase, ESovCloudPhase::Reading);
    Adapter->Reads[2](true, false, {}, {});
    TestTrue(TEXT("Successor local bytes were not cleared by old callback"), Service->ResolveCloudReview(Service->GetCloudReview().RequestId, ESovCloudChoice::KeepLocal, Error));
    TestTrue(TEXT("Successor upload retains actual native bank bytes"), Adapter->Uploaded.Num() == 2 && !Adapter->Uploaded[1].IsEmpty());
    Listener->bRestartOnCancel = true; Service->CancelCloudOperation();
    TestTrue(TEXT("Cancelled listener can start a new review"), Listener->bRestartAccepted);
    TestEqual(TEXT("Old cancellation cannot stomp successor"), Service->GetCloudReview().Phase, ESovCloudPhase::Reading);
    Service->CancelCloudOperation(); Listener->bDeinitializeOnReading = true;
    const int32 BeforeShutdown = Adapter->Reads.Num();
    TestFalse(TEXT("Reading listener may deinitialize subsystem safely"), Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error));
    TestEqual(TEXT("Shutdown before dispatch creates no provider request"), Adapter->Reads.Num(), BeforeShutdown);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCloudSaveAuthorityRuntime, "ProjectVelkorran.Campaign.PlatformServices.NativeBankArchivesAndPreflight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCloudSaveAuthorityRuntime::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> Save(NewObject<USovSaveSubsystem>(Instance.Get()));
    TStrongObjectPtr<USovPlatformServicesSubsystem> Service(NewObject<USovPlatformServicesSubsystem>(Instance.Get()));
    auto Adapter = MakeShared<FSovCloudTestAdapter>(); auto* Storage = FSovPlatformServicesTestAccess::Initialize(*Service, *Save, Adapter);
    FSovPlatformServicesTestAccess::Write(*Save, FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("last-good")));
    TArray<uint8> Local, Remote; FSovSaveSlotHeader Header; bool Exists; FString Error;
    Save->ExportPlatformSnapshot(ESovSaveSlotKind::Manual, 0, Local, Header, Exists, Error);
    auto* Candidate = FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("cloud"));
    UGameplayStatics::SaveGameToMemory(Candidate, Remote);
    TestEqual(TEXT("Public import rejects unavailable required assets before any archive or bank mutation"),
        Save->ImportPlatformSnapshot(Remote, Local, ESovSaveSlotKind::Manual, 0, Error), ESovSaveResult::IncompatibleSave);
    TestEqual(TEXT("Preflight failure retains original sole bank"), Storage->CountCampaignFiles(), 1);
    // Exercise the production commit boundary separately from asset/canon preflight. This fixture deliberately
    // has no authored map, so it must never be accepted by the public ImportPlatformSnapshot path above.
    Storage->bFailArchive = true;
    TestEqual(TEXT("Archive storage denial aborts before native bank write"), FSovPlatformServicesTestAccess::Commit(*Save, Remote, Local, Error), ESovSaveResult::WriteFailed);
    TestEqual(TEXT("Denied archive leaves original bank alone"), Storage->CountCampaignFiles(), 1);
    Storage->bFailArchive = false;
    TestEqual(TEXT("Exact reviewed bytes admit native commit"), FSovPlatformServicesTestAccess::Commit(*Save, Remote, Local, Error), ESovSaveResult::Success);
    TestEqual(TEXT("Both review archives plus both native banks retained"), Storage->CountCampaignFiles(), 4);
    TArray<uint8> Imported; Save->ExportPlatformSnapshot(ESovSaveSlotKind::Manual, 0, Imported, Header, Exists, Error);
    TestEqual(TEXT("Local generation advances locally, ignoring remote generation"), Header.Generation, int64(2));
    TestEqual(TEXT("Imported candidate has expected boundary"), Header.BoundaryId, FName(TEXT("cloud")));
    TestEqual(TEXT("Stale local comparison cannot overwrite imported data"), FSovPlatformServicesTestAccess::Commit(*Save, Remote, Local, Error), ESovSaveResult::Busy);
    TestEqual(TEXT("No additional archives on stale comparison"), Storage->CountCampaignFiles(), 4);
    Storage->bTornBank = true;
    TestEqual(TEXT("Torn native bank write is not success"), FSovPlatformServicesTestAccess::Commit(*Save, Remote, Imported, Error), ESovSaveResult::ReadbackFailed);
    bool FoundGoodBytes = false; for (const auto& Pair : Storage->Files) { FoundGoodBytes |= Pair.Value == Imported; }
    TestTrue(TEXT("Latest good local bank survives failed replacement"), FoundGoodBytes);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCloudValidationRuntime, "ProjectVelkorran.Campaign.PlatformServices.CloudBytesAreUntrusted",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovCloudValidationRuntime::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<USovSaveSubsystem> Save(NewObject<USovSaveSubsystem>(Instance.Get()));
    TStrongObjectPtr<USovPlatformServicesSubsystem> Service(NewObject<USovPlatformServicesSubsystem>(Instance.Get()));
    auto Adapter = MakeShared<FSovCloudTestAdapter>(); FSovPlatformServicesTestAccess::Initialize(*Service, *Save, Adapter);
    auto* Candidate = FSovPlatformServicesTestAccess::Envelope(*Save, TEXT("cloud"));
    TArray<uint8> Bytes; FSovSaveSlotHeader Header; FString Error;
    auto Validate = [&]() { Bytes.Reset(); UGameplayStatics::SaveGameToMemory(Candidate, Bytes); return Save->ValidatePlatformSnapshot(Bytes, ESovSaveSlotKind::Manual, 0, Header, Error); };
    TestTrue(TEXT("Actual serialized compatible envelope can be reviewed"), Validate());
    Candidate->Header.AccountNamespace = TEXT("wrong-owner"); Candidate->IntegrityChecksum = Candidate->CalculateChecksum();
    TestFalse(TEXT("Recomputed checksum cannot bypass account binding"), Validate());
    Candidate->Header.AccountNamespace = Save->GetAccountNamespace(); Candidate->Header.SchemaMajor = 99; Candidate->IntegrityChecksum = Candidate->CalculateChecksum();
    TestFalse(TEXT("Unknown schema rejected"), Validate());
    Candidate->Header.SchemaMajor = 1; Candidate->Header.SlotIndex = 1; Candidate->IntegrityChecksum = Candidate->CalculateChecksum();
    TestFalse(TEXT("Wrong logical slot rejected"), Validate());
    Candidate->Header.SlotIndex = 0; Candidate->IntegrityChecksum = Candidate->CalculateChecksum(); Candidate->NarrativePayload[0] ^= 1;
    TestFalse(TEXT("Payload checksum mutation rejected"), Validate());
    Service->SetCloudEnabled(true, Error); Service->InspectCloudSlot(ESovSaveSlotKind::Manual, 0, Error);
    Adapter->Reads[0](true, true, Bytes, {});
    TestEqual(TEXT("Malformed remote remains failed, never an import choice"), Service->GetCloudReview().Phase, ESovCloudPhase::Failed);
    return true;
}
#endif
