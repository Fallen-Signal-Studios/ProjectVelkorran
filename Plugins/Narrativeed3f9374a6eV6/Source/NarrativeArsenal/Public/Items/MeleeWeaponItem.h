// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Items/WeaponItem.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "MeleeWeaponItem.generated.h"

/**
 * Specialized weapon that contains values pertaining to melee specific logic - in base pro this is actually empty
 * as we found it was better to store melee info like attack animations in the attack ability itself. 
 * 
 * However we/you may end up wanting to add some melee related data in here if required. 
 */
UCLASS()
class NARRATIVEARSENAL_API UMeleeWeaponItem : public UWeaponItem
{
	GENERATED_BODY()

};
