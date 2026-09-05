// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Ticker.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "SovCampaignCinematicComponent.generated.h"
class ANarrativeLevelSequenceActor;
class ASovPlayerController;
class USovCampaignStateComponent;
class ULevelSequence;
class ULevelStreaming;
class UAbilitySystemComponent;
class UWorldPartitionStreamingSourceComponent;
class ASovWorldTransitActor;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class ESovCinematicPhase : uint8 { Idle, Loading, Playing, Paused, Committing, Completed, Failed };
UENUM(BlueprintType)
enum class ESovCinematicExitWield : uint8 { Keep, Holster, DrawRequiredWeapon };

/** Runtime cells intersecting this bounded sphere must reach Activated, not merely Loaded. */
USTRUCT(BlueprintType)
struct FSovCinematicPartitionRegion
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RegionId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector Center = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100",ClampMax="200000")) float Radius = 5000.f;
    /** Real actors in visible partition cell levels inside this region prove activation. All runtime grids are covered. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TSoftObjectPtr<AActor>> RequiredActors;
};

/** Only stable native door/lift power and lock state. Movement and arbitrary actor writes are excluded. */
USTRUCT(BlueprintType)
struct FSovCinematicTransitPostcondition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<ASovWorldTransitActor> Transit;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ExpectedTransitId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bSetPower = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bPowered = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bSetLock = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText LockReason;
};

USTRUCT()
struct FSovCinematicPartitionLease
{
    GENERATED_BODY()
    UPROPERTY() FSovCinematicPartitionRegion Contract;
    UPROPERTY() TObjectPtr<AActor> Anchor;
    UPROPERTY() TObjectPtr<UWorldPartitionStreamingSourceComponent> Source;
};

USTRUCT()
struct FSovCinematicTransitSnapshot
{
    GENERATED_BODY()
    UPROPERTY() FSovCinematicTransitPostcondition Contract;
    UPROPERTY() TWeakObjectPtr<ASovWorldTransitActor> Transit;
    UPROPERTY() bool bPowered = true;
    UPROPERTY() FText LockReason;
    UPROPERTY() uint8 Endpoint = 0;
    bool bPowerApplied = false;
    bool bLockApplied = false;
    uint64 OriginalPowerRevision = 0;
    uint64 OriginalLockRevision = 0;
    uint64 AppliedPowerRevision = 0;
    uint64 AppliedLockRevision = 0;
};

USTRUCT(BlueprintType)
struct FSovCinematicParticipant
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName BindingTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bControlledProtagonist = false;
    /** Unique actor tag in this physical world, required for every non-player participant. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ActorTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bRequireLiving = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagContainer RequiredState;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagContainer BlockedState;
    /** This contract requires an already-owned equipped item; it never conjures a cinematic-only weapon. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<UWeaponItem> RequiredWeapon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag EquipmentSlot;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag WieldSlot;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ESovCinematicExitWield ExitWield = ESovCinematicExitWield::Keep;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bApplyExitTransform = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform ExitTransform;
};

USTRUCT()
struct FSovCinematicParticipantSnapshot
{
    GENERATED_BODY()
    UPROPERTY() TWeakObjectPtr<ANarrativeCharacter> Character;
    UPROPERTY() TWeakObjectPtr<UAbilitySystemComponent> ASC;
    UPROPERTY() TWeakObjectPtr<UWeaponItem> RequiredWeapon;
    UPROPERTY() FTransform Transform;
    UPROPERTY() FWeaponWieldState Wield;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovCinematicPhaseChanged, ESovCinematicPhase, Phase, const FString&, Reason);

/** Mission contract attached to the existing Narrative sequence actor. Sequencer still owns playback and restoration. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCampaignCinematicComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USovCampaignCinematicComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign Cinematic") FName MissionId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign Cinematic") FName BeatId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign Cinematic") TSoftObjectPtr<ULevelSequence> Sequence;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign Cinematic") TArray<FSovCinematicParticipant> Participants;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign Cinematic") TArray<FSoftObjectPath> PreloadAssets;
    /** Existing streaming levels, including authored cinematic sublevels. Missing levels fail before playback. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign Cinematic") TArray<FName> RequiredStreamingLevels;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign Cinematic") TArray<FSovCinematicPartitionRegion> RequiredPartitionRegions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign Cinematic") TArray<FSovCinematicTransitPostcondition> TransitPostconditions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign Cinematic", meta=(ClampMin="1",ClampMax="60")) float LoadingTimeoutSeconds = 15.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign Cinematic", meta=(ClampMin="1",ClampMax="2000")) float RequestRange = 400.f;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign Cinematic") bool RequestPlay(ASovPlayerController* Player, FString& OutError);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign Cinematic") bool SetCinematicPaused(bool bPause);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign Cinematic") bool RequestSkip(FString& OutError);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign Cinematic") void Abort(const FString& Reason);
    UFUNCTION(BlueprintPure, Category="Campaign Cinematic") ESovCinematicPhase GetPhase() const { return Phase; }
    UPROPERTY(BlueprintAssignable, Category="Campaign Cinematic") FSovCinematicPhaseChanged OnPhaseChanged;
    bool ValidateConfiguration(FString& OutError) const;
protected:
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
    friend class USovCampaignStateComponent;
    friend struct FSovCinematicTestAccess;
    bool ConsumeCommitReceipt(const USovCampaignStateComponent* State, FName RequestedBeat, bool bSkipped);
    bool HasCommitReceipt(const USovCampaignStateComponent* State, FName RequestedBeat, bool bSkipped) const;
    bool IsContextCurrent() const;
    bool CheckPreparationWatchdog(uint64 Epoch);
    bool ResolveParticipants(FString& OutError);
    bool AcquirePartitionSources(FString& OutError);
    bool ArePartitionRegionsReady() const;
    void ReleasePartitionSources();
    bool ResolveTransitPostconditions(FString& OutError);
    bool ValidateTransitPostconditions(bool bApplied, FString& OutError) const;
    bool ApplyTransitPostconditions(FString& OutError);
    void RestoreTransitPostconditions();
    bool ValidateParticipants(bool bCheckExit, FString& OutError) const;
    bool ValidatePresentationSequence(ULevelSequence* Asset, FString& OutError) const;
    void StartPreparedPlayback();
    bool ObservePlaybackProgress(bool bTerminal);
    bool ValidateBindings() const;
    bool OwnsPlaybackGeneration() const;
    bool ValidateExitPostconditions(FString& OutError) const;
    bool Commit(bool bSkipped, FString& OutError);
    void RestoreParticipants();
    void ReleaseOwnership();
    void ChangePhase(ESovCinematicPhase NewPhase, const FString& Reason = FString());
    UFUNCTION() void HandleStarted();
    UFUNCTION() void HandleFinished();
    UFUNCTION() void HandleStopped();
    UFUNCTION() void HandleFailed();
    UPROPERTY(Transient) TWeakObjectPtr<ASovPlayerController> Controller;
    UPROPERTY(Transient) TWeakObjectPtr<ANarrativeCharacter> PlayerPawn;
    UPROPERTY(Transient) TWeakObjectPtr<UAbilitySystemComponent> PlayerASC;
    UPROPERTY(Transient) TArray<FSovCinematicParticipantSnapshot> Snapshot;
    UPROPERTY(Transient) TArray<TObjectPtr<ULevelStreaming>> StreamingLevels;
    UPROPERTY(Transient) TArray<FSovCinematicPartitionLease> PartitionLeases;
    UPROPERTY(Transient) TArray<FSovCinematicTransitSnapshot> TransitSnapshots;
    UPROPERTY(Transient) TWeakObjectPtr<AActor> OriginalViewTarget;
    FRotator OriginalControlRotation;
    TSharedPtr<FStreamableHandle> LoadHandle;
    ESovCinematicPhase Phase = ESovCinematicPhase::Idle;
    FGuid SessionId;
    FTSTicker::FDelegateHandle PreparationWatchdog;
    uint64 RequestEpoch = 0;
    uint64 ExpectedPlaybackGeneration = 0;
    uint64 ReservedPlaybackGeneration = 0;
    double LoadingStartedSeconds = 0.0;
    uint8 ConsecutivePartitionReadyTicks = 0;
    double PlayedSeconds = 0.0;
    double DurationSeconds = 0.0;
    double LastPositionSeconds = 0.0;
    double LastSampleWorldSeconds = 0.0;
    double EndPositionSeconds = 0.0;
    bool bRequestStarting = false;
    bool bPlaybackSuperseded = false;
    bool bOwnInput = false;
    bool bOwnSequenceTag = false;
    bool bFinishing = false;
    bool bEndingPlay = false;
    bool bFullViewEligible = true;
    bool bReceiptAvailable = false;
    bool bReceiptSkipped = false;
};
