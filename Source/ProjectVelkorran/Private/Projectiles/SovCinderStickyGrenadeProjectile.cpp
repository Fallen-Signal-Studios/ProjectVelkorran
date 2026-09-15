// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Projectiles/SovCinderStickyGrenadeProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Components/DecalComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NarrativeArsenal.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicsEngine/RadialForceComponent.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovCinderStickyGrenade, Log, All);

ASovCinderStickyGrenadeProjectile::ASovCinderStickyGrenadeProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(10.0f);
	CollisionSphere->SetCollisionObjectType(TraceChannel_NarrativeProjectile);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	CollisionSphere->SetCanEverAffectNavigation(false);
	CollisionSphere->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleProjectileHit);

	GrenadeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GrenadeMesh"));
	GrenadeMesh->SetupAttachment(CollisionSphere);
	GrenadeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrenadeMesh->SetCanEverAffectNavigation(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 1.0f;

	ExplosionRadialForce = CreateDefaultSubobject<URadialForceComponent>(TEXT("ExplosionRadialForce"));
	ExplosionRadialForce->SetupAttachment(CollisionSphere);
	ExplosionRadialForce->Radius = 350.0f;
	ExplosionRadialForce->Falloff = RIF_Linear;
	ExplosionRadialForce->ForceStrength = 0.0f;
	ExplosionRadialForce->ImpulseStrength = 1800.0f;
	ExplosionRadialForce->bImpulseVelChange = true;
	ExplosionRadialForce->bIgnoreOwningActor = true;
	ExplosionRadialForce->bAutoActivate = false;
}

void ASovCinderStickyGrenadeProjectile::InitializeGrenade(
	UAbilitySystemComponent* InSourceAbilitySystem,
	AActor* InSourceAvatar,
	UObject* InDamageSourceObject,
	const TSubclassOf<UGameplayEffect> InExplosionDamageEffectClass,
	const TSubclassOf<UGameplayEffect> InBurnEffectClass,
	const FGameplayTag& InAbilityIdentityTag,
	const float InEffectLevel,
	const FVector& InInitialVelocity,
	const float InGravityScale,
	const float InFuseDuration,
	const float InExplosionRadius,
	const float InExplosionDamage,
	const float InExplosionPoiseDamage,
	const float InMinimumDamageFraction,
	const float InBurnDamagePerTick,
	const float InBurnDuration,
	const bool bInRequiresLineOfSight)
{
	checkf(!HasActorBegunPlay(), TEXT("InitializeGrenade must run before FinishSpawning."));

	SourceAbilitySystem = InSourceAbilitySystem;
	SourceAvatar = InSourceAvatar;
	DamageSourceObject = InDamageSourceObject;
	ExplosionDamageEffectClass = InExplosionDamageEffectClass;
	BurnEffectClass = InBurnEffectClass;
	AbilityIdentityTag = InAbilityIdentityTag;
	EffectLevel = FMath::Max(InEffectLevel, 1.0f);
	ReplicatedInitialVelocity = InInitialVelocity;
	ReplicatedGravityScale = FMath::Max(InGravityScale, 0.0f);
	FuseDuration = FMath::Max(InFuseDuration, 0.0f);
	ExplosionRadius = FMath::Max(InExplosionRadius, 0.0f);
	ExplosionDamage = FMath::Max(InExplosionDamage, 0.0f);
	ExplosionPoiseDamage = FMath::Max(InExplosionPoiseDamage, 0.0f);
	MinimumDamageFraction = FMath::Clamp(InMinimumDamageFraction, 0.0f, 1.0f);
	BurnDamagePerTick = FMath::Max(InBurnDamagePerTick, 0.0f);
	BurnDuration = FMath::Max(InBurnDuration, 0.0f);
	bRequiresLineOfSight = bInRequiresLineOfSight;
	bPayloadInitialized = true;
}

void ASovCinderStickyGrenadeProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (!bPayloadInitialized
			|| !SourceAbilitySystem.IsValid()
			|| !SourceAvatar.IsValid()
			|| !ExplosionDamageEffectClass.Get()
			|| FuseDuration <= KINDA_SMALL_NUMBER
			|| ExplosionRadius <= KINDA_SMALL_NUMBER
			|| ExplosionDamage <= KINDA_SMALL_NUMBER
			|| BurnDamagePerTick <= KINDA_SMALL_NUMBER
			|| BurnDuration <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(
				LogSovCinderStickyGrenade,
				Error,
				TEXT("%s was spawned without a complete authoritative payload and will be destroyed."),
				*GetNameSafe(this));
			Destroy();
			return;
		}

		CollisionSphere->IgnoreActorWhenMoving(GetOwner(), true);
		CollisionSphere->IgnoreActorWhenMoving(GetInstigator(), true);
		CollisionSphere->IgnoreActorWhenMoving(SourceAvatar.Get(), true);

		StartProjectileMovement();

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				FuseTimerHandle,
				this,
				&ThisClass::Detonate,
				FuseDuration,
				false);
		}
		SetLifeSpan(FuseDuration + 2.0f);
	}
	else
	{
		// Authority owns every collision decision, as for Velkorran's Hunger and the drone rocket. A proxy that
		// blocked locally would stop or stick where the server did not, until replicated movement corrected it.
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		StartProjectileMovement();
	}

	ReceiveGrenadeLaunched();
}

void ASovCinderStickyGrenadeProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FuseTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ASovCinderStickyGrenadeProjectile::HandleProjectileHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!HasAuthority()
		|| bIsStuck
		|| bHasDetonated
		|| OtherActor == this
		|| OtherActor == GetOwner()
		|| OtherActor == GetInstigator()
		|| OtherActor == SourceAvatar.Get())
	{
		return;
	}

	DeactivateProjectile();
	StuckActor = OtherActor;
	StuckLocation = GetActorLocation();
	StuckNormal = Hit.ImpactNormal.GetSafeNormal();
	if (StuckNormal.IsNearlyZero())
	{
		StuckNormal = FVector::UpVector;
	}

	if (IsValid(OtherComponent))
	{
		AttachToComponent(
			OtherComponent,
			FAttachmentTransformRules::KeepWorldTransform,
			Hit.BoneName);
	}

	bIsStuck = true;
	PlayStuckPresentation();
	ForceNetUpdate();
}

void ASovCinderStickyGrenadeProjectile::DeactivateProjectile()
{
	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
	if (IsValid(CollisionSphere))
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ASovCinderStickyGrenadeProjectile::StartProjectileMovement()
{
	if (bIsStuck
		|| bHasDetonated
		|| !IsValid(ProjectileMovement)
		|| FVector(ReplicatedInitialVelocity).IsNearlyZero())
	{
		return;
	}

	ProjectileMovement->ProjectileGravityScale = ReplicatedGravityScale;
	ProjectileMovement->Velocity = ReplicatedInitialVelocity;
	ProjectileMovement->Activate(true);
}

void ASovCinderStickyGrenadeProjectile::Detonate()
{
	if (!HasAuthority() || bHasDetonated)
	{
		return;
	}

	bHasDetonated = true;
	DetonationLocation = GetActorLocation();
	DeactivateProjectile();
	ApplyExplosion();
	ApplyExplosionPhysicsImpulse();
	PlayDetonationPresentation();
	ForceNetUpdate();
	SetLifeSpan(FMath::Max(DetonationCleanupDelay, 0.1f));
}

void ASovCinderStickyGrenadeProjectile::ApplyExplosionPhysicsImpulse()
{
	if (!HasAuthority()
		|| !bApplyExplosionPhysicsImpulse
		|| !IsValid(ExplosionRadialForce))
	{
		return;
	}

	const float PhysicsRadius = ExplosionRadius * FMath::Max(ExplosionPhysicsRadiusScale, 0.0f);
	if (PhysicsRadius <= KINDA_SMALL_NUMBER
		|| ExplosionRadialForce->ImpulseStrength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// A slightly lowered origin turns part of the outward impulse upward. This
	// keeps floor-bound ragdolls and props from only skidding across the ground.
	const FVector ImpulseOrigin = DetonationLocation
		- (FVector::UpVector * FMath::Max(ExplosionPhysicsUpwardBias, 0.0f));
	ExplosionRadialForce->SetWorldLocation(ImpulseOrigin);
	ExplosionRadialForce->Radius = PhysicsRadius;
	ExplosionRadialForce->FireImpulse();
}

void ASovCinderStickyGrenadeProjectile::ApplyExplosion()
{
	UWorld* World = GetWorld();
	UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get();
	AActor* SourceActor = SourceAvatar.Get();
	if (!IsValid(World)
		|| !IsValid(SourceASC)
		|| !IsValid(SourceActor)
		|| !ExplosionDamageEffectClass.Get())
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovCinderStickyGrenadeExplosion),
		false,
		this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(SourceActor);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		DetonationLocation,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(ExplosionRadius),
		QueryParams);

	const INarrativeTeamAgentInterface* SourceTeam =
		Cast<const INarrativeTeamAgentInterface>(SourceActor);
	if (!SourceTeam)
	{
		return;
	}

	TSet<UAbilitySystemComponent*> UniqueTargets;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Overlap.GetActor());
		AActor* TargetActor = IsValid(TargetASC) ? TargetASC->GetAvatarActor() : nullptr;
		if (!IsValid(TargetASC)
			|| !IsValid(TargetActor)
			|| TargetASC == SourceASC
			|| UniqueTargets.Contains(TargetASC)
			|| !TargetASC->GetSet<UNarrativeAttributeSetBase>()
			|| !IsTargetAlive(TargetASC)
			|| SourceTeam->GetTeamAttitudeTowards(*TargetActor) != ETeamAttitude::Hostile
			|| (bRequiresLineOfSight && !HasExplosionLineOfSight(TargetActor)))
		{
			continue;
		}
		UniqueTargets.Add(TargetASC);

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(SourceActor, this);
		Context.AddSourceObject(DamageSourceObject.IsValid() ? DamageSourceObject.Get() : this);
		Context.AddOrigin(DetonationLocation);

		FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
			ExplosionDamageEffectClass,
			EffectLevel,
			Context);
		FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
		if (!DamageSpec)
		{
			continue;
		}

		const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
		DamageSpec->AddDynamicAssetTag(AbilityIdentityTag);
		DamageSpec->AddDynamicAssetTag(SovTags.Damage_Channel_Kinetic);
		DamageSpec->AddDynamicAssetTag(SovTags.Damage_Channel_Thermal);
		DamageSpec->AddDynamicAssetTag(SovTags.Damage_GuardClass_Standard);
		DamageSpec->AddDynamicAssetTag(SovTags.Status_Apply_Burn);
		DamageSpec->SetSetByCallerMagnitude(
			FNarrativeGameplayTags::Get().SetByCaller_Damage,
			ExplosionDamage);

		const float DistanceAlpha = FMath::Clamp(
			FVector::Distance(DetonationLocation, TargetActor->GetActorLocation()) / ExplosionRadius,
			0.0f,
			1.0f);
		const float FalloffScalar = FMath::Lerp(1.0f, MinimumDamageFraction, DistanceAlpha);
		DamageSpec->SetSetByCallerMagnitude(
			SovTags.SetByCaller_Damage_SourceModifier,
			FalloffScalar);
		if (ExplosionPoiseDamage > KINDA_SMALL_NUMBER)
		{
			DamageSpec->SetSetByCallerMagnitude(
				SovTags.SetByCaller_Damage_PoiseDamage,
				ExplosionPoiseDamage * FalloffScalar);
		}
		DamageSpec->SetSetByCallerMagnitude(
			SovTags.SetByCaller_Status_Magnitude,
			BurnDamagePerTick);
		DamageSpec->SetSetByCallerMagnitude(
			SovTags.SetByCaller_Status_Duration,
			BurnDuration);

		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);
	}
}

bool ASovCinderStickyGrenadeProjectile::IsTargetAlive(
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
		&& !TargetAbilitySystem->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal);
}

bool ASovCinderStickyGrenadeProjectile::HasExplosionLineOfSight(AActor* TargetActor) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(TargetActor))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovCinderStickyGrenadeLineOfSight),
		false,
		this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(SourceAvatar.Get());
	if (IsValid(StuckActor)
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(StuckActor))
	{
		// A grenade attached to one enemy should not let that enemy's runtime
		// appearance occlude every other target in the radial blast.
		QueryParams.AddIgnoredActor(StuckActor);
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);

	FHitResult BlockingHit;
	const FVector TraceStart = DetonationLocation + (FVector(StuckNormal) * 2.0f);
	if (!World->LineTraceSingleByObjectType(
		BlockingHit,
		TraceStart,
		TargetActor->GetActorLocation(),
		ObjectQuery,
		QueryParams))
	{
		return true;
	}

	AActor* BlockingActor = BlockingHit.GetActor();
	return BlockingActor == TargetActor
		|| (IsValid(BlockingActor) && BlockingActor->IsOwnedBy(TargetActor))
		|| (IsValid(BlockingActor) && TargetActor->IsOwnedBy(BlockingActor));
}

bool ASovCinderStickyGrenadeProjectile::FindExplosionDecalSurface(
	FHitResult& OutSurfaceHit) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || ExplosionDecalSurfaceSearchDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovCinderStickyGrenadeDecalSurface),
		false,
		this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(SourceAvatar.Get());
	if (IsValid(StuckActor)
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(StuckActor))
	{
		// A sticky hit on a character should still leave its mark on the nearby
		// environment instead of attaching a world decal to the victim's gear.
		QueryParams.AddIgnoredActor(StuckActor);
	}

	TArray<FVector, TInlineAllocator<12>> SearchDirections;
	if (bIsStuck && !FVector(StuckNormal).IsNearlyZero())
	{
		// Usually finds the exact ground or wall the grenade struck first.
		SearchDirections.Add(-FVector(StuckNormal).GetSafeNormal());
	}
	SearchDirections.Add(FVector::DownVector);
	SearchDirections.Add(FVector::UpVector);
	for (int32 DirectionIndex = 0; DirectionIndex < 8; ++DirectionIndex)
	{
		const float AngleRadians = (PI * 2.0f * static_cast<float>(DirectionIndex)) / 8.0f;
		SearchDirections.Add(FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f));
	}

	bool bFoundSurface = false;
	float NearestDistance = TNumericLimits<float>::Max();
	for (const FVector& SearchDirection : SearchDirections)
	{
		FHitResult CandidateHit;
		const FVector TraceEnd = DetonationLocation
			+ (SearchDirection.GetSafeNormal() * ExplosionDecalSurfaceSearchDistance);
		if (!World->LineTraceSingleByObjectType(
				CandidateHit,
				DetonationLocation,
				TraceEnd,
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

void ASovCinderStickyGrenadeProjectile::SpawnExplosionDecal()
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

	const FVector SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
	const FVector DecalLocation = SurfaceHit.ImpactPoint
		+ (SurfaceNormal * FMath::Max(ExplosionDecalSurfaceOffset, 0.0f));
	const FVector DecalSize(
		FMath::Max(ExplosionDecalSize.X, 1.0f),
		FMath::Max(ExplosionDecalSize.Y, 1.0f),
		FMath::Max(ExplosionDecalSize.Z, 1.0f));
	const float VisibleDuration = FMath::Max(ExplosionDecalVisibleDuration, 0.0f);
	const float FadeDuration = FMath::Max(ExplosionDecalFadeDuration, 0.0f);
	const float TotalLifetime = FMath::Max(VisibleDuration + FadeDuration, 0.1f);
	const FRotator DecalRotation = SurfaceNormal.Rotation();

	UDecalComponent* SpawnedDecal = nullptr;
	if (USceneComponent* HitComponent = SurfaceHit.GetComponent())
	{
		SpawnedDecal = UGameplayStatics::SpawnDecalAttached(
			ExplosionDecalMaterial,
			DecalSize,
			HitComponent,
			SurfaceHit.BoneName,
			DecalLocation,
			DecalRotation,
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
			DecalRotation,
			TotalLifetime);
	}

	if (IsValid(SpawnedDecal) && FadeDuration > KINDA_SMALL_NUMBER)
	{
		SpawnedDecal->SetFadeOut(VisibleDuration, FadeDuration, false);
	}
}

void ASovCinderStickyGrenadeProjectile::OnRep_IsStuck()
{
	if (bIsStuck)
	{
		DeactivateProjectile();
		PlayStuckPresentation();
	}
}

void ASovCinderStickyGrenadeProjectile::OnRep_HasDetonated()
{
	if (bHasDetonated)
	{
		DeactivateProjectile();
		PlayDetonationPresentation();
	}
}

void ASovCinderStickyGrenadeProjectile::OnRep_InitialVelocity()
{
	if (!HasAuthority())
	{
		StartProjectileMovement();
	}
}

void ASovCinderStickyGrenadeProjectile::PlayStuckPresentation()
{
	if (bPlayedStuckPresentation)
	{
		return;
	}
	bPlayedStuckPresentation = true;
	ReceiveGrenadeStuck(StuckActor, StuckLocation, StuckNormal);
}

void ASovCinderStickyGrenadeProjectile::PlayDetonationPresentation()
{
	if (bPlayedDetonationPresentation)
	{
		return;
	}
	bPlayedDetonationPresentation = true;
	if (IsValid(GrenadeMesh))
	{
		GrenadeMesh->SetVisibility(false, true);
	}
	UWorld* World = GetWorld();
	if (IsValid(World)
		&& World->GetNetMode() != NM_DedicatedServer
		&& IsValid(ExplosionNiagaraSystem))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ExplosionNiagaraSystem,
			DetonationLocation,
			FRotator::ZeroRotator,
			ExplosionNiagaraScale,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}
	SpawnExplosionDecal();
	ReceiveGrenadeDetonated(DetonationLocation, StuckNormal);
}

void ASovCinderStickyGrenadeProjectile::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASovCinderStickyGrenadeProjectile, StuckActor);
	DOREPLIFETIME(ASovCinderStickyGrenadeProjectile, StuckLocation);
	DOREPLIFETIME(ASovCinderStickyGrenadeProjectile, StuckNormal);
	DOREPLIFETIME(ASovCinderStickyGrenadeProjectile, bIsStuck);
	DOREPLIFETIME(ASovCinderStickyGrenadeProjectile, DetonationLocation);
	DOREPLIFETIME(ASovCinderStickyGrenadeProjectile, bHasDetonated);
	DOREPLIFETIME(ASovCinderStickyGrenadeProjectile, ReplicatedInitialVelocity);
	DOREPLIFETIME(ASovCinderStickyGrenadeProjectile, ReplicatedGravityScale);
}
