// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Platform/SovPlatformServicesSubsystem.h"
#include "Platform/SovPlatformServicesAdapter.h"
#include "Platform/SovPlatformServicesPolicy.h"
#include "Save/SovSaveSubsystem.h"
#include "HAL/PlatformTime.h"
#include "Misc/SecureHash.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    FString CloudPrefix(ESovSaveSlotKind Kind, int32 Index)
    { return FString::Printf(TEXT("SovCloud1_%d_%d_"), static_cast<int32>(Kind), Index); }
}
void USovPlatformServicesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Saves = Collection.InitializeDependency<USovSaveSubsystem>();
    Adapter = MakeSovConfiguredPlatformAdapter(GetWorld());
    const TWeakObjectPtr<USovPlatformServicesSubsystem> Weak(this);
    Adapter->Start([Weak]() { if (IsInGameThread() && Weak.IsValid()) { Weak->ObserveAccount(); } });
    ObserveAccount();
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &USovPlatformServicesSubsystem::Tick));
}
void USovPlatformServicesSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    bCloudEnabled = false;
    CancelCloudOperation();
    if (Adapter) { Adapter->Stop(); Adapter.Reset(); }
    Saves = nullptr;
    Super::Deinitialize();
}
void USovPlatformServicesSubsystem::RefreshPlatformAccount() { ObserveAccount(); }
void USovPlatformServicesSubsystem::ObserveAccount()
{
    if (!IsInGameThread() || bObservingAccount || !Adapter || !Saves) { return; }
    TGuardValue<bool> Observing(bObservingAccount, true);
    const auto Transport = Adapter;
    TStrongObjectPtr<USovSaveSubsystem> SaveOwner(Saves);
    const auto Account = Transport->GetAccount();
    const bool Changed = ObservedStableId != Account.StableId || ObservedLocalUser != Account.LocalUser
        || bObservedSignedIn != Account.bSignedIn || bObservedCloudAvailable != Account.bCloudAvailable;
    if (Changed)
    {
        bCloudEnabled = false;
        ObservedStableId = Account.StableId; ObservedLocalUser = Account.LocalUser;
        bObservedSignedIn = Account.bSignedIn; bObservedCloudAvailable = Account.bCloudAvailable;
        CancelCloudOperation();
        if (Adapter != Transport || Saves != SaveOwner.Get()) { return; }
    }
    FString Error;
    const bool PreviouslyDeferred = bAccountSelectionDeferred;
    // A rejected switch leaves the old account/paused failure/load entirely owned by the native save subsystem.
    if (Account.bIdentityKnown)
    {
        const bool AlreadySelected = Saves->GetAccountNamespace() == FMD5::HashAnsiString(*ObservedStableId)
            && Saves->GetLocalSaveUserIndex() == ObservedLocalUser;
        bAccountSelectionDeferred = !AlreadySelected && !Saves->SelectPlatformUser(ObservedStableId, ObservedLocalUser, Error);
        Saves->ObservePlatformStorageOwner(ObservedStableId, ObservedLocalUser);
    }
    // Unknown identity preserves the restored hash-only profile and any last confirmed access fence.
    // It does not silently select a fresh empty offline namespace over the existing offline campaign.
    if (Changed || PreviouslyDeferred != bAccountSelectionDeferred)
    {
        const uint64 Published = Publish(bCloudEnabled ? ESovCloudPhase::Idle : ESovCloudPhase::Disabled,
            bAccountSelectionDeferred ? TEXT("Account changed. Current campaign/save transaction keeps its original owner; return to the front end.")
                : TEXT("Platform account observed. Cloud remains optional and requires explicit opt-in."));
        if (StateGeneration == Published && Adapter == Transport && Saves == SaveOwner.Get())
        { OnPlatformAccountChanged.Broadcast(bObservedSignedIn, bAccountSelectionDeferred); }
    }
}
bool USovPlatformServicesSubsystem::SetCloudEnabled(bool Enabled, FString& Error)
{
    if (!Enabled)
    {
        const auto Transport = Adapter;
        bCloudEnabled = false; CancelCloudOperation();
        if (Adapter != Transport || bCloudEnabled) { Error = TEXT("Cloud preference was superseded during notification."); return false; }
        Publish(ESovCloudPhase::Disabled, TEXT("Cloud disabled. Local saves remain available.")); return true;
    }
    ObserveAccount();
    if (!Adapter || !Saves || !bObservedSignedIn || !bObservedCloudAvailable || bAccountSelectionDeferred
        || Saves->GetAccountNamespace() != FMD5::HashAnsiString(*ObservedStableId))
    { Error = TEXT("Cloud is unavailable for the selected account. Offline campaign and local saves remain available."); return false; }
    bCloudEnabled = true; Error.Reset();
    if (Review.Phase != ESovCloudPhase::Reading && Review.Phase != ESovCloudPhase::Writing && Review.Phase != ESovCloudPhase::AwaitingChoice)
    { Publish(ESovCloudPhase::Idle, TEXT("Cloud enabled for this account/session. Choose a slot to compare; no automatic upload or download.")); }
    return true;
}
bool USovPlatformServicesSubsystem::CanUseCloud(FString& Error) const
{
    if (!Adapter || !Saves) { Error = TEXT("Platform services are unavailable."); return false; }
    const auto Live = Adapter->GetAccount();
    const bool Same = Live.StableId == ObservedStableId && Live.LocalUser == ObservedLocalUser
        && Saves->GetAccountNamespace() == FMD5::HashAnsiString(*Live.StableId)
        && Saves->GetLocalSaveUserIndex() == Live.LocalUser;
    const bool Frontend = Saves->CanManagePlatformSaves(Error);
    if (!SovPlatformServicesPolicy::CanAdmit(bCloudEnabled, Live.bSignedIn, Live.bCloudAvailable, Same, Frontend, false))
    { if (Error.IsEmpty()) { Error = TEXT("Cloud requires opt-in and the same signed-in account. Local play does not."); } return false; }
    return true;
}
bool USovPlatformServicesSubsystem::IsCloudAvailable() const
{
    if (!Adapter || !Saves) { return false; }
    const auto Live = Adapter->GetAccount(); FString Error;
    return Live.bSignedIn && Live.bCloudAvailable && !bAccountSelectionDeferred
        && Saves->GetAccountNamespace() == FMD5::HashAnsiString(*Live.StableId)
        && Saves->GetLocalSaveUserIndex() == Live.LocalUser && Saves->CanManagePlatformSaves(Error);
}
bool USovPlatformServicesSubsystem::InspectCloudSlot(ESovSaveSlotKind Kind, int32 Index, FString& Error)
{
    if (Review.Phase == ESovCloudPhase::Reading || Review.Phase == ESovCloudPhase::Writing || Review.Phase == ESovCloudPhase::AwaitingChoice)
    { Error = TEXT("Finish or cancel the current cloud review first."); return false; }
    if (!SovPlatformServicesPolicy::ValidSlot(static_cast<int32>(Kind), Index)) { Error = TEXT("Invalid campaign slot."); return false; }
    if (!CanUseCloud(Error)) { return false; }
    FSovCloudReview Candidate; Candidate.Kind = Kind; Candidate.SlotIndex = Index;
    if (!Saves->ExportPlatformSnapshot(Kind, Index, LocalBytes, Candidate.Local, Candidate.bHasLocal, Error)) { return false; }
    CloudBytes.Reset(); Review = Candidate; Review.RequestId = FGuid::NewGuid();
    OperationNamespace = Saves->GetAccountNamespace(); Deadline = FPlatformTime::Seconds() + SovPlatformServicesPolicy::OperationTimeoutSeconds;
    const FGuid Request = Review.RequestId; const FString Namespace = OperationNamespace;
    const auto Transport = Adapter;
    const TWeakObjectPtr<USovPlatformServicesSubsystem> Weak(this);
    // Set ownership before calling adapters: providers are permitted to complete synchronously.
    Publish(ESovCloudPhase::Reading, TEXT("Comparing local save with the latest published cloud revision. Neither copy is changed."));
    if (Adapter != Transport || !IsCurrentOperation(Request, Namespace, ESovCloudPhase::Reading))
    { Error = TEXT("Cloud compare was cancelled or superseded before dispatch."); return false; }
    if (!Transport->ReadLatest(Request, CloudPrefix(Kind, Index), [Weak, Request, Namespace](bool Good, bool Exists, TArray<uint8> Bytes, FString Message)
        { if (IsInGameThread() && Weak.IsValid()) { Weak->OnCloudRead(Request, Namespace, Good, Exists, MoveTemp(Bytes), MoveTemp(Message)); } }))
    {
        if (Review.RequestId == Request && Review.Phase == ESovCloudPhase::Reading)
        { FinishCloudOperation(Request, Namespace, ESovCloudPhase::Reading, ESovCloudPhase::Failed, TEXT("Provider is unavailable or still draining a cancelled request. Continue offline or retry later.")); }
        Error = Review.Message; return false;
    }
    return true;
}
bool USovPlatformServicesSubsystem::IsCurrentOperation(const FGuid& Request, const FString& Namespace, ESovCloudPhase Phase) const
{
    if (!Saves || !Adapter) { return false; }
    const auto Live = Adapter->GetAccount();
    return SovPlatformServicesPolicy::CanConsume(Review.RequestId.IsValid() && Review.RequestId == Request,
        OperationNamespace == Namespace && Saves->GetAccountNamespace() == Namespace
            && Live.bSignedIn && Live.LocalUser == ObservedLocalUser && FMD5::HashAnsiString(*Live.StableId) == Namespace,
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
        { if (IsInGameThread() && Weak.IsValid()) { Weak->OnCloudWritten(Request, Namespace, Good, MoveTemp(Message)); } }))
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
    Review.RequestId.Invalidate(); Deadline = 0; OperationNamespace.Reset(); LocalBytes.Reset(); CloudBytes.Reset();
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
    LocalBytes.Reset(); CloudBytes.Reset(); Deadline = 0; OperationNamespace.Reset();
    Publish(TerminalPhase, Message);
}
bool USovPlatformServicesSubsystem::Tick(float)
{
    const double Now = FPlatformTime::Seconds();
    if (Now >= NextAccountPoll) { NextAccountPoll = Now + 1.0; ObserveAccount(); }
    if (Deadline > 0 && Now >= Deadline)
    { CancelCloudOperationInternal(ESovCloudPhase::Failed, TEXT("Cloud service timed out. Local saves remain available; provider requests drain before retry.")); }
    return true;
}
