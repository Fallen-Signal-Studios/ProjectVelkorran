// Copyright Narrative Tools 2024. 


#include "Items/AmmoItem.h"
#include "Items/WeaponItem.h"

UAmmoItem::UAmmoItem()
{

}

void UAmmoItem::AddedToInventory(class UNarrativeInventoryComponent* Inventory, const bool bFromLoad)
{
	Super::AddedToInventory(Inventory, bFromLoad);

	//Ask all wielded weapons to init their ammo source, this might be valid ammo. 
	if (!bFromLoad && Inventory && !Inventory->IsVendor())
	{
		if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
		{
			for (auto& Weapon : CharacterOwner->GetWieldedWeapons())
			{
				if (Weapon)
				{	
					Weapon->InitAmmoSource();
				}
			}
		}
	}
}
