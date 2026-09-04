// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "NarrativeSavableComponent.h"
#include "TimerManager.h"
#include "SovWeakPointComponent.generated.h"

class ANarrativeCharacter;
class ANarrativeCharacterVisual;
class FLifetimeProperty;
class UNarrativeAbilitySystemComponent;
class UDecalComponent;
class UMaterialInterface;
class UMeshComponent;
class UPhysicalMaterial;
class USkeletalMeshComponent;
struct FActiveGameplayEffect;

UENUM(BlueprintType)
enum class ESovWeakPointHitResolution : uint8
{
	NotWeakPoint UMETA(DisplayName = "Not a Weak Point"),
	AlreadyBroken UMETA(DisplayName = "Already Broken"),
	NewlyBroken UMETA(DisplayName = "Newly Broken"),
	AcceptedUnbrokenHit UMETA(DisplayName = "Accepted Unbroken Hit")
};

/** Sustained capability loss authored on a zone, independent of its hit matcher. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovWeakPointConsequence
{
	GENERATED_BODY()

	/** Ability asset tags blocked while the consequence is active. Shared counts are preserved. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consequence")
	FGameplayTagContainer BlockedAbilityTags;

	/** Active abilities with these tags are canceled once on a new break, never during restore. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consequence")
	FGameplayTagContainer CancelAbilityTags;

	/** Equipment/system state tags supplied by an owned native GameplayEffect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consequence")
	FGameplayTagContainer GrantedStateTags;

	/** Zero lasts until zone reset. Positive durations expire without repairing the zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consequence", meta = (ClampMin = "0.0", Units = "s"))
	float Duration = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovWeakPointConsequenceSnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Snapshot")
	FName ZoneId = NAME_None;
	/** -1 denotes a permanent consequence; positive values are remaining world seconds. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Snapshot")
	float RemainingSeconds = -1.f;
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovWeakPointStateSnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Snapshot")
	TArray<FName> BrokenZoneIds;
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Snapshot")
	TArray<FSovWeakPointConsequenceSnapshot> Consequences;
};

/** One authored, independently breakable weak-point zone. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovWeakPointZone
{
	GENERATED_BODY()

	/** Stable gameplay identity. This must be unique within the component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point")
	FName ZoneId = NAME_None;

	/** Skeletal hit bones that identify this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point")
	TArray<FName> HitBones;

	/** When enabled, descendants of an authored HitBones entry also match. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point")
	bool bMatchDescendantBones = true;

	/** Optional physical-material identities for non-skeletal or layered targets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point")
	TArray<TObjectPtr<UPhysicalMaterial>> PhysicalMaterials;

	/** Allows an encounter variant to begin with this zone unavailable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point")
	bool bStartsBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point|Consequence")
	FSovWeakPointConsequence Consequence;

	/**
	 * Bone or socket used to anchor the temporary reveal decal. When empty, the
	 * first authored HitBones entry is used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	FName RevealAttachPoint = NAME_None;

	/** Optional component tag used when more than one skeletal mesh owns the bone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	FName RevealMeshComponentTag = NAME_None;

	/** Bone/socket-relative placement and orientation of the projected decal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	FTransform RevealRelativeTransform = FTransform::Identity;

	/** Decal projection depth, width, and height in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	FVector RevealDecalSize = FVector(12.0f, 24.0f, 24.0f);

	/** Optional zone-specific material; otherwise the component default is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	TObjectPtr<UMaterialInterface> RevealDecalMaterialOverride = nullptr;
};

/** Replicated timing for a temporary weak-point reveal. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovWeakPointRevealState
{
	GENERATED_BODY()

	/** Changes on every authoritative start, refresh, or clear. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	int32 Serial = 0;

	/** Synchronized GameState server time at which the reveal ends. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	float EndServerWorldTime = 0.0f;

	/** Actor responsible for the reveal, normally Selene. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	TObjectPtr<AActor> RevealInstigator = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSovWeakPointStateChangedSignature,
	FName, WeakPointId,
	bool, bIsBroken);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSovWeakPointBrokenSignature,
	FName, WeakPointId,
	const FSovDamageResult&, DamageResult);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovWeakPointRevealStateChangedSignature,
	bool, bIsRevealed,
	float, RemainingSeconds,
	AActor*, RevealInstigator);

/**
 * Authoritative, replicated lifecycle for explicitly authored weak points.
 *
 * The target resolves a weak-point break from its typed damage callback before
 * the source ASC receives the same transaction. Source-specific reward systems
 * may then consume that exact break once without guessing from a bone name or
 * repeatedly rewarding hits against a zone that is already broken.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovWeakPointComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()

public:
	USovWeakPointComponent();
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point|Save")
	FSovWeakPointStateSnapshot CaptureWeakPointState() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point|Save")
	bool CanRestoreWeakPointState(const FSovWeakPointStateSnapshot& State) const;

	/** Restores state and remaining sustained consequences, with no hit, reward, or cancellation replay. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weak Point|Save")
	bool RestoreWeakPointState(const FSovWeakPointStateSnapshot& State);

	/** Authored anatomical/system failure. No damage transaction or Echo award is fabricated. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weak Point")
	bool BreakWeakPointWithoutReward(FName WeakPointId);

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Weak Point")
	bool InitializeWithAbilitySystem(
		UNarrativeAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point")
	bool IsWeakPointBroken(FName WeakPointId) const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point")
	TArray<FName> GetBrokenWeakPointIds() const { return BrokenWeakPointIds; }

	/** Begins or extends a replicated reveal of every currently unbroken zone. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weak Point|Reveal")
	bool RevealWeakPoints(
		float Duration,
		AActor* RevealInstigator = nullptr);

	/** Clears the replicated reveal immediately. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weak Point|Reveal")
	void ClearWeakPointReveal();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point|Reveal")
	bool IsWeakPointRevealActive() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point|Reveal")
	float GetWeakPointRevealRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point|Reveal")
	FLinearColor GetWeakPointRevealColor() const
	{
		return WeakPointRevealColor;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point|Reveal")
	FName GetWeakPointRevealColorParameterName() const
	{
		return RevealColorParameterName;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point|Reveal")
	float GetWeakPointRevealFadeOutDuration() const
	{
		return FMath::Max(RevealFadeOutDuration, 0.0f);
	}

	/** Returns the unbroken zone IDs visible during the current reveal. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point|Reveal")
	TArray<FName> GetRevealedWeakPointIds() const;

	/** Rebuilds local decals after an appearance or material change. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Weak Point|Reveal")
	void RefreshWeakPointRevealPresentation();

	/** Reports invalid IDs, duplicate IDs, and zones with no authored matcher. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point")
	bool HasValidWeakPointConfiguration() const;

	/**
	 * Atomically resolves the target side of one authoritative damage result.
	 * This explicit outcome keeps body hits and already-broken zones distinct.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weak Point")
	ESovWeakPointHitResolution ResolveWeakPointHit(
		const FSovDamageResult& DamageResult,
		FName& OutWeakPointId);

	/**
	 * Consumes the break produced by this exact damage transaction. The target's
	 * ASC resolves first, so this is intended for the source ASC's typed callback.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weak Point")
	bool ConsumeWeakPointBreak(
		const FSovDamageResult& DamageResult,
		FName& OutWeakPointId);

	/** Consumes a positive applied Health/Shield hit on a zone that was unbroken before this transaction. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weak Point")
	bool ConsumeWeakPointHit(const FSovDamageResult& DamageResult, FName& OutWeakPointId);

	/** Restores the authored starting state. Only authority may mutate it. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weak Point")
	void ResetWeakPoints();

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Weak Point|Presentation")
	FSovWeakPointStateChangedSignature OnWeakPointStateChanged;

	/** Authoritative detailed notification for gameplay and server presentation. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Weak Point|Presentation")
	FSovWeakPointBrokenSignature OnWeakPointBroken;

	/** Local notification reconstructed from the replicated timed reveal state. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Weak Point|Reveal")
	FSovWeakPointRevealStateChangedSignature OnWeakPointRevealStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point")
	TArray<FSovWeakPointZone> WeakPointZones;

	/** Minimum applied Shield or Health damage required to break a zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point", meta = (ClampMin = "0.0"))
	float MinimumAppliedDamage = 0.01f;

	/**
	 * Deferred Decal material used to project a localized reveal onto the mesh.
	 * Its opacity should multiply Unreal's Decal Lifetime Opacity for fading.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	TObjectPtr<UMaterialInterface> WeakPointRevealDecalMaterial = nullptr;

	/** Color supplied to RevealColorParameterName on a per-reveal material instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	FLinearColor WeakPointRevealColor = FLinearColor(1.0f, 0.015f, 0.01f, 1.0f);

	/** Vector parameter expected by the reveal decal material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal")
	FName RevealColorParameterName = TEXT("WeakPointRevealColor");

	/** Seconds at the end of the reveal during which the decal fades out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Weak Point|Reveal", meta = (ClampMin = "0.0", Units = "s"))
	float RevealFadeOutDuration = 0.35f;

private:
	struct FActiveConsequence
	{
		FName ZoneId = NAME_None;
		uint32 Serial = 0;
		FActiveGameplayEffectHandle Handle;
		FGameplayTagContainer BlockedAbilityTags;
	};
	void ApplyConsequence(const FSovWeakPointZone& Zone, float RemainingSeconds, bool bCancelActiveAbilities);
	void ClearConsequences();
	void HandleConsequenceRemoved(const FActiveGameplayEffect& Effect);
	const FSovWeakPointZone* FindZoneById(FName ZoneId) const;
	TArray<FActiveConsequence> ActiveConsequences;
	uint32 ConsequenceSerial = 0;
	uint32 StateEpoch = 0;
	bool bRestoringState = false;
	bool bUninitializingState = false;
	bool bPendingConsequenceRestore = false;
	FSovWeakPointStateSnapshot PendingConsequenceRestore;
	UPROPERTY(SaveGame)
	FSovWeakPointStateSnapshot SavedWeakPointState;
	UPROPERTY(SaveGame)
	bool bHasSavedWeakPointState = false;
	struct FPendingWeakPointBreak
	{
		FGuid TransactionId;
		TWeakObjectPtr<AActor> SourceActor;
		FGameplayEffectContextHandle EffectContext;
		FName HitZone = NAME_None;
		FName WeakPointId = NAME_None;
	};

	struct FDecalReceiverBinding
	{
		TWeakObjectPtr<UMeshComponent> MeshComponent;
		bool bPreviouslyReceivedDecals = false;
	};

	void TryInitializeFromOwner();
	void UninitializeFromAbilitySystem();
	void ApplyAuthoredStartingState();
	void ClearPendingBreaks();
	void BindCharacterVisual(ANarrativeCharacterVisual* NewCharacterVisual);
	void ApplyWeakPointRevealState();
	void ScheduleWeakPointRevealExpiry();
	void ScheduleWeakPointRevealVisualRefresh();
	void HandleWeakPointRevealExpired();
	void HandleDeferredWeakPointRevealVisualRefresh();
	void ClearWeakPointRevealPresentation();
	void DestroyActiveRevealDecals();
	void RefreshDecalReceiverBindings();
	void ClearDecalReceiverBindings();
	void GatherPresentationMeshes(TArray<UMeshComponent*>& OutMeshes) const;
	UMeshComponent* ResolveRevealAttachmentMesh(
		const FSovWeakPointZone& Zone,
		FName AttachPoint) const;
	FName ResolveRevealAttachPoint(const FSovWeakPointZone& Zone) const;
	float GetSynchronizedServerWorldTimeSeconds() const;
	const FSovWeakPointZone* FindMatchingZone(
		const FSovDamageResult& DamageResult) const;
	bool MatchesBone(
		const FSovWeakPointZone& Zone,
		const FSovDamageResult& DamageResult) const;
	bool MatchesPhysicalMaterial(
		const FSovWeakPointZone& Zone,
		const FSovDamageResult& DamageResult) const;
	void SetWeakPointBroken(FName WeakPointId, bool bShouldBeBroken);

	UFUNCTION()
	void HandleOwnerASCInitialized();

	UFUNCTION()
	void HandleCharacterVisualInitialized(ANarrativeCharacter* Character);

	UFUNCTION()
	void HandleBaseAppearanceApplied();

	UFUNCTION()
	void HandleAppearancePartChanged(FGameplayTag AppearanceSlot);

	UFUNCTION()
	void HandleDamageResolvedAsTarget(const FSovDamageResult& DamageResult);

	UFUNCTION()
	void HandleDeathStateChanged(
		AActor* ChangedActor,
		UNarrativeAbilitySystemComponent* ChangedActorASC,
		bool bIsDead);

	UFUNCTION()
	void OnRep_BrokenWeakPointIds(const TArray<FName>& OldBrokenWeakPointIds);

	UFUNCTION()
	void OnRep_WeakPointRevealState();

	UPROPERTY(ReplicatedUsing = OnRep_BrokenWeakPointIds, Transient)
	TArray<FName> BrokenWeakPointIds;

	UPROPERTY(ReplicatedUsing = OnRep_WeakPointRevealState, Transient)
	FSovWeakPointRevealState WeakPointRevealState;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<ANarrativeCharacterVisual> BoundCharacterVisual;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> ActiveRevealDecals;

	/**
	 * Normally consumed immediately by the source callback. A small array keeps
	 * that contract correct if a presentation listener causes nested damage.
	 */
	TArray<FPendingWeakPointBreak> PendingBreaks;
	TArray<FPendingWeakPointBreak> PendingHits;
	TSet<FGuid> ResolvedHitTransactions;
	TArray<FDecalReceiverBinding> DecalReceiverBindings;
	FTimerHandle WeakPointRevealExpiryTimerHandle;
	FTimerHandle WeakPointRevealVisualRefreshTimerHandle;
	bool bAppliedStartingState = false;
	bool bLocalWeakPointRevealActive = false;
	bool bWeakPointRevealVisualRefreshPending = false;
};
