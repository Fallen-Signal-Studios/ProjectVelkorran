// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/SovCampaignSaveGame.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "SovSaveSubsystem.generated.h"
class UNarrativeSave;
class ASovPlayerController;
struct FSovObservedPlatformAccount;
class USovCampaignDefinition;

/** Immutable admission receipt. Namespace alone is insufficient after revoke/re-authorize cycles. */
struct FSovStorageOwnerToken
{
    FString Namespace;
    int32 User = INDEX_NONE;
    uint64 Generation = 0;
    bool bAuthorized = false;
};

/** Testable platform storage seam; production delegates to Unreal's platform save API. */
class PROJECTVELKORRAN_API ISovSaveStorage
{
public:
    virtual ~ISovSaveStorage() = default;
    virtual bool Read(const FString& Slot, int32 User, TArray<uint8>& Bytes) = 0;
    virtual bool Write(const FString& Slot, int32 User, const TArray<uint8>& Bytes) = 0;
    virtual bool Exists(const FString& Slot, int32 User) = 0;
    virtual bool Remove(const FString&, int32) { return false; }
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
    bool IsPlatformStorageOwnerAvailable() const { return !bSessionAbandoned && bPlatformStorageOwnerAvailable && !AccountNamespace.IsEmpty() && UserIndex >= 0; }
    bool IsPlatformStorageSuspended() const { return bPlatformSuspended; }
    FSovStorageOwnerToken CaptureStorageOwner() const;
    bool IsStorageOwnerCurrent(const FSovStorageOwnerToken& Token) const;
    /** Tear down transient ownership without saving or granting a continue-without-save receipt. */
    bool AbandonSessionForTitle(FString& Error);
    bool BeginMissionTravel(USovCampaignDefinition* Destination, FString& Error);
    void CancelMissionTravel();
    void CompleteMissionTravel(USovCampaignDefinition* Destination);
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
    TArray<FString> ListPreservedSaveArchives(ESovSaveSlotKind Kind, int32 SlotIndex);
    /** Explicit confirmed retention action; never deletes a bank or the only recoverable campaign copy. */
    UFUNCTION(BlueprintCallable, Category="Campaign|Save")
    bool DeletePreservedSaveArchive(ESovSaveSlotKind Kind, int32 SlotIndex, const FString& ArchiveId, FString& Error);
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
    UFUNCTION(BlueprintPure, Category="Campaign|Save")
    bool IsAwaitingFailureDecision() const { return bAwaitingFailureDecision; }
    UFUNCTION(BlueprintPure, Category="Campaign|Save")
    bool IsLoadPending() const { return PendingSave != nullptr; }
    UFUNCTION(BlueprintPure, Category="Campaign|Save") double GetCampaignPlaySeconds() const { return PlaySeconds; }
    bool CanCapture(FString& Error) const;
    /** Controller calls after its full managed readiness transaction; failure never becomes load success. */
    void NotifyCampaignReady(ASovPlayerController* Controller, bool bSucceeded);
    /** Used by GameMode to fail closed before spawning a new campaign over a rejected load. */
    bool ValidatePendingWorld(UWorld& World, FString& Error) const;
    UPROPERTY(BlueprintAssignable, Category="Campaign|Save") FSovSaveCompleted OnSaveCompleted;
    UPROPERTY(BlueprintAssignable, Category="Campaign|Save") FSovSaveCompleted OnLoadCompleted;
private:
    friend class USovPlatformServicesSubsystem;
    friend struct FSovSaveTestAccess;
    friend struct FSovSaveWorldLoadTestAccess;
    friend struct FSovPlatformServicesTestAccess;
    /** Only the native provider observer can establish authorization; never exposed to Blueprint callers. */
    void ObserveNativePlatformAccount(const FSovObservedPlatformAccount& Account);
    void FenceUnresolvedAccountChange();
    struct FQueuedBoundary { ESovSaveBoundary Kind; FName Id; };
    bool CanCaptureInternal(FString& Error, bool bAllowEntrySuspension) const;
    ESovSaveResult CaptureAndWrite(ESovSaveSlotKind Kind, int32 SlotIndex, FName BoundaryId, FString& Error, bool bAllowEntrySuspension = false, ESovSaveBoundary Boundary = ESovSaveBoundary::ExplicitCheckpoint);
    ESovSaveResult WriteEnvelope(USovCampaignSaveGame* Save, FString& Error);
    ESovSaveResult CommitPlatformSnapshot(const TArray<uint8>& Bytes, const TArray<uint8>& ReviewedLocalBytes,
        ESovSaveSlotKind Kind, int32 SlotIndex, FString& Error);
    bool RestorePlatformProfileHint(int32 LocalUserIndex);
    bool PersistPlatformProfileHint(const FString& Namespace, int32 LocalUserIndex, FString& Error);
    USovCampaignSaveGame* ReadBest(ESovSaveSlotKind Kind, int32 Index, int32& OutBank, bool& bDamaged, FString& Error);
    bool ValidateEnvelope(USovCampaignSaveGame* Save, bool bValidateAssets, FString& Error) const;
    bool MatchesPendingLoadRequest(const FString& Options) const;
    void CompletePendingLoad(bool bSucceeded, const FString& Error);
    void OnTravelFailure(UWorld* World, ETravelFailure::Type Failure, const FString& Error);
    void FailMissionTravel(const FString& Error);
    void PublishSettingsOwner();
    UNarrativeSave* DecodeNarrative(USovCampaignSaveGame* Save, FString& Error) const;
    FString BankName(ESovSaveSlotKind Kind, int32 Index, int32 Bank) const;
    void ResolveInitialSave(UWorld& World, UNarrativeSave*& Snapshot, bool& bOverride);
    void ReportSave(ESovSaveResult Result, const FSovSaveSlotHeader& Header, const FString& Error);
    bool Tick(float DeltaSeconds);
protected:
    /** Native owner lookup is overridable by isolated world tests without replacing storage/receipt logic. */
    virtual ASovPlayerController* Controller() const;
private:
    TUniquePtr<ISovSaveStorage> Storage;
    UPROPERTY(Transient) TObjectPtr<USovCampaignSaveGame> PendingSave;
    UPROPERTY(Transient) TObjectPtr<USovCampaignSaveGame> FailedWrite;
    UPROPERTY(Transient) TObjectPtr<UNarrativeSave> PendingNarrative;
    TWeakObjectPtr<UWorld> PendingDestination;
    TWeakObjectPtr<UWorld> RejectedLoadWorld;
    TWeakObjectPtr<ASovPlayerController> PausedController;
    TArray<FQueuedBoundary> PendingAutosaves;
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
    bool bEnding = false;
    bool bSessionAbandoned = false;
    uint64 StorageGeneration = 1;
    FSovStorageOwnerToken PendingLoadOwner;
    FSovStorageOwnerToken MissionTravelOwner;
    UPROPERTY(Transient) TObjectPtr<USovCampaignDefinition> MissionTravelDestination;
    double MissionTravelDeadline = 0;
    bool bMissionTravelRecoveryAttempted = false;
    FString MissionTravelFailure;
    bool bPlatformStorageOwnerAvailable = true;
    bool bPlatformSuspended = false;
    bool bDiscardPlatformResumeDelta = false;
    double PlatformSuspendedAt = 0;
    bool bAwaitingFailureDecision = false;
    bool bOwnPause = false;
    bool bPendingWorldApplied = false;
    bool bPendingLoadFailed = false;
    FSovSaveSlotHeader AcknowledgedBoundary;
    TWeakObjectPtr<UWorld> AcknowledgedWorld;
    double AcknowledgmentExpiresAt = 0;
    FDelegateHandle InitialSaveHandle;
    FDelegateHandle TravelFailureHandle;
    FTSTicker::FDelegateHandle TickHandle;
};
