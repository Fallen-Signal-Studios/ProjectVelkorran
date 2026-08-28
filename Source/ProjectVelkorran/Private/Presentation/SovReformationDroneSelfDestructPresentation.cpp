// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Presentation/SovReformationDroneSelfDestructPresentation.h"

#include "Camera/CameraShakeBase.h"
#include "Character/NarrativeCharacterVisual.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "Sound/SoundBase.h"
#include "UnrealFramework/NarrativeCharacter.h"

ASovReformationDroneSelfDestructPresentation::
	ASovReformationDroneSelfDestructPresentation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetNetUpdateFrequency(20.0f);
	SetMinNetUpdateFrequency(5.0f);

	PresentationRoot =
		CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	SetRootComponent(PresentationRoot);
	PresentationRoot->SetMobility(EComponentMobility::Movable);

	TravelNiagaraComponent =
		CreateDefaultSubobject<UNiagaraComponent>(TEXT("TravelNiagara"));
	TravelNiagaraComponent->SetupAttachment(PresentationRoot);
	TravelNiagaraComponent->SetAutoActivate(false);
	TravelNiagaraComponent->SetCanEverAffectNavigation(false);

	WarningNiagaraComponent =
		CreateDefaultSubobject<UNiagaraComponent>(TEXT("WarningNiagara"));
	WarningNiagaraComponent->SetupAttachment(PresentationRoot);
	WarningNiagaraComponent->SetAutoActivate(false);
	WarningNiagaraComponent->SetCanEverAffectNavigation(false);

	TravelAudioComponent =
		CreateDefaultSubobject<UAudioComponent>(TEXT("TravelAudio"));
	TravelAudioComponent->SetupAttachment(PresentationRoot);
	TravelAudioComponent->SetAutoActivate(false);

	WarningAudioComponent =
		CreateDefaultSubobject<UAudioComponent>(TEXT("WarningAudio"));
	WarningAudioComponent->SetupAttachment(PresentationRoot);
	WarningAudioComponent->SetAutoActivate(false);

	ExplosionRadialForce =
		CreateDefaultSubobject<URadialForceComponent>(TEXT("ExplosionRadialForce"));
	ExplosionRadialForce->SetupAttachment(PresentationRoot);
	ExplosionRadialForce->Radius = 425.0f;
	ExplosionRadialForce->Falloff = RIF_Linear;
	ExplosionRadialForce->ForceStrength = 0.0f;
	ExplosionRadialForce->ImpulseStrength = 2200.0f;
	ExplosionRadialForce->bImpulseVelChange = true;
	ExplosionRadialForce->bIgnoreOwningActor = true;
	ExplosionRadialForce->bAutoActivate = false;
}

void ASovReformationDroneSelfDestructPresentation::InitializeForDrone(
	AActor* InSourceDrone,
	const float InWarningDuration)
{
	if (!HasAuthority()
		|| !IsValid(InSourceDrone)
		|| PresentationState.Phase
			!= ESovReformationDroneSelfDestructPhase::Inactive)
	{
		return;
	}

	PresentationState.SourceDrone = InSourceDrone;
	PresentationState.WarningDuration = FMath::Max(InWarningDuration, 0.0f);
	PresentationState.ServerPhaseStartTime = GetSynchronizedServerTime();
	PresentationState.Phase = ESovReformationDroneSelfDestructPhase::Pursuit;
	SetActorTransform(
		InSourceDrone->GetActorTransform(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void ASovReformationDroneSelfDestructPresentation::EnterWarning(
	const float InWarningDuration)
{
	if (!HasAuthority()
		|| PresentationState.Phase
			!= ESovReformationDroneSelfDestructPhase::Pursuit)
	{
		return;
	}

	PresentationState.WarningDuration = FMath::Max(InWarningDuration, 0.0f);
	PresentationState.ServerPhaseStartTime = GetSynchronizedServerTime();
	PresentationState.Phase = ESovReformationDroneSelfDestructPhase::Warning;
	ApplyPresentationState();
	FlushNetDormancy();
	ForceNetUpdate();
}

void ASovReformationDroneSelfDestructPresentation::PrepareDetonation(
	const FVector& InDetonationLocation,
	const float InExplosionRadius)
{
	if (!HasAuthority()
		|| PresentationState.Phase
			== ESovReformationDroneSelfDestructPhase::Detonated
		|| PresentationState.Phase
			== ESovReformationDroneSelfDestructPhase::Cancelled)
	{
		return;
	}

	FVector SafeLocation = InDetonationLocation;
	if (SafeLocation.ContainsNaN())
	{
		SafeLocation = GetActorLocation();
	}
	PresentationState.DetonationLocation = SafeLocation;
	PresentationState.ExplosionRadius = FMath::Max(InExplosionRadius, 0.0f);
	PresentationState.bDamagedAnyTarget = false;
	PresentationState.bDetonationFinalized = false;
	PresentationState.ServerPhaseStartTime = GetSynchronizedServerTime();
	PresentationState.Phase = ESovReformationDroneSelfDestructPhase::Detonated;
	SetActorLocation(
		SafeLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	FlushNetDormancy();
	ForceNetUpdate();
}

void ASovReformationDroneSelfDestructPresentation::FinalizeDetonation(
	const bool bInDamagedAnyTarget)
{
	if (!HasAuthority()
		|| PresentationState.Phase
			!= ESovReformationDroneSelfDestructPhase::Detonated
		|| PresentationState.bDetonationFinalized)
	{
		return;
	}
	PresentationState.bDamagedAnyTarget = bInDamagedAnyTarget;
	PresentationState.bDetonationFinalized = true;

	const FVector DetonationLocation =
		FVector(PresentationState.DetonationLocation);
	if (bApplyExplosionPhysicsImpulse
		&& IsValid(ExplosionRadialForce)
		&& PresentationState.ExplosionRadius > KINDA_SMALL_NUMBER
		&& ExplosionRadialForce->ImpulseStrength > KINDA_SMALL_NUMBER)
	{
		ExplosionRadialForce->SetWorldLocation(
			DetonationLocation
			- (FVector::UpVector
				* FMath::Max(ExplosionPhysicsUpwardBias, 0.0f)));
		ExplosionRadialForce->Radius = PresentationState.ExplosionRadius
			* FMath::Max(ExplosionPhysicsRadiusScale, 0.0f);
		ExplosionRadialForce->FireImpulse();
	}

	ApplyPresentationState();
	FlushNetDormancy();
	ForceNetUpdate();
	SetLifeSpan(FMath::Max(ExplosionCleanupDelay, 0.1f));
}

void ASovReformationDroneSelfDestructPresentation::CancelPresentation()
{
	if (!HasAuthority()
		|| PresentationState.Phase
			== ESovReformationDroneSelfDestructPhase::Detonated
		|| PresentationState.Phase
			== ESovReformationDroneSelfDestructPhase::Cancelled)
	{
		return;
	}

	PresentationState.ServerPhaseStartTime = GetSynchronizedServerTime();
	PresentationState.Phase = ESovReformationDroneSelfDestructPhase::Cancelled;
	ApplyPresentationState();
	FlushNetDormancy();
	ForceNetUpdate();
	SetLifeSpan(0.5f);
}

void ASovReformationDroneSelfDestructPresentation::BeginPlay()
{
	Super::BeginPlay();

	if (const UWorld* World = GetWorld();
		!IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
	{
		SetActorTickEnabled(false);
		return;
	}

	ApplyPresentationState();
}

void ASovReformationDroneSelfDestructPresentation::Tick(
	const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const ESovReformationDroneSelfDestructPhase Phase = PresentationState.Phase;
	if (Phase != ESovReformationDroneSelfDestructPhase::Pursuit
		&& Phase != ESovReformationDroneSelfDestructPhase::Warning)
	{
		return;
	}

	FollowSourceDrone();
	MaterialRescanAccumulator += FMath::Max(DeltaSeconds, 0.0f);
	if (ChargeMaterials.IsEmpty()
		|| MaterialRescanAccumulator
			>= FMath::Max(MaterialRescanInterval, 0.02f))
	{
		RefreshChargeMaterials();
		MaterialRescanAccumulator = 0.0f;
	}
	SetChargeMaterialValue(CalculateChargeValue());
}

void ASovReformationDroneSelfDestructPresentation::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	StopPersistentPresentation();
	SetChargeMaterialValue(InactiveChargeValue);
	ChargeMaterials.Reset();
	Super::EndPlay(EndPlayReason);
}

void ASovReformationDroneSelfDestructPresentation::OnRep_PresentationState()
{
	ApplyPresentationState();
}

void ASovReformationDroneSelfDestructPresentation::ApplyPresentationState()
{
	const UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	switch (PresentationState.Phase)
	{
	case ESovReformationDroneSelfDestructPhase::Pursuit:
		FollowSourceDrone();
		RefreshChargeMaterials();
		PlayPursuitPresentation();
		SetActorTickEnabled(true);
		break;

	case ESovReformationDroneSelfDestructPhase::Warning:
		FollowSourceDrone();
		RefreshChargeMaterials();
		PlayWarningPresentation();
		SetActorTickEnabled(true);
		break;

	case ESovReformationDroneSelfDestructPhase::Detonated:
		SetActorLocation(
			FVector(PresentationState.DetonationLocation),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		StopPersistentPresentation();
		if (PresentationState.bDetonationFinalized)
		{
			RefreshChargeMaterials();
			SetChargeMaterialValue(WarningChargeEndValue);
			PlayDetonationPresentation();
			SetChargeMaterialValue(InactiveChargeValue);
		}
		SetActorTickEnabled(false);
		break;

	case ESovReformationDroneSelfDestructPhase::Cancelled:
		RefreshChargeMaterials();
		PlayCancellationPresentation();
		SetActorTickEnabled(false);
		break;

	default:
		break;
	}
}

void ASovReformationDroneSelfDestructPresentation::PlayPursuitPresentation()
{
	if (bPlayedPursuitPresentation)
	{
		return;
	}
	bPlayedPursuitPresentation = true;

	if (IsValid(TravelNiagaraComponent) && IsValid(TravelNiagaraSystem))
	{
		TravelNiagaraComponent->SetAsset(TravelNiagaraSystem);
		TravelNiagaraComponent->SetRelativeTransform(
			TravelNiagaraRelativeTransform);
		TravelNiagaraComponent->Activate(true);
	}
	if (IsValid(TravelAudioComponent) && IsValid(TravelLoopSound))
	{
		TravelAudioComponent->SetSound(TravelLoopSound);
		TravelAudioComponent->SetRelativeTransform(TravelAudioRelativeTransform);
		TravelAudioComponent->SetVolumeMultiplier(
			FMath::Max(TravelLoopVolumeMultiplier, 0.0f));
		TravelAudioComponent->SetPitchMultiplier(
			FMath::Max(TravelLoopPitchMultiplier, 0.01f));
		TravelAudioComponent->Play();
	}
	ReceivePursuitStarted();
}

void ASovReformationDroneSelfDestructPresentation::PlayWarningPresentation()
{
	if (bPlayedWarningPresentation)
	{
		return;
	}
	bPlayedWarningPresentation = true;
	// The drone has stopped moving. End travel presentation before starting the
	// distinct fuse/indication layer so the audio sequence remains readable.
	if (IsValid(TravelNiagaraComponent))
	{
		TravelNiagaraComponent->Deactivate();
	}
	if (IsValid(TravelAudioComponent))
	{
		TravelAudioComponent->Stop();
	}

	const float PhaseAge = GetCurrentPhaseAge();
	const float RemainingWarningTime = FMath::Max(
		PresentationState.WarningDuration - PhaseAge,
		0.0f);
	if (IsValid(WarningNiagaraComponent) && IsValid(WarningNiagaraSystem))
	{
		WarningNiagaraComponent->SetAsset(WarningNiagaraSystem);
		WarningNiagaraComponent->SetRelativeTransform(
			WarningNiagaraRelativeTransform);
		WarningNiagaraComponent->Activate(true);
	}
	if (IsValid(WarningAudioComponent)
		&& IsValid(DetonationIndicationSound)
		&& PhaseAge <= FMath::Max(WarningOneShotMaximumAge, 0.0f)
		&& RemainingWarningTime > KINDA_SMALL_NUMBER)
	{
		WarningAudioComponent->SetSound(DetonationIndicationSound);
		WarningAudioComponent->SetVolumeMultiplier(
			FMath::Max(DetonationIndicationVolumeMultiplier, 0.0f));
		WarningAudioComponent->SetPitchMultiplier(
			FMath::Max(DetonationIndicationPitchMultiplier, 0.01f));
		WarningAudioComponent->Play(FMath::Max(PhaseAge, 0.0f));
	}
	ReceiveWarningStarted(RemainingWarningTime);
}

void ASovReformationDroneSelfDestructPresentation::
	PlayDetonationPresentation()
{
	if (bPlayedDetonationPresentation)
	{
		return;
	}
	bPlayedDetonationPresentation = true;
	StopPersistentPresentation();

	const float PhaseAge = GetCurrentPhaseAge();
	if (PhaseAge > FMath::Max(ExplosionOneShotMaximumAge, 0.0f))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	const FVector DetonationLocation = FVector(PresentationState.DetonationLocation);
	const FTransform ExplosionFrame(
		FRotator::ZeroRotator,
		DetonationLocation,
		FVector::OneVector);
	if (IsValid(ExplosionNiagaraSystem))
	{
		const FTransform SpawnTransform =
			ExplosionNiagaraRelativeTransform * ExplosionFrame;
		if (UNiagaraComponent* SpawnedExplosion =
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				ExplosionNiagaraSystem,
				SpawnTransform.GetLocation(),
				SpawnTransform.Rotator(),
				SpawnTransform.GetScale3D(),
				true,
				false,
				ENCPoolMethod::AutoRelease,
				true))
		{
			if (!ExplosionRadiusNiagaraParameter.IsNone())
			{
				SpawnedExplosion->SetVariableFloat(
					ExplosionRadiusNiagaraParameter,
					PresentationState.ExplosionRadius);
			}
			SpawnedExplosion->Activate(true);
		}
	}
	if (IsValid(ExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ExplosionSound,
			DetonationLocation,
			FRotator::ZeroRotator,
			FMath::Max(ExplosionVolumeMultiplier, 0.0f),
			FMath::Max(ExplosionPitchMultiplier, 0.01f));
	}
	if (ExplosionCameraShakeClass.Get())
	{
		const float InnerRadius =
			FMath::Max(ExplosionCameraShakeInnerRadius, 0.0f);
		const float OuterRadius = FMath::Max(
			ExplosionCameraShakeOuterRadius,
			InnerRadius);
		UGameplayStatics::PlayWorldCameraShake(
			this,
			ExplosionCameraShakeClass,
			DetonationLocation,
			InnerRadius,
			OuterRadius,
			FMath::Max(ExplosionCameraShakeFalloff, 0.0f),
			true);
	}
	SpawnExplosionDecal();
	ReceiveDetonated(DetonationLocation);
}

void ASovReformationDroneSelfDestructPresentation::
	PlayCancellationPresentation()
{
	if (bPlayedCancellationPresentation)
	{
		return;
	}
	bPlayedCancellationPresentation = true;
	StopPersistentPresentation();
	SetChargeMaterialValue(InactiveChargeValue);
	ReceiveCancelled();
}

void ASovReformationDroneSelfDestructPresentation::
	StopPersistentPresentation()
{
	if (IsValid(TravelNiagaraComponent))
	{
		TravelNiagaraComponent->Deactivate();
	}
	if (IsValid(WarningNiagaraComponent))
	{
		WarningNiagaraComponent->Deactivate();
	}
	if (IsValid(TravelAudioComponent))
	{
		TravelAudioComponent->Stop();
	}
	if (IsValid(WarningAudioComponent))
	{
		WarningAudioComponent->Stop();
	}
}

void ASovReformationDroneSelfDestructPresentation::FollowSourceDrone()
{
	AActor* SourceDrone = PresentationState.SourceDrone.Get();
	if (!IsValid(SourceDrone))
	{
		return;
	}
	SetActorTransform(
		SourceDrone->GetActorTransform(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void ASovReformationDroneSelfDestructPresentation::RefreshChargeMaterials()
{
	ChargeMaterials.Reset();
	AActor* SourceDrone = PresentationState.SourceDrone.Get();
	if (!IsValid(SourceDrone) || ChargeMaterialParameter.IsNone())
	{
		return;
	}

	AddChargeMaterialsFromActor(SourceDrone);
	if (const ANarrativeCharacter* NarrativeCharacter =
		Cast<ANarrativeCharacter>(SourceDrone))
	{
		ANarrativeCharacterVisual* CharacterVisual =
			NarrativeCharacter->GetCharacterVisual();
		if (IsValid(CharacterVisual) && CharacterVisual != SourceDrone)
		{
			AddChargeMaterialsFromActor(CharacterVisual);
		}
	}
}

void ASovReformationDroneSelfDestructPresentation::
	AddChargeMaterialsFromActor(AActor* MeshOwner)
{
	if (!IsValid(MeshOwner))
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	MeshOwner->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!IsValid(MeshComponent) || !MeshComponent->IsRegistered())
		{
			continue;
		}

		for (int32 MaterialIndex = 0;
			MaterialIndex < MeshComponent->GetNumMaterials();
			++MaterialIndex)
		{
			UMaterialInterface* Material =
				MeshComponent->GetMaterial(MaterialIndex);
			if (!IsValid(Material))
			{
				continue;
			}

			UMaterialInstanceDynamic* DynamicMaterial =
				Cast<UMaterialInstanceDynamic>(Material);
			if (!IsValid(DynamicMaterial))
			{
				DynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(
					MaterialIndex,
					Material);
			}
			if (IsValid(DynamicMaterial))
			{
				ChargeMaterials.AddUnique(DynamicMaterial);
			}
		}
	}
}

void ASovReformationDroneSelfDestructPresentation::SetChargeMaterialValue(
	const float Value)
{
	if (ChargeMaterialParameter.IsNone())
	{
		return;
	}
	const float SafeValue = FMath::Clamp(Value, 0.0f, 1.0f);
	for (UMaterialInstanceDynamic* DynamicMaterial : ChargeMaterials)
	{
		if (IsValid(DynamicMaterial))
		{
			DynamicMaterial->SetScalarParameterValue(
				ChargeMaterialParameter,
				SafeValue);
		}
	}
}

void ASovReformationDroneSelfDestructPresentation::SpawnExplosionDecal()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| !IsValid(ExplosionDecalMaterial)
		|| ExplosionDecalSearchDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovReformationDroneSelfDestructDecal),
		false,
		this);
	QueryParams.AddIgnoredActor(this);
	if (IsValid(PresentationState.SourceDrone.Get()))
	{
		QueryParams.AddIgnoredActor(PresentationState.SourceDrone.Get());
	}
	if (const ANarrativeCharacter* NarrativeCharacter =
		Cast<ANarrativeCharacter>(PresentationState.SourceDrone.Get()))
	{
		if (ANarrativeCharacterVisual* CharacterVisual =
			NarrativeCharacter->GetCharacterVisual())
		{
			QueryParams.AddIgnoredActor(CharacterVisual);
		}
	}

	TArray<FVector, TInlineAllocator<9>> SearchDirections;
	SearchDirections.Add(FVector::DownVector);
	for (int32 DirectionIndex = 0; DirectionIndex < 8; ++DirectionIndex)
	{
		const float Angle =
			(PI * 2.0f * static_cast<float>(DirectionIndex)) / 8.0f;
		SearchDirections.Add(
			FVector(FMath::Cos(Angle), FMath::Sin(Angle), -0.25f).GetSafeNormal());
	}

	const FVector TraceStart = FVector(PresentationState.DetonationLocation);
	FHitResult NearestHit;
	float NearestDistance = TNumericLimits<float>::Max();
	for (const FVector& Direction : SearchDirections)
	{
		FHitResult CandidateHit;
		if (World->LineTraceSingleByObjectType(
			CandidateHit,
			TraceStart,
			TraceStart + (Direction * ExplosionDecalSearchDistance),
			ObjectQuery,
			QueryParams)
			&& CandidateHit.bBlockingHit
			&& CandidateHit.Distance < NearestDistance)
		{
			NearestDistance = CandidateHit.Distance;
			NearestHit = CandidateHit;
		}
	}
	if (!NearestHit.bBlockingHit)
	{
		return;
	}

	const FVector SurfaceNormal =
		FVector(NearestHit.ImpactNormal).GetSafeNormal();
	const FVector DecalLocation = FVector(NearestHit.ImpactPoint)
		+ (SurfaceNormal * 1.0f);
	const FVector DecalSize(
		FMath::Max(ExplosionDecalSize.X, 1.0f),
		FMath::Max(ExplosionDecalSize.Y, 1.0f),
		FMath::Max(ExplosionDecalSize.Z, 1.0f));
	const float VisibleDuration =
		FMath::Max(ExplosionDecalVisibleDuration, 0.0f);
	const float FadeDuration =
		FMath::Max(ExplosionDecalFadeDuration, 0.0f);
	const float Lifetime = FMath::Max(VisibleDuration + FadeDuration, 0.1f);

	UDecalComponent* SpawnedDecal = nullptr;
	if (USceneComponent* SurfaceComponent = NearestHit.GetComponent())
	{
		SpawnedDecal = UGameplayStatics::SpawnDecalAttached(
			ExplosionDecalMaterial,
			DecalSize,
			SurfaceComponent,
			NearestHit.BoneName,
			DecalLocation,
			SurfaceNormal.Rotation(),
			EAttachLocation::KeepWorldPosition,
			Lifetime);
	}
	else
	{
		SpawnedDecal = UGameplayStatics::SpawnDecalAtLocation(
			World,
			ExplosionDecalMaterial,
			DecalSize,
			DecalLocation,
			SurfaceNormal.Rotation(),
			Lifetime);
	}
	if (IsValid(SpawnedDecal) && FadeDuration > KINDA_SMALL_NUMBER)
	{
		SpawnedDecal->SetFadeOut(VisibleDuration, FadeDuration, false);
	}
}

float ASovReformationDroneSelfDestructPresentation::
	GetSynchronizedServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World)
		? World->GetGameState()
		: nullptr;
	return IsValid(GameState)
		? GameState->GetServerWorldTimeSeconds()
		: (IsValid(World) ? World->GetTimeSeconds() : 0.0f);
}

float ASovReformationDroneSelfDestructPresentation::GetCurrentPhaseAge() const
{
	return FMath::Max(
		GetSynchronizedServerTime() - PresentationState.ServerPhaseStartTime,
		0.0f);
}

float ASovReformationDroneSelfDestructPresentation::CalculateChargeValue() const
{
	const float PhaseAge = GetCurrentPhaseAge();
	if (PresentationState.Phase
		== ESovReformationDroneSelfDestructPhase::Pursuit)
	{
		const float Pulse = FMath::Sin(
			PhaseAge * FMath::Max(PursuitPulseFrequency, 0.0f) * PI * 2.0f)
			* FMath::Max(PursuitPulseAmplitude, 0.0f);
		return FMath::Clamp(PursuitChargeValue + Pulse, 0.0f, 1.0f);
	}
	if (PresentationState.Phase
		== ESovReformationDroneSelfDestructPhase::Warning)
	{
		const float WarningAlpha = PresentationState.WarningDuration
			> KINDA_SMALL_NUMBER
			? FMath::Clamp(
				PhaseAge / PresentationState.WarningDuration,
				0.0f,
				1.0f)
			: 1.0f;
		const float BaseValue = FMath::Lerp(
			WarningChargeStartValue,
			WarningChargeEndValue,
			WarningAlpha);
		const float Pulse = FMath::Sin(
			PhaseAge * FMath::Max(WarningPulseFrequency, 0.0f) * PI * 2.0f)
			* FMath::Max(WarningPulseAmplitude, 0.0f)
			* WarningAlpha;
		return FMath::Clamp(BaseValue + Pulse, 0.0f, 1.0f);
	}
	return InactiveChargeValue;
}

void ASovReformationDroneSelfDestructPresentation::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(
		ASovReformationDroneSelfDestructPresentation,
		PresentationState);
}
