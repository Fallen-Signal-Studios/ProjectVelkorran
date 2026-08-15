// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "NarrativeItem.h"
#include "AmmoItem.generated.h"

/**
 * Base class for ammo related items in Narrative Pro.
 * technically any item can be ammo, but in here we add some extra logic for auto-loading the ammo as the ammo source
 */
UCLASS()
class NARRATIVEARSENAL_API UAmmoItem : public UNarrativeItem
{
	GENERATED_BODY()
	
	UAmmoItem();

	virtual void AddedToInventory(class UNarrativeInventoryComponent* Inventory, const bool bFromLoad);

};
