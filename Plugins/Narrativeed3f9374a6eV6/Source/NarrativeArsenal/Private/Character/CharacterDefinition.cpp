// Copyright Narrative Tools 2024. 


#include "Character/CharacterDefinition.h"

UCharacterDefinition::UCharacterDefinition()
{
	AssetType = TEXT("CharacterDefinition");

	CharacterID = FName(GetName());
	
	AttackPriority = 1.f;
}
