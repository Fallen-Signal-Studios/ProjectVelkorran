// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Presentation/SovReformationDroneGunshotPresentation.h"

#include "Camera/CameraShakeBase.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

ASovReformationDroneGunshotPresentation::
	ASovReformationDroneGunshotPresentation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	bNetLoadOnClient = false;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);

	PresentationRoot = CreateDefaultSubobject<USceneComponent>(
		TEXT("PresentationRoot"));
	SetRootComponent(PresentationRoot);
}

void ASovReformationDroneGunshotPresentation::InitializeGunshotPresentation(
	const FVector& InTraceStart,
	const FVector& InTraceEnd,
	const FVector& InImpactNormal,
	AActor* InHitActor,
	const FName InHitBone,
	const EPhysicalSurface InImpactSurfaceType,
	const bool bInBlockingHit,
	const bool bInDamagedTarget,
	const int32 InShotIndex)
{
	checkf(
		!HasActorBegunPlay(),
		TEXT("InitializeGunshotPresentation must run before FinishSpawning."));
	if (!HasAuthority())
	{
		return;
	}

	TraceStart = InTraceStart;
	TraceEnd = InTraceEnd;
	FVector SafeNormal = InImpactNormal.GetSafeNormal();
	if (SafeNormal.IsNearlyZero())
	{
		SafeNormal = -((InTraceEnd - InTraceStart).GetSafeNormal());
	}
	if (SafeNormal.IsNearlyZero())
	{
		SafeNormal = FVector::UpVector;
	}
	ImpactNormal = SafeNormal;
	HitActor = bInBlockingHit ? InHitActor : nullptr;
	HitBone = bInBlockingHit ? InHitBone : NAME_None;
	ImpactSurfaceType = bInBlockingHit
		? InImpactSurfaceType
		: SurfaceType_Default;
	ShotIndex = static_cast<uint8>(FMath::Clamp(InShotIndex, 0, 255));
	PresentationFlags = ReadyFlag;
	if (bInBlockingHit)
	{
		PresentationFlags |= BlockingHitFlag;
	}
	if (bInBlockingHit && bInDamagedTarget)
	{
		PresentationFlags |= DamagedTargetFlag;
	}
}

bool ASovReformationDroneGunshotPresentation::HasBlockingHit() const
{
	return (PresentationFlags & BlockingHitFlag) != 0;
}

bool ASovReformationDroneGunshotPresentation::DamagedTarget() const
{
	return (PresentationFlags & DamagedTargetFlag) != 0;
}

void ASovReformationDroneGunshotPresentation::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ForceNetUpdate();
		SetLifeSpan(FMath::Max(PresentationLifetime, 0.1f));
	}
	PlayPresentation();
}

void ASovReformationDroneGunshotPresentation::OnRep_PresentationFlags()
{
	if (HasActorBegunPlay())
	{
		PlayPresentation();
	}
}

void ASovReformationDroneGunshotPresentation::PlayPresentation()
{
	if (bPresentationPlayed || (PresentationFlags & ReadyFlag) == 0)
	{
		return;
	}
	bPresentationPlayed = true;

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FVector Direction = (FVector(TraceEnd) - FVector(TraceStart)).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = FVector::ForwardVector;
	}

	SpawnMuzzlePresentation(Direction);
	SpawnTracerPresentation(Direction);
	if (HasBlockingHit())
	{
		SpawnImpactPresentation();
	}

	ReceiveGunshotPresented(
		FVector(TraceStart),
		FVector(TraceEnd),
		FVector(ImpactNormal),
		HitActor.Get(),
		HitBone,
		ImpactSurfaceType.GetValue(),
		HasBlockingHit(),
		DamagedTarget(),
		static_cast<int32>(ShotIndex));
}

void ASovReformationDroneGunshotPresentation::SpawnMuzzlePresentation(
	const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const FRotator MuzzleRotation =
		(Direction.Rotation() + MuzzleNiagaraRotationOffset).GetNormalized();
	if (IsValid(MuzzleNiagaraSystem))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			MuzzleNiagaraSystem,
			FVector(TraceStart),
			MuzzleRotation,
			MuzzleNiagaraScale,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}
	if (IsValid(FireSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			FireSound,
			FVector(TraceStart),
			FMath::Max(FireSoundVolumeMultiplier, 0.0f),
			FMath::Max(FireSoundPitchMultiplier, 0.01f));
	}
	if (FireCameraShakeClass.Get())
	{
		const float InnerRadius = FMath::Max(FireCameraShakeInnerRadius, 0.0f);
		const float OuterRadius = FMath::Max(FireCameraShakeOuterRadius, InnerRadius);
		UGameplayStatics::PlayWorldCameraShake(
			World,
			FireCameraShakeClass,
			FVector(TraceStart),
			InnerRadius,
			OuterRadius,
			FMath::Max(FireCameraShakeFalloff, 0.0f),
			false);
	}
}

void ASovReformationDroneGunshotPresentation::SpawnTracerPresentation(
	const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(TracerNiagaraSystem))
	{
		return;
	}

	UNiagaraComponent* Tracer = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		TracerNiagaraSystem,
		FVector(TraceStart),
		Direction.Rotation(),
		TracerNiagaraScale,
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);
	if (!IsValid(Tracer))
	{
		return;
	}
	if (!TracerEndParameter.IsNone())
	{
		Tracer->SetVariableVec3(TracerEndParameter, FVector(TraceEnd));
	}
	if (!TracerLengthParameter.IsNone())
	{
		Tracer->SetVariableFloat(
			TracerLengthParameter,
			FVector::Distance(FVector(TraceStart), FVector(TraceEnd)));
	}
}

void ASovReformationDroneGunshotPresentation::SpawnImpactPresentation()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const FRotator ImpactRotation =
		(FVector(ImpactNormal).Rotation() + ImpactNiagaraRotationOffset).GetNormalized();
	if (IsValid(ImpactNiagaraSystem))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ImpactNiagaraSystem,
			FVector(TraceEnd),
			ImpactRotation,
			ImpactNiagaraScale,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}
	if (IsValid(ImpactSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ImpactSound,
			FVector(TraceEnd),
			FMath::Max(ImpactSoundVolumeMultiplier, 0.0f),
			FMath::Max(ImpactSoundPitchMultiplier, 0.01f));
	}
	if (ImpactCameraShakeClass.Get())
	{
		const float InnerRadius = FMath::Max(ImpactCameraShakeInnerRadius, 0.0f);
		const float OuterRadius = FMath::Max(ImpactCameraShakeOuterRadius, InnerRadius);
		UGameplayStatics::PlayWorldCameraShake(
			World,
			ImpactCameraShakeClass,
			FVector(TraceEnd),
			InnerRadius,
			OuterRadius,
			FMath::Max(ImpactCameraShakeFalloff, 0.0f),
			false);
	}
	SpawnImpactDecal();
}

void ASovReformationDroneGunshotPresentation::SpawnImpactDecal()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(ImpactDecalMaterial))
	{
		return;
	}

	const FVector SafeNormal = FVector(ImpactNormal).GetSafeNormal();
	const FVector DecalLocation = FVector(TraceEnd)
		+ (SafeNormal * FMath::Max(ImpactDecalSurfaceOffset, 0.0f));
	const FVector DecalSize(
		FMath::Max(ImpactDecalSize.X, 1.0f),
		FMath::Max(ImpactDecalSize.Y, 1.0f),
		FMath::Max(ImpactDecalSize.Z, 1.0f));
	const float VisibleDuration = FMath::Max(ImpactDecalVisibleDuration, 0.0f);
	const float FadeDuration = FMath::Max(ImpactDecalFadeDuration, 0.0f);
	const float TotalLifetime = FMath::Max(VisibleDuration + FadeDuration, 0.1f);
	const FRotator DecalRotation = SafeNormal.Rotation();

	UDecalComponent* SpawnedDecal = nullptr;
	FName AttachBone = NAME_None;
	USceneComponent* AttachComponent = bAttachImpactDecal
		? ResolveDecalAttachment(AttachBone)
		: nullptr;
	if (IsValid(AttachComponent))
	{
		SpawnedDecal = UGameplayStatics::SpawnDecalAttached(
			ImpactDecalMaterial,
			DecalSize,
			AttachComponent,
			AttachBone,
			DecalLocation,
			DecalRotation,
			EAttachLocation::KeepWorldPosition,
			TotalLifetime);
	}
	else
	{
		SpawnedDecal = UGameplayStatics::SpawnDecalAtLocation(
			World,
			ImpactDecalMaterial,
			DecalSize,
			DecalLocation,
			DecalRotation,
			TotalLifetime);
	}

	if (IsValid(SpawnedDecal) && FadeDuration > KINDA_SMALL_NUMBER)
	{
		SpawnedDecal->SetFadeOut(VisibleDuration, FadeDuration, false);
	}
}

USceneComponent*
ASovReformationDroneGunshotPresentation::ResolveDecalAttachment(
	FName& OutAttachBone) const
{
	OutAttachBone = NAME_None;
	AActor* ResolvedHitActor = HitActor.Get();
	if (!IsValid(ResolvedHitActor))
	{
		return nullptr;
	}

	if (!HitBone.IsNone())
	{
		TArray<USkeletalMeshComponent*> SkeletalMeshes;
		ResolvedHitActor->GetComponents<USkeletalMeshComponent>(SkeletalMeshes);
		for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshes)
		{
			if (IsValid(SkeletalMesh)
				&& SkeletalMesh->GetBoneIndex(HitBone) != INDEX_NONE)
			{
				OutAttachBone = HitBone;
				return SkeletalMesh;
			}
		}
	}
	return ResolvedHitActor->GetRootComponent();
}

void ASovReformationDroneGunshotPresentation::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASovReformationDroneGunshotPresentation, TraceStart);
	DOREPLIFETIME(ASovReformationDroneGunshotPresentation, TraceEnd);
	DOREPLIFETIME(ASovReformationDroneGunshotPresentation, ImpactNormal);
	DOREPLIFETIME(ASovReformationDroneGunshotPresentation, HitActor);
	DOREPLIFETIME(ASovReformationDroneGunshotPresentation, HitBone);
	DOREPLIFETIME(ASovReformationDroneGunshotPresentation, ImpactSurfaceType);
	DOREPLIFETIME(ASovReformationDroneGunshotPresentation, ShotIndex);
	DOREPLIFETIME(ASovReformationDroneGunshotPresentation, PresentationFlags);
}
