// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_TarrikEcho.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Effects/SovGameplayEffect_CinderJudgement.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Effects/SovGameplayEffect_CinderGrenade.h"
#include "Effects/SovGameplayEffect_VelkorransHunger.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeGameplayTags.h"
#include "Presentation/SovCinderJudgementPresentation.h"
#include "Projectiles/SovCinderStickyGrenadeProjectile.h"
#include "Projectiles/SovVelkorransHungerProjectile.h"
#include "Targeting/SovAimAssist.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Weapons/NarrativeProjectile.h"
#include "Weapons/WeaponVisual.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovTarrikEchoAbility, Log, All);

namespace
{
	/** Resolve Narrative's runtime CharacterVisual/attachment ownership chain. */
	UAbilitySystemComponent* ResolveTarrikTargetAbilitySystem(AActor* InActor)
	{
		AActor* Candidate = InActor;
		TSet<const AActor*> VisitedActors;
		for (int32 Depth = 0;
			IsValid(Candidate) && Depth < 6 && !VisitedActors.Contains(Candidate);
			++Depth)
		{
			VisitedActors.Add(Candidate);
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
				ANarrativeCharacter* Character =
					CharacterProvider->GetNarrativeCharacter();
				if (IsValid(Character) && Character != Candidate)
				{
					if (UAbilitySystemComponent* AbilitySystem =
						UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
							Character))
					{
						return AbilitySystem;
					}
				}
			}

			Candidate = Candidate->GetOwner();
		}
		return nullptr;
	}

	bool IsTarrikTargetAlive(
		const UAbilitySystemComponent* TargetAbilitySystem)
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

	bool IsHostileTarrikTarget(
		const UAbilitySystemComponent* SourceAbilitySystem,
		const AActor* SourceActor,
		const UAbilitySystemComponent* TargetAbilitySystem)
	{
		AActor* TargetActor = IsValid(TargetAbilitySystem)
			? TargetAbilitySystem->GetAvatarActor()
			: nullptr;
		const INarrativeTeamAgentInterface* SourceTeam =
			Cast<const INarrativeTeamAgentInterface>(SourceActor);
		return IsValid(SourceAbilitySystem)
			&& IsValid(SourceActor)
			&& IsValid(TargetAbilitySystem)
			&& IsValid(TargetActor)
			&& TargetAbilitySystem != SourceAbilitySystem
			&& TargetAbilitySystem->GetSet<UNarrativeAttributeSetBase>()
			&& SourceTeam
			&& SourceTeam->GetTeamAttitudeTowards(*TargetActor)
				== ETeamAttitude::Hostile;
	}

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
		const FVector AgainstGravity = GravityZ > 0.f ? FVector::DownVector : FVector::UpVector;
		const double HeightAgainstGravity = FVector::DotProduct(Delta, AgainstGravity);
		const float HorizontalDistance = HorizontalDelta.Size();
		const double SpeedSquared = static_cast<double>(LaunchSpeed) * LaunchSpeed;
		if (HorizontalDistance <= KINDA_SMALL_NUMBER)
		{
			const double MaximumRise = SpeedSquared / (2.0 * GravityMagnitude);
			if (HeightAgainstGravity > MaximumRise)
			{
				return false;
			}
			OutVelocity = AgainstGravity * FMath::Sign(HeightAgainstGravity) * LaunchSpeed;
			return !OutVelocity.IsNearlyZero();
		}

		const double HorizontalDistanceSquared =
			static_cast<double>(HorizontalDistance) * HorizontalDistance;
		const double Discriminant = (SpeedSquared * SpeedSquared)
			- (GravityMagnitude
				* ((GravityMagnitude * HorizontalDistanceSquared)
					+ (2.0 * HeightAgainstGravity * SpeedSquared)));
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
			+ (AgainstGravity * LaunchSpeed * Sine);
		return !OutVelocity.ContainsNaN() && !OutVelocity.IsNearlyZero();
	}
}

USovGameplayAbility_TarrikEchoBase::USovGameplayAbility_TarrikEchoBase()
{
	RequiredCharacterTag = FSovGameplayTags::Get().Character_Player_Tarrik;
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
	const uint64 Activation = GetTarrikActivationSerial() + 1;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!bHungerReleaseAttempted) { ArmTarrikPayload(PayloadReleaseDelay, Activation); }
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
		&& MaximumHungerSpawnDistance > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(PayloadReleaseDelay) && PayloadReleaseDelay >= 0.0f
		&& FMath::IsFinite(PostReleaseRecovery) && PostReleaseRecovery >= 0.0f
		&& MaximumActiveDuration >= PayloadReleaseDelay + PostReleaseRecovery;
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
	const FVector AimTarget = BlockingAimHit
		? FVector(BlockingAimHit->ImpactPoint)
		: AimEnd;

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
	FVector InitialVelocity = LaunchDirection * HungerProjectileSpeed;
	const auto* ProjectileDefaults = ResolveHungerProjectileClass().GetDefaultObject();
	const auto* Sphere = ProjectileDefaults ? ProjectileDefaults->FindComponentByClass<USphereComponent>() : nullptr;
	const auto* Movement = ProjectileDefaults ? ProjectileDefaults->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
	if (Sphere && Movement && !Movement->bIsHomingProjectile)
	{
		SovAimAssist::FProjectileLeadRequest Request;
		Request.AimDirection = LaunchDirection;
		Request.InitialVelocity = InitialVelocity;
		Request.Gravity = FVector(0., 0., World->GetGravityZ() * HungerProjectileGravityScale);
		Request.Range = HungerAimTraceDistance;
		Request.MaximumFlightSeconds = HungerProjectileFlightDuration;
		Request.MaximumFlightSpeed = HungerProjectileSpeed;
		Request.CollisionRadius = Sphere->GetScaledSphereRadius();
		Request.CollisionChannel = Sphere->GetCollisionObjectType();
		Request.CollisionResponses = Sphere->GetCollisionResponseToChannels();
		SovAimAssist::GetBallisticProjectileLead(Avatar, SpawnTransform.GetLocation(), Request, InitialVelocity);
		LaunchDirection = InitialVelocity.GetSafeNormal();
	}
	SpawnTransform.SetRotation(LaunchDirection.ToOrientationQuat());
	SpawnTransform.SetScale3D(FVector::OneVector);
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

	if (!IsTarrikReleaseContextValid()) { FinishEchoAbility(true); return nullptr; }
	const uint64 Activation = GetTarrikActivationSerial();
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

	BeginTarrikRecovery(PostReleaseRecovery, Activation);
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
	const uint64 Activation = GetTarrikActivationSerial() + 1;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!bGrenadeReleaseAttempted) { ArmTarrikPayload(PayloadReleaseDelay, Activation); }
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
		&& MaximumGrenadeSpawnDistance > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(PayloadReleaseDelay) && PayloadReleaseDelay >= 0.0f
		&& FMath::IsFinite(PostReleaseRecovery) && PostReleaseRecovery >= 0.0f
		&& MaximumActiveDuration >= PayloadReleaseDelay + PostReleaseRecovery;
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
	const FVector AimTarget = BlockingAimHit
		? FVector(BlockingAimHit->ImpactPoint)
		: AimEnd;

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
	const auto* GrenadeDefaults = ResolveGrenadeClass().GetDefaultObject();
	const auto* Sphere = GrenadeDefaults ? GrenadeDefaults->FindComponentByClass<USphereComponent>() : nullptr;
	const auto* Movement = GrenadeDefaults ? GrenadeDefaults->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
	if (Sphere && Movement && !Movement->bIsHomingProjectile)
	{
		SovAimAssist::FProjectileLeadRequest Request;
		Request.AimDirection = (AimTarget - SpawnLocation).GetSafeNormal();
		Request.InitialVelocity = InitialVelocity;
		Request.Gravity = FVector(0., 0., GravityZ);
		Request.Range = GrenadeAimTraceDistance;
		Request.MaximumFlightSeconds = FuseDuration;
		Request.MaximumFlightSpeed = Movement->MaxSpeed;
		Request.bPreferHighArc = bUseHighGrenadeThrowArc;
		Request.CollisionRadius = Sphere->GetScaledSphereRadius();
		Request.CollisionChannel = Sphere->GetCollisionObjectType();
		Request.CollisionResponses = Sphere->GetCollisionResponseToChannels();
		SovAimAssist::GetBallisticProjectileLead(Avatar, SpawnLocation, Request, InitialVelocity);
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

	if (!IsTarrikReleaseContextValid()) { FinishEchoAbility(true); return nullptr; }
	const uint64 Activation = GetTarrikActivationSerial();
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

	BeginTarrikRecovery(PostReleaseRecovery, Activation);
	return Grenade;
}

/** One immutable paid payload, retained across callback-capable engine boundaries. */
struct USovGameplayAbility_TarrikCinderJudgement::FJudgementShotContext
{
	uint64 Activation;
	TStrongObjectPtr<UAbilitySystemComponent> SourceASC;
	TStrongObjectPtr<AActor> Avatar;
	TStrongObjectPtr<UWorld> World;
	TStrongObjectPtr<UObject> SourceObject;
	FGameplayEffectContextHandle Context;
	TSubclassOf<UGameplayEffect> DirectEffect, ExplosionEffect;
	TSubclassOf<ASovCinderJudgementPresentation> PresentationClass;
	FGameplayTag SpendTag;
	float Level, DirectDamage, DirectPoise, DirectShield, ExplosionDamage, ExplosionPoise, ExplosionShield;
	float Radius, MinimumFraction, ImpulseStrength, UpwardBias, Recovery;
	bool bRequiresLineOfSight, bApplyImpulse;

	explicit FJudgementShotContext(const USovGameplayAbility_TarrikCinderJudgement& Ability)
		: Activation(Ability.GetTarrikActivationSerial()),
		SourceASC(Ability.GetAbilitySystemComponentFromActorInfo()), Avatar(Ability.GetAvatarActorFromActorInfo()),
		World(Ability.GetWorld()), SourceObject(Ability.GetCurrentSourceObject()),
		DirectEffect(Ability.ResolveJudgementDirectEffectClass()), ExplosionEffect(Ability.ResolveJudgementExplosionEffectClass()),
		PresentationClass(Ability.PresentationClass), SpendTag(Ability.EchoSpendTag), Level(static_cast<float>(Ability.GetAbilityLevel())),
		DirectDamage(Ability.DirectDamage), DirectPoise(Ability.DirectPoiseDamage), DirectShield(Ability.DirectShieldCoefficient),
		ExplosionDamage(Ability.ExplosionDamage), ExplosionPoise(Ability.ExplosionPoiseDamage), ExplosionShield(Ability.ExplosionShieldCoefficient),
		Radius(Ability.ExplosionRadius), MinimumFraction(Ability.MinimumExplosionDamageFraction),
		ImpulseStrength(Ability.ExplosionPhysicsImpulseStrength), UpwardBias(Ability.ExplosionPhysicsUpwardBias),
		Recovery(Ability.PostReleaseRecovery), bRequiresLineOfSight(Ability.bExplosionRequiresLineOfSight),
		bApplyImpulse(Ability.bApplyExplosionPhysicsImpulse)
	{
		if (!SourceObject.IsValid() || SourceObject->IsA<UGameplayAbility>()) { SourceObject.Reset(Avatar.Get()); }
		Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(Avatar.Get(), Avatar.Get());
		Context.AddSourceObject(SourceObject.Get());
	}
};

bool USovGameplayAbility_TarrikCinderJudgement::IsJudgementShotCurrent(const FJudgementShotContext& Shot) const
{
	return GetTarrikActivationSerial() == Shot.Activation && IsTarrikReleaseContextValid()
		&& Shot.SourceASC.IsValid() && Shot.Avatar.IsValid() && Shot.World.IsValid()
		&& !Shot.Avatar->IsActorBeingDestroyed() && CurrentActorInfo->AbilitySystemComponent.Get() == Shot.SourceASC.Get()
		&& CurrentActorInfo->AvatarActor.Get() == Shot.Avatar.Get() && GetWorld() == Shot.World.Get()
		&& GetTarrikActivationSerial() == Shot.Activation && IsCurrentEchoExecutionValid();
}

USovGameplayAbility_TarrikCinderJudgement::USovGameplayAbility_TarrikCinderJudgement()
{
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	PresentationClass = ASovCinderJudgementPresentation::StaticClass();
	DirectDamageEffectClass =
		USovGameplayEffect_CinderJudgementDamage::StaticClass();
	ExplosionDamageEffectClass =
		USovGameplayEffect_CinderJudgementDamage::StaticClass();
	MinimumEchoRequired = 50.0f;
	EchoCost = 50.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderJudgement;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	AssetTags.AddTag(NarrativeTags.Ability_WeaponFire);
	AssetTags.AddTag(NarrativeTags.Ability_DamageType_Ranged);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderJudgement;
	InputTag = NarrativeTags.Narrative_Input_Ability2;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Cinderline;
	ActivationBlockedTags.AddTag(NarrativeTags.State_Weapon_BlockFiring);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Weapon_IsFiring);
	ActivationOwnedTags.AddTag(NarrativeTags.State_Weapon_IsFiring);
	MaximumActiveDuration = 3.0f;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderJudgementName", "Cinder Judgement");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"CinderJudgementDescription",
		"Fire an overcharged Cinderline shot that deals direct impact damage and detonates in a controlled Cinder blast.");
}

void USovGameplayAbility_TarrikCinderJudgement::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Super synchronously enters the Blueprint hooks, so reset first. A manual
	// authority release from that hook and the native timer share one gate.
	bJudgementReleaseAttempted = false;
	const uint64 Activation = GetTarrikActivationSerial() + 1;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()
		|| !ActorInfo
		|| !ActorInfo->IsNetAuthority()
		|| bJudgementReleaseAttempted
		|| GetTarrikActivationSerial() != Activation
		|| !bAutoReleasePayload)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		FinishEchoAbility(true);
		return;
	}
	const float ReleaseDelay = FMath::Max(PayloadReleaseDelay, 0.0f);
	if (ReleaseDelay <= KINDA_SMALL_NUMBER)
	{
		HandleAutomaticJudgementRelease();
		return;
	}
	World->GetTimerManager().SetTimer(
		JudgementReleaseTimerHandle,
		this,
		&ThisClass::HandleAutomaticJudgementRelease,
		ReleaseDelay,
		false);
}

void USovGameplayAbility_TarrikCinderJudgement::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JudgementReleaseTimerHandle);
		World->GetTimerManager().ClearTimer(JudgementRecoveryTimerHandle);
	}
	// Keep the exactly-once gate latched while the shared base invokes the
	// Blueprint Ended hook. The next activation resets it before any callbacks.
	bJudgementReleaseAttempted = true;
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

bool USovGameplayAbility_TarrikCinderJudgement::HasRequiredPayloadConfiguration() const
{
	return ResolveJudgementDirectEffectClass().Get()
		&& ResolveJudgementExplosionEffectClass().Get()
		&& FMath::IsFinite(MaximumRange)
		&& MaximumRange > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(TraceRadius)
		&& TraceRadius >= 0.0f
		&& FMath::IsFinite(ExplosionRadius)
		&& ExplosionRadius > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(DirectDamage)
		&& DirectDamage > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(DirectPoiseDamage)
		&& DirectPoiseDamage >= 0.0f
		&& FMath::IsFinite(DirectShieldCoefficient)
		&& DirectShieldCoefficient > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(ExplosionDamage)
		&& ExplosionDamage > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(ExplosionPoiseDamage)
		&& ExplosionPoiseDamage >= 0.0f
		&& FMath::IsFinite(ExplosionShieldCoefficient)
		&& ExplosionShieldCoefficient > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(MinimumExplosionDamageFraction)
		&& MinimumExplosionDamageFraction >= 0.0f
		&& MinimumExplosionDamageFraction <= 1.0f
		&& FMath::IsFinite(ExplosionPhysicsImpulseStrength)
		&& ExplosionPhysicsImpulseStrength >= 0.0f
		&& FMath::IsFinite(ExplosionPhysicsUpwardBias)
		&& ExplosionPhysicsUpwardBias >= 0.0f
		&& !FallbackMuzzleOffset.ContainsNaN()
		&& FMath::IsFinite(MaximumMuzzleDistance)
		&& MaximumMuzzleDistance > KINDA_SMALL_NUMBER
		&& FallbackMuzzleOffset.SizeSquared()
			<= FMath::Square(static_cast<double>(MaximumMuzzleDistance))
		&& FMath::IsFinite(PayloadReleaseDelay)
		&& PayloadReleaseDelay >= 0.0f
		&& FMath::IsFinite(PostReleaseRecovery)
		&& PostReleaseRecovery >= 0.0f
		&& MaximumActiveDuration + KINDA_SMALL_NUMBER >=
			((bAutoReleasePayload ? PayloadReleaseDelay : 0.0f)
				+ PostReleaseRecovery);
}

TSubclassOf<UGameplayEffect>
USovGameplayAbility_TarrikCinderJudgement::
	ResolveJudgementDirectEffectClass() const
{
	if (DirectDamageEffectClass.Get()
		&& DirectDamageEffectClass->IsChildOf(
			USovGameplayEffect_CinderJudgementDamage::StaticClass()))
	{
		return DirectDamageEffectClass;
	}
	return USovGameplayEffect_CinderJudgementDamage::StaticClass();
}

TSubclassOf<UGameplayEffect>
USovGameplayAbility_TarrikCinderJudgement::
	ResolveJudgementExplosionEffectClass() const
{
	if (ExplosionDamageEffectClass.Get()
		&& ExplosionDamageEffectClass->IsChildOf(
			USovGameplayEffect_CinderJudgementDamage::StaticClass()))
	{
		return ExplosionDamageEffectClass;
	}
	return USovGameplayEffect_CinderJudgementDamage::StaticClass();
}

void USovGameplayAbility_TarrikCinderJudgement::
	HandleAutomaticJudgementRelease()
{
	const uint64 Activation = GetTarrikActivationSerial();
	if (!ReleaseCinderJudgementFromAim() && IsActive() && GetTarrikActivationSerial() == Activation)
	{
		FinishEchoAbility(true);
	}
}

void USovGameplayAbility_TarrikCinderJudgement::
	HandleJudgementRecoveryFinished()
{
	FinishEchoAbility(false);
}

FTransform USovGameplayAbility_TarrikCinderJudgement::
	ResolveJudgementMuzzleTransform() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	FTransform MuzzleTransform = IsValid(Avatar)
		? Avatar->GetActorTransform()
		: FTransform::Identity;
	if (!IsValid(Avatar))
	{
		return MuzzleTransform;
	}

	if (AWeaponVisual* WeaponVisual = GetAbilityWeaponVisual())
	{
		USkeletalMeshComponent* WeaponMesh = WeaponVisual->WeaponMesh;
		if (!IsValid(WeaponMesh))
		{
			WeaponMesh = WeaponVisual->GetRelevantWeaponMesh();
		}
		if (IsValid(WeaponMesh)
			&& !MuzzleSocketName.IsNone()
			&& WeaponMesh->DoesSocketExist(MuzzleSocketName))
		{
			const FTransform SocketTransform = WeaponMesh->GetSocketTransform(
				MuzzleSocketName,
				RTS_World);
			if (!SocketTransform.ContainsNaN()
				&& SocketTransform.GetRotation().IsNormalized()
				&& FVector::DistSquared(
					Avatar->GetActorLocation(),
					SocketTransform.GetLocation())
					<= FMath::Square(static_cast<double>(MaximumMuzzleDistance)))
			{
				return SocketTransform;
			}
		}
	}

	MuzzleTransform.SetLocation(
		Avatar->GetActorTransform().TransformPositionNoScale(FallbackMuzzleOffset));
	MuzzleTransform.SetScale3D(FVector::OneVector);
	return MuzzleTransform;
}

bool USovGameplayAbility_TarrikCinderJudgement::
	ResolveJudgementAuthorityAimPoint(FVector& OutEye, FVector& OutAimPoint, FVector& OutForward)
{
	OutEye = OutAimPoint = OutForward = FVector::ZeroVector;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar))
	{
		return false;
	}

	FVector AimStart = Avatar->GetActorLocation();
	FRotator AimRotation = Avatar->GetActorRotation();
	Avatar->GetActorEyesViewPoint(AimStart, AimRotation);
	if (const AController* Controller = GetOwningController())
	{
		AimRotation = Controller->GetControlRotation();
	}
	if (AimStart.ContainsNaN() || AimRotation.ContainsNaN())
	{
		return false;
	}
	// Match Requiem: a displaced camera cannot originate gameplay beyond the
	// character's trusted muzzle/eye envelope. Invalid data fails before traces.
	if (FVector::DistSquared(AimStart, Avatar->GetActorLocation())
		> FMath::Square(static_cast<double>(MaximumMuzzleDistance)))
	{
		AimStart = Avatar->GetActorLocation();
	}
	const FVector AimForward = AimRotation.Vector().GetSafeNormal();
	if (AimForward.ContainsNaN() || AimForward.IsNearlyZero()) { return false; }

	const FVector AimEnd = AimStart + (AimForward * MaximumRange);
	if (AimEnd.ContainsNaN()) { return false; }
	const TArray<FHitResult> AimHits =
		PerformTraceMulti(AimStart, AimEnd, 0.0f);
	const FHitResult* BlockingHit = AimHits.FindByPredicate(
		[](const FHitResult& Hit)
		{
			return Hit.bBlockingHit;
		});
	OutEye = AimStart;
	OutForward = AimForward;
	OutAimPoint = BlockingHit ? FVector(BlockingHit->ImpactPoint) : AimEnd;
	return !OutAimPoint.ContainsNaN();
}

bool USovGameplayAbility_TarrikCinderJudgement::
	ReleaseCinderJudgementFromAim()
{
	if (!IsActive()
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| bJudgementReleaseAttempted)
	{
		return false;
	}
	if (!IsTarrikReleaseContextValid()) { FinishEchoAbility(true); return false; }
	bJudgementReleaseAttempted = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JudgementReleaseTimerHandle);
	}

	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* SourceASC =
		CurrentActorInfo->AbilitySystemComponent.Get();
	UWorld* World = GetWorld();
	if (!HasRequiredPayloadConfiguration()
		|| !IsValid(Avatar)
		|| !IsValid(SourceASC)
		|| !IsValid(World)
		|| CharacterOwner != Avatar
		|| Avatar->GetActorTransform().ContainsNaN()
		|| !Avatar->GetActorTransform().GetRotation().IsNormalized())
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("Cinder Judgement could not resolve its authoritative release context."));
		FinishEchoAbility(true);
		return false;
	}

	const uint64 Activation = GetTarrikActivationSerial();
	const FTransform MuzzleTransform = ResolveJudgementMuzzleTransform();
	if (GetTarrikActivationSerial() != Activation || !IsTarrikReleaseContextValid())
	{
		if (GetTarrikActivationSerial() == Activation) { FinishEchoAbility(true); }
		return false;
	}
	FVector TraceStart = MuzzleTransform.GetLocation();
	FVector Eye, AimPoint, AimForward;
	if (MuzzleTransform.ContainsNaN()
		|| !MuzzleTransform.GetRotation().IsNormalized()
		|| !ResolveJudgementAuthorityAimPoint(Eye, AimPoint, AimForward))
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("%s produced invalid Cinder Judgement release geometry."),
			*GetNameSafe(Avatar));
		if (GetTarrikActivationSerial() == Activation) { FinishEchoAbility(true); }
		return false;
	}
	if (GetTarrikActivationSerial() != Activation || !IsTarrikReleaseContextValid()
		|| CharacterOwner != Avatar)
	{
		if (GetTarrikActivationSerial() == Activation) { FinishEchoAbility(true); }
		return false;
	}
	// A visual muzzle may clip through a thin wall. Bridge back to the trusted
	// eye before resolving convergence, as Requiem does, rather than allowing the
	// gameplay ray (and explosion) to originate on the far side. Use the shot's
	// sweep radius too, so a wide muzzle trace cannot start beyond a missed edge.
	FCollisionQueryParams BridgeQuery = CharacterOwner->GetIgnoreCharacterParams();
	BridgeQuery.bTraceComplex = true;
	TArray<AActor*> Attachments;
	Avatar->GetAttachedActors(Attachments, true, true);
	BridgeQuery.AddIgnoredActors(Attachments);
	FHitResult BridgeHit;
	const bool bVisibilityBridgeBlocked = FMath::IsNearlyZero(TraceRadius)
		? World->LineTraceSingleByChannel(BridgeHit, Eye, TraceStart, ECC_Visibility, BridgeQuery)
		: World->SweepSingleByChannel(BridgeHit, Eye, TraceStart, FQuat::Identity,
			ECC_Visibility, FCollisionShape::MakeSphere(TraceRadius), BridgeQuery);
	// Also honor weapon-only blockers; the eye aim and release already use
	// Narrative's configured WeaponTraceChannel through this existing helper.
	const TArray<FHitResult> BridgeHits = PerformTraceMulti(Eye, TraceStart, TraceRadius);
	if (bVisibilityBridgeBlocked || BridgeHits.ContainsByPredicate(
		[](const FHitResult& Hit) { return Hit.bBlockingHit; }))
	{
		TraceStart = Eye;
	}
	FVector ShotDirection = (AimPoint - TraceStart).GetSafeNormal();
	if (ShotDirection.ContainsNaN() || ShotDirection.IsNearlyZero()
		|| FVector::DotProduct(ShotDirection, AimForward) <= 0.0)
	{
		// Close eye hits behind a forward muzzle must not fire backwards, and a
		// cosmetic socket's rotation is not authoritative player aim.
		ShotDirection = AimForward;
	}
	// Eye/weapon accessors can be authored callbacks. Do not publish an old
	// activation's ray after one of them replaces the owning execution.
	if (GetTarrikActivationSerial() != Activation || !IsTarrikReleaseContextValid())
	{
		if (GetTarrikActivationSerial() == Activation) { FinishEchoAbility(true); }
		return false;
	}

	const FVector UnblockedEnd = TraceStart + (ShotDirection * MaximumRange);
	const TArray<FHitResult> Hits = PerformTraceMulti(
		TraceStart,
		UnblockedEnd,
		FMath::Max(TraceRadius, 0.0f));
	const FHitResult* BlockingHit = Hits.FindByPredicate(
		[](const FHitResult& Hit)
		{
			return Hit.bBlockingHit;
		});
	const FVector TraceEnd = BlockingHit
		? FVector(BlockingHit->ImpactPoint)
		: UnblockedEnd;
	FVector ImpactNormal = BlockingHit
		? FVector(BlockingHit->ImpactNormal).GetSafeNormal()
		: -ShotDirection;
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = FVector::UpVector;
	}
	const bool bBlastTriggered = BlockingHit || bExplodeAtMaximumRange;

	const FJudgementShotContext Shot(*this);
	ASovCinderJudgementPresentation* Presentation =
		SpawnDeferredJudgementPresentation(Shot, TraceStart, TraceEnd);
	TStrongObjectPtr<ASovCinderJudgementPresentation> KeepPresentation(Presentation);
	const auto DiscardRetiredPacket = [&Shot, this, Presentation]()
	{
		if (IsJudgementShotCurrent(Shot)) { return false; }
		// Never finish or replicate an obsolete deferred packet. Its paid direct
		// damage, if already committed, remains intact. Destroy only this shot's actor.
		if (IsValid(Presentation) && !Presentation->IsActorBeingDestroyed()) { Presentation->Destroy(); }
		if (GetTarrikActivationSerial() == Shot.Activation && IsActive()) { FinishEchoAbility(true); }
		return true;
	};
	if (DiscardRetiredPacket()) { return false; }
	AActor* ExplosionDamageCauser = IsValid(Presentation)
		? static_cast<AActor*>(Presentation)
		: Avatar;

	bool bDirectDamageResolved = false;
	if (BlockingHit)
	{
		UAbilitySystemComponent* TargetASC =
			ResolveTarrikTargetAbilitySystem(BlockingHit->GetActor());
		FGameplayEffectContextHandle DirectContext = Shot.Context.Duplicate();
		DirectContext.AddHitResult(*BlockingHit, true);
		DirectContext.AddOrigin(TraceEnd);
		bDirectDamageResolved = ApplyJudgementDamage(
			Shot,
			TargetASC,
			DirectContext,
			Shot.DirectEffect,
			Shot.DirectDamage,
			Shot.DirectPoise,
			Shot.DirectShield,
			1.0f);
	}
	if (DiscardRetiredPacket()) { return bDirectDamageResolved; }

	int32 RadialTargetsResolved = 0;
	if (bBlastTriggered)
	{
		RadialTargetsResolved = ApplyJudgementExplosion(
			Shot,
			TraceEnd,
			ImpactNormal,
			BlockingHit ? BlockingHit->GetActor() : nullptr,
			ExplosionDamageCauser);
		if (DiscardRetiredPacket()) { return bDirectDamageResolved || RadialTargetsResolved > 0; }
		ApplyJudgementPhysicsImpulse(Shot, TraceEnd, ImpactNormal);
	}
	if (DiscardRetiredPacket()) { return bDirectDamageResolved || RadialTargetsResolved > 0; }

	FinishJudgementPresentation(
		Shot,
		Presentation,
		TraceStart,
		TraceEnd,
		BlockingHit,
		bBlastTriggered,
		bDirectDamageResolved,
		RadialTargetsResolved);
	if (!DiscardRetiredPacket()) { BeginJudgementRecovery(Shot); }
	return true;
}

bool USovGameplayAbility_TarrikCinderJudgement::ApplyJudgementDamage(
	const FJudgementShotContext& Shot,
	UAbilitySystemComponent* TargetAbilitySystem,
	const FGameplayEffectContextHandle& Context,
	const TSubclassOf<UGameplayEffect> EffectClass,
	const float Damage,
	const float PoiseDamage,
	const float ShieldCoefficient,
	const float SourceModifier) const
{
	UAbilitySystemComponent* SourceASC = Shot.SourceASC.Get();
	AActor* SourceActor = Shot.Avatar.Get();
	if (!IsJudgementShotCurrent(Shot)
		|| !IsValid(TargetAbilitySystem)
		|| !EffectClass.Get()
		|| Damage <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	TStrongObjectPtr<UAbilitySystemComponent> KeepTarget(TargetAbilitySystem);
	TStrongObjectPtr<AActor> KeepTargetAvatar(TargetAbilitySystem->GetAvatarActor());
	const auto* TargetAttributes = TargetAbilitySystem->GetSet<UNarrativeAttributeSetBase>();
	const uint64 TargetLife = TargetAttributes ? TargetAttributes->GetCombatLifeEpoch() : 0;
	const auto* TargetNarrative = Cast<UNarrativeAbilitySystemComponent>(TargetAbilitySystem);
	const uint64 TargetOwner = TargetNarrative ? TargetNarrative->GetCombatActorInfoEpoch() : 0;
	const auto TargetStillCurrent = [&]()
	{
		return KeepTarget.IsValid() && KeepTargetAvatar.IsValid() && !KeepTargetAvatar->IsActorBeingDestroyed()
			&& TargetAbilitySystem->GetAvatarActor() == KeepTargetAvatar.Get()
			&& TargetAbilitySystem->GetSet<UNarrativeAttributeSetBase>() == TargetAttributes
			&& TargetAttributes && TargetAttributes->GetCombatLifeEpoch() == TargetLife
			&& (!TargetNarrative || TargetNarrative->GetCombatActorInfoEpoch() == TargetOwner);
	};
	// Team attitude is authored code and may synchronously replace this action or target.
	if (!IsHostileTarrikTarget(SourceASC, SourceActor, TargetAbilitySystem)
		|| !IsJudgementShotCurrent(Shot) || !TargetStillCurrent() || !IsTarrikTargetAlive(TargetAbilitySystem)) { return false; }

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		EffectClass,
		Shot.Level,
		Context);
	FGameplayEffectSpec* DamageSpec = SpecHandle.Data.Get();
	if (!DamageSpec || !IsJudgementShotCurrent(Shot) || !TargetStillCurrent())
	{
		return false;
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	DamageSpec->AddDynamicAssetTag(Shot.SpendTag);
	DamageSpec->AddDynamicAssetTag(SovTags.Damage_Channel_Kinetic);
	DamageSpec->AddDynamicAssetTag(SovTags.Damage_Channel_Thermal);
	DamageSpec->AddDynamicAssetTag(SovTags.Damage_GuardClass_Standard);
	DamageSpec->SetSetByCallerMagnitude(
		FNarrativeGameplayTags::Get().SetByCaller_Damage,
		FMath::Max(Damage, 0.0f));
	DamageSpec->SetSetByCallerMagnitude(
		SovTags.SetByCaller_Damage_SourceModifier,
		FMath::Max(SourceModifier, 0.0f));
	DamageSpec->SetSetByCallerMagnitude(
		SovTags.SetByCaller_Damage_ShieldCoefficient,
		FMath::Max(ShieldCoefficient, 0.0f));
	if (PoiseDamage > KINDA_SMALL_NUMBER)
	{
		DamageSpec->SetSetByCallerMagnitude(
			SovTags.SetByCaller_Damage_PoiseDamage,
			PoiseDamage);
	}

	const float OldShield = TargetAbilitySystem->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetShieldAttribute());
	const float OldHealth = TargetAbilitySystem->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetHealthAttribute());
	const float OldPoise = TargetAbilitySystem->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetPoiseAttribute());
	const float OldStamina = TargetAbilitySystem->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetStaminaAttribute());
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetAbilitySystem);
	if (!TargetStillCurrent()) { return false; }

	return TargetAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetShieldAttribute())
			< OldShield - KINDA_SMALL_NUMBER
		|| TargetAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetHealthAttribute())
			< OldHealth - KINDA_SMALL_NUMBER
		|| TargetAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetPoiseAttribute())
			< OldPoise - KINDA_SMALL_NUMBER
		|| TargetAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetStaminaAttribute())
			< OldStamina - KINDA_SMALL_NUMBER;
}

int32 USovGameplayAbility_TarrikCinderJudgement::ApplyJudgementExplosion(
	const FJudgementShotContext& Shot,
	const FVector& Origin,
	const FVector& SurfaceNormal,
	AActor* DirectHitActor,
	AActor* ExplosionDamageCauser) const
{
	UWorld* World = Shot.World.Get();
	AActor* SourceActor = Shot.Avatar.Get();
	if (!IsJudgementShotCurrent(Shot) || Shot.Radius <= KINDA_SMALL_NUMBER)
	{
		return 0;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovCinderJudgementExplosion),
		false,
		SourceActor);
	QueryParams.AddIgnoredActor(SourceActor);
	QueryParams.AddIgnoredActor(ExplosionDamageCauser);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(Shot.Radius),
		QueryParams);

	UAbilitySystemComponent* DirectTargetASC =
		ResolveTarrikTargetAbilitySystem(DirectHitActor);
	TArray<TWeakObjectPtr<UAbilitySystemComponent>> CandidateTargets;
	if (IsValid(DirectTargetASC))
	{
		// The primary target must not lose the blast because its capsule or
		// modular presentation component falls just outside the overlap query.
		CandidateTargets.Add(DirectTargetASC);
	}
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (UAbilitySystemComponent* OverlapASC =
			ResolveTarrikTargetAbilitySystem(Overlap.GetActor()))
		{
			CandidateTargets.Add(OverlapASC);
		}
	}

	TSet<UAbilitySystemComponent*> UniqueTargets;
	int32 ResolvedTargetCount = 0;
	for (const TWeakObjectPtr<UAbilitySystemComponent>& Candidate : CandidateTargets)
	{
		if (!IsJudgementShotCurrent(Shot)) { break; }
		UAbilitySystemComponent* TargetASC = Candidate.Get();
		AActor* TargetActor = IsValid(TargetASC)
			? TargetASC->GetAvatarActor()
			: nullptr;
		TStrongObjectPtr<UAbilitySystemComponent> KeepCandidate(TargetASC);
		TStrongObjectPtr<AActor> KeepCandidateAvatar(TargetActor);
		if (!IsValid(TargetASC)
			|| !IsValid(TargetActor)
			|| UniqueTargets.Contains(TargetASC)
			|| !IsTarrikTargetAlive(TargetASC)
			|| (Shot.bRequiresLineOfSight
				&& TargetASC != DirectTargetASC
				&& !HasJudgementExplosionLineOfSight(
					Shot,
					Origin + (SurfaceNormal.GetSafeNormal() * 2.0f),
					TargetASC,
					DirectHitActor)))
		{
			continue;
		}
		if (!IsJudgementShotCurrent(Shot)) { break; }
		if (!KeepCandidate.IsValid() || !KeepCandidateAvatar.IsValid()
			|| TargetActor->IsActorBeingDestroyed() || TargetASC->GetAvatarActor() != TargetActor) { continue; }
		UniqueTargets.Add(TargetASC);

		const float DistanceAlpha = TargetASC == DirectTargetASC
			? 0.0f
			: FMath::Clamp(
				FVector::Distance(Origin, TargetActor->GetActorLocation())
					/ Shot.Radius,
				0.0f,
				1.0f);
		const float FalloffScalar = FMath::Lerp(
			1.0f,
			Shot.MinimumFraction,
			DistanceAlpha);

		FGameplayEffectContextHandle Context = Shot.Context.Duplicate();
		Context.AddInstigator(
			SourceActor,
			IsValid(ExplosionDamageCauser)
				? ExplosionDamageCauser
				: SourceActor);
		Context.AddOrigin(Origin);

		if (ApplyJudgementDamage(
			Shot,
			TargetASC,
			Context,
			Shot.ExplosionEffect,
			Shot.ExplosionDamage,
			Shot.ExplosionPoise * FalloffScalar,
			Shot.ExplosionShield,
			FalloffScalar))
		{
			++ResolvedTargetCount;
		}
	}
	return ResolvedTargetCount;
}

bool USovGameplayAbility_TarrikCinderJudgement::
	HasJudgementExplosionLineOfSight(
		const FJudgementShotContext& Shot,
		const FVector& Origin,
		UAbilitySystemComponent* TargetAbilitySystem,
		AActor* DirectHitActor) const
{
	UWorld* World = Shot.World.Get();
	AActor* SourceActor = Shot.Avatar.Get();
	AActor* TargetActor = IsValid(TargetAbilitySystem)
		? TargetAbilitySystem->GetAvatarActor()
		: nullptr;
	if (!IsJudgementShotCurrent(Shot) || !IsValid(TargetActor))
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovCinderJudgementLineOfSight),
		false,
		SourceActor);
	QueryParams.AddIgnoredActor(SourceActor);
	if (ResolveTarrikTargetAbilitySystem(DirectHitActor))
	{
		QueryParams.AddIgnoredActor(DirectHitActor);
	}

	TArray<FHitResult> BlockingHits;
	if (!World->LineTraceMultiByObjectType(
			BlockingHits,
			Origin,
			TargetActor->GetActorLocation(),
			ObjectQuery,
			QueryParams))
	{
		return true;
	}

	for (const FHitResult& BlockingHit : BlockingHits)
	{
		AActor* BlockingActor = BlockingHit.GetActor();
		UAbilitySystemComponent* BlockingASC =
			ResolveTarrikTargetAbilitySystem(BlockingActor);
		if (BlockingASC == TargetAbilitySystem)
		{
			return true;
		}
		if (IsValid(BlockingASC))
		{
			continue;
		}
		if (BlockingActor == TargetActor
			|| (IsValid(BlockingActor) && BlockingActor->IsOwnedBy(TargetActor))
			|| (IsValid(BlockingActor) && TargetActor->IsOwnedBy(BlockingActor)))
		{
			return true;
		}
		return false;
	}
	return true;
}

void USovGameplayAbility_TarrikCinderJudgement::
	ApplyJudgementPhysicsImpulse(
		const FJudgementShotContext& Shot,
		const FVector& Origin,
		const FVector& SurfaceNormal) const
{
	UWorld* World = Shot.World.Get();
	AActor* SourceActor = Shot.Avatar.Get();
	if (!Shot.bApplyImpulse || !IsJudgementShotCurrent(Shot)
		|| Shot.Radius <= KINDA_SMALL_NUMBER || Shot.ImpulseStrength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovCinderJudgementPhysicsImpulse),
		false,
		SourceActor);
	QueryParams.AddIgnoredActor(SourceActor);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		ObjectQuery,
		FCollisionShape::MakeSphere(Shot.Radius),
		QueryParams);

	const FVector ImpulseOrigin = Origin
		- (FVector::UpVector * FMath::Max(Shot.UpwardBias, 0.0f));
	const FVector LineOfSightOrigin = Origin
		+ (SurfaceNormal.GetSafeNormal() * 2.0f);
	TSet<UPrimitiveComponent*> ImpulsedComponents;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (!IsJudgementShotCurrent(Shot)) { break; }
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!IsValid(Component)
			|| ImpulsedComponents.Contains(Component)
			|| !Component->IsSimulatingPhysics()
			|| (Shot.bRequiresLineOfSight
				&& !HasJudgementPhysicsLineOfSight(
					Shot,
					LineOfSightOrigin,
					Component)))
		{
			continue;
		}
		ImpulsedComponents.Add(Component);
		Component->AddRadialImpulse(
			ImpulseOrigin,
			Shot.Radius,
			Shot.ImpulseStrength,
			ERadialImpulseFalloff::RIF_Linear,
			true);
	}
}

bool USovGameplayAbility_TarrikCinderJudgement::
	HasJudgementPhysicsLineOfSight(
		const FJudgementShotContext& Shot,
		const FVector& Origin,
		UPrimitiveComponent* TargetComponent) const
{
	UWorld* World = Shot.World.Get();
	AActor* SourceActor = Shot.Avatar.Get();
	AActor* TargetActor = IsValid(TargetComponent)
		? TargetComponent->GetOwner()
		: nullptr;
	if (!IsJudgementShotCurrent(Shot) || !IsValid(TargetComponent) || !IsValid(TargetActor))
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovCinderJudgementPhysicsLineOfSight),
		false,
		SourceActor);
	QueryParams.AddIgnoredActor(SourceActor);

	FHitResult BlockingHit;
	if (!World->LineTraceSingleByObjectType(
			BlockingHit,
			Origin,
			TargetComponent->Bounds.Origin,
			ObjectQuery,
			QueryParams))
	{
		return true;
	}
	AActor* BlockingActor = BlockingHit.GetActor();
	return BlockingHit.GetComponent() == TargetComponent
		|| BlockingActor == TargetActor
		|| (IsValid(BlockingActor) && BlockingActor->IsOwnedBy(TargetActor))
		|| (IsValid(BlockingActor) && TargetActor->IsOwnedBy(BlockingActor));
}

ASovCinderJudgementPresentation*
USovGameplayAbility_TarrikCinderJudgement::
	SpawnDeferredJudgementPresentation(
		const FJudgementShotContext& Shot,
		const FVector& TraceStart,
		const FVector& TraceEnd) const
{
	UWorld* World = Shot.World.Get();
	AActor* Avatar = Shot.Avatar.Get();
	if (!IsJudgementShotCurrent(Shot))
	{
		return nullptr;
	}

	TSubclassOf<ASovCinderJudgementPresentation> ResolvedClass =
		Shot.PresentationClass;
	if (!ResolvedClass.Get())
	{
		ResolvedClass = ASovCinderJudgementPresentation::StaticClass();
	}
	FVector Direction = (TraceEnd - TraceStart).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		Direction = Avatar->GetActorForwardVector();
	}
	const FTransform SpawnTransform(
		Direction.ToOrientationQuat(),
		TraceEnd,
		FVector::OneVector);
	ASovCinderJudgementPresentation* Presentation =
		World->SpawnActorDeferred<ASovCinderJudgementPresentation>(
		ResolvedClass,
		SpawnTransform,
		Avatar,
		Cast<APawn>(Avatar),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Presentation)
		&& IsJudgementShotCurrent(Shot)
		&& ResolvedClass.Get()
			!= ASovCinderJudgementPresentation::StaticClass())
	{
		Presentation =
			World->SpawnActorDeferred<ASovCinderJudgementPresentation>(
				ASovCinderJudgementPresentation::StaticClass(),
				SpawnTransform,
				Avatar,
				Cast<APawn>(Avatar),
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	}
	return Presentation;
}

void USovGameplayAbility_TarrikCinderJudgement::
	FinishJudgementPresentation(
		const FJudgementShotContext& Shot,
		ASovCinderJudgementPresentation* Presentation,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const FHitResult* Hit,
		const bool bBlastTriggered,
		const bool bDirectDamageResolved,
		const int32 RadialTargetsResolved) const
{
	if (!IsJudgementShotCurrent(Shot) || !IsValid(Presentation) || Presentation->IsActorBeingDestroyed())
	{
		return;
	}

	const bool bBlockingHit = Hit && Hit->bBlockingHit;
	FVector ImpactNormal = -((TraceEnd - TraceStart).GetSafeNormal());
	AActor* HitActor = nullptr;
	FName HitBone = NAME_None;
	EPhysicalSurface ImpactSurface = SurfaceType_Default;
	if (bBlockingHit)
	{
		ImpactNormal = FVector(Hit->ImpactNormal).GetSafeNormal();
		HitActor = Hit->GetActor();
		HitBone = Hit->BoneName;
		ImpactSurface = UGameplayStatics::GetSurfaceType(*Hit);
	}
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = FVector::UpVector;
	}

	Presentation->InitializeJudgementPresentation(
		TraceStart,
		TraceEnd,
		ImpactNormal,
		HitActor,
		HitBone,
		ImpactSurface,
		Shot.Radius,
		bBlockingHit,
		bBlastTriggered,
		bDirectDamageResolved,
		RadialTargetsResolved);
	UGameplayStatics::FinishSpawningActor(
		Presentation,
		Presentation->GetActorTransform());
}

void USovGameplayAbility_TarrikCinderJudgement::BeginJudgementRecovery(const FJudgementShotContext& Shot)
{
	if (!IsJudgementShotCurrent(Shot))
	{
		return;
	}
	const float Recovery = FMath::Max(Shot.Recovery, 0.0f);
	if (Recovery <= KINDA_SMALL_NUMBER)
	{
		HandleJudgementRecoveryFinished();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		const uint64 ExpectedActivation = Shot.Activation;
		World->GetTimerManager().SetTimer(
			JudgementRecoveryTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, ExpectedActivation]()
			{
				if (GetTarrikActivationSerial() == ExpectedActivation && IsCurrentEchoExecutionValid())
				{ HandleJudgementRecoveryFinished(); }
			}),
			Recovery,
			false);
		return;
	}
	FinishEchoAbility(false);
}


void USovGameplayAbility_TarrikVelkorransHunger::ExecuteAutomaticTarrikPayload()
{
	const uint64 Activation = GetTarrikActivationSerial();
	if (!ReleaseVelkorransHungerFromAim() && IsActive() && GetTarrikActivationSerial() == Activation) { FinishEchoAbility(true); }
}

void USovGameplayAbility_TarrikCinderStickyGrenade::ExecuteAutomaticTarrikPayload()
{
	const uint64 Activation = GetTarrikActivationSerial();
	if (!ReleaseCinderStickyGrenadeFromAim() && IsActive() && GetTarrikActivationSerial() == Activation) { FinishEchoAbility(true); }
}
