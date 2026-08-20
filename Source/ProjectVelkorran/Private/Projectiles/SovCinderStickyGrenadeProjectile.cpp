// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Projectiles/SovCinderStickyGrenadeProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "NarrativeArsenal.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovCinderStickyGrenade, Log, All);

ASovCinderStickyGrenadeProjectile::ASovCinderStickyGrenadeProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	NetUpdateFrequency = 30.0f;
	MinNetUpdateFrequency = 10.0f;

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
			|| !BurnEffectClass.Get()
			|| FuseDuration <= KINDA_SMALL_NUMBER
			|| ExplosionRadius <= KINDA_SMALL_NUMBER
			|| ExplosionDamage <= KINDA_SMALL_NUMBER)
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
	PlayDetonationPresentation();
	ForceNetUpdate();
	SetLifeSpan(FMath::Max(DetonationCleanupDelay, 0.1f));
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

		const float OldShield = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetShieldAttribute());
		const float OldHealth = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetHealthAttribute());
		const float OldPoise = TargetASC->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetPoiseAttribute());

		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetASC);

		const bool bExplosionResolved =
			TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute())
				< OldShield - KINDA_SMALL_NUMBER
			|| TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())
				< OldHealth - KINDA_SMALL_NUMBER
			|| TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute())
				< OldPoise - KINDA_SMALL_NUMBER;
		if (bExplosionResolved && IsTargetAlive(TargetASC))
		{
			ApplyBurn(TargetASC, Context);
		}
	}
}

void ASovCinderStickyGrenadeProjectile::ApplyBurn(
	UAbilitySystemComponent* TargetAbilitySystem,
	const FGameplayEffectContextHandle& Context)
{
	UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get();
	if (!IsValid(SourceASC)
		|| !IsValid(TargetAbilitySystem)
		|| !BurnEffectClass.Get()
		|| BurnDamagePerTick <= KINDA_SMALL_NUMBER
		|| BurnDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	FGameplayTagContainer TargetTags;
	TargetAbilitySystem->GetOwnedGameplayTags(TargetTags);
	if (TargetTags.HasTagExact(SovTags.Status_Immunity)
		|| TargetTags.HasTag(SovTags.Status_Immunity_Burn))
	{
		return;
	}

	FGameplayEffectSpecHandle BurnSpecHandle = SourceASC->MakeOutgoingSpec(
		BurnEffectClass,
		EffectLevel,
		Context);
	FGameplayEffectSpec* BurnSpec = BurnSpecHandle.Data.Get();
	if (!BurnSpec)
	{
		return;
	}

	BurnSpec->AddDynamicAssetTag(AbilityIdentityTag);
	BurnSpec->AddDynamicAssetTag(SovTags.Damage_Channel_Thermal);
	BurnSpec->AddDynamicAssetTag(SovTags.Damage_BypassGuard);
	BurnSpec->SetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Damage,
		BurnDamagePerTick);
	BurnSpec->SetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Duration,
		BurnDuration);
	SourceASC->ApplyGameplayEffectSpecToTarget(*BurnSpec, TargetAbilitySystem);
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
