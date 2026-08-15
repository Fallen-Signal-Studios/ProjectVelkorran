// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "NarrativeGASStatics.generated.h"

/**
 * Extra useful Narrative GAS functions that don't ship with vanilla GAS. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeGASStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	/** Get dynamic tags from an effect spec */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect")
	static FGameplayTagContainer GetDynamicGrantedTagsFromEffectSpec(const FGameplayEffectSpec& Spec);
	
	/** Get dynamic tags from an effect spec */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect")
	static FGameplayTagContainer GetDynamicAssetTagsFromEffectSpec(const FGameplayEffectSpec& Spec);
		
	/** Get all tags from an effect spec */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect")
	static FGameplayTagContainer GetAllAssetTagsFromEffectSpec(const FGameplayEffectSpec& Spec);

	/** Check whether a handle is valid or not (BP doesnt allow this which is dumb ) */
	UFUNCTION(BlueprintPure, Category = "Ability|GameplayEffect")
	static bool IsEffectHandleValid(const FActiveGameplayEffectHandle& Handle);

};
