// Copyright Narrative Tools 2025.


#include "Items/WeaponAttachmentItem.h"
#include "Items/WeaponItem.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "NarrativeLogChannels.h"
#include "Net/UnrealNetwork.h"
#include "Components/EquipmentComponent.h"

UWeaponAttachmentItem::UWeaponAttachmentItem()
{
	bUsedWithOtherItem = true; 
	WeaponOwnerGUID = FGuid(0,0,0,0);
	FOVOverride = -1.f; 
	WeaponRenderFOVOverride = -1.f;
	WeaponAimFStopOverride = -1.f; 
	UseActionText = NSLOCTEXT("WeaponAttachmentItem", "UseActionText", "Attach");
}

void UWeaponAttachmentItem::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWeaponAttachmentItem, WeaponOwner);
}

void UWeaponAttachmentItem::Serialize(FArchive& Ar)
{

	if (Ar.IsSaving())
	{
		//Store our weapon owner IDx so we can resolve our weapon owner when we load in later - we can save the weapon owner pointer directly as its a uobject 
		if (WeaponOwner)
		{
			if (OwningInventory)
			{
				WeaponOwnerGUID = WeaponOwner->ItemGUID;
			}
		}
	}

	Super::Serialize(Ar);
}

void UWeaponAttachmentItem::PostInventoryLoaded()
{
	//When our inventory items are all loaded back in we can reattach to our weapon owner - just need to find it. 
	if (OwningInventory && WeaponOwnerGUID.IsValid())
	{
		if (UWeaponItem* FoundOwner = Cast<UWeaponItem>(OwningInventory->FindItemByGUID(WeaponOwnerGUID)))
		{
			if (HasAuthority())
			{
				SetWeaponOwner(FoundOwner);
			}
		}
	}
}

void UWeaponAttachmentItem::AddedToInventory(class UNarrativeInventoryComponent* Inventory, const bool bFromLoad)
{
	Super::AddedToInventory(Inventory, bFromLoad);

	//When we enter the inventory, auto-equip the attachment if we're holding a weapon with a free slot for it.
	if (HasAuthority() && !bFromLoad)
	{
		if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
		{
			//Auto equip the attachment if any weapons have a slot open for it 
			if (UEquipmentComponent* Equipment = CharacterOwner->GetEquipmentComponent())
			{
				for (auto& WieldedWeapon : Equipment->GetWieldedWeapons())
				{
					if (WieldedWeapon)
					{
						if (!IsValid(WieldedWeapon->GetAttachment(WeaponAttachmentSlot)))
						{
							SetWeaponOwner(WieldedWeapon);
							return;
						}
					}
				}
			}
		}
	}
}

void UWeaponAttachmentItem::Use(UNarrativeItem* OtherItem/*=nullptr*/)
{
	//Set the weapon visual back to the holstered socket and offset 
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		//Try add the weapon to other item, provided it wasnt the gun we just removed from 
		if (UWeaponItem* WeaponToAddTo = Cast<UWeaponItem>(OtherItem))
		{
			//Do we need to just unequip, or do we need to unequip, and then continue on to remove the next item 
			const bool bUnEquip = WeaponToAddTo == WeaponOwner;

			//Remove if we're currently on a gun
			if (WeaponOwner)
			{
				WeaponOwner->TryRemoveAttachment(this);

				if (bUnEquip)
				{
					return; 
				}
			}

			if (WeaponToAddTo)
			{
				WeaponToAddTo->TryAddAttachment(this);
			}

		}
		else //If we want to support not using attachments on anything 
		{
			if (WeaponOwner)
			{
				WeaponOwner->TryRemoveAttachment(this);
			}
			else //If we have an wielded weapon with a slot open add it to that. 
			{
				UWeaponItem* WeaponToEquip = nullptr; 

				//Auto equip the attachment if any weapons have a slot open for it 
				if (UEquipmentComponent* Equipment = CharacterOwner->GetEquipmentComponent())
				{
					for (auto& WieldedWeapon : Equipment->GetWieldedWeapons())
					{
						if (WieldedWeapon)
						{
							if (!IsValid(WieldedWeapon->GetAttachment(WeaponAttachmentSlot)) && WieldedWeapon->WeaponAllowsAttachment(this))
							{
								WeaponToEquip = WieldedWeapon;
								break;
							}
						}
					}
				}

				if (WeaponToEquip)
				{
					WeaponToEquip->TryAddAttachment(this);
				}

			}
		}
	}
}

bool UWeaponAttachmentItem::CanUseItemWith_Implementation(class UNarrativeItem* TestItem) const
{
	if (UWeaponItem* WeaponToAddTo = Cast<UWeaponItem>(TestItem))
	{
		return WeaponToAddTo->WeaponAllowsAttachment(this);
	}

	return false; 
}

bool UWeaponAttachmentItem::ShouldUseOnAdd_Implementation() const
{
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		//Auto equip the attachment if any weapons have a slot open for it 
		if (UEquipmentComponent* Equipment = CharacterOwner->GetEquipmentComponent())
		{
			for (auto& WieldedWeapon : Equipment->GetWieldedWeapons())
			{
				if (WieldedWeapon)
				{
					if (!IsValid(WieldedWeapon->GetAttachment(WeaponAttachmentSlot)))
					{
						return true;
					}
				}
			}
		}
	}

	return false; 
}

void UWeaponAttachmentItem::HandleAttach_Implementation(UWeaponItem* AttachingTo)
{

}

void UWeaponAttachmentItem::HandleDetach_Implementation(UWeaponItem* DetachingFrom)
{

}

void UWeaponAttachmentItem::HandleWield_Implementation()
{

}

void UWeaponAttachmentItem::HandleUnWield_Implementation()
{

}

void UWeaponAttachmentItem::ServerSetWeaponOwner_Implementation(UWeaponItem* InWeaponOwner)
{
	SetWeaponOwner(InWeaponOwner);
}

void UWeaponAttachmentItem::SetWeaponOwner(UWeaponItem* InWeaponOwner)
{
	if (InWeaponOwner && !InWeaponOwner->WeaponAllowsAttachment(this))
	{
		return; 
	}
	
	if(!HasAuthority())
	{
		ServerSetWeaponOwner(InWeaponOwner);
		return; 
	}

	UWeaponItem* PrevOwner = WeaponOwner;
	//UE_LOG(LogNarrativeItem, Warning, TEXT("Prev item was %s"), *GetNameSafe(PrevOwner));
	WeaponOwner = InWeaponOwner;
	//UE_LOG(LogNarrativeItem, Warning, TEXT("Prev item is now %s"), *GetNameSafe(PrevOwner));
	OnRep_WeaponOwner(PrevOwner);
}

void UWeaponAttachmentItem::OnRep_WeaponOwner(class UWeaponItem* PreviousOwner)
{
	//UE_LOG(LogTemp, Warning, TEXT("on rep weapon owner is %s"), *GetNameSafe(PreviousOwner));
	//TODO Setactive calls are temporary so check shows on UI

	if (!OwningInventory)
	{
		return; 	
	}
	
	ANarrativeCharacter* NChar = Cast<ANarrativeCharacter>(OwningInventory->GetOwner());
	

	bool bAuth = OwningInventory->GetOwnerRole() >= ROLE_Authority;
	bool bLocal = NChar && NChar->IsLocallyControlled();
		
	FString RoleStr = bAuth ? "Server" : "Client";
	FString LocalStr = bLocal ? "Local" : "Remote";
	if (!bAuth && bLocal)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s %s Attachment %s OnRep_WeaponOwner now %s was %s"), *LocalStr, *RoleStr, *DisplayName.ToString(), *GetNameSafe(WeaponOwner),*GetNameSafe(PreviousOwner));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s %s Attachment %s OnRep_WeaponOwner now %s was %s"), *LocalStr, *RoleStr, *DisplayName.ToString(), *GetNameSafe(WeaponOwner),*GetNameSafe(PreviousOwner));
	}
	
	if (WeaponOwner)
	{
		WeaponOwner->AddAttachment(this);
	}

	if (PreviousOwner)
	{
		PreviousOwner->RemoveAttachment(this);
	}
}

float UWeaponAttachmentItem::OverrideWeaponCameraFOV_Implementation() const
{

	//For things like scopes etc, we only want the attachments zoom level if we're in first person - ultra high zoom in third person doesn't look right 
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		if (CharacterOwner->IsCameraInsideHead())
		{
			return FOVOverride;
		}
	}

	return -1.f;
}

float UWeaponAttachmentItem::OverrideWeaponRenderFOV_Implementation() const
{
	return WeaponRenderFOVOverride;
}

float UWeaponAttachmentItem::OverrideWeaponAimFStop_Implementation() const
{
	return WeaponAimFStopOverride;
}
