// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Components/SovWeakPointComponent.h"
#include "GameplayEffect.h"
#include "SovCombatRoutingTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class USovDamagePublicationRepairObserver : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<class UNarrativeAbilitySystemComponent> TargetASC;
	bool bRestoreOnTargetResult = false;
	bool bLastSourceFatal = false;
	int32 SourceResults = 0;
	UFUNCTION() void OnTargetResult(const FSovDamageResult& Result);
	UFUNCTION() void OnSourceResult(const FSovDamageResult& Result);
};

UCLASS(Transient, NotBlueprintable)
class USovCombatRoutingTestEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovCombatRoutingTestEffect();
};

UCLASS(Transient, NotBlueprintable)
class USovCombatDirectPoiseTestEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovCombatDirectPoiseTestEffect();
};

UCLASS(Transient, NotBlueprintable)
class USovWeakPointRoutingTestComponent : public USovWeakPointComponent
{
	GENERATED_BODY()
public:
	USovWeakPointRoutingTestComponent();
	void Observe();
	bool bResetOnBreakNotification = false;
	int32 DetailedBreakCount = 0;
	UFUNCTION() void ObserveState(FName Id, bool bBroken);
	UFUNCTION() void ObserveBreak(FName Id, const FSovDamageResult& Result);
};

UCLASS(Transient, NotBlueprintable)
class USovWeakPointFireTestAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	USovWeakPointFireTestAbility();
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};

UCLASS(Transient, NotBlueprintable)
class USovWeakPointMeleeTestAbility : public USovWeakPointFireTestAbility
{
	GENERATED_BODY()
public:
	USovWeakPointMeleeTestAbility();
};
