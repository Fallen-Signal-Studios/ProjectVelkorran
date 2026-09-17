// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/SovCombatTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovMeleeValidationLibrary.generated.h"

class UAbilitySystemComponent;
class ANarrativeNPCCharacter;
class UNPCDefinition;

/** Editor-only, read-only answers a melee play probe needs that script bindings cannot reach. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovMeleeValidationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Class names of every ability granted on the component whose input tag is exactly InputTag. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Melee Validation")
    static TArray<FString> GrantedAbilityClassesForInput(UAbilitySystemComponent* AbilitySystem, FGameplayTag InputTag);

    /** The damage packet's source object class and, when that object lives in an ability, the ability's class. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Melee Validation")
    static void DescribeDamageSource(const FSovDamageResult& Result, FString& SourceObjectClass, FString& SourceAbilityClass);

    /** One line per granted native melee ability: class, active, current node, montage playing on the avatar. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Melee Validation")
    static TArray<FString> DescribeNativeMelee(UAbilitySystemComponent* AbilitySystem);

    /** Spawns an NPC through Narrative's own character subsystem in a play world, for a live melee target. */
    UFUNCTION(BlueprintCallable, Category="Velkorran|Editor|Melee Validation", meta=(WorldContext="WorldContext"))
    static ANarrativeNPCCharacter* SpawnValidationNPC(UObject* WorldContext, UNPCDefinition* Definition, FTransform Transform);
};
