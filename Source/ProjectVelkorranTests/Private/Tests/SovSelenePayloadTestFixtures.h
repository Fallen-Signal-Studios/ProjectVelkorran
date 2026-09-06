// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/SovGameplayAbility_SeleneEcho.h"
#include "SovSelenePayloadTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class USovStillpointPayloadTestAbility : public USovGameplayAbility_SeleneStillpointGrenade
{
	GENERATED_BODY()
public:
	USovStillpointPayloadTestAbility();
};
UCLASS(Transient, NotBlueprintable)
class USovZeroPayloadTestAbility : public USovGameplayAbility_SeleneStaccatoZero
{
	GENERATED_BODY()
public:
	USovZeroPayloadTestAbility();
};
UCLASS(Transient, NotBlueprintable)
class USovWakePayloadTestAbility : public USovGameplayAbility_SeleneVeritysWake
{
	GENERATED_BODY()
public:
	USovWakePayloadTestAbility();
};
UCLASS(Transient, NotBlueprintable)
class USovDispatchPayloadTestAbility : public USovGameplayAbility_SeleneDispatch
{
	GENERATED_BODY()
public:
	USovDispatchPayloadTestAbility();
};
