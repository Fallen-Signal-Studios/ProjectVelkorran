// Copyright Narrative Tools 2024. 


#include "AI/NPCDefinition.h"
#include "GAS/AbilityConfiguration.h"
#include "AI/Activities/NPCActivityConfiguration.h"

UNPCDefinition::UNPCDefinition()
{

	AssetType = TEXT("NPCDefinition");

	bAllowMultipleInstances = false;

	//Try auto-populate some info 
	NPCID = FName(GetName());
	NPCName = FText::FromString(GetName());
	UniqueNPCGUID = FGuid::NewGuid();
	MinLevel = 1;
	MaxLevel = 1;
	CharacterID = NPCID;

	FSoftObjectPath SoftPath(TEXT("/Script/Engine.Blueprint'/NarrativePro/Pro/Core/AI/BP/BP_NarrativeNPC.BP_NarrativeNPC_C'"));

	//Moved to this instead of class finder as it was loading ABP and causing errors with Locomotion Animation library 
	NPCClassPath = SoftPath;

	//auto ActivityConfigFinder = ConstructorHelpers::FObjectFinder<UNPCActivityConfiguration>(TEXT("/Script/NarrativeArsenal.NPCActivityConfiguration'/NarrativePro/AI/ActivityConfigurations/AC_NPC_Default.AC_NPC_Default'"));
	//if (ActivityConfigFinder.Succeeded())
	//{
	//	ActivityConfiguration = ActivityConfigFinder.Object;
	//}

	//auto AbilityConfigFinder = ConstructorHelpers::FObjectFinder<UAbilityConfiguration>(TEXT("/Script/NarrativeArsenal.AbilityConfiguration'/NarrativePro/Abilities/Configurations/AC_NPC_Default.AC_NPC_Default'"));
	//if (AbilityConfigFinder.Succeeded())
	//{
	//	AbilityConfiguration = AbilityConfigFinder.Object;
	//}
}
