// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "SovWeakPointComponent.generated.h"

class FLifetimeProperty;
class UNarrativeAbilitySystemComponent;
class UPhysicalMaterial;

UENUM(BlueprintType)
enum class ESovWeakPointHitResolution : uint8
{
	NotWeakPoint UMETA(DisplayName = "Not a Weak Point"),
	AlreadyBroken UMETA(DisplayName = "Already Broken"),
	NewlyBroken UMETA(DisplayName = "Newly Broken")
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
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSovWeakPointStateChangedSignature,
	FName, WeakPointId,
	bool, bIsBroken);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSovWeakPointBrokenSignature,
	FName, WeakPointId,
	const FSovDamageResult&, DamageResult);

/**
 * Authoritative, replicated lifecycle for explicitly authored weak points.
 *
 * The target resolves a weak-point break from its typed damage callback before
 * the source ASC receives the same transaction. Source-specific reward systems
 * may then consume that exact break once without guessing from a bone name or
 * repeatedly rewarding hits against a zone that is already broken.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovWeakPointComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovWeakPointComponent();

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Weak Point")
	bool InitializeWithAbilitySystem(
		UNarrativeAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point")
	bool IsWeakPointBroken(FName WeakPointId) const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Weak Point")
	TArray<FName> GetBrokenWeakPointIds() const { return BrokenWeakPointIds; }

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

	/** Restores the authored starting state. Only authority may mutate it. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Weak Point")
	void ResetWeakPoints();

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Weak Point|Presentation")
	FSovWeakPointStateChangedSignature OnWeakPointStateChanged;

	/** Authoritative detailed notification for gameplay and server presentation. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Weak Point|Presentation")
	FSovWeakPointBrokenSignature OnWeakPointBroken;

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

private:
	struct FPendingWeakPointBreak
	{
		FGuid TransactionId;
		TWeakObjectPtr<AActor> SourceActor;
		FGameplayEffectContextHandle EffectContext;
		FName HitZone = NAME_None;
		FName WeakPointId = NAME_None;
	};

	void TryInitializeFromOwner();
	void UninitializeFromAbilitySystem();
	void ApplyAuthoredStartingState();
	void ClearPendingBreaks();
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
	void HandleDamageResolvedAsTarget(const FSovDamageResult& DamageResult);

	UFUNCTION()
	void HandleDeathStateChanged(
		AActor* ChangedActor,
		UNarrativeAbilitySystemComponent* ChangedActorASC,
		bool bIsDead);

	UFUNCTION()
	void OnRep_BrokenWeakPointIds(const TArray<FName>& OldBrokenWeakPointIds);

	UPROPERTY(ReplicatedUsing = OnRep_BrokenWeakPointIds, Transient)
	TArray<FName> BrokenWeakPointIds;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * Normally consumed immediately by the source callback. A small array keeps
	 * that contract correct if a presentation listener causes nested damage.
	 */
	TArray<FPendingWeakPointBreak> PendingBreaks;
	bool bAppliedStartingState = false;
};
