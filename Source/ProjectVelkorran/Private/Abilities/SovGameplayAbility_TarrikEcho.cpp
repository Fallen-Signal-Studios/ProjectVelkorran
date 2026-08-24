// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_TarrikEcho.h"

#include "Abilities/GameplayAbility.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Effects/SovGameplayEffect_CinderGrenade.h"
#include "Effects/SovGameplayEffect_VelkorransHunger.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeGameplayTags.h"
#include "Projectiles/SovCinderStickyGrenadeProjectile.h"
#include "Projectiles/SovVelkorransHungerProjectile.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Weapons/WeaponVisual.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovTarrikEchoAbility, Log, All);

namespace
{
	bool CalculateBallisticLaunchVelocity(
		const FVector& Start,
		const FVector& Target,
		const float LaunchSpeed,
		const float GravityZ,
		const bool bUseHighArc,
		FVector& OutVelocity)
	{
		OutVelocity = FVector::ZeroVector;
		if (Start.ContainsNaN()
			|| Target.ContainsNaN()
			|| !FMath::IsFinite(LaunchSpeed)
			|| !FMath::IsFinite(GravityZ)
			|| LaunchSpeed <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const FVector Delta = Target - Start;
		if (Delta.IsNearlyZero())
		{
			return false;
		}

		const float GravityMagnitude = FMath::Abs(GravityZ);
		if (GravityMagnitude <= KINDA_SMALL_NUMBER)
		{
			OutVelocity = Delta.GetSafeNormal() * LaunchSpeed;
			return true;
		}

		const FVector HorizontalDelta(Delta.X, Delta.Y, 0.0f);
		const float HorizontalDistance = HorizontalDelta.Size();
		const double SpeedSquared = static_cast<double>(LaunchSpeed) * LaunchSpeed;
		if (HorizontalDistance <= KINDA_SMALL_NUMBER)
		{
			const double MaximumRise = SpeedSquared / (2.0 * GravityMagnitude);
			if (Delta.Z > MaximumRise)
			{
				return false;
			}
			OutVelocity = FVector::UpVector * FMath::Sign(Delta.Z) * LaunchSpeed;
			return !OutVelocity.IsNearlyZero();
		}

		const double HorizontalDistanceSquared =
			static_cast<double>(HorizontalDistance) * HorizontalDistance;
		const double Discriminant = (SpeedSquared * SpeedSquared)
			- (GravityMagnitude
				* ((GravityMagnitude * HorizontalDistanceSquared)
					+ (2.0 * Delta.Z * SpeedSquared)));
		if (Discriminant < 0.0)
		{
			return false;
		}

		const double Root = FMath::Sqrt(Discriminant);
		const double Tangent = (SpeedSquared + (bUseHighArc ? Root : -Root))
			/ (GravityMagnitude * HorizontalDistance);
		const double Cosine = 1.0 / FMath::Sqrt(1.0 + (Tangent * Tangent));
		const double Sine = Tangent * Cosine;
		OutVelocity = (HorizontalDelta.GetSafeNormal() * LaunchSpeed * Cosine)
			+ (FVector::UpVector * LaunchSpeed * Sine);
		return !OutVelocity.ContainsNaN() && !OutVelocity.IsNearlyZero();
	}
}

USovGameplayAbility_TarrikEchoBase::USovGameplayAbility_TarrikEchoBase()
{
	RequiredCharacterTag = FSovGameplayTags::Get().Character_Player_Tarrik;
}

USovGameplayAbility_TarrikCinderSlam::USovGameplayAbility_TarrikCinderSlam()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 90.0f;
	EchoCost = 90.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderSlam;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderSlam;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability3;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Velkorran;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderSlamName", "Cinder Slam");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"CinderSlamDescription",
		"Drive Velkorran into the ground, releasing a radial Cinder blast with heavy Poise pressure and a brief protective ward around Tarrik.");
}

bool USovGameplayAbility_TarrikCinderSlam::HasRequiredPayloadConfiguration() const
{
	return RadialDamageEffectClass.Get()
		&& ProtectiveWardEffectClass.Get()
		&& SlamRadius > KINDA_SMALL_NUMBER;
}

USovGameplayAbility_TarrikVelkorransHunger::USovGameplayAbility_TarrikVelkorransHunger()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	ProjectileClass = ASovVelkorransHungerProjectile::StaticClass();
	DirectDamageEffectClass =
		USovGameplayEffect_VelkorransHungerDamage::StaticClass();
	// Reuse the existing native Burn definition so Cinder Grenade and Hunger
	// refresh the same source-owned status instead of stacking parallel Burns.
	BurnEffectClass = USovGameplayEffect_CinderGrenadeBurn::StaticClass();
	MinimumEchoRequired = 50.0f;
	EchoCost = 50.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_VelkorransHunger;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_VelkorransHunger;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Velkorran;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "VelkorransHungerName", "Velkorran's Hunger");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"VelkorransHungerDescription",
		"Sling an inferno from Velkorran's edge, dealing direct projectile damage and applying Burn.");
}

void USovGameplayAbility_TarrikVelkorransHunger::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Super synchronously enters the Blueprint hooks, so reset before it runs.
	bHungerReleaseAttempted = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool USovGameplayAbility_TarrikVelkorransHunger::HasRequiredPayloadConfiguration() const
{
	return ResolveHungerProjectileClass().Get()
		&& ResolveHungerDamageEffectClass().Get()
		&& ResolveHungerBurnEffectClass().Get()
		&& DirectDamage > KINDA_SMALL_NUMBER
		&& DirectPoiseDamage >= 0.0f
		&& BurnDamagePerTick > KINDA_SMALL_NUMBER
		&& BurnDuration > KINDA_SMALL_NUMBER
		&& !HungerFallbackSpawnOffset.ContainsNaN()
		&& HungerAimTraceDistance > KINDA_SMALL_NUMBER
		&& HungerProjectileSpeed > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(HungerProjectileGravityScale)
		&& HungerProjectileGravityScale >= 0.0f
		&& MaximumHungerProjectileSpeed + KINDA_SMALL_NUMBER >= HungerProjectileSpeed
		&& HungerProjectileCollisionRadius >= 1.0f
		&& HungerProjectileFlightDuration >= 0.1f
		&& MaximumHungerSpawnDistance > KINDA_SMALL_NUMBER;
}

TSubclassOf<ASovVelkorransHungerProjectile>
USovGameplayAbility_TarrikVelkorransHunger::ResolveHungerProjectileClass() const
{
	if (ProjectileClass.Get())
	{
		return ProjectileClass;
	}
	return ASovVelkorransHungerProjectile::StaticClass();
}

TSubclassOf<UGameplayEffect>
USovGameplayAbility_TarrikVelkorransHunger::ResolveHungerDamageEffectClass() const
{
	if (DirectDamageEffectClass.Get())
	{
		return DirectDamageEffectClass;
	}
	return USovGameplayEffect_VelkorransHungerDamage::StaticClass();
}

TSubclassOf<UGameplayEffect>
USovGameplayAbility_TarrikVelkorransHunger::ResolveHungerBurnEffectClass() const
{
	if (BurnEffectClass.Get())
	{
		return BurnEffectClass;
	}
	return USovGameplayEffect_CinderGrenadeBurn::StaticClass();
}

ASovVelkorransHungerProjectile*
USovGameplayAbility_TarrikVelkorransHunger::ReleaseVelkorransHungerFromAim()
{
	if (!IsActive()
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| bHungerReleaseAttempted)
	{
		return nullptr;
	}

	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	if (!HasRequiredPayloadConfiguration() || !IsValid(Avatar) || !IsValid(World))
	{
		bHungerReleaseAttempted = true;
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("Velkorran's Hunger could not resolve its authoritative release context."));
		FinishEchoAbility(true);
		return nullptr;
	}

	FVector AimStart = Avatar->GetActorLocation();
	FRotator AimRotation = Avatar->GetActorRotation();
	Avatar->GetActorEyesViewPoint(AimStart, AimRotation);
	if (AController* Controller = GetOwningController())
	{
		AimRotation = Controller->GetControlRotation();
	}

	const FVector AimEnd = AimStart
		+ (AimRotation.Vector() * HungerAimTraceDistance);
	const TArray<FHitResult> AimHits = PerformTraceMulti(AimStart, AimEnd, 0.0f);
	const FHitResult* BlockingAimHit = AimHits.FindByPredicate(
		[](const FHitResult& Hit)
		{
			return Hit.bBlockingHit;
		});
	const FVector AimTarget = BlockingAimHit ? BlockingAimHit->ImpactPoint : AimEnd;

	FTransform SpawnTransform = FTransform::Identity;
	bool bResolvedWeaponSocket = false;
	if (ANarrativeCharacter* NarrativeCharacter = Cast<ANarrativeCharacter>(Avatar))
	{
		if (AWeaponVisual* WeaponVisual =
			NarrativeCharacter->GetWieldedWeaponVisual(true))
		{
			USkeletalMeshComponent* WeaponMesh = WeaponVisual->WeaponMesh;
			if (!IsValid(WeaponMesh))
			{
				WeaponMesh = WeaponVisual->GetRelevantWeaponMesh();
			}
			if (IsValid(WeaponMesh))
			{
				if (!HungerReleaseSocketName.IsNone()
					&& WeaponMesh->DoesSocketExist(HungerReleaseSocketName))
				{
					SpawnTransform = WeaponMesh->GetSocketTransform(
						HungerReleaseSocketName,
						RTS_World);
					bResolvedWeaponSocket = true;
				}
			}
		}
	}

	if (!bResolvedWeaponSocket)
	{
		SpawnTransform.SetLocation(
			Avatar->GetActorTransform().TransformPositionNoScale(
				HungerFallbackSpawnOffset));
	}

	FVector LaunchDirection = (AimTarget - SpawnTransform.GetLocation()).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{
		LaunchDirection = AimRotation.Vector().GetSafeNormal();
	}
	SpawnTransform.SetRotation(LaunchDirection.ToOrientationQuat());
	SpawnTransform.SetScale3D(FVector::OneVector);
	const FVector InitialVelocity = LaunchDirection * HungerProjectileSpeed;
	return ReleaseVelkorransHunger(SpawnTransform, InitialVelocity);
}

ASovVelkorransHungerProjectile*
USovGameplayAbility_TarrikVelkorransHunger::ReleaseVelkorransHunger(
	const FTransform& SpawnTransform,
	FVector InitialVelocity)
{
	if (!IsActive()
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| bHungerReleaseAttempted)
	{
		return nullptr;
	}

	bHungerReleaseAttempted = true;
	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	if (!HasRequiredPayloadConfiguration()
		|| !IsValid(Avatar)
		|| !IsValid(World)
		|| SpawnTransform.ContainsNaN()
		|| InitialVelocity.ContainsNaN())
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("%s could not release Velkorran's Hunger because its server payload or launch data was invalid."),
			*GetNameSafe(Avatar));
		FinishEchoAbility(true);
		return nullptr;
	}

	if (FVector::DistSquared(Avatar->GetActorLocation(), SpawnTransform.GetLocation())
		> FMath::Square(MaximumHungerSpawnDistance))
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Warning,
			TEXT("%s rejected a Velkorran's Hunger release %.1f cm from its avatar."),
			*GetNameSafe(Avatar),
			FVector::Distance(Avatar->GetActorLocation(), SpawnTransform.GetLocation()));
		FinishEchoAbility(true);
		return nullptr;
	}

	FTransform ServerSpawnTransform = SpawnTransform;
	ServerSpawnTransform.NormalizeRotation();
	ServerSpawnTransform.SetScale3D(FVector::OneVector);
	if (InitialVelocity.IsNearlyZero())
	{
		InitialVelocity = ServerSpawnTransform.GetRotation().GetForwardVector()
			* HungerProjectileSpeed;
	}
	InitialVelocity = InitialVelocity.GetClampedToMaxSize(
		MaximumHungerProjectileSpeed);
	if (InitialVelocity.IsNearlyZero())
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("%s produced a zero launch velocity for Velkorran's Hunger."),
			*GetNameSafe(Avatar));
		FinishEchoAbility(true);
		return nullptr;
	}

	ASovVelkorransHungerProjectile* HungerProjectile =
		World->SpawnActorDeferred<ASovVelkorransHungerProjectile>(
			ResolveHungerProjectileClass(),
			ServerSpawnTransform,
			Avatar,
			Cast<APawn>(Avatar),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(HungerProjectile))
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("%s failed to spawn the configured Velkorran's Hunger projectile class."),
			*GetNameSafe(Avatar));
		FinishEchoAbility(true);
		return nullptr;
	}

	UObject* DamageSourceObject = GetCurrentSourceObject();
	if (IsValid(DamageSourceObject) && DamageSourceObject->IsA<UGameplayAbility>())
	{
		DamageSourceObject = nullptr;
	}
	HungerProjectile->InitializeHungerProjectile(
		CurrentActorInfo->AbilitySystemComponent.Get(),
		Avatar,
		DamageSourceObject,
		ResolveHungerDamageEffectClass(),
		ResolveHungerBurnEffectClass(),
		EchoSpendTag,
		static_cast<float>(GetAbilityLevel()),
		InitialVelocity,
		HungerProjectileGravityScale,
		HungerProjectileCollisionRadius,
		HungerProjectileFlightDuration,
		DirectDamage,
		DirectPoiseDamage,
		BurnDamagePerTick,
		BurnDuration);
	UGameplayStatics::FinishSpawningActor(HungerProjectile, ServerSpawnTransform);

	if (!IsValid(HungerProjectile) || HungerProjectile->IsActorBeingDestroyed())
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("%s created a Velkorran's Hunger projectile that failed authoritative initialization."),
			*GetNameSafe(Avatar));
		FinishEchoAbility(true);
		return nullptr;
	}

	return HungerProjectile;
}

USovGameplayAbility_TarrikCinderStickyGrenade::USovGameplayAbility_TarrikCinderStickyGrenade()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	GrenadeClass = ASovCinderStickyGrenadeProjectile::StaticClass();
	ExplosionDamageEffectClass =
		USovGameplayEffect_CinderGrenadeExplosionDamage::StaticClass();
	BurnEffectClass = USovGameplayEffect_CinderGrenadeBurn::StaticClass();
	MinimumEchoRequired = 35.0f;
	EchoCost = 35.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderStickyGrenade;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderStickyGrenade;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability1;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Universal;
	// The grenade is granted once by Tarrik's character configuration and works
	// with either loadout. It must not become unusable when a Blueprint compile,
	// reparent, or weapon-asset migration clears AllowedWeaponClasses.
	bRequiresAllowedWeapon = false;
	WeaponGatePolicy = ESovEchoWeaponGatePolicy::AnyAllowedWielded;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderStickyGrenadeName", "Cinder Sticky Grenade");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"CinderStickyGrenadeDescription",
		"Throw a Cinder charge that adheres to a target or surface, then detonates after a short fuse with thermal damage and Burn.");
}

void USovGameplayAbility_TarrikCinderStickyGrenade::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Super synchronously enters the Blueprint hooks, so reset before it runs.
	bGrenadeReleaseAttempted = false;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool USovGameplayAbility_TarrikCinderStickyGrenade::HasRequiredPayloadConfiguration() const
{
	return ResolveGrenadeClass().Get()
		&& ResolveExplosionDamageEffectClass().Get()
		&& ResolveBurnEffectClass().Get()
		&& FuseDuration > KINDA_SMALL_NUMBER
		&& ExplosionRadius > KINDA_SMALL_NUMBER
		&& ExplosionDamage > KINDA_SMALL_NUMBER
		&& ExplosionPoiseDamage >= 0.0f
		&& MinimumExplosionDamageFraction >= 0.0f
		&& MinimumExplosionDamageFraction <= 1.0f
		&& BurnDamagePerTick > KINDA_SMALL_NUMBER
		&& BurnDuration > KINDA_SMALL_NUMBER
		&& !GrenadeFallbackSpawnOffset.ContainsNaN()
		&& GrenadeAimTraceDistance > KINDA_SMALL_NUMBER
		&& DefaultGrenadeLaunchSpeed > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(GrenadeFallbackThrowPitch)
		&& GrenadeFallbackThrowPitch >= -89.0f
		&& GrenadeFallbackThrowPitch <= 89.0f
		&& FMath::IsFinite(GrenadeGravityScale)
		&& GrenadeGravityScale >= 0.0f
		&& MaximumGrenadeLaunchSpeed + KINDA_SMALL_NUMBER >= DefaultGrenadeLaunchSpeed
		&& MaximumGrenadeSpawnDistance > KINDA_SMALL_NUMBER;
}

bool USovGameplayAbility_TarrikCinderStickyGrenade::MeetsWeaponRequirement(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	static_cast<void>(Handle);
	static_cast<void>(ActorInfo);
	return true;
}

TSubclassOf<ASovCinderStickyGrenadeProjectile>
USovGameplayAbility_TarrikCinderStickyGrenade::ResolveGrenadeClass() const
{
	if (GrenadeClass.Get())
	{
		return GrenadeClass;
	}
	return ASovCinderStickyGrenadeProjectile::StaticClass();
}

TSubclassOf<UGameplayEffect>
USovGameplayAbility_TarrikCinderStickyGrenade::ResolveExplosionDamageEffectClass() const
{
	if (ExplosionDamageEffectClass.Get())
	{
		return ExplosionDamageEffectClass;
	}
	return USovGameplayEffect_CinderGrenadeExplosionDamage::StaticClass();
}

TSubclassOf<UGameplayEffect>
USovGameplayAbility_TarrikCinderStickyGrenade::ResolveBurnEffectClass() const
{
	if (BurnEffectClass.Get())
	{
		return BurnEffectClass;
	}
	return USovGameplayEffect_CinderGrenadeBurn::StaticClass();
}

ASovCinderStickyGrenadeProjectile*
USovGameplayAbility_TarrikCinderStickyGrenade::ReleaseCinderStickyGrenadeFromAim()
{
	if (!IsActive()
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| bGrenadeReleaseAttempted)
	{
		return nullptr;
	}

	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	if (!HasRequiredPayloadConfiguration() || !IsValid(Avatar) || !IsValid(World))
	{
		bGrenadeReleaseAttempted = true;
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("Cinder Sticky Grenade could not resolve its authoritative throw context."));
		FinishEchoAbility(true);
		return nullptr;
	}

	FVector AimStart = Avatar->GetActorLocation();
	FRotator AimRotation = Avatar->GetActorRotation();
	Avatar->GetActorEyesViewPoint(AimStart, AimRotation);
	if (AController* Controller = GetOwningController())
	{
		AimRotation = Controller->GetControlRotation();
	}

	const FVector AimEnd = AimStart
		+ (AimRotation.Vector() * GrenadeAimTraceDistance);
	const TArray<FHitResult> AimHits = PerformTraceMulti(AimStart, AimEnd, 0.0f);
	const FHitResult* BlockingAimHit = AimHits.FindByPredicate(
		[](const FHitResult& Hit)
		{
			return Hit.bBlockingHit;
		});
	const FVector AimTarget = BlockingAimHit ? BlockingAimHit->ImpactPoint : AimEnd;

	FVector SpawnLocation = Avatar->GetActorTransform().TransformPositionNoScale(
		GrenadeFallbackSpawnOffset);
	if (const ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		const USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
		if (IsValid(CharacterMesh)
			&& !GrenadeThrowSocketName.IsNone()
			&& CharacterMesh->DoesSocketExist(GrenadeThrowSocketName))
		{
			SpawnLocation = CharacterMesh->GetSocketLocation(GrenadeThrowSocketName);
		}
	}

	FVector InitialVelocity;
	const float GravityZ = World->GetGravityZ() * GrenadeGravityScale;
	if (!CalculateBallisticLaunchVelocity(
			SpawnLocation,
			AimTarget,
			DefaultGrenadeLaunchSpeed,
			GravityZ,
			bUseHighGrenadeThrowArc,
			InitialVelocity))
	{
		const FRotator FallbackRotation(
			FMath::Clamp(GrenadeFallbackThrowPitch, -89.0f, 89.0f),
			AimRotation.Yaw,
			0.0f);
		InitialVelocity = FallbackRotation.Vector() * DefaultGrenadeLaunchSpeed;
	}

	const FTransform SpawnTransform(
		InitialVelocity.ToOrientationQuat(),
		SpawnLocation,
		FVector::OneVector);
	return ReleaseCinderStickyGrenade(SpawnTransform, InitialVelocity);
}

ASovCinderStickyGrenadeProjectile*
USovGameplayAbility_TarrikCinderStickyGrenade::ReleaseCinderStickyGrenade(
	const FTransform& SpawnTransform,
	FVector InitialVelocity)
{
	if (!IsActive()
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| bGrenadeReleaseAttempted)
	{
		return nullptr;
	}

	bGrenadeReleaseAttempted = true;
	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	UWorld* World = GetWorld();
	if (!HasRequiredPayloadConfiguration()
		|| !IsValid(Avatar)
		|| !IsValid(World)
		|| SpawnTransform.ContainsNaN()
		|| InitialVelocity.ContainsNaN())
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("%s could not release Cinder Sticky Grenade because its server payload or launch data was invalid."),
			*GetNameSafe(Avatar));
		FinishEchoAbility(true);
		return nullptr;
	}

	if (FVector::DistSquared(Avatar->GetActorLocation(), SpawnTransform.GetLocation())
		> FMath::Square(MaximumGrenadeSpawnDistance))
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Warning,
			TEXT("%s rejected a Cinder Sticky Grenade release %.1f cm from its avatar."),
			*GetNameSafe(Avatar),
			FVector::Distance(Avatar->GetActorLocation(), SpawnTransform.GetLocation()));
		FinishEchoAbility(true);
		return nullptr;
	}

	FTransform ServerSpawnTransform = SpawnTransform;
	ServerSpawnTransform.NormalizeRotation();
	ServerSpawnTransform.SetScale3D(FVector::OneVector);
	if (InitialVelocity.IsNearlyZero())
	{
		InitialVelocity = ServerSpawnTransform.GetRotation().GetForwardVector()
			* DefaultGrenadeLaunchSpeed;
	}
	InitialVelocity = InitialVelocity.GetClampedToMaxSize(MaximumGrenadeLaunchSpeed);

	ASovCinderStickyGrenadeProjectile* Grenade =
		World->SpawnActorDeferred<ASovCinderStickyGrenadeProjectile>(
			ResolveGrenadeClass(),
			ServerSpawnTransform,
			Avatar,
			Cast<APawn>(Avatar),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Grenade))
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("%s failed to spawn the configured Cinder Sticky Grenade class."),
			*GetNameSafe(Avatar));
		FinishEchoAbility(true);
		return nullptr;
	}

	UObject* DamageSourceObject = GetCurrentSourceObject();
	if (IsValid(DamageSourceObject) && DamageSourceObject->IsA<UGameplayAbility>())
	{
		DamageSourceObject = nullptr;
	}
	Grenade->InitializeGrenade(
		CurrentActorInfo->AbilitySystemComponent.Get(),
		Avatar,
		DamageSourceObject,
		ResolveExplosionDamageEffectClass(),
		ResolveBurnEffectClass(),
		EchoSpendTag,
		static_cast<float>(GetAbilityLevel()),
		InitialVelocity,
		GrenadeGravityScale,
		FuseDuration,
		ExplosionRadius,
		ExplosionDamage,
		ExplosionPoiseDamage,
		MinimumExplosionDamageFraction,
		BurnDamagePerTick,
		BurnDuration,
		bExplosionRequiresLineOfSight);
	UGameplayStatics::FinishSpawningActor(Grenade, ServerSpawnTransform);

	if (!IsValid(Grenade) || Grenade->IsActorBeingDestroyed())
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("%s created a Cinder Sticky Grenade that failed authoritative initialization."),
			*GetNameSafe(Avatar));
		FinishEchoAbility(true);
		return nullptr;
	}

	return Grenade;
}

USovGameplayAbility_TarrikCinderJudgement::USovGameplayAbility_TarrikCinderJudgement()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 50.0f;
	EchoCost = 50.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderJudgement;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderJudgement;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Cinderline;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderJudgementName", "Cinder Judgement");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"CinderJudgementDescription",
		"Fire an overcharged Cinderline shot that deals direct impact damage and detonates in a controlled Cinder blast.");
}

bool USovGameplayAbility_TarrikCinderJudgement::HasRequiredPayloadConfiguration() const
{
	return DirectDamageEffectClass.Get()
		&& ExplosionDamageEffectClass.Get()
		&& MaximumRange > KINDA_SMALL_NUMBER
		&& ExplosionRadius > KINDA_SMALL_NUMBER;
}

USovGameplayAbility_TarrikCinderlineRequiem::USovGameplayAbility_TarrikCinderlineRequiem()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 90.0f;
	EchoCost = 90.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderlineRequiem;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderlineRequiem;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability3;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Cinderline;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderlineRequiemName", "Cinderline Requiem");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"CinderlineRequiemDescription",
		"Release a penetrating Cinderline shot that marks its path, then erupts into a chained burning line with extreme Poise pressure.");
}

bool USovGameplayAbility_TarrikCinderlineRequiem::HasRequiredPayloadConfiguration() const
{
	return PenetratingDamageEffectClass.Get()
		&& LineDetonationEffectClass.Get()
		&& BurnEffectClass.Get()
		&& MaximumRange > KINDA_SMALL_NUMBER
		&& LineDetonationRadius > KINDA_SMALL_NUMBER;
}
