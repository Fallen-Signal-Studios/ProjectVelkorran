// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_TarrikEcho.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Combat/SovNativeDamageReceipt.h"
#include "Combat/SovDamageTargetSnapshot.h"
#include "Combat/SovTarrikPayloadSupport.h"
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

/** Immutable committed-shot values. Never resolve a later activation's actor info. */
struct FSovJudgementRelease
{
	TWeakObjectPtr<UAbilitySystemComponent> Source;
	TWeakObjectPtr<AActor> Avatar;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UObject> SourceObject;
	uint64 ActorInfoEpoch = 0;
	int32 ReadyEpoch = 0;
	TWeakObjectPtr<const UNarrativeAttributeSetBase> SourceAttributes;
	uint64 SourceLifeEpoch = 0;
	float Level = 1.f;
	float Recovery = 0.f;
	TWeakObjectPtr<UAbilitySystemComponent> DirectTarget;
	TWeakObjectPtr<AActor> DirectAvatar;
	uint64 DirectActorInfoEpoch = 0;
	TArray<FSovDamageTargetSnapshot> Targets;
	FGameplayTag Identity;
	TSubclassOf<UGameplayEffect> DirectEffect;
	TSubclassOf<UGameplayEffect> ExplosionEffect;
	float DirectDamage = 0.f, DirectPoise = 0.f, DirectShield = 1.f;
	float Radius = 0.f, ExplosionDamage = 0.f, ExplosionPoise = 0.f, ExplosionShield = 1.f;
	float MinimumDamageFraction = 0.f, PhysicsStrength = 0.f, PhysicsUpwardBias = 0.f;
	bool bRequiresLineOfSight = true, bPhysicsImpulse = false;

	bool IsSourceCurrent() const
	{
		const auto* ASC = Source.Get();
		const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
		return IsValid(ASC) && Avatar.IsValid() && World.IsValid()
			&& ASC->GetAvatarActor() == Avatar.Get()
			&& SourceAttributes.IsValid() && ASC->GetSet<UNarrativeAttributeSetBase>() == SourceAttributes.Get()
			&& SourceAttributes->GetCombatLifeEpoch() == SourceLifeEpoch
			&& (!NarrativeASC || (NarrativeASC->GetCombatActorInfoEpoch() == ActorInfoEpoch
				&& NarrativeASC->GetCharacterReadyEpoch() == ReadyEpoch));
	}
};

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

	ArmTarrikPayload(PayloadReleaseDelay, Activation);
}

void USovGameplayAbility_TarrikCinderJudgement::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
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
				&& FVector::DistSquared(
					Avatar->GetActorLocation(),
					SocketTransform.GetLocation())
					<= FMath::Square(MaximumMuzzleDistance))
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

	AActor* Avatar = CurrentActorInfo->AvatarActor.Get();
	UAbilitySystemComponent* SourceASC =
		CurrentActorInfo->AbilitySystemComponent.Get();
	UWorld* World = GetWorld();
	if (!HasRequiredPayloadConfiguration()
		|| !IsValid(Avatar)
		|| !IsValid(SourceASC)
		|| !IsValid(World))
	{
		UE_LOG(
			LogSovTarrikEchoAbility,
			Error,
			TEXT("Cinder Judgement could not resolve its authoritative release context."));
		FinishEchoAbility(true);
		return false;
	}

	const uint64 Activation = GetTarrikActivationSerial();
	FSovJudgementRelease Release;
	Release.Source = SourceASC; Release.Avatar = Avatar; Release.World = World;
	Release.SourceObject = GetCurrentSourceObject();
	if (!Release.SourceObject.IsValid() || Release.SourceObject->IsA<UGameplayAbility>()) { Release.SourceObject = Avatar; }
	const auto* NarrativeSource = Cast<UNarrativeAbilitySystemComponent>(SourceASC);
	Release.ActorInfoEpoch = NarrativeSource ? NarrativeSource->GetCombatActorInfoEpoch() : 0;
	Release.ReadyEpoch = NarrativeSource ? NarrativeSource->GetCharacterReadyEpoch() : 0;
	Release.SourceAttributes = SourceASC->GetSet<UNarrativeAttributeSetBase>();
	Release.SourceLifeEpoch = Release.SourceAttributes.IsValid() ? Release.SourceAttributes->GetCombatLifeEpoch() : 0;
	Release.Level = static_cast<float>(GetAbilityLevel()); Release.Identity = EchoSpendTag;
	Release.Recovery = PostReleaseRecovery;
	Release.DirectEffect = ResolveJudgementDirectEffectClass(); Release.ExplosionEffect = ResolveJudgementExplosionEffectClass();
	Release.DirectDamage = DirectDamage; Release.DirectPoise = DirectPoiseDamage; Release.DirectShield = DirectShieldCoefficient;
	Release.Radius = ExplosionRadius; Release.ExplosionDamage = ExplosionDamage; Release.ExplosionPoise = ExplosionPoiseDamage;
	Release.ExplosionShield = ExplosionShieldCoefficient; Release.MinimumDamageFraction = MinimumExplosionDamageFraction;
	Release.bRequiresLineOfSight = bExplosionRequiresLineOfSight; Release.bPhysicsImpulse = bApplyExplosionPhysicsImpulse;
	Release.PhysicsStrength = ExplosionPhysicsImpulseStrength; Release.PhysicsUpwardBias = ExplosionPhysicsUpwardBias;

	FVector Eye = Avatar->GetActorLocation(); FRotator Aim = Avatar->GetActorRotation();
	Avatar->GetActorEyesViewPoint(Eye, Aim);
	if (const AController* Controller = GetOwningController()) { Aim = Controller->GetControlRotation(); }
	if (!Release.IsSourceCurrent() || GetTarrikActivationSerial() != Activation || !IsTarrikReleaseContextValid()) { return false; }
	if (Eye.ContainsNaN() || Aim.ContainsNaN()) { FinishEchoAbility(true); return false; }
	if (FVector::DistSquared(Eye, Avatar->GetActorLocation()) > FMath::Square(MaximumMuzzleDistance)) { Eye = Avatar->GetActorLocation(); }
	FVector TraceStart = ResolveJudgementMuzzleTransform().GetLocation();
	if (TraceStart.ContainsNaN()) { FinishEchoAbility(true); return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCinderJudgementRelease), true, Avatar);
	Query.bReturnPhysicalMaterial = true;
	SovTarrikPayload::IgnoreActorAndAttachments(Query, Avatar);
	const ECollisionChannel WeaponChannel = UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel;
	FHitResult Bridge;
	// A forward socket can already lie beyond thin cover. Return the muzzle
	// to the authoritative eye before tracing so that wall remains the impact.
	if (World->LineTraceSingleByChannel(Bridge, Eye, TraceStart, ECC_Visibility, Query)
		|| World->LineTraceSingleByChannel(Bridge, Eye, TraceStart, WeaponChannel, Query)) { TraceStart = Eye; }
	const FVector AimEnd = Eye + Aim.Vector() * MaximumRange;
	FHitResult AimHit;
	const bool bAimHit = World->LineTraceSingleByChannel(AimHit, Eye, AimEnd, WeaponChannel, Query);
	FVector ShotDirection = ((bAimHit ? FVector(AimHit.ImpactPoint) : AimEnd) - TraceStart).GetSafeNormal();
	if (ShotDirection.IsNearlyZero() || FVector::DotProduct(ShotDirection, Aim.Vector()) <= 0.f) { ShotDirection = Aim.Vector(); }
	const FVector UnblockedEnd = TraceStart + ShotDirection * MaximumRange;
	FHitResult ShotHit;
	const bool bHit = TraceRadius > KINDA_SMALL_NUMBER
		? World->SweepSingleByChannel(ShotHit, TraceStart, UnblockedEnd, FQuat::Identity,
			WeaponChannel, FCollisionShape::MakeSphere(TraceRadius), Query)
		: World->LineTraceSingleByChannel(ShotHit, TraceStart, UnblockedEnd, WeaponChannel, Query);
	const FHitResult* BlockingHit = bHit ? &ShotHit : nullptr;
	Release.DirectTarget = bHit ? ResolveTarrikTargetAbilitySystem(ShotHit.GetActor()) : nullptr;
	Release.DirectAvatar = Release.DirectTarget.IsValid() ? Release.DirectTarget->GetAvatarActor() : nullptr;
	const auto* DirectNarrativeASC = Cast<UNarrativeAbilitySystemComponent>(Release.DirectTarget.Get());
	Release.DirectActorInfoEpoch = DirectNarrativeASC ? DirectNarrativeASC->GetCombatActorInfoEpoch() : 0;
	const FVector TraceEnd = bHit ? FVector(ShotHit.ImpactPoint) : UnblockedEnd;
	FVector ImpactNormal = bHit ? FVector(ShotHit.ImpactNormal).GetSafeNormal() : -ShotDirection;
	if (ImpactNormal.IsNearlyZero()) { ImpactNormal = FVector::UpVector; }
	const bool bBlastTriggered = bHit || bExplodeAtMaximumRange;
	if (bBlastTriggered)
	{
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, TraceEnd, FQuat::Identity, Objects,
			FCollisionShape::MakeSphere(Release.Radius), Query);
		TSet<UAbilitySystemComponent*> Seen;
		if (Release.DirectTarget.IsValid())
		{
			Seen.Add(Release.DirectTarget.Get()); Release.Targets.Emplace(Release.DirectTarget.Get());
		}
		for (const FOverlapResult& Overlap : Overlaps)
		{
			UAbilitySystemComponent* Target = ResolveTarrikTargetAbilitySystem(Overlap.GetActor());
			if (IsValid(Target) && !Seen.Contains(Target)) { Seen.Add(Target); Release.Targets.Emplace(Target); }
		}
	}

	ASovCinderJudgementPresentation* Presentation =
		SpawnDeferredJudgementPresentation(TraceStart, TraceEnd);
	if (!Release.IsSourceCurrent() || GetTarrikActivationSerial() != Activation || !IsTarrikReleaseContextValid())
	{
		if (IsValid(Presentation)) { Presentation->Destroy(); }
		return false;
	}
	AActor* ExplosionDamageCauser = IsValid(Presentation)
		? static_cast<AActor*>(Presentation)
		: Avatar;

	bool bDirectDamageResolved = false;
	if (BlockingHit)
	{
		UAbilitySystemComponent* TargetASC =
			ResolveTarrikTargetAbilitySystem(BlockingHit->GetActor());
		FGameplayEffectContextHandle DirectContext = SourceASC->MakeEffectContext();
		DirectContext.AddInstigator(Avatar, Avatar);
		DirectContext.AddSourceObject(Release.SourceObject.Get());
		DirectContext.AddHitResult(*BlockingHit, true);
		DirectContext.AddOrigin(TraceEnd);
		bDirectDamageResolved = ApplyJudgementDamage(
			Release,
			TargetASC,
			DirectContext,
			Release.DirectEffect,
			Release.DirectDamage,
			Release.DirectPoise,
			Release.DirectShield,
			1.0f);
	}

	int32 RadialTargetsResolved = 0;
	if (bBlastTriggered)
	{
		RadialTargetsResolved = ApplyJudgementExplosion(
			Release,
			TraceEnd,
			ImpactNormal,
			BlockingHit ? BlockingHit->GetActor() : nullptr,
			ExplosionDamageCauser);
		ApplyJudgementPhysicsImpulse(Release, TraceEnd, ImpactNormal);
	}

	FinishJudgementPresentation(
		Release,
		Presentation,
		TraceStart,
		TraceEnd,
		BlockingHit,
		bBlastTriggered,
		bDirectDamageResolved,
		RadialTargetsResolved);
	BeginTarrikRecovery(Release.Recovery, Activation);
	return true;
}

bool USovGameplayAbility_TarrikCinderJudgement::ApplyJudgementDamage(
	const FSovJudgementRelease& Release,
	UAbilitySystemComponent* TargetAbilitySystem,
	const FGameplayEffectContextHandle& Context,
	const TSubclassOf<UGameplayEffect> EffectClass,
	const float Damage,
	const float PoiseDamage,
	const float ShieldCoefficient,
	const float SourceModifier) const
{
	UAbilitySystemComponent* SourceASC = Release.Source.Get();
	AActor* SourceActor = Release.Avatar.Get();
	AActor* TargetAvatar = IsValid(TargetAbilitySystem) ? TargetAbilitySystem->GetAvatarActor() : nullptr;
	const FSovDamageTargetSnapshot Target(TargetAbilitySystem);
	if (!Release.IsSourceCurrent() || !IsValid(SourceASC)
		|| !IsValid(SourceActor)
		|| !IsValid(TargetAbilitySystem)
		|| !EffectClass.Get()
		|| Damage <= KINDA_SMALL_NUMBER
		|| !IsHostileTarrikTarget(SourceASC, SourceActor, TargetAbilitySystem)
		|| !IsTarrikTargetAlive(TargetAbilitySystem))
	{
		return false;
	}

	if (!Release.IsSourceCurrent() || !Target.IsCurrent() || !IsValid(TargetAvatar)) { return false; }
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		EffectClass,
		Release.Level,
		Context);
	FGameplayEffectSpec* DamageSpec = SpecHandle.Data.Get();
	if (!DamageSpec)
	{
		return false;
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	DamageSpec->AddDynamicAssetTag(Release.Identity);
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

	TStrongObjectPtr<USovNativeDamageReceipt> Receipt(NewObject<USovNativeDamageReceipt>());
	Receipt->bRequireNativeProof = true;
	Receipt->ExpectedTarget = TargetAbilitySystem->GetAvatarActor();
	Receipt->ExpectedContext = DamageSpec->GetContext().Get();
	auto* NarrativeSource = Cast<UNarrativeAbilitySystemComponent>(SourceASC);
	if (NarrativeSource) { NarrativeSource->OnDamageResolvedAsSource.AddDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult); }
	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec, TargetAbilitySystem);
	if (IsValid(NarrativeSource)) { NarrativeSource->OnDamageResolvedAsSource.RemoveDynamic(Receipt.Get(), &USovNativeDamageReceipt::ReceiveResult); }
	return Receipt->bAppliedDamage;
}

int32 USovGameplayAbility_TarrikCinderJudgement::ApplyJudgementExplosion(
	const FSovJudgementRelease& Release,
	const FVector& Origin,
	const FVector& SurfaceNormal,
	AActor* DirectHitActor,
	AActor* ExplosionDamageCauser) const
{
	UWorld* World = Release.World.Get();
	UAbilitySystemComponent* SourceASC = Release.Source.Get();
	AActor* SourceActor = Release.Avatar.Get();
	if (!Release.IsSourceCurrent() || !IsValid(World)
		|| !IsValid(SourceASC)
		|| !IsValid(SourceActor)
		|| Release.Radius <= KINDA_SMALL_NUMBER)
	{
		return 0;
	}

	UAbilitySystemComponent* DirectTargetASC = Release.DirectTarget.Get();
	const auto* DirectNarrativeASC = Cast<UNarrativeAbilitySystemComponent>(DirectTargetASC);
	if (!IsValid(DirectTargetASC) || DirectTargetASC->GetAvatarActor() != Release.DirectAvatar.Get()
		|| (DirectNarrativeASC && DirectNarrativeASC->GetCombatActorInfoEpoch() != Release.DirectActorInfoEpoch))
	{
		DirectTargetASC = nullptr;
	}
	TSet<UAbilitySystemComponent*> UniqueTargets;
	int32 ResolvedTargetCount = 0;
	for (const FSovDamageTargetSnapshot& Target : Release.Targets)
	{
		if (!Release.IsSourceCurrent()) { break; }
		if (!Target.IsCurrent()) { continue; }
		UAbilitySystemComponent* TargetASC = Target.AbilitySystem.Get();
		AActor* TargetActor = IsValid(TargetASC)
			? TargetASC->GetAvatarActor()
			: nullptr;
		if (!IsValid(TargetASC)
			|| !IsValid(TargetActor)
			|| UniqueTargets.Contains(TargetASC)
			|| !IsHostileTarrikTarget(SourceASC, SourceActor, TargetASC)
			|| !IsTarrikTargetAlive(TargetASC)
			|| (Release.bRequiresLineOfSight
				&& TargetASC != DirectTargetASC
				&& !HasJudgementExplosionLineOfSight(
					Release,
					Origin + (SurfaceNormal.GetSafeNormal() * 2.0f),
					TargetASC,
					DirectHitActor)))
		{
			continue;
		}
		if (!Release.IsSourceCurrent() || !Target.IsCurrent()) { continue; }
		UniqueTargets.Add(TargetASC);

		const float DistanceAlpha = TargetASC == DirectTargetASC
			? 0.0f
			: FMath::Clamp(
				FVector::Distance(Origin, TargetActor->GetActorLocation())
					/ Release.Radius,
				0.0f,
				1.0f);
		const float FalloffScalar = FMath::Lerp(
			1.0f,
			Release.MinimumDamageFraction,
			DistanceAlpha);

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(
			SourceActor,
			IsValid(ExplosionDamageCauser)
				? ExplosionDamageCauser
				: SourceActor);
		Context.AddSourceObject(Release.SourceObject.Get());
		Context.AddOrigin(Origin);

		if (ApplyJudgementDamage(
			Release,
			TargetASC,
			Context,
			Release.ExplosionEffect,
			Release.ExplosionDamage,
			Release.ExplosionPoise * FalloffScalar,
			Release.ExplosionShield,
			FalloffScalar))
		{
			++ResolvedTargetCount;
		}
	}
	return ResolvedTargetCount;
}

bool USovGameplayAbility_TarrikCinderJudgement::
	HasJudgementExplosionLineOfSight(
		const FSovJudgementRelease& Release,
		const FVector& Origin,
		UAbilitySystemComponent* TargetAbilitySystem,
		AActor* DirectHitActor) const
{
	UWorld* World = Release.World.Get();
	AActor* SourceActor = Release.Avatar.Get();
	AActor* TargetActor = IsValid(TargetAbilitySystem)
		? TargetAbilitySystem->GetAvatarActor()
		: nullptr;
	if (!Release.IsSourceCurrent() || !IsValid(World) || !IsValid(TargetActor))
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
		const FSovJudgementRelease& Release,
		const FVector& Origin,
		const FVector& SurfaceNormal) const
{
	UWorld* World = Release.World.Get();
	AActor* SourceActor = Release.Avatar.Get();
	if (!Release.IsSourceCurrent() || !Release.bPhysicsImpulse
		|| !IsValid(World)
		|| Release.Radius <= KINDA_SMALL_NUMBER
		|| Release.PhysicsStrength <= KINDA_SMALL_NUMBER)
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
		FCollisionShape::MakeSphere(Release.Radius),
		QueryParams);

	const FVector ImpulseOrigin = Origin
		- (FVector::UpVector * FMath::Max(Release.PhysicsUpwardBias, 0.0f));
	const FVector LineOfSightOrigin = Origin
		+ (SurfaceNormal.GetSafeNormal() * 2.0f);
	TSet<UPrimitiveComponent*> ImpulsedComponents;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (!Release.IsSourceCurrent()) { break; }
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!IsValid(Component)
			|| ImpulsedComponents.Contains(Component)
			|| !Component->IsSimulatingPhysics()
			|| (Release.bRequiresLineOfSight
				&& !HasJudgementPhysicsLineOfSight(
					Release,
					LineOfSightOrigin,
					Component)))
		{
			continue;
		}
		ImpulsedComponents.Add(Component);
		Component->AddRadialImpulse(
			ImpulseOrigin,
			Release.Radius,
			Release.PhysicsStrength,
			ERadialImpulseFalloff::RIF_Linear,
			true);
	}
}

bool USovGameplayAbility_TarrikCinderJudgement::
	HasJudgementPhysicsLineOfSight(
		const FSovJudgementRelease& Release,
		const FVector& Origin,
		UPrimitiveComponent* TargetComponent) const
{
	UWorld* World = Release.World.Get();
	AActor* SourceActor = Release.Avatar.Get();
	AActor* TargetActor = IsValid(TargetComponent)
		? TargetComponent->GetOwner()
		: nullptr;
	if (!Release.IsSourceCurrent() || !IsValid(World) || !IsValid(TargetComponent) || !IsValid(TargetActor))
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
		const FVector& TraceStart,
		const FVector& TraceEnd) const
{
	UWorld* World = GetWorld();
	AActor* Avatar = CurrentActorInfo
		? CurrentActorInfo->AvatarActor.Get()
		: nullptr;
	if (!IsValid(World) || !IsValid(Avatar))
	{
		return nullptr;
	}

	TSubclassOf<ASovCinderJudgementPresentation> ResolvedClass =
		PresentationClass;
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
	const FSovJudgementRelease& Release,
		ASovCinderJudgementPresentation* Presentation,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const FHitResult* Hit,
		const bool bBlastTriggered,
		const bool bDirectDamageResolved,
		const int32 RadialTargetsResolved) const
{
	if (!IsValid(Presentation))
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
		Release.Radius,
		bBlockingHit,
		bBlastTriggered,
		bDirectDamageResolved,
		RadialTargetsResolved);
	UGameplayStatics::FinishSpawningActor(
		Presentation,
		Presentation->GetActorTransform());
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
