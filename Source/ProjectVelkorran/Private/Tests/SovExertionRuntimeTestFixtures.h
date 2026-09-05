// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Characters/SovPlayerCharacterBase.h"
#include "GAS/NarrativeCombatAbility.h"
#include "SovExertionRuntimeTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class ASovExertionRuntimeTestCharacter : public ASovPlayerCharacterBase
{
	GENERATED_BODY()
public:
	ASovExertionRuntimeTestCharacter(const FObjectInitializer& Initializer);
	void InitializeExertion(bool bSelene = false);
	virtual FGameplayTag GetProtagonistIdentityTag() const override;
	bool bCancelOnSpend = false;
	UFUNCTION() void ObserveSpend(float Amount, float Remaining);
private:
	bool bTestSelene = false;
};

UCLASS(Transient, NotBlueprintable)
class USovExertionRuntimeChargedAbility : public UNarrativeCombatAbility
{
	GENERATED_BODY()
public:
	USovExertionRuntimeChargedAbility();
	void DispatchTestHit();
	bool AdvanceTestNode() { return BeginNextSovCombatAttack(); }
	int32 Dispatches = 0;
protected:
	virtual void HandleTargetData_Implementation(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag Tag) override;
};

UCLASS(Transient, NotBlueprintable)
class USovInputReentryTestAbility : public UNarrativeGameplayAbility
{
	GENERATED_BODY()
public:
	USovInputReentryTestAbility();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event) override;
	virtual void InputReleased(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		FGameplayAbilityActivationInfo Activation) override;
	int32 ReplicatedReleases = 0;
private:
	void ObserveReplicatedRelease() { ++ReplicatedReleases; }
	bool bReenterOnce = true;
};
