// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_TarrikEcho.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "Effects/SovGameplayEffect_CinderGrenade.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeGameplayTags.h"
#include "Projectiles/SovCinderStickyGrenadeProjectile.h"
#include "Sovereign/SovGameplayTags.h"
#include "Weapons/NarrativeProjectile.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovTarrikEchoAbility, Log, All);

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

bool USovGameplayAbility_TarrikVelkorransHunger::HasRequiredPayloadConfiguration() const
{
	return ProjectileClass.Get()
		&& DirectDamageEffectClass.Get()
		&& BurnEffectClass.Get();
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
	return GrenadeClass.Get()
		&& ExplosionDamageEffectClass.Get()
		&& BurnEffectClass.Get()
		&& FuseDuration > KINDA_SMALL_NUMBER
		&& ExplosionRadius > KINDA_SMALL_NUMBER
		&& ExplosionDamage > KINDA_SMALL_NUMBER
		&& ExplosionPoiseDamage >= 0.0f
		&& MinimumExplosionDamageFraction >= 0.0f
		&& MinimumExplosionDamageFraction <= 1.0f
		&& BurnDamagePerTick > KINDA_SMALL_NUMBER
		&& BurnDuration > KINDA_SMALL_NUMBER
		&& DefaultGrenadeLaunchSpeed > KINDA_SMALL_NUMBER
		&& MaximumGrenadeLaunchSpeed + KINDA_SMALL_NUMBER >= DefaultGrenadeLaunchSpeed
		&& MaximumGrenadeSpawnDistance > KINDA_SMALL_NUMBER;
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
			GrenadeClass,
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
		ExplosionDamageEffectClass,
		BurnEffectClass,
		EchoSpendTag,
		static_cast<float>(GetAbilityLevel()),
		InitialVelocity,
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
