// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeCombatAbility.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "SovBotAttackTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class USovBotTestASC : public UNarrativeAbilitySystemComponent
{
	GENERATED_BODY()
public:
	virtual int32 GetNumAttackTokens() const override { return TestTokenBudget; }
	int32 TestTokenBudget = 1;
};

UCLASS(Transient, NotBlueprintable, NotPlaceable)
class ASovBotTestCharacter : public ANarrativeNPCCharacter
{
	GENERATED_BODY()
public:
	ASovBotTestCharacter(const FObjectInitializer& Initializer);
	void InitializeTestCombat(int32 Team);
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	virtual void GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const override;
	int32 TestTeam = 0;
};

UCLASS(Transient, NotBlueprintable)
class USovBotTestAttackAlpha : public UNarrativeCombatAbility
{
	GENERATED_BODY()
public:
	USovBotTestAttackAlpha();
	int32 ActivationCount = 0;
	int32 ReleaseCount = 0;
	float MinimumTestRange = 0.f;
	bool bRejectActivation = false;
	virtual float GetBotAttackMinimumRange_Implementation() const override { return MinimumTestRange; }
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* FailureTags = nullptr) const override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	void FinishTestAttack();
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event) override;
};

UCLASS(Transient, NotBlueprintable)
class USovBotTestAttackBeta : public USovBotTestAttackAlpha
{
	GENERATED_BODY()
public:
	USovBotTestAttackBeta();
};

UCLASS(Transient, NotBlueprintable)
class USovBotTestAttackPeer : public USovBotTestAttackAlpha
{
	GENERATED_BODY()
};
