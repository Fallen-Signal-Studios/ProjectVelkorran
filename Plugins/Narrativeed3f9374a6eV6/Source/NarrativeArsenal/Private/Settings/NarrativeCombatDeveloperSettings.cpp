// Copyright Narrative Tools 2024.

#include "Settings/NarrativeCombatDeveloperSettings.h"

UNarrativeCombatDeveloperSettings::UNarrativeCombatDeveloperSettings()
{
	bEnableDamageNumbers = false;
	bEnableDamageNumberOnSelf = false;

	AvailableAttackTokens.Add(ENarrativeGameplayDifficulty::Easy, 1);
	AvailableAttackTokens.Add(ENarrativeGameplayDifficulty::Medium, 2);
	AvailableAttackTokens.Add(ENarrativeGameplayDifficulty::Hard, 4);
	AvailableAttackTokens.Add(ENarrativeGameplayDifficulty::Insane, 6);

	NPCAttackFrequencies.Add(ENarrativeGameplayDifficulty::Easy, 5.f);
	NPCAttackFrequencies.Add(ENarrativeGameplayDifficulty::Medium, 2.5f);
	NPCAttackFrequencies.Add(ENarrativeGameplayDifficulty::Hard, 1.7f);
	NPCAttackFrequencies.Add(ENarrativeGameplayDifficulty::Insane, 1.f);

	StealTokenProximity = 0.4f;
	TokenStealableAgeSeconds = 10.f;
	NotifyTeammatesToFightRange = 3500.f;
	MeleeCombatAnimSampleAmount = 30;
	bAllowFriendlyFire = false;

	MinimumDamageMultiplier = 0.05f;
	MaximumDamageMultiplier = 3.0f;
	DefaultPartialShieldBypassRatio = 0.5f;
	GuardHalfAngleDegrees = 70.0f;
	GuardDamageMultiplier = 0.25f;
	GuardPoiseMultiplier = 0.25f;
	GuardStaminaDamageScalar = 0.5f;
	MinimumGuardStaminaDamage = 8.0f;
	MaximumGuardStaminaDamage = 20.0f;
}

int32 UNarrativeCombatDeveloperSettings::GetAttackTokensForDifficulty(const ENarrativeGameplayDifficulty Difficulty) const
{
	if (const int32* Tokens = AvailableAttackTokens.Find(Difficulty))
	{
		return *Tokens;
	}

	return INT_MAX;
}

float UNarrativeCombatDeveloperSettings::GetAttackFrequencyForDifficulty(const ENarrativeGameplayDifficulty Difficulty) const
{
	if (const float* Frequency = NPCAttackFrequencies.Find(Difficulty))
	{
		return *Frequency;
	}

	return 1.f;
}
