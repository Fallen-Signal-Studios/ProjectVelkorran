// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "SovCinderJudgementPresentation.generated.h"

class UCameraShakeBase;
class UMaterialInterface;
class UNiagaraSystem;
class USceneComponent;
class USoundBase;
class FLifetimeProperty;

/**
 * Immutable replicated presentation packet for one authoritative Cinder
 * Judgement shot. Gameplay is resolved by the ability before this actor begins
 * play; missing art can never invalidate damage or a paid Echo transaction.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Cinder Judgement Presentation"))
class PROJECTVELKORRAN_API ASovCinderJudgementPresentation : public AActor
{
	GENERATED_BODY()

public:
	ASovCinderJudgementPresentation();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Must be called by authority before FinishSpawning. */
	void InitializeJudgementPresentation(
		const FVector& InTraceStart,
		const FVector& InTraceEnd,
		const FVector& InImpactNormal,
		AActor* InHitActor,
		FName InHitBone,
		EPhysicalSurface InImpactSurfaceType,
		float InExplosionRadius,
		bool bInBlockingHit,
		bool bInBlastTriggered,
		bool bInDirectDamageResolved,
		int32 InRadialTargetsResolved);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	FVector GetTraceStart() const { return FVector(TraceStart); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	FVector GetTraceEnd() const { return FVector(TraceEnd); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	FVector GetImpactNormal() const { return FVector(ImpactNormal); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	AActor* GetHitActor() const { return HitActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	FName GetHitBone() const { return HitBone; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	EPhysicalSurface GetImpactSurfaceType() const
	{
		return ImpactSurfaceType.GetValue();
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	float GetExplosionRadius() const { return ExplosionRadius; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	bool HasBlockingHit() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	bool BlastTriggered() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	bool DirectDamageResolved() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Cinder Judgement|Presentation")
	int32 GetRadialTargetsResolved() const
	{
		return static_cast<int32>(RadialTargetsResolved);
	}

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Components")
	TObjectPtr<USceneComponent> PresentationRoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Muzzle")
	TObjectPtr<UNiagaraSystem> MuzzleNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Muzzle")
	FVector MuzzleNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Muzzle")
	FRotator MuzzleNiagaraRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Muzzle")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Muzzle", meta = (ClampMin = "0.0"))
	float FireSoundVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Muzzle", meta = (ClampMin = "0.01"))
	float FireSoundPitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Muzzle")
	TSubclassOf<UCameraShakeBase> FireCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Muzzle", meta = (EditCondition = "FireCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float FireCameraShakeInnerRadius = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Muzzle", meta = (EditCondition = "FireCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float FireCameraShakeOuterRadius = 1600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Muzzle", meta = (EditCondition = "FireCameraShakeClass != nullptr", ClampMin = "0.0"))
	float FireCameraShakeFalloff = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Beam")
	TObjectPtr<UNiagaraSystem> BeamNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Beam")
	FVector BeamNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Beam")
	FName BeamEndParameter = TEXT("User.BeamEnd");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Beam")
	FName BeamLengthParameter = TEXT("User.BeamLength");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Impact")
	TObjectPtr<UNiagaraSystem> ImpactNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Impact")
	FVector ImpactNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Impact")
	FRotator ImpactNiagaraRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Impact")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Impact", meta = (ClampMin = "0.0"))
	float ImpactSoundVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Impact", meta = (ClampMin = "0.01"))
	float ImpactSoundPitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion")
	TObjectPtr<UNiagaraSystem> ExplosionNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion")
	FVector ExplosionNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion")
	FRotator ExplosionNiagaraRotationOffset = FRotator::ZeroRotator;

	/** Optional float user parameter receiving the authored blast radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion")
	FName ExplosionRadiusParameter = TEXT("User.ExplosionRadius");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion", meta = (ClampMin = "0.0"))
	float ExplosionSoundVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion", meta = (ClampMin = "0.01"))
	float ExplosionSoundPitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion")
	TSubclassOf<UCameraShakeBase> ExplosionCameraShakeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion", meta = (EditCondition = "ExplosionCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionCameraShakeInnerRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion", meta = (EditCondition = "ExplosionCameraShakeClass != nullptr", ClampMin = "0.0", Units = "cm"))
	float ExplosionCameraShakeOuterRadius = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Explosion", meta = (EditCondition = "ExplosionCameraShakeClass != nullptr", ClampMin = "0.0"))
	float ExplosionCameraShakeFalloff = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Miss")
	TObjectPtr<UNiagaraSystem> DissipationNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Miss")
	FVector DissipationNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Miss")
	TObjectPtr<USoundBase> DissipationSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Miss", meta = (ClampMin = "0.0"))
	float DissipationSoundVolumeMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Miss", meta = (ClampMin = "0.01"))
	float DissipationSoundPitchMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Scorch")
	TObjectPtr<UMaterialInterface> ScorchDecalMaterial;

	/** Projection depth, width, and height. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Scorch", meta = (EditCondition = "ScorchDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	FVector ScorchDecalSize = FVector(12.0f, 90.0f, 90.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Scorch", meta = (EditCondition = "ScorchDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	float ScorchSurfaceSearchDistance = 160.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Scorch", meta = (EditCondition = "ScorchDecalMaterial != nullptr", ClampMin = "0.0", Units = "cm"))
	float ScorchSurfaceOffset = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Scorch", meta = (EditCondition = "ScorchDecalMaterial != nullptr", ClampMin = "0.0", Units = "s"))
	float ScorchVisibleDuration = 14.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation|Scorch", meta = (EditCondition = "ScorchDecalMaterial != nullptr", ClampMin = "0.0", Units = "s"))
	float ScorchFadeDuration = 4.0f;

	/** Prevents stale replicated packets from replaying signature one-shots. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation", meta = (ClampMin = "0.0", Units = "s"))
	float MaximumReplayAge = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Cinder Judgement|Presentation", meta = (ClampMin = "0.1", Units = "s"))
	float PresentationLifetime = 2.5f;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "Sovereign|Cinder Judgement|Presentation", meta = (DisplayName = "Cinder Judgement Presented"))
	void ReceiveCinderJudgementPresented(
		FVector InTraceStart,
		FVector InTraceEnd,
		FVector InImpactNormal,
		AActor* InHitActor,
		FName InHitBone,
		EPhysicalSurface InImpactSurfaceType,
		float InExplosionRadius,
		bool bInBlockingHit,
		bool bInBlastTriggered,
		bool bInDirectDamageResolved,
		int32 InRadialTargetsResolved);

private:
	enum EPresentationFlag : uint8
	{
		BlockingHitFlag = 1 << 0,
		BlastTriggeredFlag = 1 << 1,
		DirectDamageResolvedFlag = 1 << 2,
		RadialDamageResolvedFlag = 1 << 3,
		ReadyFlag = 1 << 4
	};

	UFUNCTION()
	void OnRep_PresentationFlags();

	void PlayPresentation();
	bool IsFreshEnoughToPresent() const;
	void SpawnMuzzlePresentation(const FVector& Direction);
	void SpawnBeamPresentation(const FVector& Direction);
	void SpawnImpactPresentation();
	void SpawnExplosionPresentation();
	void SpawnDissipationPresentation(const FVector& Direction);
	bool FindScorchSurface(FHitResult& OutSurfaceHit) const;
	void SpawnScorchDecal();

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
	float ExplosionRadius = 0.0f;

	UPROPERTY(Replicated)
	float ServerFireTime = 0.0f;

	UPROPERTY(Replicated)
	uint8 RadialTargetsResolved = 0;

	/** Final ready bit acts as the immutable packet's RepNotify fence. */
	UPROPERTY(ReplicatedUsing = OnRep_PresentationFlags)
	uint8 PresentationFlags = 0;

	bool bPresentationPlayed = false;
};
