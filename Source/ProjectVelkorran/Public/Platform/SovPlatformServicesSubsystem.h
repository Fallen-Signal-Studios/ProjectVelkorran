// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/SovCampaignSaveGame.h"
#include "SovPlatformServicesSubsystem.generated.h"

class USovSaveSubsystem;
class ISovPlatformServicesAdapter;

UENUM(BlueprintType)
enum class ESovCloudPhase : uint8 { Disabled, Unavailable, Idle, Reading, AwaitingChoice, Writing, Completed, Failed, Cancelled };
UENUM(BlueprintType)
enum class ESovCloudChoice : uint8 { KeepLocal, UseCloud, Cancel };
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCloudReview
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid RequestId;
    UPROPERTY(BlueprintReadOnly) ESovCloudPhase Phase = ESovCloudPhase::Disabled;
    UPROPERTY(BlueprintReadOnly) ESovSaveSlotKind Kind = ESovSaveSlotKind::Manual;
    UPROPERTY(BlueprintReadOnly) int32 SlotIndex = 0;
    UPROPERTY(BlueprintReadOnly) bool bHasLocal = false;
    UPROPERTY(BlueprintReadOnly) bool bHasCloud = false;
    UPROPERTY(BlueprintReadOnly) bool bCopiesIdentical = false;
    UPROPERTY(BlueprintReadOnly) FSovSaveSlotHeader Local;
    UPROPERTY(BlueprintReadOnly) FSovSaveSlotHeader Cloud;
    UPROPERTY(BlueprintReadOnly) FString Message;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovCloudReviewChanged, const FSovCloudReview&, Review);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovPlatformAccountChanged, bool, bSignedIn, bool, bSelectionDeferred);

/** Optional configured-platform integration. No login, cloud I/O or analytics is initiated at startup. */
UCLASS()
class PROJECTVELKORRAN_API USovPlatformServicesSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    /** Observe actual provider state; never prompts for credentials or performs Login/AutoLogin. */
    UFUNCTION(BlueprintCallable, Category="Campaign|Platform") void RefreshPlatformAccount();
    /** Opt-in is session/account scoped. Signing out or changing account revokes it. */
    UFUNCTION(BlueprintCallable, Category="Campaign|Platform") bool SetCloudEnabled(bool bEnabled, FString& Error);
    UFUNCTION(BlueprintPure, Category="Campaign|Platform") bool IsCloudEnabled() const { return bCloudEnabled; }
    UFUNCTION(BlueprintPure, Category="Campaign|Platform") bool IsCloudAvailable() const;
    UFUNCTION(BlueprintPure, Category="Campaign|Platform") bool IsAccountSelectionDeferred() const { return bAccountSelectionDeferred; }
    UFUNCTION(BlueprintPure, Category="Campaign|Platform") FSovCloudReview GetCloudReview() const { return Review; }
    /** Explicit frontend action: stage local and latest published cloud revision without overwriting either. */
    UFUNCTION(BlueprintCallable, Category="Campaign|Platform") bool InspectCloudSlot(ESovSaveSlotKind Kind, int32 SlotIndex, FString& Error);
    UFUNCTION(BlueprintCallable, Category="Campaign|Platform") bool ResolveCloudReview(FGuid RequestId, ESovCloudChoice Choice, FString& Error);
    UFUNCTION(BlueprintCallable, Category="Campaign|Platform") void CancelCloudOperation();
    UPROPERTY(BlueprintAssignable, Category="Campaign|Platform") FSovCloudReviewChanged OnCloudReviewChanged;
    UPROPERTY(BlueprintAssignable, Category="Campaign|Platform") FSovPlatformAccountChanged OnPlatformAccountChanged;
private:
    friend struct FSovPlatformServicesTestAccess;
    void ObserveAccount();
    bool CanUseCloud(FString& Error) const;
    bool IsCurrentOperation(const FGuid& Request, const FString& Namespace, ESovCloudPhase Phase) const;
    uint64 Publish(ESovCloudPhase Phase, const FString& Message);
    void FinishCloudOperation(FGuid Request, const FString& Namespace, ESovCloudPhase ExpectedPhase,
        ESovCloudPhase TerminalPhase, const FString& Message);
    void CancelCloudOperationInternal(ESovCloudPhase TerminalPhase, const FString& Message);
    void OnCloudRead(FGuid Request, FString Namespace, bool bSucceeded, bool bExists, TArray<uint8> Bytes, FString Error);
    void OnCloudWritten(FGuid Request, FString Namespace, bool bSucceeded, FString Error);
    bool Tick(float DeltaSeconds);
    UPROPERTY(Transient) TObjectPtr<USovSaveSubsystem> Saves;
    UPROPERTY(Transient) FSovCloudReview Review;
    TSharedPtr<ISovPlatformServicesAdapter> Adapter;
    TArray<uint8> LocalBytes;
    TArray<uint8> CloudBytes;
    FString ObservedStableId;
    FString OperationNamespace;
    int32 ObservedLocalUser = 0;
    bool bObservedSignedIn = false;
    bool bObservedCloudAvailable = false;
    bool bCloudEnabled = false;
    bool bAccountSelectionDeferred = false;
    bool bObservingAccount = false;
    uint64 StateGeneration = 0;
    double Deadline = 0;
    double NextAccountPoll = 0;
    FTSTicker::FDelegateHandle TickHandle;
};
