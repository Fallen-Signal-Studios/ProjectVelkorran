// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AbilityConfiguration.generated.h"

/**
 * Contains some default abilities to grant, attributes, etc. 
 */
UCLASS()
class NARRATIVEARSENAL_API UAbilityConfiguration : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UAbilityConfiguration(const FObjectInitializer& ObjectInitializer);

	// Default attributes for a character for initializing on spawn/respawn.
	// This is an instant GE that overrides the values for attributes that get reset on spawn/respawn.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Narrative|Abilities")
	TSubclassOf<class UGameplayEffect> DefaultAttributes;

	// These effects are only applied one time on startup
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Narrative|Abilities")
	TArray<TSubclassOf<class UGameplayEffect>> StartupEffects;

	// Default abilities to grant the player  
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Narrative|Abilities")
	TArray<TSubclassOf<class UNarrativeGameplayAbility>> DefaultAbilities;

};
