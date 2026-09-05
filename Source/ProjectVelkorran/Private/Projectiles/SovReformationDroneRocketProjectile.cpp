// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Projectiles/SovReformationDroneRocketProjectile.h"
#include "Combat/SovThreatTargeting.h"
#include "Projectiles/SovProjectileDefensePolicy.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Character/NarrativeCharacterVisual.h"
#include "CollisionQueryParams.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NarrativeArsenal.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "Sound/SoundBase.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovReformationDroneRocket, Log, All);

namespace
{
	/**
	 * Narrative traces may hit a runtime CharacterVisual or another owned
	 * presentation actor instead of the authoritative character. Resolve those
	 * provider/owner links before making hostility, life, or damage decisions.
	 */
	UAbilitySystemComponent* ResolveAbilitySystemFromActor(AActor* InActor)
	{
		AActor* Candidate = InActor;
		TSet<const AActor*> VisitedActors;
		for (int32 Depth = 0;
			IsValid(Candidate) && Depth < 6 && !VisitedActors.Contains(Candidate);
			++Depth)
		{
			VisitedActors.Add(Candidate);
			// Narrative projectiles can route their owner through
			// INarrativeCharacterOwner. Treat them as blockers, not as damage
			// proxies for the character that fired them.
			if (Candidate->IsA<ANarrativeProjectile>())
			{
				return nullptr;
			}
			if (UAbilitySystemComponent* AbilitySystem =
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate))
			{
				return AbilitySystem;
			}

			if (const INarrativeCharacterOwner* CharacterProvider =
				Cast<INarrativeCharacterOwner>(Candidate))
			{
				ANarrativeCharacter* NarrativeCharacter =
					CharacterProvider->GetNarrativeCharacter();
				if (IsValid(NarrativeCharacter)
					&& NarrativeCharacter != Candidate)
				{
					if (UAbilitySystemComponent* AbilitySystem =
						UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
							NarrativeCharacter))
					{
						return AbilitySystem;
					}
				}
			}

			Candidate = Candidate->GetOwner();
		}

		return nullptr;
	}

	bool IsActorOwnedByAbilitySystem(
		AActor* Candidate,
		const UAbilitySystemComponent* ExpectedAbilitySystem)
	{
		return IsValid(Candidate)
			&& IsValid(ExpectedAbilitySystem)
			&& ResolveAbilitySystemFromActor(Candidate)
				== ExpectedAbilitySystem;
	}
}

ASovReformationDroneRocketProjectile::
	ASovReformationDroneRocketProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(14.0f);
	CollisionSphere->SetCollisionObjectType(TraceChannel_NarrativeProjectile);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionSphere->SetGenerateOverlapEvents(true);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	CollisionSphere->SetCanEverAffectNavigation(false);
	CollisionSphere->OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&ThisClass::HandleProjectileOverlap);
	CollisionSphere->OnComponentHit.AddUniqueDynamic(
		this,
		&ThisClass::HandleProjectileHit);

	RocketMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RocketMesh"));
	RocketMesh->SetupAttachment(CollisionSphere);
	RocketMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RocketMesh->SetCanEverAffectNavigation(false);

	RocketTrailComponent =
		CreateDefaultSubobject<UNiagaraComponent>(TEXT("RocketTrail"));
	RocketTrailComponent->SetupAttachment(CollisionSphere);
	RocketTrailComponent->SetAutoActivate(false);
	RocketTrailComponent->SetCanEverAffectNavigation(false);

	FlightAudioComponent =
		CreateDefaultSubobject<UAudioComponent>(TEXT("FlightAudio"));
	FlightAudioComponent->SetupAttachment(CollisionSphere);
	FlightAudioComponent->SetAutoActivate(false);

	ProjectileMovement =
		CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bIsHomingProjectile = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	ExplosionRadialForce =
		CreateDefaultSubobject<URadialForceComponent>(TEXT("ExplosionRadialForce"));
	ExplosionRadialForce->SetupAttachment(CollisionSphere);
	ExplosionRadialForce->Radius = 450.0f;
	ExplosionRadialForce->Falloff = RIF_Linear;
	ExplosionRadialForce->ForceStrength = 0.0f;
	ExplosionRadialForce->ImpulseStrength = 2200.0f;
	ExplosionRadialForce->bImpulseVelChange = true;
	ExplosionRadialForce->bIgnoreOwningActor = true;
	ExplosionRadialForce->bAutoActivate = false;
}

void ASovReformationDroneRocketProjectile::InitializeRocket(
	UAbilitySystemComponent* InSourceAbilitySystem,
	AActor* InSourceAvatar,
	UObject* InDamageSourceObject,
	const TSubclassOf<UGameplayEffect> InExplosionDamageEffectClass,
	const FGameplayTag& InAbilityIdentityTag,
	const FGameplayTagContainer& InDamageChannels,
	const FGameplayTagContainer& InAttackClassifications,
	const float InEffectLevel,
	const FVector& InInitialVelocity,
	const float InGravityScale,
	const float InCollisionRadius,
	const float InFlightDuration,
	const float InExplosionRadius,
	const float InExplosionDamage,
	const float InExplosionPoiseDamage,
	const float InMinimumDamageFraction,
	const bool bInRequiresLineOfSight,
	AActor* InHomingTarget,
	const float InHomingAccelerationMagnitude)
{
	checkf(
		!HasActorBegunPlay(),
		TEXT("InitializeRocket must run before FinishSpawning."));

	SourceAbilitySystem = InSourceAbilitySystem;
	SourceAvatar = InSourceAvatar;
	DamageSourceObject = InDamageSourceObject;
	ExplosionDamageEffectClass = InExplosionDamageEffectClass;
	AbilityIdentityTag = InAbilityIdentityTag;
	DamageChannels = InDamageChannels;
	AttackClassifications = InAttackClassifications;
	EffectLevel = FMath::Max(InEffectLevel, 1.0f);
	FlightState.LaunchLocation = GetActorLocation();
	FlightState.LaunchRotation = GetActorRotation();
	if (const UWorld* World = GetWorld())
	{
		const AGameStateBase* GameState = World->GetGameState();
		FlightState.ServerLaunchTime = IsValid(GameState)
			? GameState->GetServerWorldTimeSeconds()
			: World->GetTimeSeconds();
	}
	FlightState.InitialVelocity = InInitialVelocity;
	FlightState.GravityScale = FMath::Max(InGravityScale, 0.0f);
	FlightState.HomingTarget = InHomingTarget;
	FlightState.HomingAccelerationMagnitude =
		IsValid(InHomingTarget)
			? FMath::Max(InHomingAccelerationMagnitude, 0.0f)
			: 0.0f;
	CollisionRadius = FMath::Max(InCollisionRadius, 1.0f);
	FlightDuration = FMath::Max(InFlightDuration, 0.1f);
	ExplosionRadius = FMath::Max(InExplosionRadius, 0.0f);
	ExplosionDamage = FMath::Max(InExplosionDamage, 0.0f);
	ExplosionPoiseDamage = FMath::Max(InExplosionPoiseDamage, 0.0f);
	MinimumDamageFraction = FMath::Clamp(InMinimumDamageFraction, 0.0f, 1.0f);
	bRequiresLineOfSight = bInRequiresLineOfSight;
	bPayloadInitialized = true;
}

void ASovReformationDroneRocketProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (!bPayloadInitialized
			|| !SourceAbilitySystem.IsValid()
			|| !SourceAvatar.IsValid()
			|| !ExplosionDamageEffectClass.Get()
			|| !AbilityIdentityTag.IsValid()
			|| DamageChannels.IsEmpty()
			|| AttackClassifications.IsEmpty()
			|| FVector(FlightState.InitialVelocity).ContainsNaN()
			|| !FMath::IsFinite(FlightState.GravityScale)
			|| !FMath::IsFinite(FlightState.HomingAccelerationMagnitude)
			|| !FMath::IsFinite(CollisionRadius) || !FMath::IsFinite(FlightDuration)
			|| !FMath::IsFinite(ExplosionRadius) || !FMath::IsFinite(ExplosionDamage)
			|| !FMath::IsFinite(ExplosionPoiseDamage) || !FMath::IsFinite(MinimumDamageFraction)
			|| FVector(FlightState.InitialVelocity).IsNearlyZero()
			|| CollisionRadius < 1.0f
			|| FlightDuration < 0.1f
			|| ExplosionRadius <= KINDA_SMALL_NUMBER
			|| ExplosionDamage <= KINDA_SMALL_NUMBER
			|| ExplosionPoiseDamage < 0.0f
			|| MinimumDamageFraction < 0.0f
			|| MinimumDamageFraction > 1.0f)
		{
			UE_LOG(
				LogSovReformationDroneRocket,
				Error,
				TEXT("%s was spawned without a complete authoritative rocket payload and will be destroyed."),
				*GetNameSafe(this));
			Destroy();
			return;
		}

		CollisionSphere->SetSphereRadius(CollisionRadius, false);
		CollisionSphere->IgnoreActorWhenMoving(GetOwner(), true);
		CollisionSphere->IgnoreActorWhenMoving(GetInstigator(), true);
		CollisionSphere->IgnoreActorWhenMoving(SourceAvatar.Get(), true);

		if (ANarrativeCharacter* SourceCharacter =
			Cast<ANarrativeCharacter>(SourceAvatar.Get()))
		{
			if (ANarrativeCharacterVisual* CharacterVisual =
				SourceCharacter->GetCharacterVisual())
			{
				CollisionSphere->IgnoreActorWhenMoving(CharacterVisual, true);
			}
		}

		TArray<AActor*> AttachedActors;
		SourceAvatar.Get()->GetAttachedActors(AttachedActors, true, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			if (IsValid(AttachedActor))
			{
				CollisionSphere->IgnoreActorWhenMoving(AttachedActor, true);
			}
		}

		bCollisionArmed = true;
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		StartProjectileMovement();

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				FlightTimerHandle,
				this,
				&ThisClass::ExpireProjectile,
				FlightDuration,
				false);
		}
		SetLifeSpan(FlightDuration + 2.0f);
	}
	else
	{
		// Authority owns collision. Proxies only simulate between movement updates.
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (!Resolution.bResolved)
		{
			StartProjectileMovement();
		}
	}

	if (Resolution.bResolved)
	{
		SetActorLocation(
			FVector(Resolution.Location),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		DeactivateProjectile();
		PlayResolutionPresentation();
	}
	else
	{
		PlayLaunchPresentation();
	}
}

void ASovReformationDroneRocketProjectile::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightTimerHandle);
	}
	StopFlightPresentation();
	Super::EndPlay(EndPlayReason);
}

void ASovReformationDroneRocketProjectile::HandleProjectileOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherBodyIndex);

	if (!HasAuthority()
		|| Resolution.bResolved
		|| bResolving
		|| !bCollisionArmed
		|| !IsValid(OtherActor)
		|| OtherActor == this
		|| OtherActor == GetOwner()
		|| OtherActor == GetInstigator()
		|| OtherActor == SourceAvatar.Get())
	{
		return;
	}

	FVector ImpactNormal = bFromSweep
		? FVector(SweepResult.ImpactNormal).GetSafeNormal()
		: -FVector(FlightState.InitialVelocity).GetSafeNormal();
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = FVector::UpVector;
	}
	const FVector ImpactLocation = bFromSweep
		? FVector(SweepResult.ImpactPoint)
		: OtherActor->GetActorLocation();

	FHitResult ImpactHit = bFromSweep
		? SweepResult
		: FHitResult(OtherActor, OtherComponent, ImpactLocation, ImpactNormal);
	ImpactHit.bBlockingHit = true;
	ImpactHit.Location = ImpactLocation;
	ImpactHit.ImpactPoint = ImpactLocation;
	ImpactHit.Normal = ImpactNormal;
	ImpactHit.ImpactNormal = ImpactNormal;
	ImpactHit.TraceStart = GetActorLocation();
	ImpactHit.TraceEnd = ImpactLocation;
	ResolveImpact(ImpactHit);
}

void ASovReformationDroneRocketProjectile::HandleProjectileHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	static_cast<void>(HitComponent);
	static_cast<void>(OtherComponent);
	static_cast<void>(NormalImpulse);

	if (!HasAuthority()
		|| Resolution.bResolved
		|| bResolving
		|| !bCollisionArmed
		|| OtherActor == this
		|| OtherActor == GetOwner()
		|| OtherActor == GetInstigator()
		|| OtherActor == SourceAvatar.Get())
	{
		return;
	}

	ResolveImpact(Hit);
}

void ASovReformationDroneRocketProjectile::OnRep_FlightState(const FSovReformationDroneRocketFlightState& PreviousState)
{
	if (HasActorBegunPlay() && !Resolution.bResolved)
	{
		if (FVector(PreviousState.InitialVelocity) != FVector(FlightState.InitialVelocity)) { StartProjectileMovement(); }
		else { ConfigureHoming(); } // Tracking retirement must not reset an in-flight proxy to muzzle velocity.
	}
}

void ASovReformationDroneRocketProjectile::OnRep_Resolution()
{
	if (!HasActorBegunPlay() || !Resolution.bResolved)
	{
		return;
	}

	SetActorLocation(
		FVector(Resolution.Location),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	DeactivateProjectile();
	PlayResolutionPresentation();
}

void ASovReformationDroneRocketProjectile::StartProjectileMovement()
{
	if (!IsValid(ProjectileMovement)
		|| Resolution.bResolved
		|| FVector(FlightState.InitialVelocity).IsNearlyZero())
	{
		return;
	}

	ProjectileMovement->ProjectileGravityScale =
		FMath::Max(FlightState.GravityScale, 0.0f);
	ProjectileMovement->Velocity = FVector(FlightState.InitialVelocity);
	ProjectileMovement->InitialSpeed = FVector(FlightState.InitialVelocity).Size();
	ProjectileMovement->MaxSpeed = FVector(FlightState.InitialVelocity).Size();
	ConfigureHoming();
	ProjectileMovement->Activate(true);
}

void ASovReformationDroneRocketProjectile::ConfigureHoming()
{
	if (!IsValid(ProjectileMovement))
	{
		return;
	}

	AActor* HomingTarget = FlightState.HomingTarget.Get();
	if (HasAuthority() && HomingTarget && !SovThreatTargeting::CanTrack(SourceAvatar.Get(), HomingTarget))
	{
		FlightState.HomingTarget = nullptr;
		FlightState.HomingAccelerationMagnitude = 0.f;
		HomingTarget = nullptr;
		ForceNetUpdate();
	}
	USceneComponent* TargetComponent =
		IsValid(HomingTarget) ? HomingTarget->GetRootComponent() : nullptr;
	const bool bCanHome = IsValid(TargetComponent)
		&& FlightState.HomingAccelerationMagnitude > KINDA_SMALL_NUMBER;
	ProjectileMovement->bIsHomingProjectile = bCanHome;
	if (bCanHome)
	{
		ProjectileMovement->HomingTargetComponent = TargetComponent;
	}
	else
	{
		ProjectileMovement->HomingTargetComponent = nullptr;
	}
	ProjectileMovement->HomingAccelerationMagnitude = bCanHome
		? FlightState.HomingAccelerationMagnitude
		: 0.0f;
	// Retire lost tracking before this frame's movement computes homing acceleration.
	ProjectileMovement->AddTickPrerequisiteActor(this);
	SetActorTickEnabled(HasAuthority() && bCanHome);
}

void ASovReformationDroneRocketProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || Resolution.bResolved) { return; }
	if (!SourceAbilitySystem.IsValid() || SourceAbilitySystem->GetAvatarActor() != SourceAvatar.Get()
		|| !SovThreatTargeting::CanTrack(SourceAvatar.Get(), FlightState.HomingTarget.Get()))
	{
		FlightState.HomingTarget = nullptr;
		FlightState.HomingAccelerationMagnitude = 0.f;
		ConfigureHoming();
		ForceNetUpdate();
	}
}

void ASovReformationDroneRocketProjectile::DeactivateProjectile()
{
	SetActorTickEnabled(false);
	bCollisionArmed = false;
	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->bIsHomingProjectile = false;
		ProjectileMovement->HomingTargetComponent = nullptr;
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
	if (IsValid(CollisionSphere))
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (IsValid(RocketMesh))
	{
		RocketMesh->SetVisibility(false, true);
	}
	StopFlightPresentation();
}

void ASovReformationDroneRocketProjectile::ExpireProjectile()
{
	if (!HasAuthority() || Resolution.bResolved)
	{
		return;
	}
	// Reflection defers the movement restart one tick. Do not lose the one-shot launch expiry
	// if that deadline arrives while the collision callback is still resolving.
	if (bResolving) { bFlightExpiredWhileResolving=true; return; }
	ResolveExpired();
}

void ASovReformationDroneRocketProjectile::ResolveImpact(
	const FHitResult& Hit)
{
	if (!HasAuthority() || Resolution.bResolved || bResolving)
	{
		return;
	}
	bResolving = true;
	if (ResolveDirectDefense(Hit)) { return; }

	FVector ImpactLocation = FVector(Hit.ImpactPoint);
	if (ImpactLocation.ContainsNaN())
	{
		ImpactLocation = GetActorLocation();
	}
	FVector ImpactNormal = FVector(Hit.ImpactNormal).GetSafeNormal();
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = -FVector(FlightState.InitialVelocity).GetSafeNormal();
	}
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = FVector::UpVector;
	}

	Resolution = FSovReformationDroneRocketResolution();
	Resolution.bExpired = false;
	Resolution.HitActor = Hit.GetActor();
	Resolution.Location = ImpactLocation;
	Resolution.Normal = ImpactNormal;
	Resolution.HitBone = Hit.BoneName;
	SetActorLocation(
		ImpactLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	DeactivateProjectile();
	Resolution.bDamagedAnyTarget = ApplyExplosion(ImpactLocation, &Hit)
		|| DirectResult.AppliedHealthDamage > 0.f || DirectResult.AppliedShieldDamage > 0.f;
	ApplyExplosionPhysicsImpulse(ImpactLocation);
	Resolution.bResolved = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightTimerHandle);
	}
	PlayResolutionPresentation();
	ForceNetUpdate();
	SetLifeSpan(FMath::Max(ResolutionCleanupDelay, 0.1f));
}

void ASovReformationDroneRocketProjectile::ReceiveDirectDefense(const FSovDamageResult& Result)
{
	if (!HasAuthority() || !PendingDirectContext || Result.EffectContext.Get()!=PendingDirectContext
		|| !DirectDamageTarget.IsValid() || Result.TargetActor!=DirectDamageTarget->GetAvatarActor()
		|| Result.SourceActor!=SourceAvatar.Get() || bReceivedDirectResult) { return; }
	bReceivedDirectResult=true; DirectResult=Result;
}

bool ASovReformationDroneRocketProjectile::ResolveDirectDefense(const FHitResult& Hit)
{
	DirectDamageTarget.Reset(); DirectResult=FSovDamageResult(); bReceivedDirectResult=false;
	UAbilitySystemComponent* SourceASC=SourceAbilitySystem.Get();
	auto* Target=Cast<UNarrativeAbilitySystemComponent>(ResolveAbilitySystemFromActor(Hit.GetActor()));
	AActor* Defender=Target?Target->GetAvatarActor():nullptr;
	const auto* Team=Cast<INarrativeTeamAgentInterface>(SourceAvatar.Get());
	if (!Target || !IsValid(SourceASC) || SourceASC->GetAvatarActor()!=SourceAvatar.Get()
		|| !IsValid(Defender) || !IsTargetAlive(Target) || !Team
		|| Team->GetTeamAttitudeTowards(*Defender)!=ETeamAttitude::Hostile) { return false; }
	FGameplayEffectContextHandle Context=SourceASC->MakeEffectContext();
	Context.AddInstigator(SourceAvatar.Get(),this); Context.AddHitResult(Hit,true);
	Context.AddSourceObject(DamageSourceObject.IsValid()?DamageSourceObject.Get():SourceAvatar.Get());
	FGameplayEffectSpecHandle Spec=SourceASC->MakeOutgoingSpec(ExplosionDamageEffectClass,EffectLevel,Context);
	if (!Spec.IsValid()) { return false; }
	Spec.Data->AddDynamicAssetTag(AbilityIdentityTag);
	for (const auto& Tag:DamageChannels) { Spec.Data->AddDynamicAssetTag(Tag); }
	for (const auto& Tag:AttackClassifications) { Spec.Data->AddDynamicAssetTag(Tag); }
	Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage,ExplosionDamage);
	Spec.Data->SetSetByCallerMagnitude(FSovGameplayTags::Get().SetByCaller_Damage_PoiseDamage,ExplosionPoiseDamage);
	DirectDamageTarget=Target; PendingDirectContext=Context.Get();
	Target->OnDamageResolvedAsTarget.AddUniqueDynamic(this,&ThisClass::ReceiveDirectDefense);
	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(),Target);
	if (IsValid(Target)) { Target->OnDamageResolvedAsTarget.RemoveDynamic(this,&ThisClass::ReceiveDirectDefense); }
	PendingDirectContext=nullptr;
	if (IsActorBeingDestroyed() || Resolution.bResolved) { return true; }
	const auto Outcome=SovProjectileDefense::Resolve(bReceivedDirectResult,DirectResult.bPerfectDefense,
		DirectResult.bDeflected,DirectResult.bGuarded,ReflectionCount,FMath::Min<uint8>(MaximumReflections,4));
	if (Outcome==SovProjectileDefense::EOutcome::Explode) { return false; }
	if (Outcome==SovProjectileDefense::EOutcome::Absorb || !IsValid(Target) || !IsTargetAlive(Target)
		|| Target->GetAvatarActor()!=Defender)
	{
		bResolving=false; ResolveExpired(true); return true;
	}
	// Redirect the real collision body and move its damage ownership to the successful defender.
	// A later impact uses normal team and immunity routing; this is never a cosmetic tracer reversal.
	AActor* PreviousSource=SourceAvatar.Get();
	const float Speed=FMath::Max(ProjectileMovement->Velocity.Size(),FVector(FlightState.InitialVelocity).Size());
	FVector Direction=IsValid(PreviousSource)?(PreviousSource->GetActorLocation()-GetActorLocation()).GetSafeNormal():-FVector(FlightState.InitialVelocity).GetSafeNormal();
	if (Direction.IsNearlyZero()) { Direction=Defender->GetActorForwardVector(); }
	++ReflectionCount; SourceAbilitySystem=Target; SourceAvatar=Defender; DamageSourceObject=Defender;
	SetOwner(Defender); SetInstigator(Cast<APawn>(Defender));
	CollisionSphere->ClearMoveIgnoreActors(); CollisionSphere->IgnoreActorWhenMoving(Defender,true);
	TArray<AActor*> Attached; Defender->GetAttachedActors(Attached,true,true);
	for (AActor* Actor:Attached) { CollisionSphere->IgnoreActorWhenMoving(Actor,true); }
	if (const auto* Character=Cast<ANarrativeCharacter>(Defender))
	{ if (auto* Visual=Character->GetCharacterVisual()) { CollisionSphere->IgnoreActorWhenMoving(Visual,true); } }
	FlightState.InitialVelocity=Direction*FMath::Max(Speed,1.f); FlightState.HomingTarget=nullptr; FlightState.HomingAccelerationMagnitude=0.f;
	// The movement component may clear its UpdatedComponent after this hit callback. Restart on the next tick.
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); bCollisionArmed=false;
	GetWorld()->GetTimerManager().SetTimerForNextTick(this,&ThisClass::ResumeReflectedFlight);
	DirectDamageTarget.Reset(); ForceNetUpdate(); return true;
}
void ASovReformationDroneRocketProjectile::ResumeReflectedFlight()
{
	if (!HasAuthority() || Resolution.bResolved || IsActorBeingDestroyed()) { return; }
	if (bFlightExpiredWhileResolving || !SourceAbilitySystem.IsValid() || SourceAbilitySystem->GetAvatarActor()!=SourceAvatar.Get())
	{ bResolving=false; ResolveExpired(); return; }
	ProjectileMovement->SetUpdatedComponent(CollisionSphere);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); bCollisionArmed=true; bResolving=false;
	StartProjectileMovement();
}

void ASovReformationDroneRocketProjectile::ResolveExpired(const bool bAbsorbed)
{
	if (!HasAuthority() || Resolution.bResolved || bResolving)
	{
		return;
	}
	bResolving = true;

	FVector DissipationNormal =
		-FVector(FlightState.InitialVelocity).GetSafeNormal();
	if (DissipationNormal.IsNearlyZero())
	{
		DissipationNormal = FVector::UpVector;
	}

	Resolution = FSovReformationDroneRocketResolution();
	Resolution.bExpired = true;
	Resolution.bAbsorbed = bAbsorbed;
	Resolution.Location = GetActorLocation();
	Resolution.Normal = DissipationNormal;
	Resolution.bResolved = true;
	DeactivateProjectile();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightTimerHandle);
	}
	PlayResolutionPresentation();
	ForceNetUpdate();
	SetLifeSpan(FMath::Max(ResolutionCleanupDelay, 0.1f));
}

bool ASovReformationDroneRocketProjectile::ApplyExplosion(
	const FVector& ExplosionLocation,
	const FHitResult* DirectHit)
{
	UWorld* World = GetWorld();
	UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get();
	AActor* SourceActor = SourceAvatar.Get();
	if (!IsValid(World)
		|| !IsValid(SourceASC)
		|| !IsValid(SourceActor)
		|| SourceASC->GetAvatarActor()!=SourceActor
		|| !ExplosionDamageEffectClass.Get()
		|| ExplosionRadius <= KINDA_SMALL_NUMBER
		|| ExplosionDamage <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const INarrativeTeamAgentInterface* SourceTeam =
		Cast<const INarrativeTeamAgentInterface>(SourceActor);
	if (!SourceTeam)
	{
		return false;
	}

	UAbilitySystemComponent* DirectTargetASC = DirectHit
		? ResolveAbilitySystemFromActor(DirectHit->GetActor())
		: nullptr;

	TSet<UAbilitySystemComponent*> UniqueTargets;
	if (IsValid(DirectTargetASC))
	{
		UniqueTargets.Add(DirectTargetASC);
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovReformationDroneRocketExplosion),
		false,
		this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(SourceActor);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		ExplosionLocation,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(ExplosionRadius),
		QueryParams);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (UAbilitySystemComponent* TargetASC =
			ResolveAbilitySystemFromActor(Overlap.GetActor()))
		{
			UniqueTargets.Add(TargetASC);
		}
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	bool bDamagedAnyTarget = false;
	for (UAbilitySystemComponent* TargetASC : UniqueTargets)
	{
		if (IsActorBeingDestroyed() || !IsValid(SourceASC) || !IsValid(SourceActor) || SourceASC->GetAvatarActor()!=SourceActor) { break; }
		AActor* TargetActor = IsValid(TargetASC)
			? TargetASC->GetAvatarActor()
			: nullptr;
		const bool bIsDirectTarget = TargetASC == DirectTargetASC;
		if (!IsValid(TargetASC)
			|| TargetASC == DirectDamageTarget.Get()
			|| !IsValid(TargetActor)
			|| TargetASC == SourceASC
			|| !TargetASC->GetSet<UNarrativeAttributeSetBase>()
			|| !IsTargetAlive(TargetASC)
			|| SourceTeam->GetTeamAttitudeTowards(*TargetActor)
				!= ETeamAttitude::Hostile
			|| (bRequiresLineOfSight
				&& !bIsDirectTarget
				&& !HasExplosionLineOfSight(
					TargetActor,
					TargetASC,
					ExplosionLocation,
					FVector(Resolution.Normal),
					DirectHit ? DirectHit->GetActor() : nullptr)))
		{
			continue;
		}

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(SourceActor, this);
		Context.AddSourceObject(
			DamageSourceObject.IsValid() ? DamageSourceObject.Get() : SourceActor);
		Context.AddOrigin(ExplosionLocation);
		if (bIsDirectTarget && DirectHit)
		{
			Context.AddHitResult(*DirectHit, true);
		}

		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
			ExplosionDamageEffectClass,
			EffectLevel,
			Context);
		FGameplayEffectSpec* DamageSpec = SpecHandle.Data.Get();
		if (!DamageSpec)
		{
			continue;
		}

		const float DistanceAlpha = bIsDirectTarget
			? 0.0f
			: FMath::Clamp(
				FVector::Distance(
					ExplosionLocation,
					TargetActor->GetActorLocation())
					/ ExplosionRadius,
				0.0f,
				1.0f);
		const float FalloffScalar =
			FMath::Lerp(1.0f, MinimumDamageFraction, DistanceAlpha);

		DamageSpec->AddDynamicAssetTag(AbilityIdentityTag);
		for (const FGameplayTag& Tag : DamageChannels)
		{
			DamageSpec->AddDynamicAssetTag(Tag);
		}
		for (const FGameplayTag& Tag : AttackClassifications)
		{
			DamageSpec->AddDynamicAssetTag(Tag);
		}
		DamageSpec->SetSetByCallerMagnitude(
			FNarrativeGameplayTags::Get().SetByCaller_Damage,
			ExplosionDamage);
		DamageSpec->SetSetByCallerMagnitude(
			SovTags.SetByCaller_Damage_SourceModifier,
			FalloffScalar);
		if (ExplosionPoiseDamage > KINDA_SMALL_NUMBER)
		{
			DamageSpec->SetSetByCallerMagnitude(
				SovTags.SetByCaller_Damage_PoiseDamage,
				ExplosionPoiseDamage * FalloffScalar);
		}

		const float OldShield = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetShieldAttribute());
		const float OldHealth = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetHealthAttribute());
		const float OldPoise = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetPoiseAttribute());
		const float OldStamina = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetStaminaAttribute());
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
		if (!IsValid(TargetASC) || TargetASC->GetAvatarActor()!=TargetActor) { continue; }

		bDamagedAnyTarget = bDamagedAnyTarget
			|| TargetASC->GetNumericAttribute(
				UNarrativeAttributeSetBase::GetShieldAttribute())
				< OldShield - KINDA_SMALL_NUMBER
			|| TargetASC->GetNumericAttribute(
				UNarrativeAttributeSetBase::GetHealthAttribute())
				< OldHealth - KINDA_SMALL_NUMBER
			|| TargetASC->GetNumericAttribute(
				UNarrativeAttributeSetBase::GetPoiseAttribute())
				< OldPoise - KINDA_SMALL_NUMBER
			|| TargetASC->GetNumericAttribute(
				UNarrativeAttributeSetBase::GetStaminaAttribute())
				< OldStamina - KINDA_SMALL_NUMBER;
	}

	return bDamagedAnyTarget;
}

void ASovReformationDroneRocketProjectile::ApplyExplosionPhysicsImpulse(
	const FVector& ExplosionLocation)
{
	if (!HasAuthority()
		|| !bApplyExplosionPhysicsImpulse
		|| !IsValid(ExplosionRadialForce))
	{
		return;
	}

	const float PhysicsRadius =
		ExplosionRadius * FMath::Max(ExplosionPhysicsRadiusScale, 0.0f);
	if (PhysicsRadius <= KINDA_SMALL_NUMBER
		|| ExplosionRadialForce->ImpulseStrength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	ExplosionRadialForce->SetWorldLocation(
		ExplosionLocation
		- (FVector::UpVector * FMath::Max(ExplosionPhysicsUpwardBias, 0.0f)));
	ExplosionRadialForce->Radius = PhysicsRadius;
	ExplosionRadialForce->FireImpulse();
}

bool ASovReformationDroneRocketProjectile::IsTargetAlive(
	const UAbilitySystemComponent* TargetAbilitySystem) const
{
	if (!IsValid(TargetAbilitySystem))
	{
		return false;
	}
	if (const UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(TargetAbilitySystem))
	{
		return !NarrativeASC->IsDead();
	}
	return !TargetAbilitySystem->HasMatchingGameplayTag(
			FNarrativeGameplayTags::Get().State_IsDead)
		&& !TargetAbilitySystem->HasMatchingGameplayTag(
			FSovGameplayTags::Get().State_Fatal);
}

bool ASovReformationDroneRocketProjectile::HasExplosionLineOfSight(
	AActor* TargetActor,
	UAbilitySystemComponent* TargetAbilitySystem,
	const FVector& ExplosionLocation,
	const FVector& ExplosionNormal,
	AActor* DirectHitActor) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| !IsValid(TargetActor)
		|| !IsValid(TargetAbilitySystem))
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovReformationDroneRocketLineOfSight),
		false,
		this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(SourceAvatar.Get());
	if (IsActorOwnedByAbilitySystem(DirectHitActor, TargetAbilitySystem))
	{
		QueryParams.AddIgnoredActor(DirectHitActor);
	}

	const FVector SafeNormal = ExplosionNormal.IsNearlyZero()
		? FVector::UpVector
		: ExplosionNormal.GetSafeNormal();
	FHitResult BlockingHit;
	if (!World->LineTraceSingleByObjectType(
		BlockingHit,
		ExplosionLocation + (SafeNormal * 2.0f),
		TargetActor->GetActorLocation(),
		ObjectQuery,
		QueryParams))
	{
		return true;
	}

	AActor* BlockingActor = BlockingHit.GetActor();
	return BlockingActor == TargetActor
		|| (IsValid(BlockingActor) && BlockingActor->IsOwnedBy(TargetActor))
		|| (IsValid(BlockingActor) && TargetActor->IsOwnedBy(BlockingActor))
		|| IsActorOwnedByAbilitySystem(
			BlockingActor,
			TargetAbilitySystem);
}

bool ASovReformationDroneRocketProjectile::FindExplosionDecalSurface(
	FHitResult& OutSurfaceHit) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| !Resolution.bResolved
		|| Resolution.bExpired
		|| ExplosionDecalSurfaceSearchDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovReformationDroneRocketDecalSurface),
		false,
		this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(SourceAvatar.Get());
	if (IsValid(Resolution.HitActor.Get())
		&& ResolveAbilitySystemFromActor(Resolution.HitActor.Get()))
	{
		QueryParams.AddIgnoredActor(Resolution.HitActor.Get());
	}

	TArray<FVector, TInlineAllocator<12>> SearchDirections;
	const FVector ResolutionNormal = FVector(Resolution.Normal).GetSafeNormal();
	if (!ResolutionNormal.IsNearlyZero())
	{
		SearchDirections.Add(-ResolutionNormal);
	}
	SearchDirections.Add(FVector::DownVector);
	SearchDirections.Add(FVector::UpVector);
	for (int32 DirectionIndex = 0; DirectionIndex < 8; ++DirectionIndex)
	{
		const float AngleRadians =
			(PI * 2.0f * static_cast<float>(DirectionIndex)) / 8.0f;
		SearchDirections.Add(
			FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f));
	}

	const FVector TraceStart = FVector(Resolution.Location)
		+ (ResolutionNormal * 2.0f);
	bool bFoundSurface = false;
	float NearestDistance = TNumericLimits<float>::Max();
	for (const FVector& SearchDirection : SearchDirections)
	{
		FHitResult CandidateHit;
		const FVector TraceEnd = TraceStart
			+ (SearchDirection.GetSafeNormal()
				* ExplosionDecalSurfaceSearchDistance);
		if (!World->LineTraceSingleByObjectType(
			CandidateHit,
			TraceStart,
			TraceEnd,
			ObjectQuery,
			QueryParams)
			|| !CandidateHit.bBlockingHit
			|| FVector(CandidateHit.ImpactNormal).IsNearlyZero()
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

void ASovReformationDroneRocketProjectile::SpawnExplosionDecal()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| World->GetNetMode() == NM_DedicatedServer
		|| !IsValid(ExplosionDecalMaterial))
	{
		return;
	}

	FHitResult SurfaceHit;
	if (!FindExplosionDecalSurface(SurfaceHit))
	{
		return;
	}

	const FVector SurfaceNormal =
		FVector(SurfaceHit.ImpactNormal).GetSafeNormal();
	const FVector DecalLocation = FVector(SurfaceHit.ImpactPoint)
		+ (SurfaceNormal * FMath::Max(ExplosionDecalSurfaceOffset, 0.0f));
	const FVector DecalSize(
		FMath::Max(ExplosionDecalSize.X, 1.0f),
		FMath::Max(ExplosionDecalSize.Y, 1.0f),
		FMath::Max(ExplosionDecalSize.Z, 1.0f));
	const float VisibleDuration =
		FMath::Max(ExplosionDecalVisibleDuration, 0.0f);
	const float FadeDuration =
		FMath::Max(ExplosionDecalFadeDuration, 0.0f);
	const float TotalLifetime =
		FMath::Max(VisibleDuration + FadeDuration, 0.1f);

	UDecalComponent* SpawnedDecal = nullptr;
	if (USceneComponent* SurfaceComponent = SurfaceHit.GetComponent())
	{
		SpawnedDecal = UGameplayStatics::SpawnDecalAttached(
			ExplosionDecalMaterial,
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
		SpawnedDecal = UGameplayStatics::SpawnDecalAtLocation(
			World,
			ExplosionDecalMaterial,
			DecalSize,
			DecalLocation,
			SurfaceNormal.Rotation(),
			TotalLifetime);
	}

	if (IsValid(SpawnedDecal) && FadeDuration > KINDA_SMALL_NUMBER)
	{
		SpawnedDecal->SetFadeOut(VisibleDuration, FadeDuration, false);
	}
}

void ASovReformationDroneRocketProjectile::PlayLaunchPresentation()
{
	if (bPlayedLaunchPresentation)
	{
		return;
	}
	bPlayedLaunchPresentation = true;

	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const AGameStateBase* GameState = World->GetGameState();
	const float LaunchAge = IsValid(GameState)
		? FMath::Max(
			GameState->GetServerWorldTimeSeconds()
				- FlightState.ServerLaunchTime,
			0.0f)
		: 0.0f;
	const bool bPlayLaunchOneShots = FlightState.ServerLaunchTime <= 0.0f
		|| LaunchAge <= FMath::Max(LaunchOneShotMaximumAge, 0.0f);
	const FTransform ReplicatedLaunchFrame(
		FlightState.LaunchRotation,
		FVector(FlightState.LaunchLocation),
		FVector::OneVector);

	if (bPlayLaunchOneShots && IsValid(LaunchNiagaraSystem))
	{
		const FTransform LaunchTransform =
			LaunchNiagaraRelativeTransform * ReplicatedLaunchFrame;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			LaunchNiagaraSystem,
			LaunchTransform.GetLocation(),
			LaunchTransform.Rotator(),
			LaunchTransform.GetScale3D(),
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}
	if (IsValid(RocketTrailComponent) && IsValid(TrailNiagaraSystem))
	{
		RocketTrailComponent->SetAsset(TrailNiagaraSystem);
		RocketTrailComponent->SetRelativeTransform(TrailRelativeTransform);
		RocketTrailComponent->Activate(true);
	}
	if (IsValid(FlightAudioComponent) && IsValid(FlightLoopSound))
	{
		FlightAudioComponent->SetSound(FlightLoopSound);
		FlightAudioComponent->SetRelativeTransform(FlightAudioRelativeTransform);
		FlightAudioComponent->SetVolumeMultiplier(
			FMath::Max(FlightLoopVolumeMultiplier, 0.0f));
		FlightAudioComponent->SetPitchMultiplier(
			FMath::Max(FlightLoopPitchMultiplier, 0.01f));
		FlightAudioComponent->Play();
	}
	if (bPlayLaunchOneShots && IsValid(LaunchSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			LaunchSound,
			FVector(FlightState.LaunchLocation));
	}

	if (bPlayLaunchOneShots)
	{
		ReceiveRocketLaunched();
	}
}

void ASovReformationDroneRocketProjectile::StopFlightPresentation()
{
	if (IsValid(RocketTrailComponent))
	{
		RocketTrailComponent->Deactivate();
	}
	if (IsValid(FlightAudioComponent))
	{
		FlightAudioComponent->Stop();
	}
}

void ASovReformationDroneRocketProjectile::PlayResolutionPresentation()
{
	if (bPlayedResolutionPresentation || !Resolution.bResolved)
	{
		return;
	}
	bPlayedResolutionPresentation = true;

	StopFlightPresentation();
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FVector ResolutionLocation = FVector(Resolution.Location);
	FVector ResolutionNormal = FVector(Resolution.Normal).GetSafeNormal();
	if (ResolutionNormal.IsNearlyZero())
	{
		ResolutionNormal = FVector::UpVector;
	}
	const FTransform ResolutionFrame(
		ResolutionNormal.Rotation(),
		ResolutionLocation,
		FVector::OneVector);

	if (Resolution.bExpired)
	{
		if (IsValid(DissipationNiagaraSystem))
		{
			const FTransform SpawnTransform =
				DissipationNiagaraRelativeTransform * ResolutionFrame;
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				DissipationNiagaraSystem,
				SpawnTransform.GetLocation(),
				SpawnTransform.Rotator(),
				SpawnTransform.GetScale3D(),
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
				ResolutionLocation);
		}
		ReceiveRocketDissipated(ResolutionLocation);
		return;
	}

	if (IsValid(ExplosionNiagaraSystem))
	{
		const FTransform SpawnTransform =
			ExplosionNiagaraRelativeTransform * ResolutionFrame;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ExplosionNiagaraSystem,
			SpawnTransform.GetLocation(),
			SpawnTransform.Rotator(),
			SpawnTransform.GetScale3D(),
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}
	if (IsValid(ExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ExplosionSound,
			ResolutionLocation);
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
			ResolutionLocation,
			InnerRadius,
			OuterRadius,
			FMath::Max(ExplosionCameraShakeFalloff, 0.0f),
			bOrientCameraShakeTowardExplosion);
	}
	SpawnExplosionDecal();
	ReceiveRocketImpacted(
		Resolution.HitActor.Get(),
		ResolutionLocation,
		ResolutionNormal,
		Resolution.HitBone,
		Resolution.bDamagedAnyTarget);
}

void ASovReformationDroneRocketProjectile::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASovReformationDroneRocketProjectile, FlightState);
	DOREPLIFETIME(ASovReformationDroneRocketProjectile, Resolution);
}
