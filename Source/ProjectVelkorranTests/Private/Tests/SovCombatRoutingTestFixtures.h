// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Components/SovWeakPointComponent.h"
#include "GameplayEffect.h"
#include "GAS/SovDamageSourcePolicy.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "SovCombatRoutingTestFixtures.generated.h"

/** Real delegate receiver. CallInEditor is needed by the non-BeginPlay Mac fixture. */
UCLASS(Transient, NotBlueprintable)
class USovDamagePublicationRepairObserver : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<class UNarrativeAbilitySystemComponent> TargetASC;
	UPROPERTY() TObjectPtr<class UNarrativeAbilitySystemComponent> SourceASC;
	UPROPERTY() TObjectPtr<AActor> ReplacementAvatar;
	UPROPERTY() TObjectPtr<class USovStatusComponent> StatusComponent;
	UPROPERTY() TArray<FSovStatusApplicationRequest> StatusRequests;
	bool bRestoreOnTargetResult = false;
	bool bRestoreSourceOnTargetResult = false;
	bool bRestoreOnFirstStatus = false;
	bool bRestoreAvatarAfterReplacement = false;
	bool bStatusPresentBeforeFirstRequest = false;
	bool bLastSourceFatal = false;
	int32 SourceResults = 0;
	FSovDamageResult LastSourceResult;
	UFUNCTION(CallInEditor) void OnTargetResult(const FSovDamageResult& Result);
	UFUNCTION(CallInEditor) void OnSourceResult(const FSovDamageResult& Result);
	UFUNCTION(CallInEditor) void OnStatusRequest(const FSovStatusApplicationRequest& Request);
};

/** Approval of a capped budget is itself a limit, even when no value changes. */
UCLASS(Transient, NotBlueprintable)
class USovIdentityDamagePolicyTestComponent : public UActorComponent, public ISovDamageSourcePolicy
{
	GENERATED_BODY()
public:
	mutable int32 PolicyCalls = 0;
	mutable float ApprovedShieldDamage = -1.f;
	mutable float ApprovedHealthDamage = -1.f;
	virtual bool LimitSovDamage(AActor* Target, const FGameplayEffectContextHandle& Context,
		float& InOutShieldDamage, float& InOutHealthDamage, float& InOutPoiseDamage) const override;
};

/** Exercises an adversarial virtual team admission callback, without content or play. */
UCLASS(Transient, NotBlueprintable)
class ASovCombatAdmissionTestCharacter : public ASovAxiomRuntimeTestCharacter
{
	GENERATED_BODY()
public:
	ASovCombatAdmissionTestCharacter(const FObjectInitializer& ObjectInitializer);
	mutable TFunction<void()> OnNextTeamQuery;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
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
	UFUNCTION(CallInEditor) void ObserveState(FName Id, bool bBroken);
	UFUNCTION(CallInEditor) void ObserveBreak(FName Id, const FSovDamageResult& Result);
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
