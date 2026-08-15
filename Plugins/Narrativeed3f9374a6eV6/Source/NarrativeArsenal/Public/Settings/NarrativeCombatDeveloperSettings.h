// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "NarrativeCombatDeveloperSettings.generated.h"

/**
 * Combat related developer settings 
 */
UCLASS(BlueprintType, config = Engine, defaultconfig, meta = (DisplayName="Narrative - Combat Settings"))
class NARRATIVEARSENAL_API UNarrativeCombatDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:

	UNarrativeCombatDeveloperSettings();

	//**If true, damage dealt popups will appear above enemies you deal damage to. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|FX")
	bool bEnableDamageNumbers;

	//**If true, damage dealt popups will appear above your own player as you take damage. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|FX")
	bool bEnableDamageNumberOnSelf;

	//** The amount of attack tokens to grant the player on each difficulty mode. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Attack Tokens")
	TMap<ENarrativeGameplayDifficulty, int32> AvailableAttackTokens;

	//** Token stealers must be this percent of the existing distance to steal a token. IE 0.2 = steal a token if we are 0.2x the distance from the target as an existing token. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Attack Tokens", meta = (ClampMin=0.01, ClampMax=1.0))
	float StealTokenProximity;

	/** Tokens more than this many seconds old can be stolen  - nice way of giving others a chance to attack for a bit. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Attack Tokens", meta = (ClampMin=0.01))
	float TokenStealableAgeSeconds;

	//** AI use this to decide how often they should fire at each difficulty. This is a multiplier of RateOfFire. IE 3.0 = attack at 3x the rate of fire */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|AI")
	TMap<ENarrativeGameplayDifficulty, float> NPCAttackFrequencies;

	/** If an NPC starts attacking an enemy, their teammates within this amount of distance will be notified to attack also.
	ie You can turn this number down to prevent the whole city getting into fights where you might only want teammates within 50 meters! */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|AI", meta = (ClampMin=10))
	float NotifyTeammatesToFightRange;

	//** We'll sample this number of anim samples when reading through a melee attack. Lower numbers give better performance but less accuracy. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|FX", meta = (ClampMin=1, ClampMax=100))
	int32 MeleeCombatAnimSampleAmount;

	//** Whether or not we'll allow aligned factions to damage each other. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|General")
	bool bAllowFriendlyFire;

	int32 GetAttackTokensForDifficulty(ENarrativeGameplayDifficulty Difficulty) const;

	UFUNCTION(BlueprintPure, Category = "Attack Frequency")
	float GetAttackFrequencyForDifficulty(ENarrativeGameplayDifficulty Difficulty) const;

};
