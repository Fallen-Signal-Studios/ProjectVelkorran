// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Abilities/SovGameplayAbility_Echo.h"
#include "GAS/NarrativeAbilityInputMapping.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "SovCombatReadinessTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class USovHUDReadinessTestAbility : public USovGameplayAbility_EchoBase
{
    GENERATED_BODY()
public:
    USovHUDReadinessTestAbility();
    static int32 ActivationQueries;
    virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
        const FGameplayTagContainer* SourceTags=nullptr, const FGameplayTagContainer* TargetTags=nullptr,
        FGameplayTagContainer* Relevant=nullptr) const override;
    virtual const FGameplayTagContainer* GetCooldownTags() const override { return &CooldownTags; }
private:
    FGameplayTagContainer CooldownTags;
};

UCLASS(Transient, NotBlueprintable)
class USovHUDReadinessCooldown : public UGameplayEffect
{
    GENERATED_BODY()
public:
    USovHUDReadinessCooldown();
};

UCLASS(Transient, NotBlueprintable)
class ASovHUDReadinessTestController : public ASovHandoffRuntimeTestController
{
    GENERATED_BODY()
public:
    ASovHUDReadinessTestController(const FObjectInitializer& Init) : Super(Init) {}
    void SetInputSchema(UNarrativeAbilityInputMapping* Schema) { AbilityInputMappings=Schema; }
};
