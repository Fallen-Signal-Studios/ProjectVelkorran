// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Presentation/SovCinderJudgementPresentation.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraShakeBase.h"
#include "CollisionQueryParams.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UnrealFramework/NarrativeCharacter.h"

namespace
{
	bool RepresentsAbilitySystemActor(AActor* InActor)
	{
		AActor* Candidate = InActor;
		TSet<const AActor*> VisitedActors;
		for (int32 Depth = 0;
			IsValid(Candidate) && Depth < 6 && !VisitedActors.Contains(Candidate);
			++Depth)
		{
			VisitedActors.Add(Candidate);
			if (UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate))
			{
				return true;
			}
			if (const INarrativeCharacterOwner* CharacterProvider =
				Cast<INarrativeCharacterOwner>(Candidate))
			{
				if (IsValid(CharacterProvider->GetNarrativeCharacter()))
				{
					return true;
				}
			}
			Candidate = Candidate->GetOwner();
		}
		return false;
	}
}

ASovCinderJudgementPresentation::ASovCinderJudgementPresentation()
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

void ASovCinderJudgementPresentation::InitializeJudgementPresentation(
	const FVector& InTraceStart,
	const FVector& InTraceEnd,
	const FVector& InImpactNormal,
	AActor* InHitActor,
	const FName InHitBone,
	const EPhysicalSurface InImpactSurfaceType,
	const float InExplosionRadius,
	const bool bInBlockingHit,
	const bool bInBlastTriggered,
	const bool bInDirectDamageResolved,
	const int32 InRadialTargetsResolved)
{
	checkf(
		!HasActorBegunPlay(),
		TEXT("InitializeJudgementPresentation must run before FinishSpawning."));
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
	ExplosionRadius = FMath::Max(InExplosionRadius, 0.0f);
	if (const UWorld* World = GetWorld())
	{
		ServerFireTime = World->GetTimeSeconds();
	}
	RadialTargetsResolved = static_cast<uint8>(
		FMath::Clamp(InRadialTargetsResolved, 0, 255));

	PresentationFlags = ReadyFlag;
	if (bInBlockingHit)
	{
		PresentationFlags |= BlockingHitFlag;
	}
	if (bInBlastTriggered)
	{
		PresentationFlags |= BlastTriggeredFlag;
	}
	if (bInDirectDamageResolved)
	{
		PresentationFlags |= DirectDamageResolvedFlag;
	}
	if (InRadialTargetsResolved > 0)
	{
		PresentationFlags |= RadialDamageResolvedFlag;
	}
}

bool ASovCinderJudgementPresentation::HasBlockingHit() const
{
	return (PresentationFlags & BlockingHitFlag) != 0;
}

bool ASovCinderJudgementPresentation::BlastTriggered() const
{
	return (PresentationFlags & BlastTriggeredFlag) != 0;
}

bool ASovCinderJudgementPresentation::DirectDamageResolved() const
{
	return (PresentationFlags & DirectDamageResolvedFlag) != 0;
}

void ASovCinderJudgementPresentation::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ForceNetUpdate();
		SetLifeSpan(FMath::Max(PresentationLifetime, 0.1f));
	}
	PlayPresentation();
}

void ASovCinderJudgementPresentation::OnRep_PresentationFlags()
{
	if (HasActorBegunPlay())
	{
		PlayPresentation();
	}
}

bool ASovCinderJudgementPresentation::IsFreshEnoughToPresent() const
{
	if (MaximumReplayAge <= KINDA_SMALL_NUMBER || ServerFireTime <= 0.0f)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}
	const AGameStateBase* GameState = World->GetGameState();
	const float SynchronizedNow = IsValid(GameState)
		? GameState->GetServerWorldTimeSeconds()
		: World->GetTimeSeconds();
	return SynchronizedNow - ServerFireTime <= MaximumReplayAge;
}

void ASovCinderJudgementPresentation::PlayPresentation()
{
	if (bPresentationPlayed || (PresentationFlags & ReadyFlag) == 0)
	{
		return;
	}
	bPresentationPlayed = true;

	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| World->GetNetMode() == NM_DedicatedServer
		|| !IsFreshEnoughToPresent())
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
	SpawnBeamPresentation(Direction);
	if (HasBlockingHit())
	{
		SpawnImpactPresentation();
	}
	if (BlastTriggered())
	{
		SpawnExplosionPresentation();
		SpawnScorchDecal();
	}
	else if (!HasBlockingHit())
	{
		SpawnDissipationPresentation(Direction);
	}

	ReceiveCinderJudgementPresented(
		FVector(TraceStart),
		FVector(TraceEnd),
		FVector(ImpactNormal),
		HitActor.Get(),
		HitBone,
		ImpactSurfaceType.GetValue(),
		ExplosionRadius,
		HasBlockingHit(),
		BlastTriggered(),
		DirectDamageResolved(),
		static_cast<int32>(RadialTargetsResolved));
}

void ASovCinderJudgementPresentation::SpawnMuzzlePresentation(
	const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const FRotator Rotation =
		(Direction.Rotation() + MuzzleNiagaraRotationOffset).GetNormalized();
	if (IsValid(MuzzleNiagaraSystem))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			MuzzleNiagaraSystem,
			FVector(TraceStart),
			Rotation,
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
			FRotator::ZeroRotator,
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

void ASovCinderJudgementPresentation::SpawnBeamPresentation(
	const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(BeamNiagaraSystem))
	{
		return;
	}

	UNiagaraComponent* Beam = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		BeamNiagaraSystem,
		FVector(TraceStart),
		Direction.Rotation(),
		BeamNiagaraScale,
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);
	if (!IsValid(Beam))
	{
		return;
	}
	if (!BeamEndParameter.IsNone())
	{
		Beam->SetVariableVec3(BeamEndParameter, FVector(TraceEnd));
	}
	if (!BeamLengthParameter.IsNone())
	{
		Beam->SetVariableFloat(
			BeamLengthParameter,
			FVector::Distance(FVector(TraceStart), FVector(TraceEnd)));
	}
}

void ASovCinderJudgementPresentation::SpawnImpactPresentation()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const FRotator Rotation =
		(FVector(ImpactNormal).Rotation() + ImpactNiagaraRotationOffset).GetNormalized();
	if (IsValid(ImpactNiagaraSystem))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ImpactNiagaraSystem,
			FVector(TraceEnd),
			Rotation,
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
			FRotator::ZeroRotator,
			FMath::Max(ImpactSoundVolumeMultiplier, 0.0f),
			FMath::Max(ImpactSoundPitchMultiplier, 0.01f));
	}
}

void ASovCinderJudgementPresentation::SpawnExplosionPresentation()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (IsValid(ExplosionNiagaraSystem))
	{
		UNiagaraComponent* Explosion =
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				ExplosionNiagaraSystem,
				FVector(TraceEnd),
				(FVector(ImpactNormal).Rotation()
					+ ExplosionNiagaraRotationOffset).GetNormalized(),
				ExplosionNiagaraScale,
				true,
				true,
				ENCPoolMethod::AutoRelease,
				true);
		if (IsValid(Explosion) && !ExplosionRadiusParameter.IsNone())
		{
			Explosion->SetVariableFloat(ExplosionRadiusParameter, ExplosionRadius);
		}
	}
	if (IsValid(ExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ExplosionSound,
			FVector(TraceEnd),
			FRotator::ZeroRotator,
			FMath::Max(ExplosionSoundVolumeMultiplier, 0.0f),
			FMath::Max(ExplosionSoundPitchMultiplier, 0.01f));
	}
	if (ExplosionCameraShakeClass.Get())
	{
		const float InnerRadius = FMath::Max(ExplosionCameraShakeInnerRadius, 0.0f);
		const float OuterRadius = FMath::Max(ExplosionCameraShakeOuterRadius, InnerRadius);
		UGameplayStatics::PlayWorldCameraShake(
			World,
			ExplosionCameraShakeClass,
			FVector(TraceEnd),
			InnerRadius,
			OuterRadius,
			FMath::Max(ExplosionCameraShakeFalloff, 0.0f),
			false);
	}
}

void ASovCinderJudgementPresentation::SpawnDissipationPresentation(
	const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	if (IsValid(DissipationNiagaraSystem))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			DissipationNiagaraSystem,
			FVector(TraceEnd),
			(-Direction).Rotation(),
			DissipationNiagaraScale,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}
	if (IsValid(DissipationSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			DissipationSound,
			FVector(TraceEnd),
			FRotator::ZeroRotator,
			FMath::Max(DissipationSoundVolumeMultiplier, 0.0f),
			FMath::Max(DissipationSoundPitchMultiplier, 0.01f));
	}
}

bool ASovCinderJudgementPresentation::FindScorchSurface(
	FHitResult& OutSurfaceHit) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || ScorchSurfaceSearchDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovCinderJudgementScorchSurface),
		false,
		this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(GetInstigator());
	if (RepresentsAbilitySystemActor(HitActor.Get()))
	{
		// Character visuals should not receive a world scorch or occlude the
		// nearby floor/wall search. World geometry must remain traceable.
		QueryParams.AddIgnoredActor(HitActor.Get());
	}

	TArray<FVector, TInlineAllocator<12>> SearchDirections;
	if (HasBlockingHit() && !FVector(ImpactNormal).IsNearlyZero())
	{
		SearchDirections.Add(-FVector(ImpactNormal).GetSafeNormal());
	}
	SearchDirections.Add(FVector::DownVector);
	SearchDirections.Add(FVector::UpVector);
	for (int32 DirectionIndex = 0; DirectionIndex < 8; ++DirectionIndex)
	{
		const float Angle =
			(PI * 2.0f * static_cast<float>(DirectionIndex)) / 8.0f;
		SearchDirections.Add(
			FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f));
	}

	bool bFoundSurface = false;
	float NearestDistance = TNumericLimits<float>::Max();
	const FVector SearchStart = FVector(TraceEnd)
		+ (FVector(ImpactNormal).GetSafeNormal() * 2.0f);
	for (const FVector& Direction : SearchDirections)
	{
		FHitResult CandidateHit;
		const FVector SearchEnd = SearchStart
			+ (Direction.GetSafeNormal() * ScorchSurfaceSearchDistance);
		if (!World->LineTraceSingleByObjectType(
				CandidateHit,
				SearchStart,
				SearchEnd,
				ObjectQuery,
				QueryParams)
			|| !CandidateHit.bBlockingHit
			|| CandidateHit.ImpactNormal.IsNearlyZero()
			|| CandidateHit.Distance >= NearestDistance)
		{
			continue;
		}

		NearestDistance = CandidateHit.Distance;
		OutSurfaceHit = CandidateHit;
		bFoundSurface = true;
	}
	return bFoundSurface;
}

void ASovCinderJudgementPresentation::SpawnScorchDecal()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(ScorchDecalMaterial))
	{
		return;
	}

	FHitResult SurfaceHit;
	if (!FindScorchSurface(SurfaceHit))
	{
		return;
	}

	const FVector SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
	const FVector DecalLocation = SurfaceHit.ImpactPoint
		+ (SurfaceNormal * FMath::Max(ScorchSurfaceOffset, 0.0f));
	const FVector DecalSize(
		FMath::Max(ScorchDecalSize.X, 1.0f),
		FMath::Max(ScorchDecalSize.Y, 1.0f),
		FMath::Max(ScorchDecalSize.Z, 1.0f));
	const float VisibleDuration = FMath::Max(ScorchVisibleDuration, 0.0f);
	const float FadeDuration = FMath::Max(ScorchFadeDuration, 0.0f);
	const float TotalLifetime = FMath::Max(VisibleDuration + FadeDuration, 0.1f);

	UDecalComponent* Decal = nullptr;
	if (USceneComponent* SurfaceComponent = SurfaceHit.GetComponent())
	{
		Decal = UGameplayStatics::SpawnDecalAttached(
			ScorchDecalMaterial,
			DecalSize,
			SurfaceComponent,
			SurfaceHit.BoneName,
			DecalLocation,
			SurfaceNormal.Rotation(),
			EAttachLocation::KeepWorldPosition,
			TotalLifetime);
	}
	else
	{
		Decal = UGameplayStatics::SpawnDecalAtLocation(
			World,
			ScorchDecalMaterial,
			DecalSize,
			DecalLocation,
			SurfaceNormal.Rotation(),
			TotalLifetime);
	}
	if (IsValid(Decal) && FadeDuration > KINDA_SMALL_NUMBER)
	{
		Decal->SetFadeOut(VisibleDuration, FadeDuration, false);
	}
}

void ASovCinderJudgementPresentation::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASovCinderJudgementPresentation, TraceStart);
	DOREPLIFETIME(ASovCinderJudgementPresentation, TraceEnd);
	DOREPLIFETIME(ASovCinderJudgementPresentation, ImpactNormal);
	DOREPLIFETIME(ASovCinderJudgementPresentation, HitActor);
	DOREPLIFETIME(ASovCinderJudgementPresentation, HitBone);
	DOREPLIFETIME(ASovCinderJudgementPresentation, ImpactSurfaceType);
	DOREPLIFETIME(ASovCinderJudgementPresentation, ExplosionRadius);
	DOREPLIFETIME(ASovCinderJudgementPresentation, ServerFireTime);
	DOREPLIFETIME(ASovCinderJudgementPresentation, RadialTargetsResolved);
	DOREPLIFETIME(ASovCinderJudgementPresentation, PresentationFlags);
}
