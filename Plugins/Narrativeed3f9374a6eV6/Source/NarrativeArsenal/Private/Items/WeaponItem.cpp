// Copyright Narrative Tools 2024. 


#include "Items/WeaponItem.h"
#include "Items/AmmoItem.h"
#include <AbilitySystemComponent.h>
#include <AbilitySystemGlobals.h>
#include <GameplayTagContainer.h>
#include <GameFramework/Character.h>
#include "NarrativeGameplayTags.h"
#include "Items/NarrativeItem.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Net/UnrealNetwork.h"
#include "ArsenalSettings.h"
#include <GameFramework/CharacterMovementComponent.h>
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "Character/NarrativeCharacterVisual.h"
#include <Engine/World.h>

#include "NarrativeLogChannels.h"
#include "Items/WeaponAttachmentItem.h"
#include "Weapons/WeaponVisual.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Components/EquipmentComponent.h"

#define LOCTEXT_NAMESPACE "WeaponItem"

#define ItemStat_Damage "Damage"

UWeaponItem::UWeaponItem()
{
	bPawnFollowsControlRotation = false;
	bPawnOrientsRotationToMovement = true;
	bAllowManualReload = true;  

	Stats.Add(FNarrativeItemStat(LOCTEXT("DamageStatDisplayText", "Base Damage"), ItemStat_Damage, LOCTEXT("DamageStatDisplayTooltip", "The base damage this weapon deals.")));

	AttackDamage = 10.f;
	HeavyAttackDamageMultiplier = 1.6f;

	bBotsConsumeAmmo = false; 
	BotAttackRange = 10000.f;

	WeaponHand = EWeaponHandRule::WHR_Both; 

	OwnerCapsuleRotationSetting = ECapsuleRotationSetting::UseControllerYawSmoothed; 
}

FString UWeaponItem::GetStringVariable_Implementation(const FString& VariableName)
{
	if (VariableName == ItemStat_Damage)
	{
		if (AttackDamage > 0.f)
		{
			return FString::SanitizeFloat(AttackDamage);
		}
		else
		{
			return FString();
		}
	}

	return Super::GetStringVariable_Implementation(VariableName);
}

void UWeaponItem::AddedToInventory(class UNarrativeInventoryComponent* Inventory, const bool bFromLoad)
{
	Super::AddedToInventory(Inventory, bFromLoad);

	//When weapon comes back in from load, restore the ammo source if we had one. 
	if (bFromLoad && HasAuthority())
	{
		if (IsValid(RequiredAmmo))
		{
			//If not using equippable ammo, need to select the ammo to load into the clip 
			if (!RequiredAmmo->IsChildOf<UEquippableItem>())
			{
				if (!WeaponClipState.AmmoItemSource && WeaponClipState.AmmoItemGUID.IsValid())
				{
					//If the clip isn't using any ammo, find first valid ammo and make the clip use that. 
					if (UNarrativeItem* AmmoItem = OwningInventory->FindItemByGUID(WeaponClipState.AmmoItemGUID))
					{
						WeaponClipState.AmmoItemSource = AmmoItem;
					}
					else
					{
						WeaponClipState.AmmoItemSource = nullptr;
						WeaponClipState.AmmoItemGUID = FGuid();
					}

					MarkDirtyForReplication();
				}
			}
		}
	}

	//By default weapons should have a full clip. 
	if (!bFromLoad)
	{
		Reload();
	}

}

void UWeaponItem::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWeaponItem, WeaponClipState);
	//DOREPLIFETIME(UWeaponItem, WieldedSlot);
}

void UWeaponItem::PostLoad()
{

	//Upgrade old equippable to new 
#if WITH_EDITOR

	if (WeaponAbilities.IsEmpty() && !EquipmentAbilities.IsEmpty())
	{
		WeaponAbilities = EquipmentAbilities; 
	}

#endif

	Super::PostLoad();

}

void UWeaponItem::WieldInSlot(FGameplayTag DesiredSlot)
{
	//Must be equipped to be wielded...
	check(CurrentSlot.IsValid() || !DesiredSlot.IsValid());

	WieldedSlot = DesiredSlot;

	if (WieldedSlot.IsValid())
	{
		HandleWield();
	}
	else
	{
		HandleUnWield();
	}
}

bool UWeaponItem::TryAddAttachment(class UWeaponAttachmentItem* Attachment)
{
	if (WeaponAllowsAttachment(Attachment))
	{
		FGameplayTag AttachSlot = Attachment->WeaponAttachmentSlot;

		if (AttachSlot.IsValid())
		{
			//Unequip the old attachment should one exist
			if (WeaponAttachments.Contains(AttachSlot))
			{
				if (UWeaponAttachmentItem* ExistingAttachment = WeaponAttachments[AttachSlot])
				{
					ExistingAttachment->SetWeaponOwner(nullptr);
				}
			}

			//Tell the new weapon we're its owner and it can call AddAttachment()
			Attachment->SetWeaponOwner(this);

		}

	}

	return false; 
}

void UWeaponItem::TryRemoveAttachment(class UWeaponAttachmentItem* Attachment)
{
	if (IsValid(Attachment))
	{
		Attachment->SetWeaponOwner(nullptr);
	}
}

void UWeaponItem::AddAttachment(class UWeaponAttachmentItem* Attachment)
{
	if (IsValid(Attachment))
	{
		//Attach the weapons visual to the non-holster socket and set offset
		WeaponAttachments.Add(Attachment->WeaponAttachmentSlot, Attachment);
		OnItemModified.Broadcast();

		//Finally handle the weapon visual update
		AddAttachmentVisual(Attachment);

		if (IsWielded())
		{
			Attachment->HandleWield();
		}
	}
}

void UWeaponItem::RemoveAttachment(class UWeaponAttachmentItem* Attachment)
{
	if (IsValid(Attachment))
	{
		Attachment->HandleDetach(this);

		WeaponAttachments.Remove(Attachment->WeaponAttachmentSlot);

		OnItemModified.Broadcast();

		RemoveAttachmentVisual(Attachment);

		if (IsWielded())
		{
			Attachment->HandleUnWield();
		}
	}
}

void UWeaponItem::AddAttachmentVisual(class UWeaponAttachmentItem* Attachment)
{
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		if (AWeaponVisual* WeaponVisual = CharacterOwner->GetWeaponVisual(CurrentSlot))
		{
			//Make sure its not the visual for a different weapon we have swapped for
			if (WeaponVisual->WeaponOwner == this)
			{
				if (WeaponAttachmentConfiguration.Contains(Attachment->WeaponAttachmentSlot))
				{
					WeaponVisual->HandleAddAttachment(Attachment, WeaponAttachmentConfiguration[Attachment->WeaponAttachmentSlot]);
					Attachment->HandleAttach(this);
				}
			}
		}
		else
		{
			FString RoleStr = HasAuthority() ? "Server" : "Client";
			UE_LOG(LogNarrativeNet, Warning, TEXT("%s Failed to attach %s to weapon %s since visual wasn't valid. "), *RoleStr, *GetNameSafe(Attachment), *GetNameSafe(this));
		}
	}
}

void UWeaponItem::RemoveAttachmentVisual(class UWeaponAttachmentItem* Attachment)
{
	//Attach the weapons visual to the non-holster socket and set offset
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		if (AWeaponVisual* WeaponVisual = CharacterOwner->GetWeaponVisual(CurrentSlot))
		{
			//Make sure its not the visual for a different weapon we have equipped. 
			if (WeaponVisual->WeaponOwner == this)
			{
				WeaponVisual->HandleRemoveAttachment(Attachment);
			}
		}
	}
}

bool UWeaponItem::WeaponAllowsAttachment(const class UWeaponAttachmentItem* Attachment) const
{
	if (IsValid(Attachment))
	{
		return WeaponAttachmentConfiguration.Contains(Attachment->WeaponAttachmentSlot);
	}

	return false; 
}

bool UWeaponItem::CanDualWieldWith_Implementation(class UWeaponItem* Other)
{
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		if (!Other)
		{
			return false;
		}

		//cant dual wield with ourselves. 
		if (Other == this)
		{
			return false; 
		}

		//If ourselves or other isn't equipped we cant dual wield it.
		if (!CurrentSlot.IsValid() || !Other->CurrentSlot.IsValid())
		{
			return false; 
		}
	
		AWeaponVisual* OurVisual = CharacterOwner->GetWeaponVisual(CurrentSlot);
		AWeaponVisual* OtherVisual = CharacterOwner->GetWeaponVisual(Other->CurrentSlot);
		
		if (!OurVisual || !OtherVisual)
		{
			return false; 
		}
		
		//If either is two handed disallow . 
		if (WeaponHand == EWeaponHandRule::WHR_Both || Other->WeaponHand == EWeaponHandRule::WHR_Both)
		{
			return false;
		}

		//If either is wieldable we're fine. 
		if (WeaponHand == EWeaponHandRule::WHR_Either || Other->WeaponHand == EWeaponHandRule::WHR_Either)
		{
			return true; 
		}

		//We're mainhand and one is offhandable, that works. 
		if (WeaponHand == EWeaponHandRule::WHR_Mainhand && Other->WeaponHand == EWeaponHandRule::WHR_Offhand)
		{
			return true;
		}

		//Other is mainhand and we're offhand, that works. 
		if (WeaponHand == EWeaponHandRule::WHR_Offhand && Other->WeaponHand == EWeaponHandRule::WHR_Mainhand)
		{
			return true;
		}
	}

	return false; 
}

void UWeaponItem::HandleWield_Implementation()
{
	//Attach the weapons visual to the non-holster socket and set offset
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		//Tell all our attachments about the wield 
		for (auto& Attachment : WeaponAttachments)
		{
			if (Attachment.Value)
			{
				Attachment.Value->HandleWield();
			}
		}

		ApplyEquipmentAttributes();

		//Grant the weapons abilities.
		GiveWeaponAbilities();

		InitAmmoSource();

	}
	else
	{
		FString RoleStr = HasAuthority() ? "Server" : "Client";
	
		UE_LOG(LogNarrativeNet, Warning, TEXT("%s: wield skipped due to invalid owner"), *RoleStr);
	}
}

void UWeaponItem::HandleUnWield_Implementation()
{
	//Set the weapon visual back to the holstered socket and offset 
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		//CharacterOwner->UnWieldWeapon(this);

		//if (ANarrativeCharacterVisual* CharVisual = CharacterOwner->GetCharacterVisual())
		//{
		//	CharVisual->HandleUnWieldWeapon(this);
		//}

		RemoveEquipmentAttributes();
		RemoveWeaponAbilities();

		//Tell all our attachments about the unwield 
		for (auto& Attachment : WeaponAttachments)
		{
			if (Attachment.Value)
			{
				Attachment.Value->HandleUnWield();
			}
		}
	}
}

void UWeaponItem::ModifyEquipmentEffectSpec(FGameplayEffectSpec* Spec)
{
	Super::ModifyEquipmentEffectSpec(Spec);

	//Weapon item overrides this to add Attack Damage modifier to weapon equipment
	if (Spec)
	{
		Spec->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_AttackDamage, AttackDamage);
	}
}

void UWeaponItem::GiveWeaponAbilities()
{
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		//Use the entire wield state, rather than our current wield slot, as we need to see if we're being dual weld. 
		FWeaponWieldState WieldState = CharacterOwner->GetWeaponWieldState();
		TArray<TSubclassOf<UNarrativeGameplayAbility>> AbilitiesToGrant = WeaponAbilities;

		//If we're being dual weld, grant the right abilities for the hand we're wielded in.
		if (WieldState.WieldSlots.Num() > 1)
		{
			AbilitiesToGrant = (WieldedSlot == FNarrativeGameplayTags::Get().Weapon_WieldSlot_Mainhand) ? MainhandWeaponAbilities : OffhandWeaponAbilities;
		}

		WeaponAbilityHandles = CharacterOwner->GrantAbilities(AbilitiesToGrant, this);
	}
}

void UWeaponItem::RemoveWeaponAbilities()
{
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		CharacterOwner->RemoveAbilities(WeaponAbilityHandles);
	}
}

bool UWeaponItem::ConsumeAmmo_Implementation(const int32 Amount /*= 1*/)
{
	if (GetAmmoInClip() >= Amount && OwningInventory)// && HasAuthority())
	{
		if (!bBotsConsumeAmmo)
		{
			if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
			{
				if (CharacterOwner->IsBotControlled())
				{
					return true; 
				}
			}
		}

		if (HasAuthority())
		{
			if (UNarrativeItem* Item = GetAmmoSource())
			{
				OwningInventory->ConsumeItem(Item, Amount);
			}
			WeaponClipState.AmmoInClip -= Amount;
			MarkDirtyForReplication();
		}
		else
		{
			WeaponClipState.ClientAmmoInClip -= Amount;
		}
		
		return true;
	}
	return false;
}

float UWeaponItem::GetWeaponSpread_Implementation() const
{
	return 0.f; 
}


void UWeaponItem::OnRep_WieldedSlot(const FGameplayTag& OldWieldedSlot)
{
	//Check whether weapon is equipped or not 
	if (WieldedSlot.IsValid())
	{
		if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
		{
			if (ANarrativeCharacterVisual* CharVisual = CharacterOwner->GetCharacterVisual())
			{
				CharVisual->HandleWieldWeapon(this);
			}
		}
	}
	else
	{
		if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
		{
			if (ANarrativeCharacterVisual* CharVisual = CharacterOwner->GetCharacterVisual())
			{
				CharVisual->HandleUnWieldWeapon(this);
			}
		}
	}
}

void UWeaponItem::OnRep_WeaponClipState(const FWeaponClipState& OldClipState)
{
	//If server added some ammo from a reload etc client should have the same state. 
	if (OldClipState.AmmoInClip < WeaponClipState.AmmoInClip)
	{
		WeaponClipState.ClientAmmoInClip = WeaponClipState.AmmoInClip;
	}//If server has repped a lower number than us, it must be ahead and we can move forward to that, otherwise we're ahead and can keep our predicted value.
	else if (WeaponClipState.AmmoInClip < WeaponClipState.ClientAmmoInClip)
	{
		WeaponClipState.ClientAmmoInClip = WeaponClipState.AmmoInClip;
	}
}

FWeaponAttachmentConfig UWeaponItem::GetWeaponHolsterAttachConfig(FGameplayTag DesiredSlot) const
{
	FWeaponAttachmentConfig AttachConfig;

	if(HolsterAttachmentConfigs.Contains(DesiredSlot))
	{
		AttachConfig = HolsterAttachmentConfigs[DesiredSlot];
	}
	else
	{
		UE_LOG(LogNarrativeNet, Warning, TEXT("GetWeaponHolsterAttachConfig couldn't find an attachment config for slot %s"), *DesiredSlot.ToString());
	}

	return AttachConfig;
}

FWeaponAttachmentConfig UWeaponItem::GetWeaponWieldAttachConfig(FGameplayTag DesiredSlot) const
{
	FWeaponAttachmentConfig AttachConfig;

	if (WieldAttachmentConfigs.Contains(DesiredSlot))
	{
		AttachConfig = WieldAttachmentConfigs[DesiredSlot];
	}
	else
	{
		UE_LOG(LogNarrativeNet, Warning, TEXT("GetWeaponWieldAttachConfig couldn't find an attachment config for slot %s"), *DesiredSlot.ToString());
	}

	return AttachConfig;
}

FText UWeaponItem::GetWeaponDisplayName_Implementation(const bool bShowAttachments, const bool bShowAmmo) const
{
	FString Temp;

	Temp.Append(DisplayName.ToString());

	if (bShowAttachments)
	{
		for (auto& Attachment : WeaponAttachments)
		{
			if (UNarrativeItem* Attach = Attachment.Value)
			{
				Temp.Append(" | ");
				Temp.Append(Attach->DisplayName.ToString());
			}
		}
	}

	if (bShowAmmo)
	{
		if (UNarrativeItem* Ammo = GetAmmoSource())
		{
			Temp.Append(" | ");
			Temp.Append(Ammo->DisplayName.ToString());
		}
	}

	return FText::FromString(Temp);

}

bool UWeaponItem::Reload_Implementation()
{
	if (IsValid(RequiredAmmo) && HasAuthority())
	{
		InitAmmoSource();

		//Reload whatever is lower, our spare ammo remaining, or the space left in our clip. 
		const int32 Amount = FMath::Min(GetSpareAmmo(), GetClipSize() - GetAmmoInClip());

		if (Amount > 0)
		{
			WeaponClipState.AmmoInClip += Amount;
			MarkDirtyForReplication();
			return true; 
		}
	}

	return false;
}

int32 UWeaponItem::GetAmmoInClip_Implementation()const
{
	if (OwningInventory && OwningInventory->GetOwningPawn())
	{
		//Local clients want the predicted ammo. 
		const bool bLocal = OwningInventory->GetOwningPawn() ? OwningInventory->GetOwningPawn()->IsLocallyControlled() : false;
		const bool bAuth = OwningInventory->GetOwnerRole() >= ROLE_Authority;
		
		if(UNarrativeItem* Item = GetAmmoSource())
		{
			return FMath::Min<int32>(Item->GetQuantity(), bLocal && !bAuth ? WeaponClipState.ClientAmmoInClip : WeaponClipState.AmmoInClip);
		}
	}

	return 0;
} 

int32 UWeaponItem::GetAuthAmmoInClip_Implementation() const
{
	if (OwningInventory)
	{
		//Local clients want the predicted ammo. 
		const bool bAuth = OwningInventory->GetOwnerRole() >= ROLE_Authority;
		
		if(UNarrativeItem* Item = GetAmmoSource())
		{
			return FMath::Min<int32>(Item->GetQuantity(), WeaponClipState.AmmoInClip);
		}
	}

	return 0;
}

int32 UWeaponItem::GetSpareAmmo_Implementation()const
{
	if (UNarrativeItem* AmmoSourceItem = GetAmmoSource())
	{
		int32 OtherWeaponAmmo = 0;

		//Special case - if we're dual wielding a weapon using the same ammo source, spare ammo is actually the ammo we have minus BOTH weapons ammo in clip. 
		if (UEquipmentComponent* EquipmentComponent = GetEquipmentComponent())
		{
			if(EquipmentComponent->IsDualWielding())
			{
				FGameplayTag OtherWeaponTag = WieldedSlot == FNarrativeGameplayTags::Get().Weapon_WieldSlot_Mainhand ? FNarrativeGameplayTags::Get().Weapon_WieldSlot_Offhand : FNarrativeGameplayTags::Get().Weapon_WieldSlot_Mainhand;
				if (UWeaponItem* OtherWeapon = EquipmentComponent->GetWieldedWeaponAtSlot(OtherWeaponTag))
				{
					if (OtherWeapon->GetAmmoSource() == AmmoSourceItem)
					{
						OtherWeaponAmmo = OtherWeapon->GetAuthAmmoInClip();
					}
				}

			}
		}

		//Spare ammo isn't predicted so we use GetAuthAmmoInClip. This is because ammo removal requires inventory to update which isnt predicted. 
		return AmmoSourceItem->GetQuantity() - (GetAuthAmmoInClip() + OtherWeaponAmmo);
	}

	return 0;
}

int32 UWeaponItem::GetClipSize_Implementation()const
{
	//TODO ask weapon attachments if they want to override our clip size. 
	return ClipSize;
}

UNarrativeItem* UWeaponItem::GetAmmoSource() const
{
	if (IsValid(RequiredAmmo))
	{
		//If using equippable ammo, return ammo at the slot, but only if we have the correct type equipped. 
		if (RequiredAmmo->IsChildOf<UEquippableItem>())
		{
			if (UEquipmentComponent* EquipmentComponent = GetEquipmentComponent())
			{
				if (UNarrativeItem* Ammo = EquipmentComponent->GetEquippedItemAtSlot(FNarrativeGameplayTags::Get().Equipment_Slot_Ammo))
				{
					if (Ammo->GetClass()->IsChildOf(RequiredAmmo))
					{
						return Ammo;
					}
				}

			}
		}
		else
		{
			//Ensure the ammo source is still actually in our inventory 
			if (WeaponClipState.AmmoItemSource && WeaponClipState.AmmoItemSource->OwningInventory == OwningInventory)
			{
				return WeaponClipState.AmmoItemSource;//OwningInventory->FindItemByGUID(WeaponClipState.AmmoItemGUID);
			}

		}
	}

	return nullptr; 
}

void UWeaponItem::InitAmmoSource() 
{
	//If we dont have an ammo source feeding the clip, find one and use it. 
	if (HasAuthority() && OwningInventory)
	{
		if (IsValid(RequiredAmmo))
		{
			//If not using equippable ammo, need to select the ammo to load into the clip 
			if (!RequiredAmmo->IsChildOf<UEquippableItem>())
			{
				if (!WeaponClipState.AmmoItemSource || !WeaponClipState.AmmoItemSource->OwningInventory)
				{
					//If the clip isn't using any ammo, find first valid ammo and make the clip use that. 
					if (UNarrativeItem* AmmoItem = OwningInventory->FindItemOfClass(RequiredAmmo))
					{
						WeaponClipState.AmmoItemSource = AmmoItem;
						WeaponClipState.AmmoItemGUID = AmmoItem->ItemGUID;
						MarkDirtyForReplication();
					}
				}
			}
		}
	}
}

class UWeaponAttachmentItem* UWeaponItem::GetAttachment(const FGameplayTag& AttachmentSlot) const
{
	if (WeaponAttachments.Contains(AttachmentSlot))
	{
		return WeaponAttachments[AttachmentSlot];
	}

	return nullptr; 
}

TArray<UNarrativeAnimSet*> UWeaponItem::GetComboAnims(const bool bHeavyAttack) const
{
	return {};
}

bool UWeaponItem::IsHolstered() const
{
	return !IsWielded();
}

bool UWeaponItem::IsWielded() const
{
	return WieldedSlot.IsValid();
}

bool UWeaponItem::RequiresAutoReload_Implementation() const
{
	if (!IsValid(RequiredAmmo))
	{
		return false;
	} 

	return GetAmmoInClip() <= 0;
}

bool UWeaponItem::HasAmmo_Implementation() const
{
	if (!IsValid(RequiredAmmo))
	{
		return true;
	} 

    return GetAmmoInClip() > 0;
}

void UWeaponItem::OnAttack_Implementation()
{
	if (World)
	{
		LastAttackTime = World->GetTimeSeconds();
	}
}

bool UWeaponItem::CanAttack_Implementation() const
{
	//Dont check ammo in clip because ability still needs to activate 
	return HasAmmo() && !IsHolstered();
}

FCombatTraceData UWeaponItem::GetTraceData() const
{
	return FCombatTraceData();
}

float UWeaponItem::GetAttackRange() const
{
	return BotAttackRange;
}

FName UWeaponItem::GetWeaponVisualAttachBone() const
{
	if (WieldedSlot.IsValid() && WieldAttachmentConfigs.Contains(WieldedSlot))
	{
		return WieldAttachmentConfigs[WieldedSlot].SocketName;
	}

	//fallback to just returning main 
	if (WieldAttachmentConfigs.Contains(FNarrativeGameplayTags::Get().Weapon_WieldSlot_Mainhand))
	{
		return WieldAttachmentConfigs[FNarrativeGameplayTags::Get().Weapon_WieldSlot_Mainhand].SocketName;
	}
	return FName();//WeaponVisualAttachBone;
}

void UWeaponItem::HandleEquip_Implementation()
{
	//Dont call super as that grants abilities and we actually want to do that when the weapon is wielded rather than equipped 
	//Super::HandleEquip_Implementation();
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		ANarrativeCharacterVisual* CharVisual = CharacterOwner->GetCharacterVisual();

		if (IsValid(CharVisual))
		{
			CharVisual->AddWeaponVisual(this);
		}
		else
		{
			FString RoleStr = HasAuthority() ? "Server" : "Client";

			UE_LOG(LogNarrativeNet, Warning, TEXT("%s: HandleEquip called on %s but visual wasn't loaded yet for weapon %s."), *RoleStr, *CharacterOwner->GetCharacterName().ToString(), *DisplayName.ToString());

		}
	}
}

void UWeaponItem::HandleUnequip_Implementation(const FGameplayTag& OldSlot)
{
	Super::HandleUnequip_Implementation(OldSlot);

	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		//If any wielded weapons were unequipped, unwield all wielded weapons.  
		if (WieldedSlot.IsValid())
		{
			CharacterOwner->SetWieldState(FWeaponWieldState());
		}

		if (ANarrativeCharacterVisual* CharVisual = CharacterOwner->GetCharacterVisual())
		{
			CharVisual->RemoveWeaponVisual(OldSlot);
		}
	}
}

#undef LOCTEXT_NAMESPACE
