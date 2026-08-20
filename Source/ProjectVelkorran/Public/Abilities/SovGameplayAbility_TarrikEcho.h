// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "SovGameplayAbility_TarrikEcho.generated.h"

class UGameplayEffect;
class ANarrativeProjectile;

/** Weapon context used to organize Tarrik's Echo loadout in UI and content. */
UENUM(BlueprintType)
enum class ESovTarrikEchoWeaponFamily : uint8
{
	Universal UMETA(DisplayName = "Either Weapon"),
	Velkorran UMETA(DisplayName = "Velkorran (Sword)"),
	Cinderline UMETA(DisplayName = "Cinderline")
};

/** Compatibility-preserving Tarrik adapter over the shared Echo lifecycle. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik Echo Ability Base"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikEchoBase : public USovGameplayAbility_EchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikEchoBase();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Echo Ability")
	ESovTarrikEchoWeaponFamily GetEchoWeaponFamily() const { return WeaponFamily; }

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Identity")
	ESovTarrikEchoWeaponFamily WeaponFamily = ESovTarrikEchoWeaponFamily::Universal;
};

/** Sword signature: radial damage/Poise pressure, knockback, and a short ward. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinder Slam"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderSlam : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderSlam();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> RadialDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ProtectiveWardEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float SlamRadius = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 900.0f;
};

/** Sword release: a slung inferno projectile with direct damage and Burn. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Velkorran's Hunger"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikVelkorransHunger : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikVelkorransHunger();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<ANarrativeProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> DirectDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> BurnEffectClass;
};

/** Universal utility: a thrown Cinder charge that sticks, fuses, and explodes. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinder Sticky Grenade"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderStickyGrenade : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderStickyGrenade();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<ANarrativeProjectile> GrenadeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> BurnEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "s"))
	float FuseDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float ExplosionRadius = 350.0f;
};

/** Cinderline release: direct explosive shot with impact and radial damage. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinder Judgement"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderJudgement : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderJudgement();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> DirectDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> ExplosionDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float ExplosionRadius = 300.0f;
};

/** Cinderline signature: a penetrating shot followed by a chained burning line. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Tarrik: Cinderline Requiem"))
class PROJECTVELKORRAN_API USovGameplayAbility_TarrikCinderlineRequiem : public USovGameplayAbility_TarrikEchoBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_TarrikCinderlineRequiem();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> PenetratingDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> LineDetonationEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload")
	TSubclassOf<UGameplayEffect> BurnEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumRange = 12000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Echo Ability|Payload", meta = (ClampMin = "0.0", Units = "cm"))
	float LineDetonationRadius = 220.0f;
};
