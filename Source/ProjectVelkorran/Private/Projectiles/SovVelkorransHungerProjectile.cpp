// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Projectiles/SovVelkorransHungerProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
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
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Weapons/WeaponVisual.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovVelkorransHungerProjectile, Log, All);

ASovVelkorransHungerProjectile::ASovVelkorransHungerProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(35.0f);
	CollisionSphere->SetCollisionObjectType(TraceChannel_NarrativeProjectile);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetGenerateOverlapEvents(true);
	CollisionSphere->SetNotifyRigidBodyCollision(true);
	CollisionSphere->SetCanEverAffectNavigation(false);
	CollisionSphere->OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&ThisClass::HandleProjectileOverlap);
	CollisionSphere->OnComponentHit.AddUniqueDynamic(
		this,
		&ThisClass::HandleProjectileHit);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(CollisionSphere);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetCanEverAffectNavigation(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
}

void ASovVelkorransHungerProjectile::InitializeHungerProjectile(
	UAbilitySystemComponent* InSourceAbilitySystem,
	AActor* InSourceAvatar,
	UObject* InDamageSourceObject,
	const TSubclassOf<UGameplayEffect> InDirectDamageEffectClass,
	const TSubclassOf<UGameplayEffect> InBurnEffectClass,
	const FGameplayTag& InAbilityIdentityTag,
	const float InEffectLevel,
	const FVector& InInitialVelocity,
	const float InGravityScale,
	const float InCollisionRadius,
	const float InFlightDuration,
	const float InDirectDamage,
	const float InDirectPoiseDamage,
	const float InBurnDamagePerTick,
	const float InBurnDuration)
{
	checkf(
		!HasActorBegunPlay(),
		TEXT("InitializeHungerProjectile must run before FinishSpawning."));

	SourceAbilitySystem = InSourceAbilitySystem;
	SourceAvatar = InSourceAvatar;
	DamageSourceObject = InDamageSourceObject;
	DirectDamageEffectClass = InDirectDamageEffectClass;
	BurnEffectClass = InBurnEffectClass;
	AbilityIdentityTag = InAbilityIdentityTag;
	EffectLevel = FMath::Max(InEffectLevel, 1.0f);
	ReplicatedInitialVelocity = InInitialVelocity;
	ReplicatedGravityScale = FMath::Max(InGravityScale, 0.0f);
	CollisionRadius = FMath::Max(InCollisionRadius, 1.0f);
	FlightDuration = FMath::Max(InFlightDuration, 0.1f);
	DirectDamage = FMath::Max(InDirectDamage, 0.0f);
	DirectPoiseDamage = FMath::Max(InDirectPoiseDamage, 0.0f);
	BurnDamagePerTick = FMath::Max(InBurnDamagePerTick, 0.0f);
	BurnDuration = FMath::Max(InBurnDuration, 0.0f);
	bPayloadInitialized = true;
}

void ASovVelkorransHungerProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		if (!bPayloadInitialized
			|| !SourceAbilitySystem.IsValid()
			|| !SourceAvatar.IsValid()
			|| !DirectDamageEffectClass.Get()
			|| !BurnEffectClass.Get()
			|| FVector(ReplicatedInitialVelocity).IsNearlyZero()
			|| CollisionRadius <= KINDA_SMALL_NUMBER
			|| FlightDuration <= KINDA_SMALL_NUMBER
			|| DirectDamage <= KINDA_SMALL_NUMBER
			|| BurnDamagePerTick <= KINDA_SMALL_NUMBER
			|| BurnDuration <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(
				LogSovVelkorransHungerProjectile,
				Error,
				TEXT("%s was spawned without a complete authoritative payload and will be destroyed."),
				*GetNameSafe(this));
			Destroy();
			return;
		}

		ReceiveHungerProjectileLaunched();
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
			if (AWeaponVisual* MainhandVisual =
				SourceCharacter->GetWieldedWeaponVisual(true))
			{
				CollisionSphere->IgnoreActorWhenMoving(MainhandVisual, true);
			}
			if (AWeaponVisual* OffhandVisual =
				SourceCharacter->GetWieldedWeaponVisual(false))
			{
				CollisionSphere->IgnoreActorWhenMoving(OffhandVisual, true);
			}
		}
		bCollisionArmed = true;
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		StartProjectileMovement();

		if (!bHasResolvedImpact)
		{
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
	}
	else
	{
		// Authority owns every collision decision. Simulated proxies only travel
		// cosmetically between replicated movement corrections.
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ReceiveHungerProjectileLaunched();
		StartProjectileMovement();
	}

	if (bHasResolvedImpact)
	{
		DeactivateProjectile();
		PlayResolutionPresentation();
	}
}

void ASovVelkorransHungerProjectile::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ASovVelkorransHungerProjectile::HandleProjectileOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComponent);
	static_cast<void>(OtherBodyIndex);

	if (!HasAuthority()
		|| bHasResolvedImpact
		|| bResolvingImpact
		|| !bCollisionArmed
		|| !IsValid(OtherActor)
		|| OtherActor == this
		|| OtherActor == GetOwner()
		|| OtherActor == GetInstigator()
		|| OtherActor == SourceAvatar.Get())
	{
		return;
	}

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor);
	if (!IsHostileTarget(TargetASC))
	{
		return;
	}

	const FVector HitLocation = bFromSweep
		? FVector(SweepResult.ImpactPoint)
		: OtherActor->GetActorLocation();
	FVector HitNormal = bFromSweep
		? FVector(SweepResult.ImpactNormal)
		: FVector::ZeroVector;
	if (HitNormal.IsNearlyZero())
	{
		HitNormal = -FVector(ReplicatedInitialVelocity).GetSafeNormal();
	}
	if (HitNormal.IsNearlyZero())
	{
		HitNormal = FVector::UpVector;
	}

	bResolvingImpact = true;
	DeactivateProjectile();
	const bool bDamagedTarget = ApplyDirectHit(TargetASC, HitLocation);
	ResolveImpact(OtherActor, HitLocation, HitNormal, bDamagedTarget, false);
}

void ASovVelkorransHungerProjectile::HandleProjectileHit(
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
		|| bHasResolvedImpact
		|| bResolvingImpact
		|| !bCollisionArmed
		|| OtherActor == this
		|| OtherActor == GetOwner()
		|| OtherActor == GetInstigator()
		|| OtherActor == SourceAvatar.Get())
	{
		return;
	}

	bResolvingImpact = true;
	DeactivateProjectile();
	UAbilitySystemComponent* TargetASC = IsValid(OtherActor)
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor)
		: nullptr;
	const FVector HitLocation(Hit.ImpactPoint);
	const bool bDamagedTarget = IsHostileTarget(TargetASC)
		&& ApplyDirectHit(TargetASC, HitLocation);

	FVector HitNormal = FVector(Hit.ImpactNormal).GetSafeNormal();
	if (HitNormal.IsNearlyZero())
	{
		HitNormal = -FVector(ReplicatedInitialVelocity).GetSafeNormal();
	}
	if (HitNormal.IsNearlyZero())
	{
		HitNormal = FVector::UpVector;
	}
	ResolveImpact(OtherActor, HitLocation, HitNormal, bDamagedTarget, false);
}

bool ASovVelkorransHungerProjectile::ApplyDirectHit(
	UAbilitySystemComponent* TargetAbilitySystem,
	const FVector& HitLocation)
{
	UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get();
	AActor* SourceActor = SourceAvatar.Get();
	if (!IsValid(SourceASC)
		|| !IsValid(SourceActor)
		|| !IsValid(TargetAbilitySystem)
		|| !DirectDamageEffectClass.Get()
		|| !IsHostileTarget(TargetAbilitySystem)
		|| !IsTargetAlive(TargetAbilitySystem))
	{
		return false;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(SourceActor, this);
	Context.AddSourceObject(DamageSourceObject.IsValid() ? DamageSourceObject.Get() : this);
	Context.AddOrigin(HitLocation);

	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
		DirectDamageEffectClass,
		EffectLevel,
		Context);
	FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get();
	if (!DamageSpec)
	{
		return false;
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	DamageSpec->AddDynamicAssetTag(AbilityIdentityTag);
	DamageSpec->AddDynamicAssetTag(SovTags.Damage_Channel_Edge);
	DamageSpec->AddDynamicAssetTag(SovTags.Damage_Channel_Thermal);
	DamageSpec->AddDynamicAssetTag(SovTags.Damage_GuardClass_Standard);
	DamageSpec->SetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Damage,
		DirectDamage);
	if (DirectPoiseDamage > KINDA_SMALL_NUMBER)
	{
		DamageSpec->SetSetByCallerMagnitude(
			SovTags.SetByCaller_Damage_PoiseDamage,
			DirectPoiseDamage);
	}

	const float OldShield = TargetAbilitySystem->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetShieldAttribute());
	const float OldHealth = TargetAbilitySystem->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetHealthAttribute());
	const float OldPoise = TargetAbilitySystem->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetPoiseAttribute());

	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetAbilitySystem);

	const bool bDamageResolved =
		TargetAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetShieldAttribute())
			< OldShield - KINDA_SMALL_NUMBER
		|| TargetAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetHealthAttribute())
			< OldHealth - KINDA_SMALL_NUMBER
		|| TargetAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetPoiseAttribute())
			< OldPoise - KINDA_SMALL_NUMBER;
	if (bDamageResolved && IsTargetAlive(TargetAbilitySystem))
	{
		ApplyBurn(TargetAbilitySystem, Context);
	}
	return bDamageResolved;
}

void ASovVelkorransHungerProjectile::ApplyBurn(
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
	BurnSpec->AddDynamicAssetTag(SovTags.Damage_BypassDeflection);
	BurnSpec->SetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Damage,
		BurnDamagePerTick);
	BurnSpec->SetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Duration,
		BurnDuration);
	SourceASC->ApplyGameplayEffectSpecToTarget(*BurnSpec, TargetAbilitySystem);
}

bool ASovVelkorransHungerProjectile::IsHostileTarget(
	const UAbilitySystemComponent* TargetAbilitySystem) const
{
	const UAbilitySystemComponent* SourceASC = SourceAbilitySystem.Get();
	AActor* SourceActor = SourceAvatar.Get();
	AActor* TargetActor = IsValid(TargetAbilitySystem)
		? TargetAbilitySystem->GetAvatarActor()
		: nullptr;
	const INarrativeTeamAgentInterface* SourceTeam =
		Cast<const INarrativeTeamAgentInterface>(SourceActor);
	return IsValid(SourceASC)
		&& IsValid(SourceActor)
		&& IsValid(TargetAbilitySystem)
		&& IsValid(TargetActor)
		&& TargetAbilitySystem != SourceASC
		&& TargetAbilitySystem->GetSet<UNarrativeAttributeSetBase>()
		&& SourceTeam
		&& SourceTeam->GetTeamAttitudeTowards(*TargetActor) == ETeamAttitude::Hostile;
}

bool ASovVelkorransHungerProjectile::IsTargetAlive(
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

void ASovVelkorransHungerProjectile::ExpireProjectile()
{
	if (!HasAuthority() || bHasResolvedImpact || bResolvingImpact)
	{
		return;
	}

	bResolvingImpact = true;
	DeactivateProjectile();
	FVector DissipationNormal = -FVector(ReplicatedInitialVelocity).GetSafeNormal();
	if (DissipationNormal.IsNearlyZero())
	{
		DissipationNormal = FVector::UpVector;
	}
	ResolveImpact(
		nullptr,
		GetActorLocation(),
		DissipationNormal,
		false,
		true);
}

void ASovVelkorransHungerProjectile::ResolveImpact(
	AActor* HitActor,
	const FVector& InResolvedLocation,
	const FVector& InResolvedNormal,
	const bool bDamagedTarget,
	const bool bInExpired)
{
	if (!HasAuthority() || bHasResolvedImpact)
	{
		return;
	}

	ResolvedActor = HitActor;
	ResolvedLocation = InResolvedLocation;
	ResolvedNormal = InResolvedNormal.GetSafeNormal();
	if (FVector(ResolvedNormal).IsNearlyZero())
	{
		ResolvedNormal = FVector::UpVector;
	}
	bResolvedDamage = bDamagedTarget;
	bExpired = bInExpired;
	bHasResolvedImpact = true;
	SetActorLocation(ResolvedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	DeactivateProjectile();
	PlayResolutionPresentation();
	ForceNetUpdate();
	SetLifeSpan(FMath::Max(ResolutionCleanupDelay, 0.1f));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightTimerHandle);
	}
}

void ASovVelkorransHungerProjectile::StartProjectileMovement()
{
	if (!IsValid(ProjectileMovement)
		|| bHasResolvedImpact
		|| FVector(ReplicatedInitialVelocity).IsNearlyZero())
	{
		return;
	}

	ProjectileMovement->ProjectileGravityScale = FMath::Max(ReplicatedGravityScale, 0.0f);
	ProjectileMovement->Velocity = ReplicatedInitialVelocity;
	ProjectileMovement->InitialSpeed = FVector(ReplicatedInitialVelocity).Size();
	ProjectileMovement->MaxSpeed = FVector(ReplicatedInitialVelocity).Size();
	ProjectileMovement->Activate(true);
}

void ASovVelkorransHungerProjectile::DeactivateProjectile()
{
	bCollisionArmed = false;
	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
	if (IsValid(CollisionSphere))
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (IsValid(ProjectileMesh))
	{
		ProjectileMesh->SetVisibility(false, true);
	}
}

void ASovVelkorransHungerProjectile::OnRep_HasResolvedImpact()
{
	if (!HasActorBegunPlay() || !bHasResolvedImpact)
	{
		return;
	}

	SetActorLocation(ResolvedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	DeactivateProjectile();
	PlayResolutionPresentation();
}

void ASovVelkorransHungerProjectile::OnRep_InitialVelocity()
{
	if (HasActorBegunPlay() && !bHasResolvedImpact)
	{
		StartProjectileMovement();
	}
}

void ASovVelkorransHungerProjectile::PlayResolutionPresentation()
{
	if (bPlayedResolutionPresentation || !bHasResolvedImpact)
	{
		return;
	}
	bPlayedResolutionPresentation = true;

	UNiagaraSystem* ResolutionSystem = bExpired
		? DissipationNiagaraSystem.Get()
		: ImpactNiagaraSystem.Get();
	const FVector ResolutionScale = bExpired
		? DissipationNiagaraScale
		: ImpactNiagaraScale;
	UWorld* World = GetWorld();
	if (IsValid(World)
		&& World->GetNetMode() != NM_DedicatedServer
		&& IsValid(ResolutionSystem))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ResolutionSystem,
			ResolvedLocation,
			FVector(ResolvedNormal).Rotation(),
			ResolutionScale,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}

	if (bExpired)
	{
		ReceiveHungerProjectileDissipated(ResolvedLocation);
	}
	else
	{
		ReceiveHungerProjectileImpacted(
			ResolvedActor,
			ResolvedLocation,
			ResolvedNormal,
			bResolvedDamage);
	}
}

void ASovVelkorransHungerProjectile::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASovVelkorransHungerProjectile, ResolvedActor);
	DOREPLIFETIME(ASovVelkorransHungerProjectile, ResolvedLocation);
	DOREPLIFETIME(ASovVelkorransHungerProjectile, ResolvedNormal);
	DOREPLIFETIME(ASovVelkorransHungerProjectile, bResolvedDamage);
	DOREPLIFETIME(ASovVelkorransHungerProjectile, bExpired);
	DOREPLIFETIME(ASovVelkorransHungerProjectile, bHasResolvedImpact);
	DOREPLIFETIME(ASovVelkorransHungerProjectile, ReplicatedInitialVelocity);
	DOREPLIFETIME(ASovVelkorransHungerProjectile, ReplicatedGravityScale);
}
