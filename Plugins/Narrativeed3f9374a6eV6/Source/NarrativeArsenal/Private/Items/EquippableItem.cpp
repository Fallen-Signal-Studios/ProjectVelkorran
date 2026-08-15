// Copyright Narrative Tools 2022. 

#include "Items/EquippableItem.h"
#include "Components/EquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Pawn.h"
#include <GameFramework/Character.h>
#include "Engine/SkinnedAssetCommon.h"
#include <AbilitySystemGlobals.h>
#include <GameplayTagContainer.h>
#include <AbilitySystemComponent.h>
#include "NarrativeGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include <UObject/ConstructorHelpers.h>
#include "Character/NarrativeCharacterVisual.h"
#include "ArsenalStatics.h"
#include "NarrativeLogChannels.h"

#define LOCTEXT_NAMESPACE "EquippableItem"

#define ItemStat_Armor "Armor"
#define ItemStat_AttackRating "AttackRating"

UEquippableItem::UEquippableItem()
{
	UseActionText = LOCTEXT("UseActionText_Equippable", "Equip");
	bStackable = false;
	bCanActivate = true;
	//bToggleActiveOnUse = true;
	Weight = 1.f;

	Stats.Add(FNarrativeItemStat(LOCTEXT("ArmorStatDisplayText", "Armor"), ItemStat_Armor, LOCTEXT("ArmorStatTooltip", "The Armor rating - reduces damage taken.")));
	Stats.Add(FNarrativeItemStat(LOCTEXT("AttackRatingStatDisplayText", "Attack Rating"), ItemStat_AttackRating, LOCTEXT("AttackRatingStatTooltip", "The Attack Rating - increases damage dealt.")));

	auto EquipmentModGEClass = ConstructorHelpers::FClassFinder<UGameplayEffect>(TEXT("/Script/Engine.Blueprint'/NarrativePro/Pro/Core/Abilities/GameplayEffects/Equipment/GE_EquipmentModifier.GE_EquipmentModifier_C'"));

	if (EquipmentModGEClass.Succeeded())
	{
		EquipmentEffect = EquipmentModGEClass.Class;
	}

}

void UEquippableItem::PostLoad()
{
	Super::PostLoad();

	if (EquipmentEffectValues.Num() <= 0)
	{
		EquipmentEffectValues.Add(FNarrativeGameplayTags::Get().SetByCaller_Armor, ArmorRating);
		EquipmentEffectValues.Add(FNarrativeGameplayTags::Get().SetByCaller_AttackRating, AttackRating);
		EquipmentEffectValues.Add(FNarrativeGameplayTags::Get().SetByCaller_StealthRating, StealthRating);
	}

	//Upgrade old equippable to new 
#if WITH_EDITOR

	if (EquippableSlot.IsValid() && EquippableSlots.IsEmpty())
	{
		EquippableSlots.AddTagFast(EquippableSlot);
	}

#endif
}

void UEquippableItem::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UEquippableItem, CurrentSlot, COND_None, REPNOTIFY_Always);
}

void UEquippableItem::HandleEquip_Implementation()
{
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		AbilityHandles = CharacterOwner->GrantAbilities(EquipmentAbilities, this);

		ApplyEquipmentAttributes();
	}
}

void UEquippableItem::HandleUnequip_Implementation(const FGameplayTag& OldSlot)
{
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		CharacterOwner->RemoveAbilities(AbilityHandles);

		RemoveEquipmentAttributes();
	}
}

void UEquippableItem::ServerEquipItem_Implementation(FGameplayTag DesiredSlot)
{
	EquipItem(DesiredSlot);
}

bool UEquippableItem::EquipItem(FGameplayTag DesiredSlot)
{
	if (!HasAuthority())
	{
		ServerEquipItem(DesiredSlot);
		return true; 
	}

	//Equip the item via OnRep so all players see the equip. 
	if (UEquipmentComponent* EquipmentComponent = GetEquipmentComponent())
	{
		if (!DesiredSlot.IsValid() || EquippableSlots.HasTag(DesiredSlot))
		{
			FGameplayTag OldSlot = CurrentSlot;
			CurrentSlot = DesiredSlot;
			OnRep_CurrentSlot(OldSlot);
			MarkDirtyForReplication();

			return true; 
		}
	}

	return false; 
}

void UEquippableItem::UnequipItem()
{
	EquipItem(FGameplayTag());
}

void UEquippableItem::Use(UNarrativeItem* OtherItem/* =nullptr */)
{
	//Try take off/put on the equippable. 
	if (UEquipmentComponent* EquipmentComponent = GetEquipmentComponent())
	{
		if (IsEquipped())
		{
			UnequipItem();
		}
		else if(!EquippableSlots.IsEmpty())
		{
			const FGameplayTag FreeSlot = EquipmentComponent->GetFirstFreeSlot(EquippableSlots);

			if (FreeSlot.IsValid())
			{
				EquipItem(FreeSlot);
			}
			else
			{
				//No free slots, replace existing one since we're using item 
				EquipItem(EquippableSlots.GetByIndex(0));
			}
		}
	}
}

void UEquippableItem::AddedToInventory(class UNarrativeInventoryComponent* Inventory, const bool bFromLoad)
{
	Super::AddedToInventory(Inventory, bFromLoad);
	if (!HasAuthority()) { return; } // Only allow server to equip items when an item is added
	
	//Try auto-equip item when added. 
	if (!bFromLoad && Inventory && !Inventory->IsVendor())
	{
		if (UEquipmentComponent* EquipmentComponent = GetEquipmentComponent())
		{
			const FGameplayTag FreeSlot = EquipmentComponent->GetFirstFreeSlot(EquippableSlots);

			if (FreeSlot.IsValid())
			{
				EquipItem(FreeSlot);
			}
		}
	}
}

void UEquippableItem::RemovedFromInventory(class UNarrativeInventoryComponent* Inventory)
{
	//Everyone needs to forcibly unequip an item if it was removed from its owner 
	if (IsEquipped())
	{
		FGameplayTag OldSlot = CurrentSlot;
		CurrentSlot = FGameplayTag();
		OnRep_CurrentSlot(OldSlot);
	}

	Super::RemovedFromInventory(Inventory);
}

bool UEquippableItem::ShowActiveInUI_Implementation() const
{
	return CurrentSlot.IsValid(); 
}

void UEquippableItem::PostInventoryLoaded()
{
	Super::PostInventoryLoaded();

	if (CurrentSlot.IsValid())
	{
		//Try take off/put on the equippable. 
		if (UEquipmentComponent* EquipmentComponent = GetEquipmentComponent())
		{
			EquipItem(CurrentSlot);
		}
	}
}

TArray<class UNarrativeItemUseAction*> UEquippableItem::GetItemUseActions_Implementation() const
{
	return Super::GetItemUseActions_Implementation();

	//The idea was to make an equip action for each slot the item could go in, but in practice this wasn't very intuitive.
	// You can uncomment this code if you want to reenable that behavior. 
	//if (EquippableSlots.Num() <= 1)
	//{
	//	return Super::GetItemUseActions_Implementation();
	//}
	//else
	//{
	//	TArray<UNarrativeItemUseAction*> Actions;

	//	//Create a equip action for each slot
	//	for (auto& Slot : EquippableSlots.GetGameplayTagArray())
	//	{
	//		if(Slot.IsValid())
	//		{
	//			UUseAction_Equip* EquipAction = NewObject<UUseAction_Equip>();
	//			EquipAction->EquipToSlot = Slot;
	//			Actions.Add(EquipAction);
	//		}
	//	}

	//	for (auto& Fragment : Fragments)
	//	{
	//		if (Fragment)
	//		{
	//			Actions.Append(Fragment->GetItemUseActions());
	//		}
	//	}

	//	return Actions;
	//}
}

void UEquippableItem::ApplyEquipmentAttributes()
{
	if (HasAuthority())
	{
		if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
		{
			if (UAbilitySystemComponent* ASC = CharacterOwner->GetAbilitySystemComponent())
			{
				// Can run on Server and Client
				FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
				EffectContext.AddSourceObject(this);

				FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EquipmentEffect, 1.f, EffectContext);

				if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
				{
					ModifyEquipmentEffectSpec(Spec);

					if (SpecHandle.IsValid())
					{
						EquipmentGEHandle = ASC->ApplyGameplayEffectSpecToTarget(*Spec, ASC);
					}
				}
			}
		}
	}
}

void UEquippableItem::RemoveEquipmentAttributes()
{
	if (HasAuthority())
	{
		if (UAbilitySystemComponent* ASC = EquipmentGEHandle.GetOwningAbilitySystemComponent())
		{
			ASC->RemoveActiveGameplayEffect(EquipmentGEHandle);
		}
	}
}

void UEquippableItem::ModifyEquipmentEffectSpec(FGameplayEffectSpec* Spec)
{
	if (Spec)
	{
		Spec->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Armor, ArmorRating);
		Spec->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_AttackRating, AttackRating);
		Spec->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_StealthRating, StealthRating);

		for (auto& Val : EquipmentEffectValues)
		{
			if (Val.Key.IsValid() && !FMath::IsNearlyZero(Val.Value))
			{
				Spec->SetSetByCallerMagnitude(Val.Key, Val.Value);
			}
		}
	}
}

void UEquippableItem::OnRep_CurrentSlot(const FGameplayTag& OldCurrentSlot)
{
	//Simulated proxies dont have this as PState hasn't repped back multiplayer will possibly need this at some point. 
	//Possibly fix includes waiting for pstate, or just putting invnetory component on player character instead. 
	check(GetEquipmentComponent());

	ANarrativeCharacter* NChar = Cast<ANarrativeCharacter>(GetEquipmentComponent()->GetOwner());

	bool bAuth = OwningInventory ? OwningInventory->GetOwnerRole() >= ROLE_Authority : false;
	bool bLocal = NChar && NChar->IsLocallyControlled();
		
	FString RoleStr = bAuth ? "Server" : "Client";
	FString LocalStr = bLocal ? "Local" : "Remote";
	if (!bAuth && bLocal)
	{
		UE_LOG(LogNarrativeNet, Verbose, TEXT("%s %s %s OnRep_CurrentSlot NewSlot %s OldSlot %s"), *LocalStr, *RoleStr, *DisplayName.ToString(), *CurrentSlot.ToString(), *OldCurrentSlot.ToString());
	}
	else
	{
		UE_LOG(LogNarrativeNet, Verbose, TEXT("%s %s %s OnRep_CurrentSlot NewSlot %s OldSlot %s"), *LocalStr, *RoleStr, *DisplayName.ToString(), *CurrentSlot.ToString(), *OldCurrentSlot.ToString());
	}
	
	//Equip/Unequip on all clients. Ensure slots have changed
	//There is a bug here. We cannot guarantee the order OnReps fire in. In a networked game, 
	//if (OldCurrentSlot != CurrentSlot) 
	{
		if (UEquipmentComponent* EquipmentComponent = GetEquipmentComponent())
		{
			if (OldCurrentSlot.IsValid())
			{
				//Only unequip the item if we've got it on right now. Onreps aren't ordered so another item may have replaced this one by now. 
				if (EquipmentComponent->GetEquippedItemAtSlot(OldCurrentSlot) == this)
				{
					EquipmentComponent->UnequipItem(OldCurrentSlot);
				}
			}

			if (CurrentSlot.IsValid())
			{
				EquipmentComponent->EquipItem(this, CurrentSlot);
			}
		}
	}

}

class UEquipmentComponent* UEquippableItem::GetEquipmentComponent() const
{
	//Try auto-equip item when added. 
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		if (UEquipmentComponent* EquipmentComponent = CharacterOwner->GetEquipmentComponent())
		{
			check(EquipmentComponent);

			return EquipmentComponent;
		}
	}

	return nullptr; 
}

bool UEquippableItem::IsEquipped() const
{
	return CurrentSlot.IsValid();
}

bool UEquippableItem::ShouldUseOnAdd_Implementation() const
{
	//if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	//{
	//	//Auto equip items if we dont have any item at that slot
	//	if (UEquipmentComponent* EquipmentComponent = CharacterOwner->GetEquipmentComponent())
	//	{
	//		if (!EquipmentComponent->GetEquippedItemAtSlot(EquippableSlot))
	//		{
	//			return true;
	//		}
	//	}
	//}

	return false;
}

FString UEquippableItem::GetStringVariable_Implementation(const FString& VariableName)
{
	if (VariableName == ItemStat_AttackRating)
	{
		if (AttackRating > 0.f)
		{
			return "+" + FString::SanitizeFloat(AttackRating);
		}
		else
		{
			return FString::SanitizeFloat(AttackRating);
		}

	}
	else if (VariableName == ItemStat_Armor)
	{
		if (ArmorRating > 0.f)
		{
			return "+" + FString::SanitizeFloat(ArmorRating);
		}
		else
		{
			return FString::SanitizeFloat(ArmorRating);
		}
	}

	return Super::GetStringVariable_Implementation(VariableName);
}


UEquippableItem_Clothing::UEquippableItem_Clothing()
{

}

#if WITH_EDITOR
void UEquippableItem_Clothing::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	//Make sure all our meshes have materials set on them automatically 
	if (!ClothingMeshData.MeshMaterials.Num() && !ClothingMeshData.Mesh.IsNull() && !ClothingMeshData.bIsStaticMesh)
	{
		if (USkeletalMesh* MeshAsset = ClothingMeshData.Mesh.LoadSynchronous())
		{
			for (auto& MeshMat : MeshAsset->GetMaterials())
			{
				FCreatorMeshMaterial NewMeshMat;
				NewMeshMat.Material = MeshMat.MaterialInterface;

				ClothingMeshData.MeshMaterials.Add(NewMeshMat);
			}
		}
	}

	//Make sure all our meshes have materials set on them automatically 
	if (!ClothingMeshData.MeshMaterials.Num() && !ClothingMeshData.StaticMesh.IsNull() && ClothingMeshData.bIsStaticMesh)
	{
		if (UStaticMesh* MeshAsset = ClothingMeshData.StaticMesh.LoadSynchronous())
		{
			for (auto& MeshMat : MeshAsset->GetStaticMaterials())
			{
				FCreatorMeshMaterial NewMeshMat;
				NewMeshMat.Material = MeshMat.MaterialInterface;

				ClothingMeshData.MeshMaterials.Add(NewMeshMat);
			}
		}
	}
}



#endif

void UEquippableItem_Clothing::ApplyClothingMesh()
{
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		if (ANarrativeCharacterVisual* CharVisual = CharacterOwner->GetCharacterVisual())
		{
			CharVisual->SetMeshAppearance(CurrentSlot, ClothingMeshData);

		}
	}
}

void UEquippableItem_Clothing::HandleEquip_Implementation()
{
	Super::HandleEquip_Implementation();

	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{
		ANarrativeCharacterVisual* CharVisual = CharacterOwner->GetCharacterVisual();

		if (IsValid(CharVisual))
		{
			CharVisual->HandleEquipClothing(this);
		}
		else
		{
			FString RoleStr = HasAuthority() ? "Server" : "Client";
			
			UE_LOG(LogNarrativeNet, Warning, TEXT("%s: HandleEquip called on %s but visual wasn't loaded yet for clothing %s."), *RoleStr, *CharacterOwner->GetCharacterName().ToString(), *DisplayName.ToString());
		}
	}
}

void UEquippableItem_Clothing::HandleUnequip_Implementation(const FGameplayTag& OldSlot)
{

	Super::HandleUnequip_Implementation(OldSlot);

	//Set the clothing back to its default mesh and materials
	if (ANarrativeCharacter* CharacterOwner = GetOwningNarrativeCharacter())
	{	
		if (ANarrativeCharacterVisual* CharVisual = CharacterOwner->GetCharacterVisual())
		{
			CharVisual->HandleUnEquipClothing(OldSlot);
		}
	}
}

UUseAction_Equip::UUseAction_Equip(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	
}

bool UUseAction_Equip::OnUse_Implementation(class UNarrativeItem* Item, class UNarrativeItem* OtherItem)
{
	if (UEquippableItem* Equippable = Cast<UEquippableItem>(Item))
	{
		return Equippable->EquipItem(EquipToSlot);
	}

	return false; 
}

FText UUseAction_Equip::GetActionDisplayName_Implementation(class UNarrativeItem* Item)
{
	FText OutText;

	if(UArsenalStatics::GetGameplayTagFriendlyDisplayName(EquipToSlot, OutText))
	{
		return OutText;
	}

	return Super::GetActionDisplayName_Implementation(Item);

}

#undef LOCTEXT_NAMESPACE