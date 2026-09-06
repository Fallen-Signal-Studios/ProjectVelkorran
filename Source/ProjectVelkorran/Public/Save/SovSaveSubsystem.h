// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/SovCampaignSaveGame.h"
#include "Containers/Ticker.h"
#include "NarrativeSave.h"
#include "SovSaveSubsystem.generated.h"
class UNarrativeSave;
class ASovPlayerController;
class USovCampaignDefinition;
class UAbilitySystemComponent;
class APawn;
class APlayerState;
struct FNarrativeSavePlayer;
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
    UFUNCTION(BlueprintPure, Category="Campaign|Save")
    bool IsAwaitingFailureDecision() const { return bAwaitingFailureDecision; }
    UFUNCTION(BlueprintPure, Category="Campaign|Save")
    bool IsLoadPending() const { return PendingSave != nullptr; }
    UFUNCTION(BlueprintPure, Category="Campaign|Save") double GetCampaignPlaySeconds() const { return PlaySeconds; }
    bool CanCapture(FString& Error) const;
    /** Controller calls after its full managed readiness transaction; failure never becomes load success. */
    void NotifyCampaignReady(ASovPlayerController* Controller, bool bSucceeded, uint64 RestoreEpoch);
    /** Reserve the verified origin checkpoint and request-bound player record before irreversible travel. */
    bool PrepareMissionTravel(ASovPlayerController* Controller, USovCampaignDefinition* Destination,
        uint64 SourceEpoch, FString& TravelURL, FString& Error);
    void RejectMissionTravel(ASovPlayerController* Controller, uint64 SourceEpoch);
    bool CanCommitMissionTravel(ASovPlayerController* Controller, uint64 SourceEpoch) const;
    bool ValidateRestoredTravelIdentity(const ASovPlayerController* Controller) const;
    bool ReadMissionTravelRecords(UWorld& World, FNarrativeSavePlayer& Records, FString& Error);
    /** Binds the managed restoration to the exact controller, pawn, PlayerState and ASC generation. */
    bool BindPendingRestore(ASovPlayerController* Controller, uint64 RestoreEpoch, FString& Error);
    UFUNCTION(BlueprintPure, Category="Campaign|Save") bool HasTravelRecovery() const;
    /** An explicit retry after the one automatic origin recovery failed; never loops automatically. */
    UFUNCTION(BlueprintCallable, Category="Campaign|Save") ESovSaveResult RetryTravelRecovery(FString& Error);
    /** Used by GameMode to fail closed before spawning a new campaign over a rejected load. */
    bool ValidatePendingWorld(UWorld& World, FString& Error) const;
    UPROPERTY(BlueprintAssignable, Category="Campaign|Save") FSovSaveCompleted OnSaveCompleted;
    UPROPERTY(BlueprintAssignable, Category="Campaign|Save") FSovSaveCompleted OnLoadCompleted;
private:
    friend class USovPlatformServicesSubsystem;
    friend struct FSovSaveTestAccess;
    friend struct FSovTravelTransactionTestAccess;
    friend struct FSovSaveWorldLoadTestAccess;
    friend struct FSovPlatformServicesTestAccess;
    friend struct FSovObjectivePresentationTestAccess;
    /** Only the native provider observer can establish authorization; never exposed to Blueprint callers. */
    void ObserveNativePlatformAccount(const FSovObservedPlatformAccount& Account);
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
    bool OwnsPendingAccount() const;
    bool OwnsTravelSource() const;
    bool MatchesRestoreGenerations() const;
    bool MatchesRestoreOwner(ASovPlayerController* Controller, uint64 RestoreEpoch) const;
    void ArmTravelFailureHook(const FGuid& Request);
    void HandleTravelFailure(const FGuid& Request, UWorld* World, const FString& Error);
    void ClearPendingOperation();
    bool StartOriginRecovery(FString& Error);
    bool RequestPendingMap(UWorld& World, FString& Error);
    void CompletePendingLoad(bool bSucceeded, const FString& Error);
    UNarrativeSave* DecodeNarrative(USovCampaignSaveGame* Save, FString& Error) const;
    FString BankName(ESovSaveSlotKind Kind, int32 Index, int32 Bank) const;
    void ResolveInitialSave(UWorld& World, UNarrativeSave*& Snapshot, bool& bOverride);
    void ReportSave(ESovSaveResult Result, const FSovSaveSlotHeader& Header, const FString& Error);
    bool Tick(float DeltaSeconds);
    ASovPlayerController* Controller() const;
    TUniquePtr<ISovSaveStorage> Storage;
    UPROPERTY(Transient) TObjectPtr<USovCampaignSaveGame> PendingSave;
    UPROPERTY(Transient) TObjectPtr<USovCampaignSaveGame> FailedWrite;
    UPROPERTY(Transient) TObjectPtr<UNarrativeSave> PendingNarrative;
    UPROPERTY(Transient) TObjectPtr<USovCampaignSaveGame> TravelRecoverySave;
    UPROPERTY(Transient) FNarrativeSavePlayer PendingTravelRecords;
    FGuid PendingOperationId;
    FString PendingAccount;
    int32 PendingUser = INDEX_NONE;
    int32 TravelRecoveryUser = INDEX_NONE;
    FSoftObjectPath PendingTravelMission;
    FString PendingTravelMap;
    bool bPendingMissionTravel = false;
    bool bRecoveringMissionTravel = false;
    FString TravelRecoveryReason;
    TWeakObjectPtr<UWorld> PendingSource;
    TWeakObjectPtr<ASovPlayerController> TravelSourceController;
    TWeakObjectPtr<APawn> TravelSourcePawn;
    TWeakObjectPtr<APlayerState> TravelSourcePlayerState;
    TWeakObjectPtr<UAbilitySystemComponent> TravelSourceASC;
    uint64 TravelSourceEpoch = 0;
    uint64 TravelSourceASCEpoch = 0;
    int32 TravelSourcePawnGeneration = 0;
    TWeakObjectPtr<ASovPlayerController> RestoreController;
    TWeakObjectPtr<APawn> RestorePawn;
    TWeakObjectPtr<APlayerState> RestorePlayerState;
    TWeakObjectPtr<UAbilitySystemComponent> RestoreASC;
    uint64 PendingRestoreEpoch = 0;
    uint64 RestoreASCEpoch = 0;
    int32 RestorePawnGeneration = 0;
    FDelegateHandle TravelFailureHandle;
#if WITH_AUTOMATION_TESTS
    // Exercise the native transaction owner without requiring an authored/cooked map in unit worlds.
    TFunction<bool(UWorld&, const FString&)> TestTravelRequest;
#endif
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
    FTSTicker::FDelegateHandle TickHandle;
};
