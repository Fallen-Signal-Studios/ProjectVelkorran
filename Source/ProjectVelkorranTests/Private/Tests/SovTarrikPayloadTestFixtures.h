// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_TarrikEcho.h"
#include "SovTarrikPayloadTestFixtures.generated.h"
/** Concrete, content-free children still use the real source-weapon gate. */
UCLASS(Transient, NotBlueprintable)
class USovTarrikSlamTestAbility : public USovGameplayAbility_TarrikCinderSlam
{
	GENERATED_BODY()
public: USovTarrikSlamTestAbility();
};
UCLASS(Transient, NotBlueprintable)
class USovTarrikRequiemTestAbility : public USovGameplayAbility_TarrikCinderlineRequiem
{
	GENERATED_BODY()
public: USovTarrikRequiemTestAbility();
};
UCLASS(Transient, NotBlueprintable)
class USovTarrikHungerTestAbility : public USovGameplayAbility_TarrikVelkorransHunger
{
	GENERATED_BODY()
public: USovTarrikHungerTestAbility();
};
UCLASS(Transient, NotBlueprintable)
class USovTarrikGrenadeTestAbility : public USovGameplayAbility_TarrikCinderStickyGrenade
{
	GENERATED_BODY()
public: USovTarrikGrenadeTestAbility();
};
