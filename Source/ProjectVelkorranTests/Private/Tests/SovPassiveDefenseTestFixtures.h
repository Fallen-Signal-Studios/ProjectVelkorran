// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovShieldComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
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
