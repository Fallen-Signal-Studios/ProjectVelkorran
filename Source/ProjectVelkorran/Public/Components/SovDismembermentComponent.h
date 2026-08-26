// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dismemberment/SovDismembermentTypes.h"
#include "GAS/SovCombatTypes.h"
#include "NarrativeSavableComponent.h"
#include "SovDismembermentComponent.generated.h"

class ANarrativeCharacter;
class ANarrativeCharacterVisual;
class UNarrativeAbilitySystemComponent;
class UPhysicsAsset;
class USkeletalMesh;
class USkeletalMeshComponent;
class USovDismembermentProfile;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovLimbSeveredSignature,
	ESovDismembermentRegion, Region,
	FName, HitBone,
	FVector, SeverLocation);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovDismembermentStateChangedSignature,
	int32, NewSeveredRegionMask);

/**
 * Server-authoritative, bone-defined dismemberment for Narrative modular characters.
 * Add this component to any Narrative NPC or character Blueprint that can be severed.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovDismembermentComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()

public:
	USovDismembermentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Dismemberment")
	void InitializeWithAbilitySystem(UNarrativeAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dismemberment")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dismemberment")
	bool IsRegionSevered(ESovDismembermentRegion Region) const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dismemberment")
	void GetSeveredRegions(TArray<ESovDismembermentRegion>& OutRegions) const;

	/** Scripted sever path. It must be called on the authoritative character. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Dismemberment")
	bool ForceSeverRegion(
		ESovDismembermentRegion Region,
		FName HitBone,
		FVector ImpactLocation,
		FVector ImpactNormal,
		FVector DetachedLimbImpulse);

	/** Reapplies permanent state after manually changing meshes or presentation. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Dismemberment")
	void RefreshDismembermentVisuals();

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Dismemberment")
	FSovLimbSeveredSignature OnLimbSevered;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Dismemberment")
	FSovDismembermentStateChangedSignature OnDismembermentStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Configuration")
	bool bDismembermentEnabled = true;

	/** Assigned profile overrides the editable fallback skeleton mappings below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Configuration")
	TObjectPtr<USovDismembermentProfile> DismembermentProfile = nullptr;

	/** Epic human defaults are supplied by C++; edit these when no profile is assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Configuration", meta = (TitleProperty = "Region"))
	TArray<FSovDismembermentRegionDefinition> FallbackRegions;

	/** Empty uses a lethal, unguarded Edge hit fallback rule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Rules", meta = (TitleProperty = "MinimumAppliedHealthDamage"))
	TArray<FSovDismembermentRule> FallbackRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Rules", meta = (ClampMin = "0.0"))
	float DefaultMinimumAppliedHealthDamage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Rules", meta = (ClampMin = "0.0"))
	float DefaultMinimumHealthOverkillDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Impulse", meta = (ClampMin = "0.0"))
	float DetachedLimbImpulsePerDamage = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Impulse", meta = (ClampMin = "0.0"))
	float MinimumDetachedLimbImpulse = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment|Impulse", meta = (ClampMin = "0.0"))
	float MaximumDetachedLimbImpulse = 6000.f;

	/** Permanent for this actor, replicated to late joiners and serialized by Narrative Save. */
	UPROPERTY(ReplicatedUsing = OnRep_SeveredRegionMask, SaveGame, BlueprintReadOnly, Category = "Dismemberment|State")
	int32 SeveredRegionMask = 0;

private:
	UFUNCTION()
	void HandleDamageResolved(const FSovDamageResult& Result);

	UFUNCTION()
	void HandleAbilitySystemInitialized();

	UFUNCTION()
	void HandleDeathStateChanged(
		AActor* KilledActor,
		UNarrativeAbilitySystemComponent* KilledActorASC,
		const bool bIsDead);

	UFUNCTION()
	void HandleCharacterVisualInitialized(ANarrativeCharacter* Character);

	UFUNCTION()
	void HandleBaseAppearanceApplied();

	UFUNCTION()
	void HandleAppearancePartChanged(FGameplayTag AppearanceSlot);

	UFUNCTION()
	void OnRep_SeveredRegionMask();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySever(
		ESovDismembermentRegion Region,
		FName HitBone,
		FVector ImpactLocation,
		FVector ImpactNormal,
		FVector SeverLocation,
		FRotator SeverRotation,
		FVector DetachedLimbImpulse,
		int32 CosmeticSeed);

	void TryInitializeFromOwner();
	void BindCharacterVisual(ANarrativeCharacterVisual* NewCharacterVisual);
	void ScheduleVisualRefresh();
	void ScheduleVisualRefreshRetry();
	void HandleDeferredVisualRefresh();

	bool TrySeverFromDamageResult(const FSovDamageResult& Result);
	bool DoesDamageMeetAnyRule(const FSovDamageResult& Result) const;
	bool DoesDamageMeetRule(const FSovDamageResult& Result, const FSovDismembermentRule& Rule) const;
	const FSovDismembermentRegionDefinition* FindRegionDefinition(ESovDismembermentRegion Region) const;
	const FSovDismembermentRegionDefinition* FindRegionDefinitionForBone(FName HitBone) const;
	const TArray<FSovDismembermentRegionDefinition>& GetActiveRegionDefinitions() const;
	const TArray<FSovDismembermentRule>& GetActiveRules() const;

	bool CommitSever(
		const FSovDismembermentRegionDefinition& Definition,
		FName HitBone,
		const FVector& ImpactLocation,
		const FVector& ImpactNormal,
		const FVector& DetachedLimbImpulse);

	FTransform ResolveSeverTransform(const FSovDismembermentRegionDefinition& Definition) const;
	USkeletalMeshComponent* ResolvePrimaryMesh() const;
	void GatherPresentationMeshes(TArray<USkeletalMeshComponent*>& OutMeshes) const;
	void ApplyRegionVisualState(
		const FSovDismembermentRegionDefinition& Definition,
		const FTransform& SeverTransform);
	void EnsureStumpActor(
		const FSovDismembermentRegionDefinition& Definition,
		const FTransform& SeverTransform);
	bool FindBloodDecalSurface(
		const FSovDismembermentRegionDefinition& Definition,
		const FVector& Origin,
		const FVector& PreferredDirection,
		FHitResult& OutSurfaceHit) const;
	void PlaySeverCosmetics(
		const FSovDismembermentRegionDefinition& Definition,
		FName HitBone,
		const FVector& ImpactLocation,
		const FVector& ImpactNormal,
		const FTransform& SeverTransform,
		const FVector& DetachedLimbImpulse,
		int32 CosmeticSeed);

	static int32 GetRegionBit(ESovDismembermentRegion Region);
	static bool IsValidRegion(ESovDismembermentRegion Region);
	void ValidateConfiguration();

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ANarrativeCharacterVisual> BoundCharacterVisual = nullptr;

	UPROPERTY(Transient)
	TMap<ESovDismembermentRegion, TObjectPtr<AActor>> SpawnedStumpActors;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> LastPrimaryMeshAsset = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsAsset> LastPrimaryPhysicsAsset = nullptr;

	FTimerHandle VisualRefreshTimerHandle;
	bool bVisualRefreshScheduled = false;
	int32 VisualRefreshRetryCount = 0;
	int32 AppliedPhysicsRegionMask = 0;
	int32 CosmeticEventCounter = 0;
};
