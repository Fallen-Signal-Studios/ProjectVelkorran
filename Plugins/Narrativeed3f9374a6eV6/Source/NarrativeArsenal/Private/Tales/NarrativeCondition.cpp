// Copyright Narrative Tools 2022. 


#include "Tales/NarrativeCondition.h"
#include "Character/CharacterDefinition.h"
#include "Character/PlayerDefinition.h"
#include "AI/NPCDefinition.h"

UNarrativeCondition::UNarrativeCondition()
{
	ConditionFilter = EConditionFilter::CF_AnyCharacter;
	PartyConditionPolicy = EPartyConditionPolicy::None;
}

bool UNarrativeCondition::CheckCondition_Implementation(APawn* Pawn, APlayerController* Controller, class UTalesComponent* NarrativeComponent)
{
	return true;
}

FString UNarrativeCondition::GetGraphDisplayText_Implementation()
{
	return GetName();
}

TArray<class UCharacterDefinition*> UNarrativeCondition::GetCharacterTargets() const
{
	
	//Doing some pretty gross stuff in here efficiencywise but we're calling this infrequently on a tiny array size 
	if (ConditionFilter == EConditionFilter::CF_DontTarget)
	{
		return {};
	}
	else if (ConditionFilter == EConditionFilter::CF_AnyCharacter)
	{
		return CharacterTargets;
	}
	else if(ConditionFilter == EConditionFilter::CF_OnlyNPCs)
	{
		TArray<class UCharacterDefinition*> CharDefs;
		for (auto& NPCTarget : NPCTargets)
		{
			CharDefs.Add(NPCTarget);
		}
		return CharDefs;
	}
	else
	{
		TArray<class UCharacterDefinition*> CharDefs;
		for (auto& PlayerTarget : PlayerTargets)
		{
			CharDefs.Add(PlayerTarget);
		}
		return CharDefs;
	}
}
