// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovShieldComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeCombatAbility.h"
#include "GameFramework/Actor.h"
#include "SovPassiveDefenseTestFixtures.generated.h"

class UNarrativeAbilitySystemComponent;
class UNarrativeAttributeSetBase;

UCLASS(Transient, NotBlueprintable)
class USovPassiveDefenseTestASC : public UNarrativeAbilitySystemComponent
{
	GENERATED_BODY()
public:
	void PublishDeathForTest() { const bool bPrevious = bIsDead; bIsDead = true; OnRep_bIsDead(bPrevious); }
};

UCLASS(Transient, NotBlueprintable)
class USovPassiveDefenseTestShield : public USovShieldComponent
{
	GENERATED_BODY()
public:
	void UseFastTimers() { RechargeDelay = 0.2f; RechargePercentPerSecond = 0.5f; RechargeTimerInterval = 0.05f; }
};

UCLASS(Transient, NotBlueprintable)
class USovPassiveDefenseTestPoise : public USovPoiseComponent
{
	GENERATED_BODY()
public:
	void UseFastTimers()
	{
		RegenerationDelay = 0.2f; RegenerationPercentPerSecond = 0.5f; RegenerationTimerInterval = 0.05f;
		BrokenFallbackDuration = 0.2f; RecoveryImmunityDuration = 0.2f;
	}
};

/**
 * An attack that overrides nothing.
 *
 * This is deliberately the least capable subclass that can exist, because that is exactly what an
 * authored Blueprint attack is: it inherits whatever the base class enforces and adds no fences of
 * its own. If break handling only works when a subclass implements it, this ability proves it.
 */
UCLASS(Transient, NotBlueprintable)
class USovPlainCombatTestAbility : public UNarrativeCombatAbility
{
	GENERATED_BODY()
public:
	USovPlainCombatTestAbility()
	{
		InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
		NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
		bRequiresAmmo = false;
	}
	int32 Activations = 0;
	int32 Cancellations = 0;
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event) override
	{
		++Activations;
		Super::ActivateAbility(Handle, Info, ActivationInfo, Event);
	}
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicate, bool bWasCancelled) override
	{
		if (bWasCancelled) { ++Cancellations; }
		Super::EndAbility(Handle, Info, ActivationInfo, bReplicate, bWasCancelled);
	}
};

/** The authored super-armour case: a committed swing that should land through a stagger. */
UCLASS(Transient, NotBlueprintable)
class USovUnstoppableCombatTestAbility : public USovPlainCombatTestAbility
{
	GENERATED_BODY()
public:
	USovUnstoppableCombatTestAbility() { bHonoursCombatInterruptions = false; }
};

/** Content-free owner with a swappable canonical ASC, matching a retained pawn handoff. */
UCLASS(Transient, NotBlueprintable)
class ASovPassiveDefenseTestActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	ASovPassiveDefenseTestActor();
	void InitializeCombat(bool bInitializeComponents = true);
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> OwnedASC;
	UPROPERTY() TObjectPtr<UNarrativeAbilitySystemComponent> ActiveASC;
	UPROPERTY() TObjectPtr<UNarrativeAttributeSetBase> Attributes;
	UPROPERTY() TObjectPtr<USovPassiveDefenseTestShield> Shield;
	UPROPERTY() TObjectPtr<USovPassiveDefenseTestPoise> Poise;
	TFunction<void()> OnNextShieldChange;
	TFunction<void()> OnNextPoiseStateChange;
	int32 ShieldBreaks = 0;
	int32 PoiseBreaks = 0;
	int32 PoiseRecoveries = 0;
	UFUNCTION(CallInEditor) void ObserveShield(float OldShield, float NewShield, float MaxShield);
	UFUNCTION(CallInEditor) void ObservePoise(ESovPoiseState Previous, ESovPoiseState Current);
	UFUNCTION(CallInEditor) void ObserveShieldBreak() { ++ShieldBreaks; }
	UFUNCTION(CallInEditor) void ObservePoiseBreak() { ++PoiseBreaks; }
	UFUNCTION(CallInEditor) void ObservePoiseRecovery() { ++PoiseRecoveries; }
	UFUNCTION(CallInEditor) void InitializeResourcesOnRevive(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bDead);
};
