// Copyright Narrative Tools 2024. 


#include "Settings/NarrativeCombatDeveloperSettings.h"

#define LOCTEXT_NAMESPACE

UNarrativeCombatDeveloperSettings::UNarrativeCombatDeveloperSettings()
{
	bEnableDamageNumbers = true; 
	bEnableDamageNumberOnSelf = true;

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
}

int32 UNarrativeCombatDeveloperSettings::GetAttackTokensForDifficulty(ENarrativeGameplayDifficulty Difficulty) const
{
	if (AvailableAttackTokens.Contains(Difficulty))
	{
		return AvailableAttackTokens[Difficulty];
	}

	return INT_MAX;
}

float UNarrativeCombatDeveloperSettings::GetAttackFrequencyForDifficulty(ENarrativeGameplayDifficulty Difficulty) const
{
	if (NPCAttackFrequencies.Contains(Difficulty))
	{
		return NPCAttackFrequencies[Difficulty];
	}

	return 1.f;
}

#undef LOCTEXT_NAMESPACE