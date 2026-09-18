// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/SovCampaignSaveGame.h"
#include "Containers/Ticker.h"
#include "SovSaveSubsystem.generated.h"
class UNarrativeSave;
class ASovPlayerController;
class USovCampaignDefinition;
class UAbilitySystemComponent;
class APawn;
class APlayerState;
struct FSovObservedPlatformAccount;

/** Testable platform storage seam; production delegates to Unreal's platform save API. */
class PROJECTVELKORRAN_API ISovSaveStorage
{
public:
    virtual ~ISovSaveStorage() = default;
    virtual bool Read(const FString& Slot, int32 User, TArray<uint8>& Bytes) = 0;
    virtual bool Write(const FString& Slot, int32 User, const TArray<uint8>& Bytes) = 0;
    virtual bool Exists(const FString& Slot, int32 User) = 0;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSovSaveCompleted, ESovSaveResult, Result, const FSovSaveSlotHeader&, Slot, const FString&, Message);

/** Native slot policy over Narrative's existing serializer. Offline standalone campaign only. */
UCLASS()
class PROJECTVELKORRAN_API USovSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    /** Platform login owner supplies its stable account ID; stored names contain only its hash. */
    UFUNCTION(BlueprintCallable, Category="Campaign|Save")
    bool SelectPlatformUser(const FString& StablePlatformUserId, int32 LocalUserIndex, FString& Error);
    /** Frontend-only admission shared by account changes and explicit cloud imports. */
    bool CanManagePlatformSaves(FString& Error) const;
    const FString& GetAccountNamespace() const { return AccountNamespace; }
    int32 GetLocalSaveUserIndex() const { return UserIndex; }
    UFUNCTION(BlueprintPure, Category="Campaign|Save")
    bool IsPlatformStorageOwnerAvailable() const { return bPlatformStorageOwnerAvailable && !AccountNamespace.IsEmpty() && UserIndex >= 0; }
    bool IsPlatformStorageSuspended() const { return bPlatformSuspended; }
    /** Suspend does not initiate I/O. It holds native save/load watchdog time until foreground ownership is revalidated. */
    void SetPlatformSuspended(bool bSuspended);
    /** Cloud transports never become save authorities: export only the verified native bank. */
    bool ExportPlatformSnapshot(ESovSaveSlotKind Kind, int32 SlotIndex, TArray<uint8>& Bytes,
        FSovSaveSlotHeader& Header, bool& bExists, FString& Error);
    bool ValidatePlatformSnapshot(const TArray<uint8>& Bytes, ESovSaveSlotKind Kind, int32 SlotIndex,
        FSovSaveSlotHeader& Header, FString& Error) const;
    /** Exact reviewed local bytes must still match. Archives are durable/readback-verified before bank mutation. */
    ESovSaveResult ImportPlatformSnapshot(const TArray<uint8>& Bytes, const TArray<uint8>& ReviewedLocalBytes,
        ESovSaveSlotKind Kind, int32 SlotIndex, FString& Error);
    UFUNCTION(BlueprintCallable, Category="Campaign|Save")
    ESovSaveResult SaveManual(int32 SlotIndex, FString& Error);
    UFUNCTION(BlueprintCallable, Category="Campaign|Save")
    ESovSaveResult WriteCheckpoint(ESovSaveBoundary Boundary, FName BoundaryId, FString& Error);
    /** Deferred until a fully safe state. Native beat/mission/encounter owners supply stable IDs. */
    UFUNCTION(BlueprintCallable, Category="Campaign|Save")
    void QueueAutosave(ESovSaveBoundary Boundary, FName BoundaryId);
    UFUNCTION(BlueprintCallable, Category="Campaign|Save")
    ESovSaveResult LoadSlot(ESovSaveSlotKind Kind, int32 SlotIndex, FString& Error, bool bAcceptRecoveredBank = false);
    UFUNCTION(BlueprintCallable, Category="Campaign|Save")
    TArray<FSovSaveSlotHeader> ListSlots();
    UFUNCTION(BlueprintCallable, Category="Campaign|Save")
    bool FindRecoveryAutosave(FSovSaveSlotHeader& Slot);
    /** Explicit informed continuation after a disk failure; releases only this subsystem's pause. */
    UFUNCTION(BlueprintCallable, Category="Campaign|Save")
    void AcknowledgeSaveFailure();
    /** Retry the already captured safe snapshot; gameplay stays paused until a verified write or explicit continuation. */
    UFUNCTION(BlueprintCallable, Category="Campaign|Save")
    ESovSaveResult RetryFailedWrite(FString& Error);
    /** Native irreversible owner may consume only the exact failed boundary the player acknowledged. */
    bool ConsumeAcknowledgedBoundary(ESovSaveBoundary Boundary, FName BoundaryId);
    /** Every irreversible owner's pre-boundary write: Success after a verified checkpoint, or once for the exact
     * failed boundary the player chose to continue past. Writing directly would ask the same failing storage
     * again after that choice and block the boundary for as long as storage keeps failing. */
    ESovSaveResult EnsureCheckpointBoundary(ESovSaveBoundary Boundary, FName BoundaryId, FString& Error);
    UFUNCTION(BlueprintPure, Category="Campaign|Save")
    bool IsAwaitingFailureDecision() const { return bAwaitingFailureDecision; }
    UFUNCTION(BlueprintPure, Category="Campaign|Save")
    bool IsLoadPending() const { return PendingSave != nullptr || MissionTravelRequest.IsValid(); }
    UFUNCTION(BlueprintPure, Category="Campaign|Save") bool HasTravelRecovery() const;
    UFUNCTION(BlueprintCallable, Category="Campaign|Save") bool RetryTravelRecovery(FString& Error) { return RetryMissionTravelRecovery(Error); }
    UFUNCTION(BlueprintPure, Category="Campaign|Save") double GetCampaignPlaySeconds() const { return PlaySeconds; }
    bool CanCapture(FString& Error) const;
    /** Controller calls after its full managed readiness transaction; failure never becomes load success. */
    void NotifyCampaignReady(ASovPlayerController* Controller, bool bSucceeded, uint64 RestoreEpoch = 0);
    bool BindPendingRestore(ASovPlayerController* Controller, uint64 RestoreEpoch, FString& Error);
    bool ValidateRestoredTravelIdentity(ASovPlayerController* Controller) const;
    /** Used by GameMode to fail closed before spawning a new campaign over a rejected load. */
    bool ValidatePendingWorld(UWorld& World, FString& Error) const;
    /** Retains the verified origin in the GameInstance before a controller requests irreversible travel. */
    bool ArmMissionTravelRecovery(ASovPlayerController* Source, USovCampaignDefinition* Destination, FGuid& Request, FString& Error);
    void CancelMissionTravelRecovery(const FGuid& Request);
    bool OwnsMissionTravelRequest(const FGuid& Request, FString& Error) const;
    TFunction<bool()> CaptureMissionTravelStorageFence(const FGuid& Request) const;
    bool ValidateMissionTravelWorld(UWorld& World, FString& Error);
    UFUNCTION(BlueprintPure, Category="Campaign|Save") bool IsMissionTravelPending() const { return MissionTravelRequest.IsValid(); }
    /** Explicit retry after a failed recovery; never selects a different account's save. */
    UFUNCTION(BlueprintCallable, Category="Campaign|Save") bool RetryMissionTravelRecovery(FString& Error);
    UPROPERTY(BlueprintAssignable, Category="Campaign|Save") FSovSaveCompleted OnSaveCompleted;
    UPROPERTY(BlueprintAssignable, Category="Campaign|Save") FSovSaveCompleted OnLoadCompleted;
protected:
    /** Single engine travel boundary; the retained transaction is established before this callback. */
    virtual void StartMissionRecoveryTravel(UWorld& World, const FString& MapPackage, const FString& Options);
private:
    friend struct FSovTravelTransactionTestAccess;
    friend struct FSovObjectivePresentationTestAccess;
    void ResetRestoreOwner();
    bool MatchesRestoreGenerations() const;
    bool MatchesRestoreOwner(ASovPlayerController* Controller, uint64 RestoreEpoch) const;
    friend struct FSovMissionTravelTestAccess;
    void InitializeMissionTravelRecovery();
    void DeinitializeMissionTravelRecovery();
    void TickMissionTravelRecovery();
    void NotifyMissionTravelReady(ASovPlayerController* Controller, bool bSucceeded);
    void CompleteMissionTravelRecovery(bool bSucceeded, const FString& Error);
    void RecordMissionTravelFailure(UWorld* World, const FString& Error);
    void RecordMissionTravelFailureForRequest(const FGuid& Request, UWorld* World, const FString& Error);
    void ResetMissionTravelRecovery();
    bool BeginMissionOriginRecovery(FString& Error);
    friend class USovPlatformServicesSubsystem;
    friend struct FSovSaveTestAccess;
    friend struct FSovSaveWorldLoadTestAccess;
    friend struct FSovPlatformServicesTestAccess;
    friend struct FSovSaveOperationTestAccess;
    friend struct FSovCheckpointContractTestAccess;
    /** Captured before any callback-capable boundary. Epochs reject authorization/suspend ABA. */
    struct FOperationOwner
    {
        FString Namespace;
        int32 LocalUser = INDEX_NONE;
        uint64 SelectionEpoch = 0;
        uint64 AuthorizationEpoch = 0;
        uint64 SuspensionEpoch = 0;
        bool bAvailable = false;
        bool bRetryOperation = false;
        TWeakObjectPtr<USovCampaignSaveGame> RetrySnapshot;
    };
    FOperationOwner CaptureOperationOwner() const;
    bool IsOperationOwnerCurrent(const FOperationOwner& Owner, FString& Error, bool bRequireAvailable = true) const;
    /** Decoded pending loads do no storage I/O; same-owner suspend/resume holds rather than cancels them. */
    bool IsPendingLoadOwnerCurrent(FString& Error) const;
    bool IsRetainedOwnerCurrent(const FOperationOwner& Owner, FString& Error) const;
    ESovSaveResult OwnershipFailureResult() const;
    bool ReadOwned(const FOperationOwner& Owner, const FString& Slot, int32 LocalUser, TArray<uint8>& Bytes,
        FString& Error, bool bRequireAvailable = true);
    bool WriteOwned(const FOperationOwner& Owner, const FString& Slot, int32 LocalUser, const TArray<uint8>& Bytes,
        FString& Error, bool bRequireAvailable = true);
    bool ExistsOwned(const FOperationOwner& Owner, const FString& Slot, bool& bExists, FString& Error);
    /** Only the native provider observer can establish authorization; never exposed to Blueprint callers. */
    void ObserveNativePlatformAccount(const FSovObservedPlatformAccount& Account);
    struct FQueuedBoundary { ESovSaveBoundary Kind; FName Id; FOperationOwner Owner; };
    bool CanCaptureInternal(FString& Error, bool bAllowEntrySuspension) const;
    ESovSaveResult CaptureAndWrite(ESovSaveSlotKind Kind, int32 SlotIndex, FName BoundaryId, FString& Error, bool bAllowEntrySuspension = false, ESovSaveBoundary Boundary = ESovSaveBoundary::ExplicitCheckpoint);
    ESovSaveResult WriteEnvelope(USovCampaignSaveGame* Save, FString& Error, const FOperationOwner* Operation = nullptr);
    ESovSaveResult CommitPlatformSnapshot(const TArray<uint8>& Bytes, const TArray<uint8>& ReviewedLocalBytes,
        ESovSaveSlotKind Kind, int32 SlotIndex, FString& Error);
    bool RestorePlatformProfileHint(int32 LocalUserIndex);
    bool PersistPlatformProfileHint(const FString& Namespace, int32 LocalUserIndex, FString& Error);
    /** What each bank of a slot holds. A caller that writes needs this to avoid discarding progress. */
    struct FSovSlotBanks
    {
        bool bValid[2] = { false, false };
        /** Intact, but written by a build with a newer save schema. Not damaged, and not spare space. */
        bool bNewerVersion[2] = { false, false };
        int64 Generation[2] = { 0, 0 };
        bool HasNewerVersion() const { return bNewerVersion[0] || bNewerVersion[1]; }
    };
    /** Intact bytes whose schema is ahead of this build. Corruption fails integrity first and is not this. */
    bool IsNewerVersionBank(const USovCampaignSaveGame* Save) const;

    /**
     * What a bank's header said the last time this subsystem read it.
     *
     * A pause-menu refresh listed up to 28 banks by reading and fully deserializing each one, and an
     * envelope is several hundred kilobytes, so printing a column of mission names cost megabytes of
     * work on the game thread (audit AR2-06).
     *
     * Deliberately used for listing only. Loading, writing and bank selection always re-read and
     * re-validate, so a stale entry can misdescribe a slot in a menu and can never decide anything.
     */
    struct FSovBankSummary
    {
        FSovSaveSlotHeader Header;
        /** False records "read, and not a usable bank", which is worth remembering too. */
        bool bUsable = false;
    };
    TMap<FString, FSovBankSummary> BankSummaries;
    void ForgetBankSummary(const FString& BankSlot) { BankSummaries.Remove(BankSlot); }
    void ForgetAllBankSummaries() { BankSummaries.Reset(); }
    USovCampaignSaveGame* ReadBest(ESovSaveSlotKind Kind, int32 Index, int32& OutBank, bool& bDamaged, FString& Error,
        const FOperationOwner* Operation = nullptr, FSovSlotBanks* OutBanks = nullptr);
    bool ValidateEnvelope(USovCampaignSaveGame* Save, bool bValidateAssets, FString& Error, const FOperationOwner* Operation = nullptr) const;
    bool MatchesPendingLoadRequest(const FString& Options) const;
    void CompletePendingLoad(bool bSucceeded, const FString& Error);
    UNarrativeSave* DecodeNarrative(USovCampaignSaveGame* Save, FString& Error, const FOperationOwner* Operation = nullptr) const;
    FString BankName(ESovSaveSlotKind Kind, int32 Index, int32 Bank) const;
    void ResolveInitialSave(UWorld& World, UNarrativeSave*& Snapshot, bool& bOverride);
    void ReportSave(ESovSaveResult Result, const FSovSaveSlotHeader& Header, const FString& Error);
    bool Tick(float DeltaSeconds);
    ASovPlayerController* Controller() const;
    TUniquePtr<ISovSaveStorage> Storage;
    UPROPERTY(Transient) TObjectPtr<USovCampaignSaveGame> PendingSave;
    UPROPERTY(Transient) TObjectPtr<USovCampaignSaveGame> FailedWrite;
    UPROPERTY(Transient) TObjectPtr<UNarrativeSave> PendingNarrative;
    TWeakObjectPtr<UWorld> PendingDestination;
    TWeakObjectPtr<UWorld> RestoreWorld;
    TWeakObjectPtr<ASovPlayerController> RestoreController;
    TWeakObjectPtr<APawn> RestorePawn;
    TWeakObjectPtr<APlayerState> RestorePlayerState;
    TWeakObjectPtr<UAbilitySystemComponent> RestoreASC;
    uint64 PendingRestoreEpoch = 0;
    uint64 RestoreASCEpoch = 0;
    uint64 RestorePawnGeneration = 0;
    TWeakObjectPtr<UWorld> RejectedLoadWorld;
    TWeakObjectPtr<ASovPlayerController> PausedController;
    TArray<FQueuedBoundary> PendingAutosaves;
    FOperationOwner PendingLoadOwner;
    UPROPERTY(Transient) TObjectPtr<USovCampaignSaveGame> MissionTravelOrigin;
    UPROPERTY(Transient) TObjectPtr<UNarrativeSave> MissionTravelNarrative;
    FOperationOwner MissionTravelOwner;
    FGuid MissionTravelRequest;
    FGuid MissionRecoveryRequest;
    FName MissionTravelDestinationId;
    FSoftObjectPath MissionTravelDestinationDefinition;
    FString MissionTravelDestinationMap;
    TWeakObjectPtr<UWorld> MissionTravelSourceWorld;
    TWeakObjectPtr<UWorld> MissionTravelDestinationWorld;
    double MissionTravelDeadline = 0;
    bool bMissionTravelFailurePending = false;
    bool bMissionRecoveryAttempted = false;
    FString MissionTravelError;
    FDelegateHandle MissionTravelFailureHandle;
    FDelegateHandle MissionNetworkFailureHandle;
    FOperationOwner FailedWriteOwner;
    uint64 SelectionEpoch = 1;
    uint64 AuthorizationEpoch = 1;
    uint64 SuspensionEpoch = 1;
    bool bShuttingDown = false;
    FString AccountNamespace;
    FString AuthorizedPlatformId;
    int32 AuthorizedPlatformLocalUser = INDEX_NONE;
    bool bHasNativePlatformAuthorization = false;
    bool bRequiresNativePlatformAuthorization = !PLATFORM_DESKTOP;
    int32 UserIndex = 0;
    double PlaySeconds = 0;
    double PendingLoadDeadline = 0;
    FGuid PendingLoadRequest;
    FString PendingLoadError;
    bool bBusy = false;
    bool bPlatformStorageOwnerAvailable = true;
    bool bPlatformSuspended = false;
    bool bDiscardPlatformResumeDelta = false;
    double PlatformSuspendedAt = 0;
    bool bAwaitingFailureDecision = false;
    bool bOwnPause = false;
    bool bPendingWorldApplied = false;
    bool bPendingLoadFailed = false;
    FSovSaveSlotHeader AcknowledgedBoundary;
    FOperationOwner AcknowledgedOwner;
    TWeakObjectPtr<UWorld> AcknowledgedWorld;
    /** The sealed snapshot the acknowledged write failed to store; a consumed travel boundary keeps it as the travel origin. */
    UPROPERTY(Transient) TObjectPtr<USovCampaignSaveGame> AcknowledgedSnapshot;
    UPROPERTY(Transient) TObjectPtr<USovCampaignSaveGame> ContinuedTravelOrigin;
    FOperationOwner ContinuedTravelOwner;
    FName ContinuedTravelBoundaryId;
    /** The travel origin: a snapshot the player continued past for this exact travel, else the stored checkpoint. Consumes the former. */
    USovCampaignSaveGame* SelectMissionTravelOrigin(const UWorld* SourceWorld, FName DestinationId, const FOperationOwner& Owner, bool& bDamaged, FString& Error);
    TWeakObjectPtr<UWorld> ContinuedTravelWorld;
    double AcknowledgmentExpiresAt = 0;
    FDelegateHandle InitialSaveHandle;
    FTSTicker::FDelegateHandle TickHandle;
};
