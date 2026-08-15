// Copyright Narrative Tools 2024. 


#include "Character/PlayerDefinition.h"
#include "GAS/AbilityConfiguration.h"

UPlayerDefinition::UPlayerDefinition()
{
	AssetType = TEXT("PlayerDefinition");
	

	//
	//auto AbilityConfigFinder = ConstructorHelpers::FObjectFinder<UAbilityConfiguration>(TEXT("/Script/NarrativeArsenal.AbilityConfiguration'/NarrativePro/Abilities/Configurations/AC_Player_Default.AC_Player_Default'"));
	//if (AbilityConfigFinder.Succeeded())
	//{
	//	AbilityConfiguration = AbilityConfigFinder.Object;
	//}
}
