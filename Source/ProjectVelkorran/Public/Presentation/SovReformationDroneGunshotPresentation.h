// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "SovReformationDroneGunshotPresentation.generated.h"

class UCameraShakeBase;
class UMaterialInterface;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class FLifetimeProperty;

/**
 * Short-lived replicated presentation packet for one authoritative drone shot.
 *
 * Gameplay never runs here. Authority initializes compact trace data before
 * FinishSpawning, then every non-dedicated net instance renders that immutable
 * packet exactly once. Class-default presentation slots make the native class
 * safe to use directly while allowing a Blueprint child to supply project art.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Reformation Drone Gunshot Presentation"))
class PROJECTVELKORRAN_API ASovReformationDroneGunshotPresentation : public AActor
{
	GENERATED_BODY()

public:
	ASovReformationDroneGunshotPresentation();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Must be called by authority before FinishSpawning. */
	void InitializeGunshotPresentation(
		const FVector& InTraceStart,
		const FVector& InTraceEnd,
		const FVector& InImpactNormal,
		AActor* InHitActor,
		FName InHitBone,
		EPhysicalSurface InImpactSurfaceType,
		bool bInBlockingHit,
		bool bInDamagedTarget,
		int32 InShotIndex);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Gunshot")
	FVector GetTraceStart() const { return FVector(TraceStart); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Gunshot")
	FVector GetTraceEnd() const { return FVector(TraceEnd); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Gunshot")
	FVector GetImpactNormal() const { return FVector(ImpactNormal); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Gunshot")
	AActor* GetHitActor() const { return HitActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Gunshot")
	FName GetHitBone() const { return HitBone; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Gunshot")
	EPhysicalSurface GetImpactSurfaceType() const
	{
		return ImpactSurfaceType.GetValue();
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Gunshot")
	bool HasBlockingHit() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Gunshot")
	bool DamagedTarget() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Reformation Drone|Gunshot")
	int32 GetShotIndex() const { return static_cast<int32>(ShotIndex); }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Components")
	TObjectPtr<USceneComponent> PresentationRoot;

	/** One-shot flash spawned at the replicated muzzle position. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Muzzle")
	TObjectPtr<UNiagaraSystem> MuzzleNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Muzzle")
	FVector MuzzleNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Muzzle")
	FRotator MuzzleNiagaraRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Muzzle")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Muzzle", meta = (ClampMin = "0.0"))
	float FireSoundVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Muzzle", meta = (ClampMin = "0.01"))
	float FireSoundPitchMultiplier = 1.0f;

	/** Optional world camera shake centered on the muzzle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Muzzle")
	TSubclassOf<UCameraShakeBase> FireCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Muzzle", meta = (EditCondition = "FireCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float FireCameraShakeInnerRadius = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Muzzle", meta = (EditCondition = "FireCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float FireCameraShakeOuterRadius = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Muzzle", meta = (EditCondition = "FireCameraShakeClass != nullptr", ClampMin = "0.0"))
	float FireCameraShakeFalloff = 1.0f;

	/** Beam/tracer system spawned at TraceStart and pointed along the trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Tracer")
	TObjectPtr<UNiagaraSystem> TracerNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Tracer")
	FVector TracerNiagaraScale = FVector::OneVector;

	/** Optional Niagara vector user parameter receiving the world-space endpoint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Tracer")
	FName TracerEndParameter = TEXT("User.TracerEnd");

	/** Optional Niagara float user parameter receiving trace length in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Tracer")
	FName TracerLengthParameter = TEXT("User.TracerLength");

	/** One-shot surface or target impact system. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact")
	TObjectPtr<UNiagaraSystem> ImpactNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact")
	FVector ImpactNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact")
	FRotator ImpactNiagaraRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact", meta = (ClampMin = "0.0"))
	float ImpactSoundVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact", meta = (ClampMin = "0.01"))
	float ImpactSoundPitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact")
	TSubclassOf<UCameraShakeBase> ImpactCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact", meta = (EditCondition = "ImpactCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float ImpactCameraShakeInnerRadius = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact", meta = (EditCondition = "ImpactCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float ImpactCameraShakeOuterRadius = 800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact", meta = (EditCondition = "ImpactCameraShakeClass != nullptr", ClampMin = "0.0"))
	float ImpactCameraShakeFalloff = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact|Decal")
	TObjectPtr<UMaterialInterface> ImpactDecalMaterial;

	/** Projection depth, width, and height. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact|Decal", meta = (EditCondition = "ImpactDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	FVector ImpactDecalSize = FVector(6.0f, 12.0f, 12.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact|Decal", meta = (EditCondition = "ImpactDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	float ImpactDecalSurfaceOffset = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact|Decal", meta = (EditCondition = "ImpactDecalMaterial != nullptr", ClampMin = "0.0", Units = "s"))
	float ImpactDecalVisibleDuration = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact|Decal", meta = (EditCondition = "ImpactDecalMaterial != nullptr", ClampMin = "0.0", Units = "s"))
	float ImpactDecalFadeDuration = 2.0f;

	/** Keeps decals attached to replicated moving targets or surfaces when possible. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation|Impact|Decal", meta = (EditCondition = "ImpactDecalMaterial != nullptr"))
	bool bAttachImpactDecal = true;

	/** Server retention window for this immutable replicated packet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Reformation Drone|Presentation", meta = (ClampMin = "0.1", Units = "s"))
	float PresentationLifetime = 1.5f;

	/** Cosmetic extension point called after native presentation has been emitted. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Reformation Drone|Presentation", meta = (DisplayName = "Gunshot Presented"))
	void ReceiveGunshotPresented(
		FVector InTraceStart,
		FVector InTraceEnd,
		FVector InImpactNormal,
		AActor* InHitActor,
		FName InHitBone,
		EPhysicalSurface InImpactSurfaceType,
		bool bInBlockingHit,
		bool bInDamagedTarget,
		int32 InShotIndex);

private:
	enum EPresentationFlag : uint8
	{
		BlockingHitFlag = 1 << 0,
		DamagedTargetFlag = 1 << 1,
		ReadyFlag = 1 << 2
	};

	UFUNCTION()
	void OnRep_PresentationFlags();

	void PlayPresentation();
	void SpawnMuzzlePresentation(const FVector& Direction);
	void SpawnTracerPresentation(const FVector& Direction);
	void SpawnImpactPresentation();
	void SpawnImpactDecal();
	USceneComponent* ResolveDecalAttachment(FName& OutAttachBone) const;

	UPROPERTY(Replicated)
	FVector_NetQuantize TraceStart = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector_NetQuantize TraceEnd = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal ImpactNormal = FVector::UpVector;

	UPROPERTY(Replicated)
	TObjectPtr<AActor> HitActor;

	UPROPERTY(Replicated)
	FName HitBone = NAME_None;

	UPROPERTY(Replicated)
	TEnumAsByte<EPhysicalSurface> ImpactSurfaceType = SurfaceType_Default;

	UPROPERTY(Replicated)
	uint8 ShotIndex = 0;

	/** Packed immutable state, with ReadyFlag acting as the RepNotify fence. */
	UPROPERTY(ReplicatedUsing = OnRep_PresentationFlags)
	uint8 PresentationFlags = 0;

	bool bPresentationPlayed = false;
};
