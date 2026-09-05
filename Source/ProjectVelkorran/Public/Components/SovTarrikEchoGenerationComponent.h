// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffect.h"
#include "GAS/SovCombatTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SovTarrikEchoGenerationComponent.generated.h"

class FLifetimeProperty;
class UNarrativeAbilitySystemComponent;
class URangedWeaponItem;
class USovEchoComponent;
class USovProtectionInterceptReceipt;

UENUM(BlueprintType)
enum class ESovTarrikEchoAwardType : uint8 { PoiseBreak, HeavyMultiHit, CommandTargetKill, ProtectionIntercept };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSovTarrikEchoAwardedSignature,
	float, AwardedEcho, float, NewEcho, ESovTarrikEchoAwardType, AwardType, AActor*, Target);

UENUM(BlueprintType)
enum class ESovCinderlineEchoAwardType : uint8
{
	Cadence UMETA(DisplayName = "Cadence"),
	PrecisionKill UMETA(DisplayName = "Precision Kill")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSovCinderlineCadenceChangedSignature,
	int32, OldCadence,
	int32, NewCadence,
	int32, CadenceThreshold);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FSovCinderlineEchoAwardedSignature,
	float, AwardedEcho,
	float, NewEcho,
	ESovCinderlineEchoAwardType, AwardType,
	bool, bBossReduced);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FSovCinderlineHitConfirmedSignature,
	int32, NewCadence,
	int32, CadenceThreshold,
	bool, bPrecisionHit,
	FName, HitBone,
	bool, bBossReduced);

/**
 * Tarrik-specific Echo award rules.
 *
 * The shared USovEchoComponent remains the authoritative resource controller.
 * This component listens to Narrative's server-resolved damage notifications,
 * identifies ordinary Cinderline primary fire, and converts rhythmic confirmed
 * hits into Echo without allowing Echo abilities, Burn, explosions, guarded
 * shots, or other weapons to refund the resource.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovTarrikEchoGenerationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovTarrikEchoGenerationComponent();

	bool InitializeWithAbilitySystem(UNarrativeAbilitySystemComponent* InAbilitySystemComponent);

	/** Native only: source-owned receipt must prove the committed interception before reward policy. */
	void ConsumeProtectionIntercept(USovProtectionInterceptReceipt* Receipt, const FSovDamageResult& Result);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo|Cinderline")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo|Cinderline")
	int32 GetCinderlineCadence() const { return CinderlineCadence; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo|Cinderline")
	int32 GetCinderlineCadenceThreshold() const { return FMath::Max(CadenceThreshold, 1); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo|Cinderline")
	float GetCinderlineCadenceNormalized() const;

	/** Clears banked Cadence. Only authority can mutate the gameplay state. */
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Echo|Cinderline")
	void ResetCinderlineCadence();

	/** Called by Tarrik's concrete player class after Narrative applies a new wield state. */
	void HandleOwnerWieldStateChanged();

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Echo|Cinderline|Presentation")
	FSovCinderlineCadenceChangedSignature OnCinderlineCadenceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Echo|Cinderline|Presentation")
	FSovCinderlineEchoAwardedSignature OnCinderlineEchoAwarded;

	/** Owning-client cosmetic hook for reticle, audio, muzzle, and weapon feedback. */
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Echo|Cinderline|Presentation")
	FSovCinderlineHitConfirmedSignature OnCinderlineHitConfirmed;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Echo|Tarrik|Presentation")
	FSovTarrikEchoAwardedSignature OnTarrikEchoAwarded;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Optional stable class allow-list. An empty list still works when the
	 * primary-fire GA has Sov.Ability.Weapon.Cinderline.PrimaryFire, or when the
	 * source weapon grants Cinder Judgement/Requiem from that same item instance.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Identification")
	TArray<TSubclassOf<URangedWeaponItem>> AllowedCinderlineWeaponClasses;

	/** Cadence contributed by an ordinary confirmed body hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Tuning", meta = (ClampMin = "1"))
	int32 BodyHitCadence = 1;

	/** Cadence contributed by a configured weak-point hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Tuning", meta = (ClampMin = "1"))
	int32 PrecisionHitCadence = 2;

	/** Progress required to complete one firing cadence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Tuning", meta = (ClampMin = "1"))
	int32 CadenceThreshold = 6;

	/** Echo awarded for one completed firing cadence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Tuning", meta = (ClampMin = "0.0"))
	float CadenceEchoReward = 4.0f;

	/** Additional Echo awarded when the qualifying weak-point hit is fatal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Tuning", meta = (ClampMin = "0.0"))
	float PrecisionKillEchoReward = 3.0f;

	/** Time without a qualifying hit before unfinished Cadence is lost. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Tuning", meta = (ClampMin = "0.05"))
	float CadenceTimeout = 1.5f;

	/** Minimum time between ordinary Cadence payouts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Tuning", meta = (ClampMin = "0.0"))
	float CadenceAwardCooldown = 1.0f;

	/** Multiplier used when the damaged ASC owns Sov.Character.Enemy.Boss. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BossEchoMultiplier = 0.75f;

	/**
	 * SK Mannequin defaults. A hit on one of these bones or any descendant of
	 * one of them counts as precision when the hit component is skinned.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Precision")
	TArray<FName> PrecisionBoneNames;

	/** Also treat Narrative physical materials above the threshold as weak points. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Precision")
	bool bUsePhysicalMaterialWeakPoints = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo|Cinderline|Precision", meta = (EditCondition = "bUsePhysicalMaterialWeakPoints", ClampMin = "1.0"))
	float PrecisionPhysicalMaterialMultiplierThreshold = 1.01f;

	UPROPERTY(EditDefaultsOnly, Category = "Sovereign|Echo|Tarrik|Tuning", meta = (ClampMin = "0.1"))
	float ProtectionInterceptSourceCooldown = 5.0f;

private:
	bool CanGenerateTarrikEcho() const;
	TMap<TWeakObjectPtr<AActor>, float> ProtectionSourceAwardTimes;
	UFUNCTION()
	void HandleDamageResolvedAsSource(const FSovDamageResult& Result);
	UFUNCTION()
	void HandleEncounterScopeChanged(bool bStarted);
	void AwardTarrikEcho(float Amount, FGameplayTag Tag, ESovTarrikEchoAwardType Type, AActor* Target);
	UFUNCTION(Client, Unreliable)
	void ClientNotifyTarrikEchoAwarded(float Amount, float NewEcho, ESovTarrikEchoAwardType Type, AActor* Target);
	uint32 ResourceScopeEpoch = 0;
	void TryInitializeFromOwner();
	void UninitializeFromAbilitySystem();
	bool CanGenerateCinderlineEcho() const;
	bool IsQualifyingPrimaryFire(
		const FGameplayEffectSpec& EffectSpec,
		URangedWeaponItem*& OutSourceWeapon) const;
	bool IsConfiguredCinderlineWeapon(const URangedWeaponItem* SourceWeapon) const;
	bool WeaponGrantsCinderlineEchoAbility(const URangedWeaponItem* SourceWeapon) const;
	bool IsSourceWeaponStillWielded(const URangedWeaponItem* SourceWeapon) const;
	bool IsPrecisionHit(const FGameplayEffectSpec& EffectSpec) const;
	bool IsBossTarget(const UNarrativeAbilitySystemComponent* DamagedAbilitySystem) const;
	void AddCinderlineCadence(
		int32 Points,
		URangedWeaponItem* SourceWeapon,
		bool bBossReduced,
		bool bPrecisionHit,
		FName HitBone);
	void TryAwardCompletedCadence();
	void AwardEcho(
		float RequestedEcho,
		const FGameplayTag& SourceTag,
		ESovCinderlineEchoAwardType AwardType,
		bool bBossReduced);
	void HandleCadenceTimeout();
	void ResetCinderlineCadenceInternal();
	void SetCinderlineCadence(int32 NewCadence);
	float GetWorldTimeSeconds() const;

	UFUNCTION()
	void HandleOwnerASCInitialized();

	UFUNCTION()
	void HandleDealtDamage(
		UNarrativeAbilitySystemComponent* DamagedAbilitySystem,
		float Damage,
		const FGameplayEffectSpec& EffectSpec);

	UFUNCTION()
	void HandleEchoChanged(float OldEcho, float NewEcho, float MaxEcho);

	void HandleBlockingTagChanged(FGameplayTag CallbackTag, int32 NewCount);

	UFUNCTION()
	void OnRep_CinderlineCadence(int32 OldCadence);

	UFUNCTION(Client, Unreliable)
	void ClientNotifyCinderlineEchoAwarded(
		float AwardedEcho,
		float NewEcho,
		ESovCinderlineEchoAwardType AwardType,
		bool bBossReduced);

	UFUNCTION(Client, Unreliable)
	void ClientNotifyCinderlineHitConfirmed(
		int32 NewCadence,
		bool bPrecisionHit,
		FName HitBone,
		bool bBossReduced);

	UPROPERTY(ReplicatedUsing = OnRep_CinderlineCadence, Transient)
	int32 CinderlineCadence = 0;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<USovEchoComponent> EchoComponent;

	TWeakObjectPtr<URangedWeaponItem> PendingCadenceWeapon;
	FTimerHandle CadenceTimeoutTimerHandle;
	FTimerHandle CadenceAwardTimerHandle;
	FDelegateHandle DeadTagChangedHandle;
	FDelegateHandle FatalTagChangedHandle;
	FDelegateHandle EchoAbilityTagChangedHandle;
	float LastCadenceAwardWorldTime = -BIG_NUMBER;
	float PendingCadenceWeightedMultiplierSum = 0.0f;
	bool bBindingsActive = false;
	mutable bool bWarnedMissingCinderlineIdentity = false;
};
