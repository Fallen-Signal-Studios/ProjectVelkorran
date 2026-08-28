// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "SovReformationDroneSelfDestructPresentation.generated.h"

class FLifetimeProperty;
class UAudioComponent;
class UCameraShakeBase;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class URadialForceComponent;
class USceneComponent;
class USoundBase;

UENUM(BlueprintType)
enum class ESovReformationDroneSelfDestructPhase : uint8
{
	Inactive,
	Pursuit,
	Warning,
	Detonated,
	Cancelled
};

/** One semantic packet reconstructs the correct phase for late-relevant clients. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovReformationDroneSelfDestructPresentationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct")
	TObjectPtr<AActor> SourceDrone = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct")
	ESovReformationDroneSelfDestructPhase Phase =
		ESovReformationDroneSelfDestructPhase::Inactive;

	/** Synchronized server clock at which the current phase began. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct")
	float ServerPhaseStartTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct")
	float WarningDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct")
	FVector_NetQuantize10 DetonationLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct")
	float ExplosionRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct")
	bool bDamagedAnyTarget = false;

	/** Prevents a prepared packet from playing before gameplay resolution. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct")
	bool bDetonationFinalized = false;
};

/**
 * Replicated cosmetic state for a Reformation drone suicide run.
 *
 * Gameplay remains authority-owned by the ability. This actor follows the
 * drone cosmetically during pursuit, then freezes at the blast location and
 * survives Narrative's synchronous death teardown long enough for the true
 * explosion audio, Niagara, decal, shake, and physics impulse to finish.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Reformation Drone Self Destruct Presentation"))
class PROJECTVELKORRAN_API ASovReformationDroneSelfDestructPresentation : public AActor
{
	GENERATED_BODY()

public:
	ASovReformationDroneSelfDestructPresentation();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Called on authority before FinishSpawningActor. */
	void InitializeForDrone(AActor* InSourceDrone, float InWarningDuration);

	/** Begins the synchronized warning/fuse phase. */
	void EnterWarning(float InWarningDuration);

	/** Freezes replicated state before damage callbacks can cancel the ability. */
	void PrepareDetonation(
		const FVector& InDetonationLocation,
		float InExplosionRadius);

	/** Finalizes cosmetics and physics after the authoritative radial pass. */
	void FinalizeDetonation(bool bInDamagedAnyTarget);

	/** Stops persistent presentation and resets only this system's scalar. */
	void CancelPresentation();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Self Destruct")
	FSovReformationDroneSelfDestructPresentationState GetPresentationState() const
	{
		return PresentationState;
	}

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Components")
	TObjectPtr<USceneComponent> PresentationRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Components")
	TObjectPtr<UNiagaraComponent> TravelNiagaraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Components")
	TObjectPtr<UNiagaraComponent> WarningNiagaraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Components")
	TObjectPtr<UAudioComponent> TravelAudioComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Components")
	TObjectPtr<UAudioComponent> WarningAudioComponent;

	/** Select this component in a child Blueprint to tune physical blast impulse. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Components")
	TObjectPtr<URadialForceComponent> ExplosionRadialForce;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Material")
	FName ChargeMaterialParameter = TEXT("SelfDestructCharge");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InactiveChargeValue = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PursuitChargeValue = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PursuitPulseAmplitude = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Material", meta = (ClampMin = "0.0", Units = "Hz"))
	float PursuitPulseFrequency = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WarningChargeStartValue = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WarningChargeEndValue = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Material", meta = (ClampMin = "0.0"))
	float WarningPulseAmplitude = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Material", meta = (ClampMin = "0.0", Units = "Hz"))
	float WarningPulseFrequency = 5.0f;

	/** Runtime appearance pieces are rescanned so asynchronously loaded Narrative meshes inherit the charge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Material", meta = (ClampMin = "0.02", Units = "s"))
	float MaterialRescanInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Travel")
	TObjectPtr<UNiagaraSystem> TravelNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Travel")
	FTransform TravelNiagaraRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Travel")
	TObjectPtr<USoundBase> TravelLoopSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Travel", meta = (ClampMin = "0.0"))
	float TravelLoopVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Travel", meta = (ClampMin = "0.01"))
	float TravelLoopPitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Travel")
	FTransform TravelAudioRelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Warning")
	TObjectPtr<UNiagaraSystem> WarningNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Warning")
	FTransform WarningNiagaraRelativeTransform = FTransform::Identity;

	/** Non-looping detonation indication/fuse sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Warning")
	TObjectPtr<USoundBase> DetonationIndicationSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Warning", meta = (ClampMin = "0.0"))
	float DetonationIndicationVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Warning", meta = (ClampMin = "0.01"))
	float DetonationIndicationPitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Warning", meta = (ClampMin = "0.0", Units = "s"))
	float WarningOneShotMaximumAge = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion")
	FTransform ExplosionNiagaraRelativeTransform = FTransform::Identity;

	/** Optional Niagara float parameter populated with the gameplay blast radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion")
	FName ExplosionRadiusNiagaraParameter = TEXT("User.ExplosionRadius");

	/** Non-looping true explosion sound, distinct from the warning cue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion", meta = (ClampMin = "0.0"))
	float ExplosionVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion", meta = (ClampMin = "0.01"))
	float ExplosionPitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion", meta = (ClampMin = "0.0", Units = "s"))
	float ExplosionOneShotMaximumAge = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion", meta = (ClampMin = "0.1", Units = "s"))
	float ExplosionCleanupDelay = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Camera Shake")
	TSubclassOf<UCameraShakeBase> ExplosionCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Camera Shake", meta = (EditCondition = "ExplosionCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionCameraShakeInnerRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Camera Shake", meta = (EditCondition = "ExplosionCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionCameraShakeOuterRadius = 1600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Camera Shake", meta = (EditCondition = "ExplosionCameraShakeClass != nullptr", ClampMin = "0.0"))
	float ExplosionCameraShakeFalloff = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Decal")
	TObjectPtr<UMaterialInterface> ExplosionDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	FVector ExplosionDecalSize = FVector(18.0f, 180.0f, 180.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionDecalSearchDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "s"))
	float ExplosionDecalVisibleDuration = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Decal", meta = (EditCondition = "ExplosionDecalMaterial != nullptr", ClampMin = "0.0", Units = "s"))
	float ExplosionDecalFadeDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Physics")
	bool bApplyExplosionPhysicsImpulse = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Physics", meta = (EditCondition = "bApplyExplosionPhysicsImpulse", ClampMin = "0.0"))
	float ExplosionPhysicsRadiusScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Self Destruct|Explosion|Physics", meta = (EditCondition = "bApplyExplosionPhysicsImpulse", ClampMin = "0.0", Units = "cm"))
	float ExplosionPhysicsUpwardBias = 65.0f;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Reformation Drone|Self Destruct|Presentation", meta = (DisplayName = "Self Destruct Pursuit Started"))
	void ReceivePursuitStarted();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Reformation Drone|Self Destruct|Presentation", meta = (DisplayName = "Self Destruct Warning Started"))
	void ReceiveWarningStarted(float RemainingWarningTime);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Reformation Drone|Self Destruct|Presentation", meta = (DisplayName = "Self Destruct Detonated"))
	void ReceiveDetonated(FVector DetonationLocation);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Reformation Drone|Self Destruct|Presentation", meta = (DisplayName = "Self Destruct Cancelled"))
	void ReceiveCancelled();

private:
	UFUNCTION()
	void OnRep_PresentationState();

	void ApplyPresentationState();
	void PlayPursuitPresentation();
	void PlayWarningPresentation();
	void PlayDetonationPresentation();
	void PlayCancellationPresentation();
	void StopPersistentPresentation();
	void FollowSourceDrone();
	void RefreshChargeMaterials();
	void AddChargeMaterialsFromActor(AActor* MeshOwner);
	void SetChargeMaterialValue(float Value);
	void SpawnExplosionDecal();
	float GetSynchronizedServerTime() const;
	float GetCurrentPhaseAge() const;
	float CalculateChargeValue() const;

	UPROPERTY(ReplicatedUsing = OnRep_PresentationState)
	FSovReformationDroneSelfDestructPresentationState PresentationState;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ChargeMaterials;

	float MaterialRescanAccumulator = 0.0f;
	bool bPlayedPursuitPresentation = false;
	bool bPlayedWarningPresentation = false;
	bool bPlayedDetonationPresentation = false;
	bool bPlayedCancellationPresentation = false;
};
