// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_DominionHound.h"
#include "Abilities/SovGameplayAbility_ReformationDrone.h"
#include "SovThreatAttackTestFixtures.generated.h"
UCLASS(Transient, NotBlueprintable)
class USovThreatHoundBite : public USovGameplayAbility_DominionHoundBite
{
	GENERATED_BODY()
public:
	USovThreatHoundBite() { bOnlyAcquirePlayerControlledTargets = false; CooldownDuration = 0.f; }
};
UCLASS(Transient, NotBlueprintable)
class USovThreatDroneGun : public USovGameplayAbility_ReformationDroneGunfire
{
	GENERATED_BODY()
public:
	USovThreatDroneGun() { bAutoReleasePayload = false; CooldownDuration = 0.f; FallbackMuzzleOffset = FVector::ZeroVector; }
};
UCLASS(Transient, NotBlueprintable)
class USovThreatDroneExploder : public USovGameplayAbility_ReformationDroneSelfDestruct
{
	GENERATED_BODY()
public:
	USovThreatDroneExploder() { bOnlyAcquirePlayerControlledTargets = false; bAutoReleasePayload = false; CooldownDuration = 0.f; }
};
