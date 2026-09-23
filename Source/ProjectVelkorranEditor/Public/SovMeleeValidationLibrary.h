// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/SovCombatTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovMeleeValidationLibrary.generated.h"

class UAbilitySystemComponent;
class ANarrativeNPCCharacter;
class ANarrativePlayerController;
class UNPCDefinition;
class USovGameplayAbility_EchoBase;

/** Editor-only access to the live combat paths needed by PIE validation probes. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovMeleeValidationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Class names of every ability granted on the component whose input tag is exactly InputTag. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Melee Validation")
    static TArray<FString> GrantedAbilityClassesForInput(UAbilitySystemComponent* AbilitySystem, FGameplayTag InputTag);

    /** The live Echo instance for one semantic input, if the cast is still active. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Echo Validation")
    static USovGameplayAbility_EchoBase* ActiveEchoAbilityForInput(UAbilitySystemComponent* AbilitySystem, FGameplayTag InputTag);

    /** The damage packet's source object class and, when that object lives in an ability, the ability's class. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Melee Validation")
    static void DescribeDamageSource(const FSovDamageResult& Result, FString& SourceObjectClass, FString& SourceAbilityClass);

    /** One line per granted native melee ability: class, active, current node, montage playing on the avatar. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Melee Validation")
    static TArray<FString> DescribeNativeMelee(UAbilitySystemComponent* AbilitySystem);

    /** Presses a semantic input tag through the controller's own routing, exactly as a bound key does.
     * A probe cannot reach ANarrativePlayerController::AbilityInputPressed, which is not reflected. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Melee Validation")
    static bool PressAndReleaseSemanticInput(ANarrativePlayerController* PlayerController, FGameplayTag InputTag);

    /** Keep a charge input held so live PIE can interrupt it before release. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Echo Validation")
    static bool PressSemanticInput(ANarrativePlayerController* PlayerController, FGameplayTag InputTag);

    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Echo Validation")
    static bool ReleaseSemanticInput(ANarrativePlayerController* PlayerController, FGameplayTag InputTag);

    /** One line per input action the live controller maps: action asset, semantic tag, bound keys. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Melee Validation")
    static TArray<FString> DescribeInputRouting(ANarrativePlayerController* PlayerController);

    /** Spawns an NPC through Narrative's own character subsystem in a play world, for a live melee target. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Melee Validation", meta=(WorldContext="WorldContext"))
    static ANarrativeNPCCharacter* SpawnValidationNPC(UObject* WorldContext, UNPCDefinition* Definition, FTransform Transform);
};
