// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SovLegacyCorruptionComponent.generated.h"

class FLifetimeProperty;
class UAbilitySystemComponent;
class UGameplayEffect;
class UNarrativeAbilitySystemComponent;
struct FOnAttributeChangeData;

/** Exact TDD exposure bands. None is the internal zero-exposure state. */
UENUM(BlueprintType)
enum class ESovLegacyCorruptionBand : uint8
{
	None UMETA(DisplayName = "None"),
	Trace UMETA(DisplayName = "Trace"),
	Intrusion UMETA(DisplayName = "Intrusion"),
	Contest UMETA(DisplayName = "Contest"),
	OverwriteRisk UMETA(DisplayName = "Overwrite Risk")
};

UENUM(BlueprintType)
enum class ESovCorruptionFalloffPolicy : uint8
{
	None UMETA(DisplayName = "No Falloff"),
	Linear UMETA(DisplayName = "Linear")
};

/** Opaque authority handle for one independently authored exposure source. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovLegacyCorruptionSourceHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	FGuid Id;

	bool IsValid() const { return Id.IsValid(); }
	void Invalidate() { Id.Invalidate(); }
};

/**
 * Complete authored contract for one corruption source.
 *
 * Gameplay remains identical when reduced-effects presentation is enabled;
 * PresentationProfile and AccessibilitySubstitute select only how the same
 * band/source information is communicated.
 */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCorruptionSourceSpec
{
	GENERATED_BODY()

	/** Stable authored identity used by checkpoint/encounter reconstruction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName SourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure", meta = (ClampMin = "0.0"))
	float ExposurePerSecond = 5.0f;

	/** Applied once when RegisterCorruptionSource accepts the source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure", meta = (ClampMin = "0.0"))
	float InstantExposure = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial")
	ESovCorruptionFalloffPolicy FalloffPolicy = ESovCorruptionFalloffPolicy::None;

	/** Full strength at or inside this radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial", meta = (ClampMin = "0.0", Units = "cm"))
	float InnerRadius = 0.0f;

	/** Zero strength at or beyond this radius when Linear falloff is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial", meta = (ClampMin = "0.0", Units = "cm"))
	float OuterRadius = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial")
	bool bRequireLineOfSight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial")
	TEnumAsByte<ECollisionChannel> OcclusionTraceChannel = ECC_Visibility;

	/** Empty accepts any target; otherwise the target ASC's owned tags must match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FGameplayTagQuery AllowedTargetQuery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure")
	bool bEnforceBandCap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure")
	ESovLegacyCorruptionBand BandCap = ESovLegacyCorruptionBand::Contest;

	/** Required for every source so the player is never shown exposure without a remedy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remedy")
	FGameplayTag RemedyTag;

	/** Required player-facing cleanse or escape instruction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remedy")
	FText RemedyInstruction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	FName PresentationProfile = NAME_None;

	/** Non-distorting substitute cue for reduced-effects/accessibility mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation")
	FText AccessibilitySubstitute;

	/**
	 * Save this live source's stable ID so re-registration after load suppresses
	 * its one-time InstantExposure. Aggregate exposure follows component policy.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	bool bSaveAtCheckpoint = true;

	/** Extends that source-ID reconstruction behavior to canon checkpoints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
	bool bPersistsAcrossCanonCheckpoints = false;

	/** Still requires the owning component's explicit mission permission. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overwrite Risk")
	bool bAuthorizesOverwriteRisk = false;

	bool HasValidNumbers() const;
};

/** Local presentation view derived from replicated gameplay-equivalent state. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCorruptionPresentationSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	float Corruption = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	float MaxCorruption = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	float NormalizedCorruption = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	ESovLegacyCorruptionBand Band = ESovLegacyCorruptionBand::None;

	/** Unit vector from target toward the most relevant active source. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	FVector SourceDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	bool bHasDirectionalSource = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	FGameplayTag RemedyTag;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	FText RemedyInstruction;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	FName PresentationProfile = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	FText AccessibilitySubstitute;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	bool bReducedEffectsPresentation = false;

	/** Always true: accessibility changes cues, never thresholds or outcomes. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	bool bGameplayEquivalentInReducedEffects = true;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption")
	int32 PresentationRevision = 0;
};

/** Compact, versioned state; live actor pointers and GAS handles are excluded. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCorruptionCheckpointData
{
	GENERATED_BODY()

	static constexpr int32 CurrentVersion = 3;

	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	int32 Version = CurrentVersion;

	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	float Corruption = 0.0f;

	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	float MaxCorruption = 100.0f;

	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	bool bOverwriteRiskAuthorizedForCurrentExposure = false;

	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	FGameplayTag RemedyTag;

	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	FText RemedyInstruction;

	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	FName PresentationProfile = NAME_None;

	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	FText AccessibilitySubstitute;

	/** Encounter code may use these stable IDs to rebuild opted-in live sources. */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	TArray<FName> ActiveSourceIds;

	/** Subset whose authored policy permits reconstruction at canon checkpoints. */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	TArray<FName> CanonPersistentSourceIds;

	/**
		 * All one-shot sources already represented in Corruption, including sources
		 * that do not opt into encounter reconstruction. These load guards remain
		 * until consumed or explicitly finalized; they are not actor references or
		 * runtime source handles.
	 */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Sovereign|Corruption")
	TArray<FName> InstantExposureReplayGuardSourceIds;
};

USTRUCT()
struct FSovCorruptionReplicatedPresentationState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector SourceDirection = FVector::ZeroVector;

	UPROPERTY()
	bool bHasDirectionalSource = false;

	UPROPERTY()
	FGameplayTag RemedyTag;

	UPROPERTY()
	FText RemedyInstruction;

	UPROPERTY()
	FName PresentationProfile = NAME_None;

	UPROPERTY()
	FText AccessibilitySubstitute;

	UPROPERTY()
	int32 Revision = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovCorruptionChangedSignature,
	float, OldCorruption,
	float, NewCorruption,
	float, MaxCorruption);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSovCorruptionBandChangedSignature,
	ESovLegacyCorruptionBand, OldBand,
	ESovLegacyCorruptionBand, NewBand);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovCorruptionPresentationChangedSignature,
	const FSovCorruptionPresentationSnapshot&, Snapshot);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FSovCorruptionRemediedSignature,
	FGameplayTag, RemedyTag,
	float, OldCorruption,
	float, NewCorruption,
	AActor*, RemedyInstigator);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FSovCorruptionOverwriteRiskSignature);

/**
 * Server-owned Eclipse corruption lifecycle and presentation contract.
 *
 * Independent authored sources accumulate on a fixed timer using elapsed world
 * time. Corruption is stored on the persistent ASC, while this avatar component
 * rejects stale PlayerState actor-info and owns only its exact band-effect
 * handle. Overwrite Risk never kills, hijacks input, or removes control: entry
 * merely emits an explicit mission hook.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent, DisplayName = "Sovereign Legacy Corruption (Opt-in)"))
class PROJECTVELKORRAN_API USovLegacyCorruptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovLegacyCorruptionComponent();

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Corruption")
	bool InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption")
	float GetCorruption() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption")
	float GetMaxCorruption() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption")
	float GetNormalizedCorruption() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption")
	ESovLegacyCorruptionBand GetCorruptionBand() const { return CurrentBand; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption")
	ESovLegacyCorruptionBand DetermineBandForExposure(float Exposure) const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption")
	float GetExposureCeilingForBand(ESovLegacyCorruptionBand Band) const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption|Presentation")
	FSovCorruptionPresentationSnapshot GetPresentationSnapshot() const;

	/** Registers one source; same-frame InstantExposure payloads resolve together once. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Sources")
	FSovLegacyCorruptionSourceHandle RegisterCorruptionSource(
		AActor* SourceActor,
		const FSovCorruptionSourceSpec& SourceSpec);

	/** Preserves elapsed contribution under the old spec; instant exposure is not replayed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Sources")
	bool UpdateCorruptionSource(
		FSovLegacyCorruptionSourceHandle SourceHandle,
		const FSovCorruptionSourceSpec& SourceSpec);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Sources")
	bool UnregisterCorruptionSource(FSovLegacyCorruptionSourceHandle SourceHandle);

	/** Authority-side validity check used by overlap owners after death/rebind cleanup. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption|Sources")
	bool HasRegisteredCorruptionSource(
		FSovLegacyCorruptionSourceHandle SourceHandle) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Sources")
	void ClearCorruptionSources();

	/** Direct non-field path used by damage/status requests and scripted hazards. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption")
	float ApplyInstantExposure(
		float ExposureAmount,
		AActor* SourceActor,
		ESovLegacyCorruptionBand BandCap);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Remedy")
	float ReduceCorruption(float ExposureReduction, AActor* RemedyInstigator);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Remedy")
	bool ClearCorruption(AActor* RemedyInstigator);

	/** Applies a typed remedy; a currently required tag must match exactly. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Remedy")
	bool ApplyRemedy(
		FGameplayTag RemedyTag,
		float ExposureReduction,
		bool bClearAll,
		AActor* RemedyInstigator);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Overwrite Risk")
	void SetMissionAllowsOverwriteRisk(bool bAllowed);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption|Overwrite Risk")
	bool DoesMissionAllowOverwriteRisk() const { return bMissionAllowsOverwriteRisk; }

	/** Local accessibility choice; it never changes exposure or bands. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Corruption|Presentation")
	void SetReducedEffectsPresentationEnabled(bool bEnabled);

	/** Settles elapsed sources before taking the authoritative snapshot. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Checkpoint")
	FSovCorruptionCheckpointData CaptureCorruptionCheckpoint(bool bCanonCheckpoint);

	/** Queues data until the persistent ASC and this avatar are both ready. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Checkpoint")
	bool RestoreCorruptionCheckpoint(const FSovCorruptionCheckpointData& CheckpointData);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption|Checkpoint")
	TArray<FName> GetRestoredSourceIds() const { return RestoredSourceIds; }

	/** Completes reconstruction and clears the restored-ID view plus load guards. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption|Checkpoint")
	void FinalizeCorruptionSourceRestore();

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Corruption")
	FSovCorruptionChangedSignature OnCorruptionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Corruption")
	FSovCorruptionBandChangedSignature OnCorruptionBandChanged;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Corruption|Presentation")
	FSovCorruptionPresentationChangedSignature OnCorruptionPresentationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Corruption|Remedy")
	FSovCorruptionRemediedSignature OnCorruptionRemedied;

	/** Mission/boss logic may bind this hook; core corruption applies no punishment. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Corruption|Overwrite Risk")
	FSovCorruptionOverwriteRiskSignature OnOverwriteRiskEntered;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Prototype fractions of MaxCorruption; intentionally data-editable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntrusionThresholdFraction = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ContestThresholdFraction = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OverwriteRiskThresholdFraction = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Sources", meta = (ClampMin = "0.02", Units = "s"))
	float SourceEvaluationInterval = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Effects")
	TSubclassOf<UGameplayEffect> BandStateEffectClass;

	/** Disabled by default: a mission must explicitly opt into Overwrite Risk. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Sovereign|Corruption|Overwrite Risk")
	bool bMissionAllowsOverwriteRisk = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Death")
	bool bResetCorruptionOnDeath = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Death", meta = (ClampMin = "0.0"))
	float DeathResetCorruption = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Remedy")
	bool bRequireMatchingRemedyWhenSpecified = true;

	/** Aggregate player exposure is saved at ordinary checkpoints. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Checkpoint")
	bool bSaveExposureAtCheckpoints = true;

	/** Aggregate exposure normally resets across canon/mission boundaries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Checkpoint")
	bool bPersistExposureAcrossCanonCheckpoints = false;

	/** Delay before warning that checkpoint source reconstruction was not finalized. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Checkpoint", meta = (ClampMin = "0.1", Units = "s"))
	float InstantReplayGuardWarningDelay = 2.0f;

private:
	struct FSovActiveSource
	{
		TWeakObjectPtr<AActor> SourceActor;
		FSovCorruptionSourceSpec Spec;
	};

	void TryInitializeFromOwner();

	UFUNCTION()
	void HandleOwnerASCInitialized();

	UFUNCTION()
	void HandleDeathStateChanged(
		AActor* ChangedActor,
		UNarrativeAbilitySystemComponent* ChangedASC,
		bool bIsDead);

	void UninitializeFromAbilitySystem();
	bool HasCurrentAvatarBinding() const;
	bool CanWriteCorruption(bool bAllowDead = false) const;

	void HandleCorruptionAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxCorruptionAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void SetCorruptionInternal(float NewCorruption, bool bAllowDead = false);
	void SetMaxCorruptionInternal(float NewMaxCorruption, bool bAllowDead = false);

	void RefreshSourceTimer();
	void StopSourceTimer();
	void EvaluateSourceTimer();
	void EvaluateAccumulatedSources(bool bAllowDead = false);
	void SchedulePendingInstantExposureEvaluation();
	void EvaluatePendingInstantExposures();
	void ClearPendingInstantExposures();
	float CalculateSourceStrength(
		const FSovActiveSource& ActiveSource,
		FVector* OutDirection = nullptr) const;
	bool SourceAllowsTarget(const FSovActiveSource& ActiveSource) const;
	bool SourceHasLineOfSight(const FSovActiveSource& ActiveSource) const;
	float ApplyExposureInternal(
		float ExposureAmount,
		ESovLegacyCorruptionBand BandCap,
		bool bEnforceBandCap,
		bool bSourceAuthorizesOverwriteRisk,
		const FSovCorruptionSourceSpec* PresentationSpec,
		AActor* SourceActor);

	void RefreshBand(bool bBroadcastChanges);
	void SetBand(ESovLegacyCorruptionBand NewBand, bool bBroadcastChanges);
	void RemoveBandEffect();
	void ApplyBandEffect();
	FGameplayTag GetTagForBand(ESovLegacyCorruptionBand Band) const;
	float GetThresholdForBand(ESovLegacyCorruptionBand Band) const;

	void RefreshPresentationState();
	void RememberPresentationContract(
		const FSovCorruptionSourceSpec& SourceSpec);
	void ClearRememberedPresentationContract();
	void BroadcastPresentationSnapshot();
	void WakeForReplication() const;
	void SendCorruptionEvent(FGameplayTag EventTag, float Magnitude, AActor* Instigator) const;
	void ResetForDeath();
	bool ApplyCheckpointNow(const FSovCorruptionCheckpointData& CheckpointData);
	void ScheduleInstantReplayGuardWarning();
	void HandleInstantReplayGuardWarning();
	void ClearInstantReplayGuards();

	UFUNCTION()
	void OnRep_CurrentBand(ESovLegacyCorruptionBand OldBand);

	UFUNCTION()
	void OnRep_PresentationState(
		const FSovCorruptionReplicatedPresentationState& OldState);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	FDelegateHandle CorruptionChangedDelegateHandle;
	FDelegateHandle MaxCorruptionChangedDelegateHandle;
	FTimerHandle SourceEvaluationTimerHandle;
	FTimerHandle PendingInstantExposureTimerHandle;
	FTimerHandle InstantReplayGuardTimerHandle;
	float LastSourceEvaluationWorldTime = 0.0f;

	TMap<FGuid, FSovActiveSource> ActiveSources;
	TSet<FGuid> PendingInstantSourceHandles;
	FActiveGameplayEffectHandle ActiveBandEffectHandle;
	FGameplayTag ActiveBandEffectTag;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentBand, Transient)
	ESovLegacyCorruptionBand CurrentBand = ESovLegacyCorruptionBand::None;

	UPROPERTY(ReplicatedUsing = OnRep_PresentationState, Transient)
	FSovCorruptionReplicatedPresentationState PresentationState;

	UPROPERTY(Replicated, Transient)
	bool bOverwriteRiskAuthorizedForCurrentExposure = false;

	FGameplayTag RememberedRemedyTag;
	FText RememberedRemedyInstruction;
	FName RememberedPresentationProfile = NAME_None;
	FText RememberedAccessibilitySubstitute;
	FVector RememberedSourceDirection = FVector::ZeroVector;
	bool bHasRememberedSourceDirection = false;
	bool bReducedEffectsPresentation = false;
	bool bSuppressNotifications = false;
	bool bHasPendingCheckpointRestore = false;
	FSovCorruptionCheckpointData PendingCheckpointRestore;
	TArray<FName> RestoredSourceIds;
	TArray<FName> RestoredCanonPersistentSourceIds;
	TArray<FName> InstantExposureReplayGuardSourceIds;
	bool bWarnedMissingAttributeSet = false;
};
