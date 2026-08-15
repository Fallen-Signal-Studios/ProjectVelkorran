// Copyright Narrative Tools 2022. 

#include "Components/EquipmentComponent.h"
#include <GroomComponent.h>
#include "Components/SkeletalMeshComponent.h"
#include "Items/WeaponItem.h"


// Sets default values for this component's properties
UEquipmentComponent::UEquipmentComponent()
{
	HolsterSlots.AddTag(FGameplayTag::RequestGameplayTag("Narrative.Equipment.Slot.Weapon.HipLeft", false));
	HolsterSlots.AddTag(FGameplayTag::RequestGameplayTag("Narrative.Equipment.Slot.Weapon.HipRight", false));
	HolsterSlots.AddTag(FGameplayTag::RequestGameplayTag("Narrative.Equipment.Slot.Weapon.BackA", false));
	HolsterSlots.AddTag(FGameplayTag::RequestGameplayTag("Narrative.Equipment.Slot.Weapon.BackB", false));

	WieldSlots.AddTag(FGameplayTag::RequestGameplayTag("Narrative.Equipment.WieldSlot.Mainhand", false));
	WieldSlots.AddTag(FGameplayTag::RequestGameplayTag("Narrative.Equipment.WieldSlot.Offhand", false));
}

void UEquipmentComponent::Initialize(TMap<FGameplayTag, USkeletalMeshComponent*> ClothingMeshes, class USkeletalMeshComponent* InLeaderPoseComponent)
{
	LeaderPoseComponent = InLeaderPoseComponent;
}

class UEquippableItem* UEquipmentComponent::GetEquippedItemAtSlot(const FGameplayTag Slot)
{
	if (EquippedItems.Contains(Slot))
	{
		return *EquippedItems.Find(Slot);
	}
	else
	{
		return nullptr;
	}
}

TArray<class UEquippableItem*> UEquipmentComponent::GetItemsWithSlot(const FGameplayTag Slot)
{
	TArray<class UEquippableItem*> Items;

	for (auto& EquippedItemKP : EquippedItems)
	{
		if (EquippedItemKP.Key.MatchesTag(Slot))
		{
			Items.Add(EquippedItemKP.Value);
		}
	}

	return Items;
}

TArray<class UWeaponItem*> UEquipmentComponent::GetWieldedWeapons()
{
	TArray<class UWeaponItem*> OutWeapons;
	WieldedWeapons.GenerateValueArray(OutWeapons);
	return OutWeapons;
}

bool UEquipmentComponent::IsDualWielding() const
{
	return WieldedWeapons.Num() > 1;
}

void UEquipmentComponent::GetEquippedItemsOfClass(TSubclassOf<UEquippableItem> EquippableClass, TArray<UEquippableItem*>& OutEquippables)
{
	for (auto& EquippedItemKP : EquippedItems)
	{
		if (UEquippableItem* Equippable = EquippedItemKP.Value)
		{
			if (Equippable->GetClass()->IsChildOf(EquippableClass))
			{
				OutEquippables.Add(Equippable);
			}
		}
	}
}

class UWeaponItem* UEquipmentComponent::GetEquippedWeaponAtSlot(const FGameplayTag Slot)
{
	return Cast<UWeaponItem>(GetEquippedItemAtSlot(Slot));
}


class UWeaponItem* UEquipmentComponent::GetWieldedWeaponAtSlot(const FGameplayTag Slot)
{
	if (WieldedWeapons.Contains(Slot))
	{
		return WieldedWeapons[Slot];
	}

	return nullptr; 
}

class UGroomComponent* UEquipmentComponent::GetGroomComponentAtSlot(const FGameplayTag Slot)
{
	if (GroomComponents.Contains(Slot))
	{
		return *GroomComponents.Find(Slot);
	}
	return nullptr; 
}

float UEquipmentComponent::GetEquippedItemsWeight() const
{
	float TotalWeight = 0.f;

	for (auto& EquippedItemKP : EquippedItems)
	{
		if (EquippedItemKP.Value)
		{
			TotalWeight += EquippedItemKP.Value->Weight;
		}
	}

	return TotalWeight;
}

FGameplayTag UEquipmentComponent::GetFirstFreeSlot(const FGameplayTagContainer& SlotsToCheck)
{
	TArray<FGameplayTag> Tags;
	SlotsToCheck.GetGameplayTagArray(Tags);

	//Lets also equip the item on add. First, we need to find the first free slot this weapon supports
	for (auto& Tag : Tags)
	{
		if (!GetEquippedItemAtSlot(Tag))
		{
			return Tag;
		}
	}

	return FGameplayTag::EmptyTag;
}

void UEquipmentComponent::WieldWeapon(class UWeaponItem* Weapon, const FGameplayTag& WieldSlot)
{
	if (Weapon)
	{
		Weapon->WieldInSlot(WieldSlot);

		WieldedWeapons.Add(WieldSlot, Weapon);
	}
}

void UEquipmentComponent::UnwieldWeapon(const FGameplayTag& WieldSlot)
{
	if (WieldedWeapons.Contains(WieldSlot))
	{
		WieldedWeapons[WieldSlot]->WieldInSlot(FGameplayTag::EmptyTag);
		WieldedWeapons.Remove(WieldSlot);
	}
}

void UEquipmentComponent::EquipItem(class UEquippableItem* Equippable, const FGameplayTag& Slot)
{
	if (Equippable && Slot.IsValid())
	{	
		//Remove the old item from our equipped items if one is already equipped
		if (EquippedItems.Contains(Slot))
		{
			if (UEquippableItem* AlreadyEquippedItem = *EquippedItems.Find(Slot))
			{
				//The item is already equipped - we can early out. 
				if (AlreadyEquippedItem == Equippable)
				{
					return;
				}
				
				//AlreadyEquippedItem->SetActive(false);
				AlreadyEquippedItem->UnequipItem();
			}
		}

		ANarrativeCharacter* NChar = Cast<ANarrativeCharacter>(GetOwner());

		/*bool bAuth = GetOwnerRole() >= ROLE_Authority;
		bool bLocal = NChar && NChar->IsLocallyControlled();
		
		FString RoleStr = bAuth ? "Server" : "Client";
		FString LocalStr = bLocal ? "Local" : "Remote";
		if (!bAuth && bLocal)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s %s Equipped Items add %s %s"), *LocalStr, *RoleStr, *Slot.ToString(), *Equippable->DisplayName.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s %s Equipped Items add %s %s"), *LocalStr, *RoleStr, *Slot.ToString(), *Equippable->DisplayName.ToString());
		}*/

		EquippedItems.Add(Slot, Equippable);

		Equippable->HandleEquip();

		OnItemEquipped.Broadcast(Slot, Equippable);
	}
}

void UEquipmentComponent::UnequipItem(const FGameplayTag& Slot)
{
	if (UEquippableItem* Equippable = GetEquippedItemAtSlot(Slot))
	{
		ANarrativeCharacter* NChar = Cast<ANarrativeCharacter>(GetOwner());
		
		/*
		bool bAuth = GetOwnerRole() >= ROLE_Authority;
		bool bLocal = NChar && NChar->IsLocallyControlled();
		
		FString RoleStr = bAuth ? "Server" : "Client";
		FString LocalStr = bLocal ? "Local" : "Remote";
		
		if (!bAuth && bLocal)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s %s Equipped Items remove %s %s"), *LocalStr, *RoleStr, *Slot.ToString(), *Equippable->DisplayName.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s %s Equipped Items remove %s %s"), *LocalStr, *RoleStr, *Slot.ToString(), *Equippable->DisplayName.ToString());
		}
		*/
		
		EquippedItems.Remove(Slot);

		Equippable->HandleUnequip(Slot);

		OnItemUnequipped.Broadcast(Slot, Equippable);
	}
}
