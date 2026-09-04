// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "Abilities/SovGameplayAbility_TarrikGuard.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "SovGuardRuntimeTestFixtures.generated.h"

class USovGuardComponent;

/** Reuses the existing real ASC/attribute fixture; requires no cooked content. */
UCLASS(Transient, NotBlueprintable)
class ASovGuardRuntimeTestCharacter : public ASovAxiomRuntimeTestCharacter
{
	GENERATED_BODY()
public:
	ASovGuardRuntimeTestCharacter(const FObjectInitializer& ObjectInitializer);
	void InitializeGuardCombat();
	UPROPERTY()
	TObjectPtr<USovGuardComponent> TestGuard;
	bool bInterruptGuardOnStart = false;
	int32 GuardStartCount = 0;
	int32 GuardEndCount = 0;
	UFUNCTION()
	void ObserveGuardStart();
	UFUNCTION()
	void ObserveGuardEnd();
};

/** Models an existing Blueprint child that owns Busy during its activation. */
UCLASS(Transient, NotBlueprintable)
class USovGuardRuntimeBusyTestAbility : public USovGameplayAbility_TarrikGuard
{
	GENERATED_BODY()
public:
	USovGuardRuntimeBusyTestAbility();
};
