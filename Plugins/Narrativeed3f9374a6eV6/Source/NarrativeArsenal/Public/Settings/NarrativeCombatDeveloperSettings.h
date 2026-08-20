// Copyright Narrative Tools 2024.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "NarrativeCombatDeveloperSettings.generated.h"

/** Combat-related project settings shared by Narrative and Sovereign systems. */
UCLASS(BlueprintType, config = Engine, defaultconfig, meta = (DisplayName = "Narrative - Combat Settings"))
class NARRATIVEARSENAL_API UNarrativeCombatDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNarrativeCombatDeveloperSettings();

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|FX")
	bool bEnableDamageNumbers;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|FX")
	bool bEnableDamageNumberOnSelf;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Attack Tokens")
	TMap<ENarrativeGameplayDifficulty, int32> AvailableAttackTokens;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Attack Tokens", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float StealTokenProximity;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Attack Tokens", meta = (ClampMin = "0.01"))
	float TokenStealableAgeSeconds;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|AI")
	TMap<ENarrativeGameplayDifficulty, float> NPCAttackFrequencies;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|AI", meta = (ClampMin = "10.0"))
	float NotifyTeammatesToFightRange;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Melee", meta = (ClampMin = "2", ClampMax = "100"))
	int32 MeleeCombatAnimSampleAmount;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Damage")
	bool bAllowFriendlyFire;

	/** Bounds the final combined Armor/resistance/authored mitigation scalar. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumDamageMultiplier;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Damage", meta = (ClampMin = "1.0"))
	float MaximumDamageMultiplier;

	/** Default ratio used by the partial-bypass tag when a spec omits the SetByCaller. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultPartialShieldBypassRatio;

	/** Half-angle of Tarrik's frontal guard plane. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Guard", meta = (ClampMin = "0.0", ClampMax = "180.0", ForceUnits = "Degrees"))
	float GuardHalfAngleDegrees;

	/** Fraction of ordinary damage retained after a successful standard guard. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Guard", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GuardDamageMultiplier;

	/** Fraction of ordinary Poise damage retained after a successful standard guard. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Guard", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GuardPoiseMultiplier;

	/** Minimum current Stamina required to enter Guard. This is a threshold, not an activation cost. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Guard", meta = (ClampMin = "0.0"))
	float MinimumGuardStartStamina;

	/** Fixed Stamina paid by a successful perfect defense. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Guard", meta = (ClampMin = "0.0"))
	float PerfectGuardStaminaDamage;

	/** Converts resolved damage into guard Stamina impact before clamping. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Guard", meta = (ClampMin = "0.0"))
	float GuardStaminaDamageScalar;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Guard", meta = (ClampMin = "0.0"))
	float MinimumGuardStaminaDamage;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat|Guard", meta = (ClampMin = "0.0"))
	float MaximumGuardStaminaDamage;

	int32 GetAttackTokensForDifficulty(ENarrativeGameplayDifficulty Difficulty) const;

	UFUNCTION(BlueprintPure, Category = "Attack Frequency")
	float GetAttackFrequencyForDifficulty(ENarrativeGameplayDifficulty Difficulty) const;
};
