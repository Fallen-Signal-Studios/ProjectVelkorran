// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "GameplayEffectTypes.h"
#include "NarrativeSavableComponent.h"
#include "Status/SovStatusDefinition.h"
#include "TimerManager.h"
#include "SovStatusComponent.generated.h"

class UNarrativeAbilitySystemComponent;
class ANarrativePlayerCharacter;
class FLifetimeProperty;

/** Why an authoritative status record changed. */
UENUM(BlueprintType)
enum class ESovStatusChangeReason : uint8
{
	Applied,
	Refreshed,
	Expired,
	Cleansed,
	Removed,
	Restored
};

/** Compact semantic record. Runtime actors, timers, and GE handles are excluded. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovStatusCheckpointRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	FPrimaryAssetId DefinitionId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	int32 DefinitionSchemaVersion = 1;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	FGameplayTag RequestTag;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	int32 StackCount = 1;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	float Magnitude = 1.0f;

	/** Effective GE level, not a transient source actor or captured attribute. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	float EffectLevel = 1.0f;

	/** Stable Sov.Ability.* provenance retained by periodic restored effects. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	FGameplayTagContainer SourceAbilityTags;

	/** Ignored for infinite definitions. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	float RemainingDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	bool bInfinite = false;
};

/** Versioned semantic state serialized by Narrative's existing component record. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovStatusCheckpointState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	int32 SchemaVersion = 1;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Sovereign|Status|Checkpoint")
	TArray<FSovStatusCheckpointRecord> Statuses;
};

/** Compact replicated view for owner/observer UI, audio, and VFX only. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovStatusPresentationEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	FGameplayTag RequestTag;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	FGameplayTag StateTag;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	int32 StackCount = 1;

	/** Absolute authoritative world time; ignored when bInfinite is true. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	float ServerWorldExpiry = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	bool bInfinite = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	int32 UIPriority = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	FGameplayTag PresentationTag;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status|Presentation")
	FGameplayTag AccessibilityPresentationTag;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovStatusApplicationResolvedSignature,
	const FSovStatusApplicationRequest&, Request,
	ESovStatusApplicationResult, Result,
	int32, NewStackCount);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FSovStatusChangedSignature,
	FGameplayTag, RequestTag,
	FGameplayTag, StateTag,
	ESovStatusChangeReason, Reason,
	int32, StackCount,
	AActor*, SourceActor);

/**
 * Authoritative owner of project status application and cleanup.
 *
 * GAS owns replicated state tags/modifiers. This component owns policy,
 * provenance, exact effect handles, replay protection, and checkpoint
 * semantics; it never removes unrelated effects by a broad tag query.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovStatusComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()

public:
	USovStatusComponent();

	virtual ENarrativeRestorePhase GetSaveRestorePhase() const override { return ENarrativeRestorePhase::Player; }
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual bool ValidateSaveRecord(const TArray<uint8>& RecordBytes) const override;
	virtual bool WasSaveRecordLoadAccepted() const override { return bLastSaveRecordLoadAccepted; }
	virtual bool LoadMissingSaveRecord() override;

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Status")
	bool InitializeWithAbilitySystem(
		UNarrativeAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Status")
	bool IsInitialized() const;

	/** Authority-only direct path shared by abilities, hazards, and restored state. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Status")
	ESovStatusApplicationResult ApplyStatus(
		const FSovStatusApplicationRequest& Request);

	/** Blueprint convenience path; generates the immutable transaction identity and exact target. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Status")
	ESovStatusApplicationResult ApplyStatusByTag(
		UPARAM(meta = (Categories = "Sov.Status.Apply")) FGameplayTag StatusTag,
		AActor* SourceActor,
		float Magnitude = 1.0f,
		float Duration = 0.0f,
		float EffectLevel = 1.0f);

	/** Removes statuses whose definitions contain this exact cleanse tag. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Status")
	int32 CleanseStatuses(
		UPARAM(meta = (Categories = "Sov.Status.Cleanse")) FGameplayTag CleanseTag,
		AActor* SourceActorFilter = nullptr);

	/** Removes one exact active request/state tag without touching unrelated GEs. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Status")
	bool RemoveStatus(FGameplayTag StatusTag, bool bApplyRecoveryImmunity = true);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Status")
	bool HasActiveStatus(FGameplayTag StatusTag) const;

	/** Runtime-authority provenance query; checkpoint restore intentionally clears actor attribution. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Status")
	bool WasStatusAppliedBy(FGameplayTag StatusTag, const AActor* SourceActor) const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Status")
	int32 GetStatusStackCount(FGameplayTag StatusTag) const;

	/** Returns -1 for infinite, zero for absent, or authoritative remaining seconds. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Status")
	float GetStatusRemainingDuration(FGameplayTag StatusTag) const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Status")
	USovStatusDefinition* GetStatusDefinition(FGameplayTag StatusTag) const;

	/** Replicated presentation snapshots sorted by UI priority, then request tag. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Status|Presentation")
	TArray<FSovStatusPresentationEntry> GetActiveStatusPresentation() const
	{
		return ReplicatedStatusPresentation;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Status|Device")
	bool IsDeviceStatusEligible() const { return bDeviceStatusEligible; }

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Status|Device")
	void SetDeviceStatusEligible(bool bEligible) { bDeviceStatusEligible = bEligible; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Status|Checkpoint")
	FSovStatusCheckpointState CaptureCheckpointState() const;

	/** May be called before ASC readiness; a valid state is then applied once initialization completes. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Status|Checkpoint")
	bool RestoreCheckpointState(const FSovStatusCheckpointState& State);

	/** Native resource/readiness boundary. True also covers an accepted queue awaiting player readiness. */
	bool CompletePendingCheckpointRestore();

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Status|Events")
	FSovStatusApplicationResolvedSignature OnStatusApplicationResolved;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Status|Events")
	FSovStatusChangedSignature OnStatusChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Assets replace the canonical built-in definition with the same exact request tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Definitions")
	TArray<TObjectPtr<USovStatusDefinition>> StatusDefinitionOverrides;

	/** Deliberately false: only explicitly authored devices accept Device Disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Status|Device")
	bool bDeviceStatusEligible = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Status|Replay", meta = (ClampMin = "16", ClampMax = "2048"))
	int32 ReplayLedgerCapacity = 256;

private:
	static constexpr int32 CheckpointSchemaVersion = 1;

	enum class EStatusRemovalPolicy : uint8
	{
		Expired,
		Cleansed,
		Explicit,
		Reapply,
		Death,
		CheckpointRestore,
		ASCReplacement
	};

	struct FRuntimeStatusRecord
	{
		TWeakObjectPtr<USovStatusDefinition> Definition;
		TWeakObjectPtr<AActor> SourceActor;
		FGuid LastRequestId;
		FGameplayTagContainer SourceAbilityTags;
		FActiveGameplayEffectHandle EffectHandle;
		float Magnitude = 1.0f;
		float EffectLevel = 1.0f;
		float AppliedDuration = 0.0f;
		float EndWorldTime = 0.0f;
		int32 StackCount = 1;
		bool bInfinite = false;
	};

	struct FReplayKey
	{
		FGuid RequestId;
		FGameplayTag StatusTag;

		bool operator==(const FReplayKey& Other) const
		{
			return RequestId == Other.RequestId && StatusTag == Other.StatusTag;
		}

		friend uint32 GetTypeHash(const FReplayKey& Key)
		{
			return HashCombine(GetTypeHash(Key.RequestId), GetTypeHash(Key.StatusTag));
		}
	};

	void TryInitializeFromOwner();
	void UninitializeFromAbilitySystem();
	void BuildDefinitionRegistry();
	void CreateBuiltInDefinitions();
	USovStatusDefinition* ResolveDefinition(FGameplayTag StatusTag) const;
	FGameplayTag ResolveRequestTag(FGameplayTag StatusTag) const;
	bool IsRequestStructurallyValid(const FSovStatusApplicationRequest& Request) const;
	bool ConsumeReplayKey(const FGuid& RequestId, const FGameplayTag& StatusTag);
	bool IsTargetDead() const;
	bool IsDefinitionEligible(
		const USovStatusDefinition& Definition,
		const FGameplayTagContainer& OwnedTags) const;
	FActiveGameplayEffectHandle ApplyDefinitionEffect(
		const USovStatusDefinition& Definition,
		const FSovStatusApplicationRequest& Request,
		float EffectiveMagnitude,
		float EffectiveDuration,
		int32 StackCount);
	void ScheduleExpiry(FGameplayTag RequestTag, float Duration);
	bool RemoveStatusInternal(
		FGameplayTag RequestTag,
		EStatusRemovalPolicy RemovalPolicy,
		bool bForceRecoveryImmunity = false);
	void ClearAllTrackedEffects(EStatusRemovalPolicy RemovalPolicy);
	void ApplyRecoveryImmunity(const USovStatusDefinition& Definition);
	void ApplyPendingCheckpointRestore();
	bool ApplyCheckpointStateNow(const FSovStatusCheckpointState& State);
	bool ValidateCheckpointState(const FSovStatusCheckpointState& State) const;
	bool IsCheckpointTargetReady() const;
	bool StageNativeCheckpointState(const FSovStatusCheckpointState& State);
	void BindCheckpointLifecycle();
	void CleanupCheckpointLifecycle();
	void HandleDeferredCheckpointRestore(uint64 ExpectedGeneration);

	UFUNCTION()
	void HandleCheckpointCharacterReady(ANarrativePlayerCharacter* Character);

	void UpsertReplicatedPresentation(
		const USovStatusDefinition& Definition,
		const FRuntimeStatusRecord& RuntimeRecord);
	void RemoveReplicatedPresentation(FGameplayTag RequestTag);
	void MarkReplicatedPresentationDirty();
	const FSovStatusPresentationEntry* FindReplicatedPresentation(
		FGameplayTag StatusTag) const;
	static bool ArePresentationEntriesEqual(
		const FSovStatusPresentationEntry& Left,
		const FSovStatusPresentationEntry& Right);
	void BroadcastApplicationResult(
		const FSovStatusApplicationRequest& Request,
		ESovStatusApplicationResult Result,
		int32 NewStackCount);
	void SendLifecycleEvent(
		FGameplayTag EventTag,
		FGameplayTag RequestTag,
		FGameplayTag StateTag,
		AActor* SourceActor,
		float Magnitude) const;

	UFUNCTION()
	void HandleOwnerASCInitialized();

	UFUNCTION()
	void HandleStatusApplicationRequested(
		const FSovStatusApplicationRequest& Request);

	UFUNCTION()
	void HandleDeathStateChanged(
		AActor* KilledActor,
		UNarrativeAbilitySystemComponent* KilledActorASC,
		bool bIsDead);

	void HandleDeferredDeathCleanup();
	void HandleStatusExpired(FGameplayTag RequestTag);

	UFUNCTION()
	void OnRep_ReplicatedStatusPresentation();

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USovStatusDefinition>> BuiltInDefinitions;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<USovStatusDefinition>> DefinitionRegistry;

	/** GAS remains gameplay truth; this array carries only semantic presentation data. */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedStatusPresentation)
	TArray<FSovStatusPresentationEntry> ReplicatedStatusPresentation;

	TMap<FGameplayTag, FRuntimeStatusRecord> ActiveStatuses;
	TMap<FGameplayTag, FTimerHandle> StatusExpiryTimers;
	TMap<FGameplayTag, FActiveGameplayEffectHandle> RecoveryImmunityHandles;
	TSet<FReplayKey> ReplayLedger;
	TArray<FReplayKey> ReplayLedgerOrder;
	TMap<FGameplayTag, FSovStatusPresentationEntry> LastObservedPresentation;
	FTimerHandle DeferredDeathCleanupTimer;
	FTimerHandle DeferredCheckpointRestoreTimer;
	uint64 CheckpointLifecycleGeneration = 0;
	bool bAwaitingRestoreBoundary = false;
	bool bCompletingRestoreBoundary = false;
	bool bCheckpointLifecycleEnding = false;
	TArray<FSovStatusPresentationEntry> PendingCheckpointPresentation;

	/** Only stable definitions and resolved semantic values enter Narrative's save archive. */
	UPROPERTY(SaveGame)
	FSovStatusCheckpointState SavedCheckpointState;

	FSovStatusCheckpointState PendingCheckpointState;
	FSovStatusCheckpointState RestoringCheckpointState;
	bool bHasPendingCheckpointState = false;
	bool bRestoringCheckpoint = false;
	bool bClearingOwnedEffects = false;
	bool bChangingAbilitySystem = false;
	bool bLastSaveRecordLoadAccepted = true;
	bool bDeferredDeathCleanupPending = false;
};
