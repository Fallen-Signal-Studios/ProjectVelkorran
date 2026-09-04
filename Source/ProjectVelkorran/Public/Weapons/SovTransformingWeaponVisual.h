// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameplayEffectTypes.h"
#include "Weapons/WeaponVisual.h"
#include "SovTransformingWeaponVisual.generated.h"

class FLifetimeProperty;
class UAnimMontage;
class UAnimInstance;
class UAnimSequenceBase;
class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UPrimitiveComponent;
class USoundBase;

UENUM(BlueprintType)
enum class ESovWeaponTransitionPhase : uint8
{
	Holstered,
	Drawing,
	Deploying,
	Ready,
	Retracting,
	Stowing
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovWeaponTransitionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Weapon Transition")
	ESovWeaponTransitionPhase Phase = ESovWeaponTransitionPhase::Holstered;

	/** Semantic wield slot requested by Narrative. Empty means the holster. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Weapon Transition")
	FGameplayTag TargetWieldSlot;

	/** Synchronized server clock at which this phase began. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Weapon Transition")
	float ServerPhaseStartTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Weapon Transition")
	float PhaseDuration = 0.0f;

	/** Absolute blade extension at phase start. Enables seamless reversals. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Weapon Transition")
	float PhaseStartProgress = 0.0f;

	/** Increments once for each draw or stow request. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Weapon Transition")
	int32 TransitionSerial = 0;
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovWeaponTransitionCue
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue")
	TObjectPtr<USoundBase> Sound = nullptr;

	/** Optional socket on the active weapon mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue")
	FName AttachSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue")
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue", meta = (ClampMin = "0.01"))
	float PitchMultiplier = 1.0f;

	/** Late-relevant clients older than this do not replay the one-shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cue", meta = (ClampMin = "0.0", Units = "s"))
	float MaximumLatePlaybackAge = 0.35f;
};

/**
 * Reusable staged Narrative weapon visual for mechanically transforming weapons.
 *
 * Narrative's replicated wield state remains authoritative for equipment,
 * abilities, and saves. This actor defers only the physical socket handoff,
 * reconstructing presentation from a replicated semantic phase and server time.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Transforming Weapon Visual"))
class PROJECTVELKORRAN_API ASovTransformingWeaponVisual : public AWeaponVisual
{
	GENERATED_BODY()

public:
	ASovTransformingWeaponVisual(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool HandleAttachmentRequest_Implementation(
		const FGameplayTag& EquipSlot,
		const FGameplayTag& TargetWieldSlot) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weapon Transition")
	bool BeginDraw(FGameplayTag TargetWieldSlot);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weapon Transition")
	bool BeginStow();

	/** Optional authoritative montage-notify handoff. Native timers remain the watchdog. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weapon Transition")
	bool CommitWieldAttachment(int32 ExpectedTransitionSerial);

	/** Optional authoritative montage-notify handoff. Native timers remain the watchdog. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weapon Transition")
	bool CommitHolsterAttachment(int32 ExpectedTransitionSerial);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weapon Transition")
	bool IsWeaponReady() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weapon Transition")
	FSovWeaponTransitionState GetTransitionState() const
	{
		return TransitionState;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weapon Transition")
	float GetTransitionProgress() const;

	/**
	 * Plays the optional weapon-skeleton montage authored for a Deflection attempt.
	 * The predicted owner plays immediately; authority multicasts the cosmetic to
	 * observers. The visual must be wielded and fully Ready.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Weapon Transition|Deflection")
	bool PlayDeflectionWeaponMontage();

	/** Stops only the montage started by PlayDeflectionWeaponMontage. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Weapon Transition|Deflection")
	void StopDeflectionWeaponMontage();

protected:
	virtual void OnWielded() override;
	virtual void OnHolstered() override;
	virtual void HandleAttachedToOwner_Implementation() override;
	virtual TArray<UPrimitiveComponent*> GetCollidingPrimitives_Implementation() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition")
	bool bEnableStagedTransitions = true;

	/** Character montage played while the weapon remains collapsed on its holster. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Character")
	TObjectPtr<UAnimMontage> CharacterDrawMontage = nullptr;

	/** Character montage played while the weapon retracts in hand. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Character")
	TObjectPtr<UAnimMontage> CharacterStowMontage = nullptr;

	/** Optional first-person override. Falls back to Character Draw Montage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Character")
	TObjectPtr<UAnimMontage> LocalCharacterDrawMontage = nullptr;

	/** Optional first-person override. Falls back to Character Stow Montage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Character")
	TObjectPtr<UAnimMontage> LocalCharacterStowMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Character", meta = (ClampMin = "0.01"))
	float CharacterDrawPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Character", meta = (ClampMin = "0.01"))
	float CharacterStowPlayRate = 1.0f;

	/** Time from draw request until Narrative commits the hand socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float DrawAttachmentDelay = 0.25f;

	/** Used only when the deploy animation is absent or has zero length. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float FallbackDeployDuration = 0.35f;

	/** Used only when the retract animation is absent or has zero length. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float FallbackRetractDuration = 0.35f;

	/** Collapsed hold between retract completion and the holster socket handoff. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float HolsterAttachmentDelayAfterRetract = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Weapon")
	TObjectPtr<UAnimSequenceBase> WeaponHolsteredAnimation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Weapon")
	TObjectPtr<UAnimSequenceBase> WeaponDeployAnimation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Weapon")
	TObjectPtr<UAnimSequenceBase> WeaponReadyAnimation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Weapon")
	TObjectPtr<UAnimSequenceBase> WeaponRetractAnimation = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Weapon", meta = (ClampMin = "0.01"))
	float WeaponDeployPlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Weapon", meta = (ClampMin = "0.01"))
	float WeaponRetractPlayRate = 1.0f;

	/** Disable if a child Blueprint drives a custom weapon AnimBP instead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Weapon")
	bool bUseNativeSingleNodeWeaponAnimation = true;

	/**
	 * Optional Verity-skeleton montage played when Selene starts Deflection.
	 * This requires a weapon AnimBP with the montage's Slot node and requires
	 * native single-node weapon animation to be disabled on the visual.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Deflection")
	TObjectPtr<UAnimMontage> WeaponDeflectionMontage = nullptr;

	/** Optional first-person weapon override. Falls back to Weapon Deflection Montage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Deflection")
	TObjectPtr<UAnimMontage> LocalWeaponDeflectionMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Deflection", meta = (ClampMin = "0.01"))
	float WeaponDeflectionMontagePlayRate = 1.0f;

	/** Optional montage section to start from. Empty starts at the montage beginning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Deflection")
	FName WeaponDeflectionMontageStartSection = NAME_None;

	/** Used when death, Fatal, Poise break, ragdoll, or Sequencer control cancels Deflection. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Deflection", meta = (ClampMin = "0.0", Units = "s"))
	float WeaponDeflectionMontageCancelBlendOutTime = 0.10f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Presentation")
	FSovWeaponTransitionCue DrawCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Presentation")
	FSovWeaponTransitionCue DeployCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Presentation")
	FSovWeaponTransitionCue RetractCue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Presentation")
	FSovWeaponTransitionCue StowCue;

	/** Optional scalar driven from collapsed to extended across the transition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Material")
	FName TransformationMaterialParameter = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Material")
	float CollapsedMaterialValue = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Weapon Transition|Material")
	float ExtendedMaterialValue = 1.0f;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Weapon Transition", meta = (DisplayName = "Weapon Transition Phase Changed"))
	void ReceiveTransitionPhaseChanged(
		ESovWeaponTransitionPhase Phase,
		float NormalizedProgress,
		int32 TransitionSerial);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Weapon Transition", meta = (DisplayName = "Wield Attachment Committed"))
	void ReceiveWieldAttachmentCommitted();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Weapon Transition", meta = (DisplayName = "Holster Attachment Committed"))
	void ReceiveHolsterAttachmentCommitted();

private:
	friend struct FSovEchoResourceTestAccess;
	UFUNCTION()
	void OnRep_TransitionState();

	void EnterPhase(
		ESovWeaponTransitionPhase NewPhase,
		float Duration,
		bool bBeginNewTransition);
	bool BeginStowInternal();
	bool BeginWieldSlotChange(FGameplayTag TargetWieldSlot);
	void HandlePhaseTimerExpired();
	void ForceCompleteTransition();
	void RecoverFromDeathInterruption();
	void ApplyTransitionState();
	void ApplyWeaponAnimation(float PhaseAge);
	void ApplyCharacterMontage(float PhaseAge);
	void StopCharacterTransitionMontages(float BlendOutTime = 0.10f);
	bool PlayDeflectionWeaponMontageLocal();
	void StopDeflectionWeaponMontageLocal(float BlendOutTime);
	void ApplyStableWeaponPose(bool bReadyPose);
	void PlayPhaseCue(const FSovWeaponTransitionCue& Cue, float PhaseAge);
	void RefreshDynamicMaterials();
	void ApplyMaterialProgress(float NormalizedProgress);
	void SetTransitionGateActive(bool bActive);
	void RefreshReadyCollisionData();
	void ProcessQueuedAttachmentRequest();
	float GetSynchronizedServerTime() const;
	float GetCurrentPhaseAge() const;
	float GetAnimationDuration(
		const UAnimSequenceBase* Animation,
		float PlayRate,
		float FallbackDuration) const;
	bool IsTransitionPhase() const;
	bool IsOwnerDead() const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayDeflectionWeaponMontage();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastStopDeflectionWeaponMontage(float BlendOutTime);

	UPROPERTY(ReplicatedUsing = OnRep_TransitionState)
	FSovWeaponTransitionState TransitionState;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TransformationMaterials;

	FTimerHandle PhaseTimerHandle;
	FTimerHandle CollisionRefreshTimerHandle;
	FActiveGameplayEffectHandle TransitionGateEffectHandle;
	TWeakObjectPtr<UAbilitySystemComponent> TransitionGateAbilitySystem;
	TWeakObjectPtr<UAnimInstance> ActiveMainCharacterAnimInstance;
	TWeakObjectPtr<UAnimInstance> ActiveLocalCharacterAnimInstance;
	bool bUpdatingTransitionGate = false;
	bool bRequestedTransitionGate = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMainCharacterMontage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveLocalCharacterMontage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMainWeaponDeflectionMontage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveLocalWeaponDeflectionMontage = nullptr;

	ESovWeaponTransitionPhase LastPresentedPhase = ESovWeaponTransitionPhase::Holstered;
	int32 LastPresentedSerial = INDEX_NONE;
	bool bOwnsLocalLooseTransitionGate = false;
	bool bHasObservedPhysicalAttachment = false;
	bool bPhysicalAttachmentCommitInProgress = false;
	bool bHasQueuedAttachmentRequest = false;
	bool bAwaitingDeathRecovery = false;
	bool bHasReceivedAuthoritativeTransitionState = false;
	bool bLoggedDeflectionMontageSetupWarning = false;
	bool bAwaitingAuthoritativeDeflectionMontage = false;
	uint32 AttachmentRequestGeneration = 0;
	uint32 QueuedRequestGeneration = 0;
	FGameplayTag LatestRequestedWieldSlot;
	FGameplayTag QueuedEquipSlot;
	FGameplayTag QueuedTargetWieldSlot;
};
