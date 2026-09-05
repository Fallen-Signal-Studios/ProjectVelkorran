// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "Abilities/SovGameplayAbility_ReformationDrone.h"
#include "Abilities/SovGameplayAbility_SeleneDeflection.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "SovDefenseLifecycleTestFixtures.generated.h"

class USovShieldComponent;
class USovPoiseComponent;

/** Real ASC, resource delegates, tag ownership and GAS activation; no content required. */
UCLASS(Transient, NotBlueprintable)
class ASovDefenseLifecycleTestCharacter : public ASovAxiomRuntimeTestCharacter
{
	GENERATED_BODY()
public:
	ASovDefenseLifecycleTestCharacter(const FObjectInitializer& ObjectInitializer);
	void InitializeDefenseCombat();
	UPROPERTY() TObjectPtr<USovShieldComponent> TestShield;
	UPROPERTY() TObjectPtr<USovPoiseComponent> TestPoise;
	FGameplayAbilitySpecHandle DeflectionHandle;
	bool bCancelDeflectionOnStart = false;
	int32 DeflectionStartCount = 0;
	int32 DeflectionCloseCount = 0;
	TWeakObjectPtr<ASovDefenseLifecycleTestCharacter> BlastSourceToInterrupt;
	UFUNCTION() void InterruptBlastSource(const FSovDamageResult& Result);
	UFUNCTION() void ObserveDeflectionStarted();
	UFUNCTION() void ObserveDeflectionClosed();
};

/** Intercepts the actual Blueprint hook, so an aborted start cannot hide a continuation. */
UCLASS(Transient, NotBlueprintable)
class USovDefenseLifecycleDeflection : public USovGameplayAbility_SeleneDeflection
{
	GENERATED_BODY()
public:
	virtual void ProcessEvent(UFunction* Function, void* Parameters) override;
	int32 StartedHookCount = 0;
};

UCLASS(Transient, NotBlueprintable)
class USovDefenseLifecycleRocket : public USovGameplayAbility_ReformationDroneRocketLauncher
{
	GENERATED_BODY()
public:
	USovDefenseLifecycleRocket();
	virtual void ProcessEvent(UFunction* Function, void* Parameters) override;
	bool bInterruptOnRelease = false;
	int32 ReleasedHookCount = 0;
};

UCLASS(Transient, NotBlueprintable)
class USovDefenseLifecycleSelfDestruct : public USovGameplayAbility_ReformationDroneSelfDestruct
{
	GENERATED_BODY()
public:
	USovDefenseLifecycleSelfDestruct()
	{
		bAutoReleasePayload = false; CooldownDuration = 0.f;
		bOnlyAcquirePlayerControlledTargets = false;
		bExplosionRequiresLineOfSight = false;
	}
};
