// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Platform/SovPlatformServicesSubsystem.h"
#include "Platform/SovPlatformServicesAdapter.h"
#include "Platform/SovPlatformServicesPolicy.h"
#include "Save/SovSaveSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Misc/SecureHash.h"
#include "UObject/StrongObjectPtr.h"
#include "Async/Async.h"

namespace
{
    FString CloudPrefix(ESovSaveSlotKind Kind, int32 Index)
    { return FString::Printf(TEXT("SovCloud1_%d_%d_"), static_cast<int32>(Kind), Index); }
    template<typename WorkType> void DispatchPlatform(TWeakObjectPtr<USovPlatformServicesSubsystem> Weak, WorkType Work)
    {
        auto Run = [Weak, Work = MoveTemp(Work)]() mutable { if (auto* Service = Weak.Get()) { Work(*Service); } };
        if (IsInGameThread()) { Run(); } else { AsyncTask(ENamedThreads::GameThread, MoveTemp(Run)); }
    }
}
void USovPlatformServicesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Saves = Collection.InitializeDependency<USovSaveSubsystem>();
    Adapter = MakeSovConfiguredPlatformAdapter(GetGameInstance());
    const TWeakObjectPtr<USovPlatformServicesSubsystem> Weak(this);
    Adapter->Start([Weak]() { DispatchPlatform(Weak, [](auto& Service) { Service.ObserveAccount(true); }); });
    ObserveAccount();
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USovPlatformServicesSubsystem::Tick));
}
void USovPlatformServicesSubsystem::Deinitialize()
{
    bEnding = true;
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    bCloudEnabled = false;
    CancelCloudOperation();
    if (Adapter) { Adapter->Stop(); Adapter.Reset(); }
    Saves = nullptr;
    Super::Deinitialize();
}
void USovPlatformServicesSubsystem::RefreshPlatformAccount() { ObserveAccount(); }
void USovPlatformServicesSubsystem::ObserveAccount(bool bProviderEvent)
{
    if (bObservingAccount && bProviderEvent && Saves)
    { NextAccountPoll = 0; Saves->FenceUnresolvedAccountChange(); return; }
    if (bEnding || !IsInGameThread() || bObservingAccount || !Adapter || !Saves) { return; }
    TGuardValue<bool> Observing(bObservingAccount, true);
    const auto Transport = Adapter;
    TStrongObjectPtr<USovSaveSubsystem> SaveOwner(Saves);
    const auto Account = Transport->GetAccount();
    if (Adapter != Transport || Saves != SaveOwner.Get()) { return; }
    // Revocation precedes every cancellation/account callback: listeners cannot write under an
    // outgoing user's authorization while this observer is processing a replacement or signout.
    Saves->ObserveNativePlatformAccount(Account);
    if (bEnding || Adapter != Transport || Saves != SaveOwner.Get()) { return; }
    const bool CloudAvailable = Account.bCloudAvailable && !Account.bUsesPlatformManagedCloud;
    const bool Changed = ObservedStableId != Account.StableId || ObservedLocalUser != Account.LocalUser
        || bObservedSignedIn != Account.bSignedIn || bObservedCloudAvailable != CloudAvailable
        || bObservedStorageAuthorized != Account.bStorageAccessAuthorized || bObservedPlatformManagedCloud != Account.bUsesPlatformManagedCloud;
    if (Changed)
    {
        bCloudEnabled = false;
        ObservedStableId = Account.StableId; ObservedLocalUser = Account.LocalUser;
        bObservedSignedIn = Account.bSignedIn; bObservedCloudAvailable = CloudAvailable;
        bObservedStorageAuthorized = Account.bStorageAccessAuthorized; bObservedPlatformManagedCloud = Account.bUsesPlatformManagedCloud;
        CancelCloudOperation();
        if (Adapter != Transport || Saves != SaveOwner.Get()) { return; }
    }
    FString Error;
    const bool PreviouslyDeferred = bAccountSelectionDeferred;
    // A rejected switch leaves the old account/paused failure/load entirely owned by the native save subsystem.
    const bool Authorized = !Account.bRequiresKnownStorageOwner || Account.bStorageAccessAuthorized;
    if (Account.bIdentityKnown && Authorized)
    {
        const bool AlreadySelected = Saves->GetAccountNamespace() == FMD5::HashAnsiString(*ObservedStableId)
            && Saves->GetLocalSaveUserIndex() == ObservedLocalUser;
        bAccountSelectionDeferred = !AlreadySelected && !Saves->SelectPlatformUser(ObservedStableId, ObservedLocalUser, Error);
    }
    else if (Account.bRequiresKnownStorageOwner)
    {
        bAccountSelectionDeferred = true;
    }
    // Unknown identity preserves the restored hash-only profile and any last confirmed access fence.
    // It does not silently select a fresh empty offline namespace over the existing offline campaign.
    if (Changed || PreviouslyDeferred != bAccountSelectionDeferred)
    {
        const FString ProviderRecovery = Transport->GetRecoveryMessage();
        const uint64 Published = Publish(bCloudEnabled ? ESovCloudPhase::Idle : ESovCloudPhase::Disabled,
            !ProviderRecovery.IsEmpty() ? ProviderRecovery
                : !Authorized ? TEXT("The campaign's local platform account is unavailable. Restore that account to continue saving.")
                : bAccountSelectionDeferred ? TEXT("Account changed. Current campaign/save transaction keeps its original owner; return to the front end.")
                : bObservedPlatformManagedCloud ? TEXT("Platform account observed. Manual cloud synchronization is not available on this platform.")
                    : TEXT("Platform account observed. Cloud remains optional and requires explicit opt-in."));
        if (StateGeneration == Published && Adapter == Transport && Saves == SaveOwner.Get())
        { OnPlatformAccountChanged.Broadcast(bObservedSignedIn, bAccountSelectionDeferred); }
    }
}
bool USovPlatformServicesSubsystem::SetCloudEnabled(bool Enabled, FString& Error)
{
    if (bEnding) { Error = TEXT("Platform services are shutting down."); return false; }
    if (!Enabled)
    {
        const auto Transport = Adapter;
        bCloudEnabled = false; CancelCloudOperation();
        if (Adapter != Transport || bCloudEnabled) { Error = TEXT("Cloud preference was superseded during notification."); return false; }
        Publish(ESovCloudPhase::Disabled, TEXT("Cloud disabled. Existing saves are retained.")); return true;
    }
    ObserveAccount();
    if (bObservedPlatformManagedCloud)
    { Error = TEXT("Manual cloud synchronization is not available on this platform."); return false; }
    if (!Adapter || !Saves || !bObservedSignedIn || !bObservedCloudAvailable || bAccountSelectionDeferred
        || Saves->GetAccountNamespace() != FMD5::HashAnsiString(*ObservedStableId))
    { Error = TEXT("Cloud is unavailable for the selected account. Local saving requires the campaign's platform account."); return false; }
    bCloudEnabled = true; Error.Reset();
    if (Review.Phase != ESovCloudPhase::Reading && Review.Phase != ESovCloudPhase::Writing && Review.Phase != ESovCloudPhase::AwaitingChoice)
    { Publish(ESovCloudPhase::Idle, TEXT("Cloud enabled for this account/session. Choose a slot to compare; no automatic upload or download.")); }
    return true;
}
bool USovPlatformServicesSubsystem::CanUseCloud(FString& Error) const
{
    if (bEnding || !Adapter || !Saves) { Error = TEXT("Platform services are unavailable."); return false; }
    const auto Transport = Adapter; TStrongObjectPtr<USovSaveSubsystem> SaveOwner(Saves);
    const auto Live = Transport->GetAccount();
    if (bEnding || Adapter != Transport || Saves != SaveOwner.Get() || !Saves->IsPlatformStorageOwnerAvailable() || Saves->IsPlatformStorageSuspended())
    { Error = TEXT("Platform save ownership changed."); return false; }
    const bool Same = Live.StableId == ObservedStableId && Live.LocalUser == ObservedLocalUser
        && Saves->GetAccountNamespace() == FMD5::HashAnsiString(*Live.StableId)
        && Saves->GetLocalSaveUserIndex() == Live.LocalUser;
    const bool Frontend = Saves->CanManagePlatformSaves(Error);
    if (!SovPlatformServicesPolicy::CanAdmit(bCloudEnabled, Live.bSignedIn,
        Live.bCloudAvailable && !Live.bUsesPlatformManagedCloud, Same, Frontend, false))
    { if (Error.IsEmpty()) { Error = TEXT("Cloud requires opt-in and the same signed-in account. Local play does not."); } return false; }
    return true;
}
bool USovPlatformServicesSubsystem::IsCloudAvailable() const
{
    if (bEnding || !Adapter || !Saves) { return false; }
    const auto Transport = Adapter; TStrongObjectPtr<USovSaveSubsystem> SaveOwner(Saves);
    const auto Live = Transport->GetAccount(); FString Error;
    if (bEnding || Adapter != Transport || Saves != SaveOwner.Get() || !Saves->IsPlatformStorageOwnerAvailable() || Saves->IsPlatformStorageSuspended()) { return false; }
    return Live.bSignedIn && Live.bCloudAvailable && !Live.bUsesPlatformManagedCloud && !bAccountSelectionDeferred
        && Saves->GetAccountNamespace() == FMD5::HashAnsiString(*Live.StableId)
        && Saves->GetLocalSaveUserIndex() == Live.LocalUser && Saves->CanManagePlatformSaves(Error);
}
bool USovPlatformServicesSubsystem::InspectCloudSlot(ESovSaveSlotKind Kind, int32 Index, FString& Error)
{
    if (Review.Phase == ESovCloudPhase::Reading || Review.Phase == ESovCloudPhase::Writing || Review.Phase == ESovCloudPhase::AwaitingChoice)
    { Error = TEXT("Finish or cancel the current cloud review first."); return false; }
    if (!SovPlatformServicesPolicy::ValidSlot(static_cast<int32>(Kind), Index)) { Error = TEXT("Invalid campaign slot."); return false; }
    if (!CanUseCloud(Error)) { return false; }
    const auto Owner = Saves->CaptureStorageOwner();
    FSovCloudReview Candidate; Candidate.Kind = Kind; Candidate.SlotIndex = Index;
    if (!Saves->ExportPlatformSnapshot(Kind, Index, LocalBytes, Candidate.Local, Candidate.bHasLocal, Error)) { return false; }
    if (!Saves->IsStorageOwnerCurrent(Owner)) { Error = TEXT("Platform owner changed while staging the cloud review."); return false; }
    OperationOwner = Owner;
    CloudBytes.Reset(); Revisions.Reset(); ReadingRevisionId.Reset(); RevisionScanIndex = INDEX_NONE;
    Review = Candidate; Review.RequestId = FGuid::NewGuid();
    OperationNamespace = Saves->GetAccountNamespace(); Deadline = FPlatformTime::Seconds() + SovPlatformServicesPolicy::OperationTimeoutSeconds;
    const FGuid Request = Review.RequestId; const FString Namespace = OperationNamespace;
    const auto Transport = Adapter;
    const TWeakObjectPtr<USovPlatformServicesSubsystem> Weak(this);
    // Set ownership before calling adapters: providers are permitted to complete synchronously.
    Publish(ESovCloudPhase::Reading, TEXT("Reading retained cloud revisions for comparison with the local save. Existing copies are retained."));
    if (Adapter != Transport || !IsCurrentOperation(Request, Namespace, ESovCloudPhase::Reading))
    { Error = TEXT("Cloud compare was cancelled or superseded before dispatch."); return false; }
    if (Transport->SupportsRevisionHistory())
    {
        if (!Transport->ListRevisions(Request, CloudPrefix(Kind, Index), [Weak, Request, Namespace](bool Good, TArray<FSovCloudRevisionFile> Files, FString Message)
            { DispatchPlatform(Weak, [Request, Namespace, Good, Files = MoveTemp(Files), Message = MoveTemp(Message)](auto& Service) mutable
              { Service.OnRevisionList(Request, Namespace, Good, MoveTemp(Files), MoveTemp(Message)); }); }))
        { FinishCloudOperation(Request, Namespace, ESovCloudPhase::Reading, ESovCloudPhase::Failed, TEXT("Cloud history is unavailable or a cancelled request is still draining.")); return false; }
        return true;
    }
    if (!Transport->ReadLatest(Request, CloudPrefix(Kind, Index), [Weak, Request, Namespace](bool Good, bool Exists, TArray<uint8> Bytes, FString Message)
        { DispatchPlatform(Weak, [Request, Namespace, Good, Exists, Bytes = MoveTemp(Bytes), Message = MoveTemp(Message)](auto& Service) mutable
          { Service.OnCloudRead(Request, Namespace, Good, Exists, MoveTemp(Bytes), MoveTemp(Message)); }); }))
    {
        if (Review.RequestId == Request && Review.Phase == ESovCloudPhase::Reading)
        { FinishCloudOperation(Request, Namespace, ESovCloudPhase::Reading, ESovCloudPhase::Failed, TEXT("Provider is unavailable or still draining a cancelled request. Continue offline or retry later.")); }
        Error = Review.Message; return false;
    }
    return true;
}
void USovPlatformServicesSubsystem::OnRevisionList(FGuid Request, FString Namespace, bool Good, TArray<FSovCloudRevisionFile> Files, FString Error)
{
    if (!IsCurrentOperation(Request, Namespace, ESovCloudPhase::Reading)) { return; }
    if (!Good || Files.Num() > 128)
    { FinishCloudOperation(Request, Namespace, ESovCloudPhase::Reading, ESovCloudPhase::Failed, Error.IsEmpty() ? TEXT("Cloud history could not be enumerated within its budget.") : Error); return; }
    TSet<FString> Seen;
    for (const auto& File : Files)
    {
        if (File.RevisionId.IsEmpty() || Seen.Contains(File.RevisionId)) { continue; }
        Seen.Add(File.RevisionId); FSovCloudRevision Revision; Revision.RevisionId = File.RevisionId;
        if (File.Size <= 0 || File.Size > SovPlatformServicesPolicy::MaximumEnvelopeBytes)
        { Revision.Message = TEXT("Revision exceeds the supported raw envelope size."); }
        Revisions.Add(MoveTemp(Revision));
    }
    RevisionScanIndex = 0; ReadNextRevision(Request, Namespace);
}
void USovPlatformServicesSubsystem::ReadNextRevision(FGuid Request, const FString& Namespace)
{
    if (!IsCurrentOperation(Request, Namespace, ESovCloudPhase::Reading)) { return; }
    while (Revisions.IsValidIndex(RevisionScanIndex) && !Revisions[RevisionScanIndex].Message.IsEmpty()) { ++RevisionScanIndex; }
    if (!Revisions.IsValidIndex(RevisionScanIndex))
    {
        RevisionScanIndex = INDEX_NONE; ReadingRevisionId.Reset(); Deadline = 0;
        int32 Valid = 0; FString Only;
        for (const auto& Revision : Revisions) { if (Revision.bValid) { ++Valid; Only = Revision.RevisionId; } }
        Review.bHasCloud = Valid > 0; Review.bRevisionSelectionRequired = Valid > 0;
        Publish(ESovCloudPhase::AwaitingChoice, Valid ? TEXT("Choose a verified cloud revision. Client clocks never select the save for import.")
            : TEXT("No compatible cloud revision is available. Existing files are retained; local saving remains available."));
        if (Valid == 1 && IsCurrentOperation(Request, Namespace, ESovCloudPhase::AwaitingChoice))
        { FString Error; SelectCloudRevision(Request, Only, Error); }
        return;
    }
    ReadingRevisionId = Revisions[RevisionScanIndex].RevisionId;
    const FString Revision = ReadingRevisionId;
    const auto Transport = Adapter; const TWeakObjectPtr<USovPlatformServicesSubsystem> Weak(this);
    Deadline = FPlatformTime::Seconds() + SovPlatformServicesPolicy::OperationTimeoutSeconds;
    if (!Transport->ReadRevision(Request, CloudPrefix(Review.Kind, Review.SlotIndex), Revision,
        [Weak, Request, Namespace, Revision](bool Good, bool Exists, TArray<uint8> Bytes, FString Error)
        { DispatchPlatform(Weak, [Request, Namespace, Revision, Good, Exists, Bytes = MoveTemp(Bytes), Error = MoveTemp(Error)](auto& Service) mutable
          { Service.OnRevisionRead(Request, Namespace, Revision, Good, Exists, MoveTemp(Bytes), MoveTemp(Error)); }); }))
    { FinishCloudOperation(Request, Namespace, ESovCloudPhase::Reading, ESovCloudPhase::Failed, TEXT("Provider could not read the requested cloud history revision.")); }
}
void USovPlatformServicesSubsystem::OnRevisionRead(FGuid Request, FString Namespace, FString RevisionId,
    bool Good, bool Exists, TArray<uint8> Bytes, FString Error)
{
    if (!IsCurrentOperation(Request, Namespace, ESovCloudPhase::Reading) || ReadingRevisionId != RevisionId) { return; }
    FSovSaveSlotHeader Header;
    const bool Valid = Good && Exists && Saves->ValidatePlatformSnapshot(Bytes, Review.Kind, Review.SlotIndex, Header, Error);
    if (!IsCurrentOperation(Request, Namespace, ESovCloudPhase::Reading) || ReadingRevisionId != RevisionId) { return; }
    auto* Entry = Revisions.FindByPredicate([&](const auto& Value) { return Value.RevisionId == RevisionId; });
    if (!Entry) { return; }
    Entry->bValid = Valid; Entry->Header = Header; Entry->Message = Valid ? FString() : Error.IsEmpty() ? TEXT("Revision is missing or incompatible.") : Error;
    if (RevisionScanIndex != INDEX_NONE)
    {
        ++RevisionScanIndex;
        // Do not retain all remote payloads or recursively scan synchronous providers on one stack.
        const TWeakObjectPtr<USovPlatformServicesSubsystem> Weak(this);
        AsyncTask(ENamedThreads::GameThread, [Weak, Request, Namespace]()
        { if (auto* Service = Weak.Get()) { Service->ReadNextRevision(Request, Namespace); } });
        return;
    }
    Deadline = 0; ReadingRevisionId.Reset();
    Review.SelectedRevisionId = Valid ? RevisionId : FString(); Review.bRevisionSelectionRequired = !Valid;
    Review.Cloud = Header; CloudBytes = Valid ? MoveTemp(Bytes) : TArray<uint8>();
    Review.bCopiesIdentical = Valid && Review.bHasLocal && CloudBytes == LocalBytes;
    Publish(ESovCloudPhase::AwaitingChoice, Valid ? TEXT("Exact cloud revision selected and verified. Choose Use Cloud to import, Keep Local to publish, or Cancel.") : Entry->Message);
}
bool USovPlatformServicesSubsystem::SelectCloudRevision(FGuid Request, const FString& RevisionId, FString& Error)
{
    const FString RequestedRevision = RevisionId;
    if (!IsCurrentOperation(Request, OperationNamespace, ESovCloudPhase::AwaitingChoice) || !CanUseCloud(Error)) { return false; }
    const auto* Entry = Revisions.FindByPredicate([&](const auto& Value) { return Value.RevisionId == RevisionId && Value.bValid; });
    if (!Entry) { Error = TEXT("Select a compatible revision from this exact review."); return false; }
    const FString Namespace = OperationNamespace;
    RevisionScanIndex = INDEX_NONE; ReadingRevisionId = RevisionId; CloudBytes.Reset(); Review.SelectedRevisionId.Reset();
    Review.bRevisionSelectionRequired = true; Deadline = FPlatformTime::Seconds() + SovPlatformServicesPolicy::OperationTimeoutSeconds;
    const auto Transport = Adapter; const TWeakObjectPtr<USovPlatformServicesSubsystem> Weak(this);
    Publish(ESovCloudPhase::Reading, TEXT("Reading the explicitly selected cloud revision."));
    if (Adapter != Transport || !IsCurrentOperation(Request, Namespace, ESovCloudPhase::Reading)) { return false; }
    if (!Transport->ReadRevision(Request, CloudPrefix(Review.Kind, Review.SlotIndex), RequestedRevision,
        [Weak, Request, Namespace, RequestedRevision](bool Good, bool Exists, TArray<uint8> Bytes, FString Message)
        { DispatchPlatform(Weak, [Request, Namespace, RequestedRevision, Good, Exists, Bytes = MoveTemp(Bytes), Message = MoveTemp(Message)](auto& Service) mutable
          { Service.OnRevisionRead(Request, Namespace, RequestedRevision, Good, Exists, MoveTemp(Bytes), MoveTemp(Message)); }); }))
    { FinishCloudOperation(Request, Namespace, ESovCloudPhase::Reading, ESovCloudPhase::Failed, TEXT("Provider could not read the chosen revision.")); return false; }
    Error.Reset(); return true;
}
bool USovPlatformServicesSubsystem::DeleteCloudRevision(FGuid Request, const FString& RevisionId, FString& Error)
{
    const FString RequestedRevision = RevisionId;
    if (!IsCurrentOperation(Request, OperationNamespace, ESovCloudPhase::AwaitingChoice) || !CanUseCloud(Error)) { return false; }
    if (!Revisions.ContainsByPredicate([&](const auto& Revision) { return Revision.RevisionId == RevisionId; }))
    { Error = TEXT("The requested revision is not part of this review."); return false; }
    TArray<uint8> Current; FSovSaveSlotHeader Header; bool Exists = false;
    if (!Saves->ExportPlatformSnapshot(Review.Kind, Review.SlotIndex, Current, Header, Exists, Error)) { return false; }
    int32 OtherGood = 0;
    for (const auto& Revision : Revisions) { if (Revision.bValid && Revision.RevisionId != RevisionId) { ++OtherGood; } }
    if ((!Exists || Current != LocalBytes) && OtherGood < 2)
    { Error = TEXT("Keep a verified local copy or at least two other compatible cloud revisions before deleting this one."); return false; }
    const FString Namespace = OperationNamespace; const auto Transport = Adapter;
    const TWeakObjectPtr<USovPlatformServicesSubsystem> Weak(this);
    Deadline = FPlatformTime::Seconds() + SovPlatformServicesPolicy::OperationTimeoutSeconds;
    Publish(ESovCloudPhase::Writing, TEXT("Deleting the explicitly confirmed cloud revision. Local banks are retained."));
    if (Adapter != Transport || !IsCurrentOperation(Request, Namespace, ESovCloudPhase::Writing)) { return false; }
    if (!Transport->DeleteRevision(Request, CloudPrefix(Review.Kind, Review.SlotIndex), RequestedRevision,
        [Weak, Request, Namespace](bool Good, FString Message)
        { DispatchPlatform(Weak, [Request, Namespace, Good, Message = MoveTemp(Message)](auto& Service) mutable
          { Service.FinishCloudOperation(Request, Namespace, ESovCloudPhase::Writing, Good ? ESovCloudPhase::Completed : ESovCloudPhase::Failed,
              Good ? TEXT("Revision deletion acknowledged. Compare again to refresh history.") : Message); }); }))
    { FinishCloudOperation(Request, Namespace, ESovCloudPhase::Writing, ESovCloudPhase::Failed, TEXT("Provider rejected deletion.")); return false; }
    Error.Reset(); return true;
}
bool USovPlatformServicesSubsystem::IsCurrentOperation(const FGuid& Request, const FString& Namespace, ESovCloudPhase Phase) const
{
    if (bEnding || !Saves || !Adapter) { return false; }
    const auto Transport = Adapter; TStrongObjectPtr<USovSaveSubsystem> SaveOwner(Saves);
    const auto Live = Transport->GetAccount();
    if (bEnding || Adapter != Transport || Saves != SaveOwner.Get() || !Saves->IsPlatformStorageOwnerAvailable() || Saves->IsPlatformStorageSuspended()) { return false; }
    return SovPlatformServicesPolicy::CanConsume(Saves->IsStorageOwnerCurrent(OperationOwner) && Review.RequestId.IsValid() && Review.RequestId == Request,
        OperationNamespace == Namespace && Saves->GetAccountNamespace() == Namespace
            && Live.bSignedIn && Live.bCloudAvailable && !Live.bUsesPlatformManagedCloud
            && Live.LocalUser == ObservedLocalUser && FMD5::HashAnsiString(*Live.StableId) == Namespace,
        Review.Phase == Phase, bCloudEnabled);
}
void USovPlatformServicesSubsystem::OnCloudRead(FGuid Request, FString Namespace, bool Good, bool Exists, TArray<uint8> Bytes, FString Error)
{
    if (!IsInGameThread() || !IsCurrentOperation(Request, Namespace, ESovCloudPhase::Reading)) { return; }
    if (!Good) { FinishCloudOperation(Request, Namespace, ESovCloudPhase::Reading, ESovCloudPhase::Failed, Error.IsEmpty() ? TEXT("Cloud read failed. Local save was not changed.") : Error); return; }
    FSovSaveSlotHeader CloudHeader;
    if (Exists && !Saves->ValidatePlatformSnapshot(Bytes, Review.Kind, Review.SlotIndex, CloudHeader, Error))
    { FinishCloudOperation(Request, Namespace, ESovCloudPhase::Reading, ESovCloudPhase::Failed, Error); return; }
    if (!IsCurrentOperation(Request, Namespace, ESovCloudPhase::Reading)) { return; }
    Review.Cloud = CloudHeader; Review.bHasCloud = Exists; CloudBytes = MoveTemp(Bytes);
    Review.bCopiesIdentical = Review.bHasLocal && Exists && LocalBytes == CloudBytes;
    Deadline = 0;
    Publish(ESovCloudPhase::AwaitingChoice, TEXT("Choose Keep Local to publish a new cloud revision, Use Cloud to import the reviewed copy, or Cancel. No timestamp chooses for you."));
}
bool USovPlatformServicesSubsystem::ResolveCloudReview(FGuid Request, ESovCloudChoice Choice, FString& Error)
{
    if (!IsCurrentOperation(Request, OperationNamespace, ESovCloudPhase::AwaitingChoice))
    { Error = TEXT("This cloud review is stale or belongs to another account."); return false; }
    if (Choice == ESovCloudChoice::Cancel) { CancelCloudOperation(); return true; }
    if (!CanUseCloud(Error)) { return false; }
    FSovSaveSlotHeader CurrentHeader; bool Exists; TArray<uint8> Current;
    if (!Saves->ExportPlatformSnapshot(Review.Kind, Review.SlotIndex, Current, CurrentHeader, Exists, Error)) { return false; }
    if (!IsCurrentOperation(Request, OperationNamespace, ESovCloudPhase::AwaitingChoice))
    { Error = TEXT("Cloud review was cancelled while checking local data."); return false; }
    if (Current != LocalBytes)
    { Error = TEXT("Local save changed since review. Cancel and compare again."); return false; }
    if (Choice == ESovCloudChoice::UseCloud)
    {
        if (Review.bRevisionSelectionRequired) { Error = TEXT("Choose and verify a specific cloud revision before importing."); return false; }
        if (!Review.bHasCloud) { Error = TEXT("There is no reviewed cloud copy to import."); return false; }
        const FString Namespace = OperationNamespace;
        const TArray<uint8> ReviewedCloud = CloudBytes, ReviewedLocal = LocalBytes;
        const auto Kind = Review.Kind; const int32 Index = Review.SlotIndex;
        TStrongObjectPtr<USovSaveSubsystem> SaveOwner(Saves);
        Publish(ESovCloudPhase::Writing, TEXT("Importing the explicitly reviewed cloud copy through native save validation."));
        if (Saves != SaveOwner.Get() || !IsCurrentOperation(Request, Namespace, ESovCloudPhase::Writing))
        { Error = TEXT("Cloud import was cancelled before local commit."); return false; }
        const auto Result = SaveOwner->ImportPlatformSnapshot(ReviewedCloud, ReviewedLocal, Kind, Index, Error);
        FinishCloudOperation(Request, Namespace, ESovCloudPhase::Writing,
            Result == ESovSaveResult::Success ? ESovCloudPhase::Completed : ESovCloudPhase::Failed,
            Result == ESovSaveResult::Success ? TEXT("Reviewed cloud copy committed locally. Previous local copy and both reviewed archives are retained.") : Error);
        return Result == ESovSaveResult::Success;
    }
    if (Choice != ESovCloudChoice::KeepLocal || !Review.bHasLocal)
    { Error = TEXT("There is no reviewed local copy to publish."); return false; }
    const FString Namespace = OperationNamespace;
    const auto Transport = Adapter;
    const TArray<uint8> UploadBytes = LocalBytes;
    const FString Prefix = CloudPrefix(Review.Kind, Review.SlotIndex);
    const TWeakObjectPtr<USovPlatformServicesSubsystem> Weak(this);
    Deadline = FPlatformTime::Seconds() + SovPlatformServicesPolicy::OperationTimeoutSeconds;
    Publish(ESovCloudPhase::Writing, TEXT("Publishing a new immutable cloud revision. Existing local and cloud copies are retained."));
    if (Adapter != Transport || !IsCurrentOperation(Request, Namespace, ESovCloudPhase::Writing))
    { Error = TEXT("Cloud upload was cancelled or superseded before dispatch."); return false; }
    if (!Transport->WriteRevision(Request, Prefix, UploadBytes,
        [Weak, Request, Namespace](bool Good, FString Message)
        { DispatchPlatform(Weak, [Request, Namespace, Good, Message = MoveTemp(Message)](auto& Service) mutable
          { Service.OnCloudWritten(Request, Namespace, Good, MoveTemp(Message)); }); }))
    {
        if (Review.RequestId == Request && Review.Phase == ESovCloudPhase::Writing)
        { FinishCloudOperation(Request, Namespace, ESovCloudPhase::Writing, ESovCloudPhase::Failed, TEXT("Provider did not accept the upload. Local save is unchanged.")); }
        Error = Review.Message; return false;
    }
    return true;
}
void USovPlatformServicesSubsystem::OnCloudWritten(FGuid Request, FString Namespace, bool Good, FString Error)
{
    if (!IsInGameThread()) { return; }
    FinishCloudOperation(Request, Namespace, ESovCloudPhase::Writing, Good ? ESovCloudPhase::Completed : ESovCloudPhase::Failed,
        Good ? TEXT("New cloud revision acknowledged and readback verified. Local save remains authoritative.")
            : (Error.IsEmpty() ? TEXT("Upload was not confirmed. Local and older cloud copies are retained.") : Error));
}
void USovPlatformServicesSubsystem::CancelCloudOperation()
{
    CancelCloudOperationInternal(ESovCloudPhase::Cancelled,
        TEXT("Cloud operation cancelled. An already-issued upload may exist remotely; no local save was changed."));
}
void USovPlatformServicesSubsystem::CancelCloudOperationInternal(ESovCloudPhase TerminalPhase, const FString& Message)
{
    const auto Transport = Adapter;
    const FGuid Request = Review.RequestId;
    const bool Active = Review.Phase == ESovCloudPhase::Reading || Review.Phase == ESovCloudPhase::Writing || Review.Phase == ESovCloudPhase::AwaitingChoice;
    // Invalidate first because provider cancellation may synchronously invoke a terminal delegate.
    Review.RequestId.Invalidate(); Deadline = 0; OperationNamespace.Reset(); OperationOwner = {}; LocalBytes.Reset(); CloudBytes.Reset();
    Revisions.Reset(); RevisionScanIndex = INDEX_NONE; ReadingRevisionId.Reset();
    const uint64 CancelGeneration = ++StateGeneration;
    if (Active)
    {
        Review.Phase = TerminalPhase;
        Review.Message = Message;
    }
    const FSovCloudReview CancelledReview = Review;
    if (Transport && Request.IsValid()) { Transport->Cancel(Request); }
    if (Active && StateGeneration == CancelGeneration && Adapter == Transport) { OnCloudReviewChanged.Broadcast(CancelledReview); }
}
uint64 USovPlatformServicesSubsystem::Publish(ESovCloudPhase Phase, const FString& Message)
{
    const uint64 Generation = ++StateGeneration;
    Review.Phase = Phase; Review.Message = Message;
    const FSovCloudReview Snapshot = Review; OnCloudReviewChanged.Broadcast(Snapshot); return Generation;
}
void USovPlatformServicesSubsystem::FinishCloudOperation(FGuid Request, const FString& Namespace,
    ESovCloudPhase ExpectedPhase, ESovCloudPhase TerminalPhase, const FString& Message)
{
    if (!IsCurrentOperation(Request, Namespace, ExpectedPhase)) { return; }
    // Retire every resource before terminal listeners may open a successor review.
    LocalBytes.Reset(); CloudBytes.Reset(); Deadline = 0; OperationNamespace.Reset(); OperationOwner = {};
    Publish(TerminalPhase, Message);
}
bool USovPlatformServicesSubsystem::Tick(float)
{
    const double Now = FPlatformTime::Seconds();
    if (Now >= NextAccountPoll) { NextAccountPoll = Now + 1.0; ObserveAccount(); }
    const bool Active = Review.Phase == ESovCloudPhase::Reading || Review.Phase == ESovCloudPhase::Writing || Review.Phase == ESovCloudPhase::AwaitingChoice;
    if (Active && (!Saves || !Saves->IsStorageOwnerCurrent(OperationOwner)))
    { CancelCloudOperationInternal(ESovCloudPhase::Cancelled, TEXT("Cloud review expired because platform storage ownership changed. Inspect the slot again.")); return true; }
    if (Deadline > 0 && Now >= Deadline)
    { CancelCloudOperationInternal(ESovCloudPhase::Failed, TEXT("Cloud service timed out. Local saves remain available; provider requests drain before retry.")); }
    return true;
}
