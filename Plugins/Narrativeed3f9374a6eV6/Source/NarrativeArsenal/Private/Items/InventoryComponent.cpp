// Copyright Narrative Tools 2022. 

#include "Items/InventoryComponent.h"
#include "Engine/ActorChannel.h"
#include "Net/UnrealNetwork.h"
#include "Items/NarrativeItem.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include "Items/NarrativeInventorySettings.h"
#include <Serialization/ObjectAndNameAsStringProxyArchive.h>
#include <Templates/SubclassOf.h>
#include <Serialization/MemoryReader.h>
#include <Serialization/MemoryWriter.h>
#include "NarrativeLogChannels.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "Inventory"

FNarrativeSavedItem::FNarrativeSavedItem(class UNarrativeItem* Item)
{
	if (Item)
	{
		ItemClass = Item->GetClass();
		Quantity = Item->GetQuantity();
		bActive = Item->bActive;
		//bFavourited = Item->bFavourite;

		//Serialize the items savegame vars into byte data 
		FMemoryWriter MemWriter(ByteData);
		FObjectAndNameAsStringProxyArchive Ar(MemWriter, true);
		Ar.ArIsSaveGame = true;

		Item->Serialize(Ar);
	}
}


// Sets default values for this component's properties
UNarrativeInventoryComponent::UNarrativeInventoryComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; 

	WeightCapacity = 100.f;
	Capacity = 50;

	BuyItemPct = 0.5f;
	SellItemPct = 2.f;

	bGaveDefaultItems = false; 
}

void UNarrativeInventoryComponent::PrepareForSave_Implementation()
{

	SavedCurrency = Currency;

	SavedItems.Empty();

	for (auto& Item : Items)
	{
		if (Item)
		{
			SavedItems.Add(FNarrativeSavedItem(Item));
		}
	}
}

void UNarrativeInventoryComponent::Load_Implementation()
{
	++CinematicLoadRevision;
	bIsLoading = true;

	SetCurrency(SavedCurrency);

	//Deactivate any active items we currently have 
	for (auto& Item : Items)
	{
		if (Item->bActive)
		{
			Item->SetActive(false, true);
		}
	}

	Items.Empty();
	ItemGUIDMap.Empty();
	ReplicatedItems.Items.Empty();
	ReplicatedItems.MarkArrayDirty();

	for (auto& InventoryItem : SavedItems)
	{
		if (IsValid(InventoryItem.ItemClass) && InventoryItem.Quantity > 0)
		{
			FItemAddResult AddResult = TryAddItemFromClass(InventoryItem.ItemClass, InventoryItem.Quantity, false);

			//1 saved item is saved per item, we should never have multiples 
			check(AddResult.Stacks.Num() && AddResult.Stacks.IsValidIndex(0));

			FMemoryReader MemReader(InventoryItem.ByteData);
			FObjectAndNameAsStringProxyArchive Ar(MemReader, true);
			Ar.ArIsSaveGame = true;

			//Reactivate any items that were active 
			if (UNarrativeItem* AddedItem = AddResult.Stacks[0])
			{
				//Activate/deactivate the item 
				AddedItem->SetActive(InventoryItem.bActive, false);

				//Load the saved variables back into the remade item
				AddedItem->Serialize(Ar);

				//Need to add this now that our serialized GUID is loaded back 
				ItemGUIDMap.Add(AddedItem->ItemGUID, AddedItem);
			}
		}
	}

	//Tell all items that the inventory has been loaded so they can do whatever they need 
	for (auto& InventoryItem : Items)
	{
		if (IsValid(InventoryItem))
		{
			InventoryItem->PostInventoryLoaded();
		}
	}

	bIsLoading = false; 
}

bool UNarrativeInventoryComponent::UseItem(class UNarrativeItem* Item, class UNarrativeItem* OtherItem/*=nullptr*/)
{
	if (!IsValid(Item))
	{
		return false;
	}

	//Ask server to use item - TODO server should probably auth this and then replicate the use back to us - we don't want to predict this 
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerUseItem(Item, OtherItem);
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		// Server should validate the inventory actually contains the item the player is attempting to use 
		if (!IsValid(FindItemByClassExact(Item->GetClass())))
		{
			return false;
		}
	}

	if (Item && Item->CanUse())
	{
		const float UseTime = GetWorld()->GetTimeSeconds();

		if (UseTime - Item->GetLastUseTime() > Item->UseRechargeDuration)
		{
			//If the item is consumable like food, but it can't be removed from the inventory then disallow using the item 
			if (!Item->CanBeRemoved() && Item->bConsumeOnUse)
			{
				return false;
			}

			Item->OnUse(OtherItem);
			Item->Use(OtherItem);

			OnItemUsed.Broadcast(Item);

			if (GetOwnerRole() < ROLE_Authority)
			{
				return true;
			}

			Item->SetLastUseTime(UseTime);

			if (Item->bToggleActiveOnUse)
			{
				Item->SetActive(!Item->bActive);
			}

			if (Item->bConsumeOnUse)
			{
				ConsumeItem(Item, 1);
			}

			return true;
		}
	}

	return false;
}

void UNarrativeInventoryComponent::TryAddFromLootTable(FLootTableRoll LootTable, TArray<FItemAddResult>& OutItemAddResults)
{

	//Grant the items and item collections 
	UE_LOG(LogNarrativeInventory, Display, TEXT("--Granting individual items--"));

	for (FItemWithQuantity& ItemToGive : LootTable.ItemsToGrant)
	{
		UE_LOG(LogNarrativeInventory, Display, TEXT("Granting %d of item %s"), ItemToGive.Quantity, *ItemToGive.Item.ToString());
		OutItemAddResults.Add(TryAddItemFromClass(ItemToGive.Item.LoadSynchronous(), ItemToGive.Quantity));
	}

	for (auto& Collection : LootTable.ItemCollectionsToGrant)
	{
		if (IsValid(Collection))
		{
			UE_LOG(LogNarrativeInventory, Display, TEXT("Granting item collection %s"), *GetNameSafe(Collection));
			for (auto& Item : Collection->Items)
			{
				if (!Item.Item.IsNull() && Item.Quantity > 0)
				{
					OutItemAddResults.Add(TryAddItemFromClass(Item.Item.LoadSynchronous(), Item.Quantity));
				}
			}
		}
	}

	if (LootTable.CanRoll())
	{
		if (LootTable.TableToRoll)
		{
			const TArray<FName> AllRows = LootTable.TableToRoll->GetRowNames();

			for (int32 i = 0; i < LootTable.NumRolls; ++i)
			{
				if (FMath::FRand() <= LootTable.Chance)
				{
					const FName RandomRowName = AllRows[FMath::RandRange(0, AllRows.Num() - 1)];
					const FString ContextString = "LootRoll";

					//Grab a row from the loot table, and if it passes, grant the item
					if (FLootTableRow* Row = LootTable.TableToRoll->FindRow<FLootTableRow>(RandomRowName, ContextString))
					{
						if (FMath::FRand() <= Row->Chance)
						{

							//Grant the items and item collections 
							for (FItemWithQuantity& ItemToGive : Row->ItemsToGrant)
							{
								OutItemAddResults.Add(TryAddItemFromClass(ItemToGive.Item.LoadSynchronous(), ItemToGive.Quantity));
							}

							for (auto& Collection : Row->ItemCollectionsToGrant)
							{
								if (IsValid(Collection))
								{
									for (auto& Item : Collection->Items)
									{
										if (!Item.Item.IsNull() && Item.Quantity > 0)
										{
											OutItemAddResults.Add(TryAddItemFromClass(Item.Item.LoadSynchronous(), Item.Quantity));
										}
									}
								}
							}

							for (auto& Subtable : Row->SubTablesToRoll)
							{
								//Make sure the subtable we're about to roll isn't the same table we're already rolling, as that would cause an infinite loop 
								if (Subtable.TableToRoll != LootTable.TableToRoll)
								{
									TryAddFromLootTable(Subtable, OutItemAddResults);
								}
							}
						}
					}
				}
			}
		}
	}

}

FItemAddResult UNarrativeInventoryComponent::TryAddItemFromClass(TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity /*=1*/, const bool bCheckAutoUse/*=false*/)
{
	if (ItemClass && Quantity > 0)
	{
		TStrongObjectPtr<UNarrativeInventoryComponent> InventoryLifetime(this);
		TStrongObjectPtr<AActor> OwnerLifetime(GetOwner());
		const uint64 LoadRevision = GetCinematicLoadRevision();
		const auto OwnsNotification = [this, &OwnerLifetime, LoadRevision]()
		{
			return IsValid(this) && IsValid(OwnerLifetime.Get()) && GetOwner() == OwnerLifetime.Get()
				&& !OwnerLifetime->IsActorBeingDestroyed() && GetCinematicLoadRevision() == LoadRevision;
		};
		FItemAddResult AddResult = TryAddItem_Internal(ItemClass, Quantity);
		TArray<TStrongObjectPtr<UNarrativeItem>> GrantedLifetimes;
		for (UNarrativeItem* Stack : AddResult.Stacks)
		{ if (IsValid(Stack)) { GrantedLifetimes.Emplace(Stack); } }

		if (AddResult.AmountGiven > 0 && OwnsNotification())
		{
			OnItemAdded.Broadcast(AddResult);
		}

		//TODO adding items on acquired makes attachment fail in networked 
		if (bCheckAutoUse && OwnsNotification() && GetNetMode() == NM_Standalone)
		{
			TArray<TWeakObjectPtr<UNarrativeItem>> GrantedStacks;
			for (UNarrativeItem* Stack : AddResult.Stacks) { GrantedStacks.Add(Stack); }
			for (const TWeakObjectPtr<UNarrativeItem>& WeakStack : GrantedStacks)
			{
				if (!OwnsNotification()) { break; }
				if (!WeakStack.IsValid()) { continue; }
				TStrongObjectPtr<UNarrativeItem> Stack(WeakStack.Get());
				const uint64 Membership = Stack->GetInventoryMembershipRevision();
				if (Stack->OwningInventory == this && Items.Contains(Stack.Get()) && Stack->ShouldUseOnAdd()
					&& OwnsNotification() && IsValid(Stack.Get()) && Stack->OwningInventory == this
					&& Items.Contains(Stack.Get()) && Stack->GetInventoryMembershipRevision() == Membership)
				{
					UseItem(Stack.Get());
				}
			}
		}


		return AddResult;
	}
	return FItemAddResult::AddedNone(Quantity, FText::FromString("Failed to add item to inventory."));
}

int32 UNarrativeInventoryComponent::ConsumeItem(class UNarrativeItem* Item, const int32 Quantity)
{
	if (Item && Item->OwningInventory == this && Items.Contains(Item) && Item->CanBeRemoved() && Quantity > 0)
	{
		if (GetOwnerRole() < ROLE_Authority)
		{
			ServerConsumeItem(Item, Quantity);
			return 0;
		}

		if (GetOwner() && GetOwner()->HasAuthority())
		{
			if (!IsValid(Item) || Item->OwningInventory != this || !Items.Contains(Item)) { return 0; }
			const int32 RemoveQuantity = FMath::Min(Quantity, Item->GetQuantity());

			//We shouldn't have a negative amount of the item after the drop
			ensure(!(Item->GetQuantity() - RemoveQuantity < 0));

			//We now have zero of this item, remove it from the inventory
			Item->SetQuantity(Item->GetQuantity() - RemoveQuantity);

			if (Item->GetQuantity() <= 0)
			{
				RemoveItem(Item);
			}

			OnItemRemoved.Broadcast(Item, RemoveQuantity);

			return RemoveQuantity;
		}
	}

	return 0;
}

int32 UNarrativeInventoryComponent::ConsumeItemExact(UNarrativeItem* Item, int32 Quantity,
	uint64 ExpectedQuantityRevision, TFunction<bool()> IsOwnerCurrent)
{
	if (Quantity <= 0 || !IsValid(Item) || !IsValid(GetOwner()) || !GetOwner()->HasAuthority()
		|| GetOwner()->IsActorBeingDestroyed() || !IsOwnerCurrent) { return 0; }
	TStrongObjectPtr<UNarrativeItem> PinnedItem(Item);
	TStrongObjectPtr<UNarrativeInventoryComponent> PinnedInventory(this);
	TStrongObjectPtr<AActor> PinnedOwner(GetOwner());
	const uint64 Membership = Item->GetInventoryMembershipRevision();
	const uint64 LoadRevision = GetCinematicLoadRevision();
	const auto CanCommit = [this, Item, Quantity, ExpectedQuantityRevision, Membership, LoadRevision, &PinnedOwner, &IsOwnerCurrent]()
	{
		return IsOwnerCurrent() && IsValid(this) && IsValid(PinnedOwner.Get())
			&& GetOwner() == PinnedOwner.Get() && !PinnedOwner->IsActorBeingDestroyed()
			&& PinnedOwner->HasAuthority() && GetCinematicLoadRevision() == LoadRevision
			&& IsValid(Item) && Item->OwningInventory == this && Items.Contains(Item)
			&& Item->GetInventoryMembershipRevision() == Membership
			&& Item->GetQuantityRevision() == ExpectedQuantityRevision && Item->GetQuantity() >= Quantity;
	};
	if (!CanCommit() || !Item->CanBeRemoved() || !CanCommit()) { return 0; }
	const int32 After = Item->GetQuantity() - Quantity;
	Item->SetQuantity(After);
	// The exact debit is already committed. Never remove a replacement/refilled stack
	// after its quantity notification changed membership or wrote a new quantity.
	if (After == 0 && IsValid(this) && IsValid(PinnedOwner.Get()) && GetOwner() == PinnedOwner.Get()
		&& !PinnedOwner->IsActorBeingDestroyed() && GetCinematicLoadRevision() == LoadRevision
		&& IsValid(Item) && Item->OwningInventory == this && Items.Contains(Item)
		&& Item->GetInventoryMembershipRevision() == Membership
		&& Item->GetQuantityRevision() == ExpectedQuantityRevision + 1 && Item->GetQuantity() == 0)
	{
		// Removal permission was already admitted. Asking the overridable permission
		// hook again would let it refill the stack between this check and deletion.
		RemoveOwnedItemInternal(Item);
	}
	if (IsValid(this) && IsValid(PinnedOwner.Get()) && GetOwner() == PinnedOwner.Get()
		&& !PinnedOwner->IsActorBeingDestroyed() && GetCinematicLoadRevision() == LoadRevision)
	{ OnItemRemoved.Broadcast(Item, Quantity); }
	return Quantity;
}

int32 UNarrativeInventoryComponent::GetTotalQuantityOfItem(TSubclassOf<UNarrativeItem> ItemClass, const bool bCheckVisibility) const
{
	TArray<UNarrativeItem*> FoundItems;
	GetItemsOfClass(ItemClass, FoundItems, bCheckVisibility);

	int32 Total = 0;

	for (auto& Item : FoundItems)
	{
		if (Item)
		{
			Total += Item->GetQuantity();
		}
	}

	return Total;
}

int32 UNarrativeInventoryComponent::GetTotalQuantityOfItemExact(TSoftClassPtr<UNarrativeItem> ItemClass, const bool bCheckVisibility /*= false*/) const
{
	TArray<UNarrativeItem*> FoundItems = FindItemsByClass(ItemClass, bCheckVisibility);

	int32 Total = 0;

	for (auto& Item : FoundItems)
	{
		if (Item)
		{
			Total += Item->GetQuantity();
		}
	}

	return Total;
}

int32 UNarrativeInventoryComponent::ConsumeItemsOfClass(TSubclassOf<UNarrativeItem> ItemClass, const int32 Quantity /*= 1*/)
{
	TArray<UNarrativeItem*> FoundItems;
	GetItemsOfClass(ItemClass, FoundItems, false);

	int32 LeftToConsume = Quantity;

	for(auto& Item : FoundItems)
	{
		if (Item)
		{
			const int32 Consumed = ConsumeItem(Item, LeftToConsume);

			LeftToConsume -= Consumed;

			//Should never consume more than what we wanted. 
			check(LeftToConsume >= 0);

			if (LeftToConsume == 0)
			{
				break;
			}
		}
	}

	return Quantity - LeftToConsume;
}

bool UNarrativeInventoryComponent::RemoveItem(class UNarrativeItem* Item)
{
	if (Item && Item->OwningInventory == this && Items.Contains(Item) && Item->CanBeRemoved())
	{
		if (GetOwnerRole() < ROLE_Authority)
		{
			ServerRemoveItem(Item);
			return false;
		}

		if (GetOwner() && GetOwner()->HasAuthority())
		{
			if (IsValid(Item) && Item->OwningInventory == this && Items.Contains(Item))
			{
				return RemoveOwnedItemInternal(Item);
			}
		}

	}

	return false;
}

bool UNarrativeInventoryComponent::HasItem(TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity /*= 1*/, const bool bCheckVisibility/*= false*/) const
{
	if (IsValid(ItemClass))
	{	
		return GetTotalQuantityOfItem(ItemClass, bCheckVisibility) >= Quantity;
	}

	return false;
}

bool UNarrativeInventoryComponent::HasItemExact(TSoftClassPtr<class UNarrativeItem> ItemClass, const int32 Quantity /*= 1*/, const bool bCheckVisibility /*= false*/) const
{
	if (ItemClass.IsValid())
	{
		if (Quantity == 1)
		{
			return IsValid(FindItemByClassExact(ItemClass));
		}
		else
		{
			return GetTotalQuantityOfItemExact(ItemClass, bCheckVisibility) >= Quantity;
		}
	}

	return false;
}

bool UNarrativeInventoryComponent::AllowLootItem(class UNarrativeInventoryComponent* Taker, TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity, FText& ErrorText)  const
{
	/**In order to prevent cheating, we'll generally assume that in a multiplayer game, you'd never want to let players loot
	from another active players inventory. Even if we're looting someones corpse, thats probably a corpse actor you'd be looting from,
	not our actual player state. */
	if (Taker)
	{
		const bool bTakerHasSpace = Taker->GetSpaceForItem(ItemClass, ErrorText) >= Quantity;
		const bool bShopHasItem = HasItem(ItemClass, Quantity);

		if (!bTakerHasSpace)
		{
			ErrorText = LOCTEXT("AllowLootItem_NotEnoughSpace", "You don't have enough space to carry this item. ");
		}
		else if (!bShopHasItem)
		{
			ErrorText = LOCTEXT("AllowLootItem_ShopHasItem", "There isn't enough of the item to take! ");
		}

		//If we're a vendor, need to check we have money 
		const bool bTheyHaveCurrency = !bIsVendor || Taker->GetCurrency() >= GetSellPrice(ItemClass, Quantity);

		if (!bTheyHaveCurrency)
		{
			ErrorText = LOCTEXT("AllowLootItem_NotEnoughMoney", "You don't have enough money to complete this trade.");
		}

		return bTakerHasSpace && bShopHasItem && bTheyHaveCurrency;
	}

	return false;
}

bool UNarrativeInventoryComponent::AllowStoreItem(class UNarrativeInventoryComponent* Storer, TSubclassOf <class UNarrativeItem> ItemClass, const int32 Quantity, FText& ErrorText) const
{
	if (Storer)
	{
		const bool bWeHaveSpace = GetSpaceForItem(ItemClass, ErrorText) >= Quantity;
		const bool bStorerHasItem = Storer->HasItem(ItemClass, Quantity);

		if (!bWeHaveSpace)
		{
			ErrorText = LOCTEXT("AllowStoreItem_WeHaveSpace", "You don't have enough space to carry this item. ");
		}
		else if (!bStorerHasItem)
		{
			//ErrorText = LOCTEXT("AllowLootItem_SellerHasItem", "You no longer have the item in your inventory.");
		}

		const bool bWeHaveCurrency = !bIsVendor || GetCurrency() >= GetBuyPrice(ItemClass, Quantity);

		if (!bWeHaveCurrency)
		{
			ErrorText = LOCTEXT("AllowStoreItem_NotEnoughMoney", "The shop doesn't have enough money to complete the trade.");
		}

		return bWeHaveSpace && bStorerHasItem && bWeHaveCurrency;
	}

	return false;
}

FItemAddResult UNarrativeInventoryComponent::PerformLootItem(class UNarrativeInventoryComponent* Taker, TSubclassOf <class UNarrativeItem> ItemClass, const int32 Quantity /*= 1*/)
{
	if (Taker)
	{
		//Give the item to the looter
		const FItemAddResult AddResult = Taker->TryAddItemFromClass(ItemClass, Quantity);

		//However many we gave, take them from our inventory 
		if (AddResult.AmountGiven > 0)
		{
			if (UNarrativeItem* Item = FindItemByClassExact(TSoftClassPtr<UNarrativeItem>(ItemClass)))
			{
				//However much of the item we looted, remove from the loot source
				ConsumeItem(Item, AddResult.AmountGiven);
			}

			if (bIsVendor && Taker)
			{
				const int32 TransactionPrice = GetSellPrice(ItemClass, AddResult.AmountGiven);

				//We sold an item, need to remove cash from taker and store in our inventory 
				Taker->AddCurrency(-TransactionPrice);
				AddCurrency(TransactionPrice);
			}
		}

		return AddResult;
	}

	return FItemAddResult::AddedNone(0, FText::GetEmpty());
}

FItemAddResult UNarrativeInventoryComponent::PerformStoreItem(class UNarrativeInventoryComponent* Storer, TSubclassOf <class UNarrativeItem> ItemClass, const int32 Quantity /*= 1*/)
{
	if (Storer)
	{
		//Store the item in our inventory
		const FItemAddResult AddResult = TryAddItemFromClass(ItemClass, Quantity);

		if (AddResult.AmountGiven > 0)
		{
			if (UNarrativeItem* Item = Storer->FindItemByClassExact(TSoftClassPtr<UNarrativeItem>(ItemClass)))
			{
				//However much of the item we managed to store, remove from the storers inventory
				Storer->ConsumeItem(Item, AddResult.AmountGiven);
			}

			if (bIsVendor && Storer)
			{
				const int32 TransactionPrice = GetBuyPrice(ItemClass, AddResult.AmountGiven);

				//We sold an item, need to remove cash from taker and store in our inventory 
				Storer->AddCurrency(TransactionPrice);
				AddCurrency(-TransactionPrice);
			}
		}



		return AddResult;
	}

	return FItemAddResult::AddedNone(0, FText::GetEmpty());
}

UNarrativeItem* UNarrativeInventoryComponent::FindItemByGUID(const FGuid& ItemGUID) const
{

	if (ItemGUID.IsValid() && GetOwnerRole() >= ROLE_Authority)
	{
		if (ItemGUIDMap.Contains(ItemGUID))
		{
			return ItemGUIDMap[ItemGUID];
		}
	}


	return nullptr; 
}

UNarrativeItem* UNarrativeInventoryComponent::FindItemOfClass(TSubclassOf<class UNarrativeItem> ItemClass, const bool bCheckVisibility /*= false*/) const
{
	for (auto& InvItem : Items)
	{
		//Find item by class checks for an exact class match instead of child of
		if (InvItem && (!bCheckVisibility || InvItem->ShouldShowInInventory()) && InvItem->GetClass()->IsChildOf(ItemClass))
		{
			return InvItem;
		}
	}
	return nullptr;
}

UNarrativeItem* UNarrativeInventoryComponent::FindItemByClassExact(TSoftClassPtr<class UNarrativeItem> ItemClass, const bool bCheckVisibility/*= false*/) const
{
	for (auto& InvItem : Items)
	{
		//Find item by class checks for an exact class match instead of child of
		if (InvItem && (!bCheckVisibility || InvItem->ShouldShowInInventory()) && InvItem->GetClass() == ItemClass)
		{
			return InvItem;
		}
	}
	return nullptr;
}

TArray<UNarrativeItem*> UNarrativeInventoryComponent::FindItemsByClass(TSoftClassPtr<class UNarrativeItem> ItemClass, const bool bCheckVisibility /*= false*/) const
{
	TArray<UNarrativeItem*> RetItems; 

	for (auto& InvItem : Items)
	{
		//Find item by class checks for an exact class match instead of child of
		if (InvItem && (!bCheckVisibility || InvItem->ShouldShowInInventory()) && InvItem->GetClass() == ItemClass)
		{
			RetItems.Add(InvItem);
		}
	}
	return RetItems; 

}

TArray<UNarrativeItem*> UNarrativeInventoryComponent::FindItemsOfClass(TSubclassOf<class UNarrativeItem> ItemClass, const bool bCheckVisibility/*= false*/) const
{
	TArray<UNarrativeItem*> ItemsOfClass;

	for (auto& InvItem : Items)
	{
		if (InvItem && (!bCheckVisibility || InvItem->ShouldShowInInventory()) && InvItem->GetClass()->IsChildOf(ItemClass))
		{
			ItemsOfClass.Add(InvItem);
		}
	}

	return ItemsOfClass;
}

bool UNarrativeInventoryComponent::GetItemsOfClass(TSubclassOf<UNarrativeItem> ItemClass, TArray<UNarrativeItem*>& OutItems, const bool bCheckVisibility/*= false*/) const
{
	TArray<UNarrativeItem*> ItemsOfClass;

	for (auto& InvItem : Items)
	{
		if (InvItem && (!bCheckVisibility || InvItem->ShouldShowInInventory()) && InvItem->GetClass()->IsChildOf(ItemClass))
		{
			OutItems.Add(InvItem);
		}
	}

	return OutItems.Num() > 0;
}

bool UNarrativeInventoryComponent::GetItemsUsableWith(UNarrativeItem* Item, TArray<UNarrativeItem*>& OutItems, const bool bCheckVisibility /*= false*/) const
{
	if (IsValid(Item))
	{
		TArray<UNarrativeItem*> ItemsOfClass;

		for (auto& InvItem : Items)
		{
			if (InvItem && Item->CanUseItemWith(InvItem))
			{
				OutItems.Add(InvItem);
			}
		}
	}

	return OutItems.Num() > 0;
}

int32 UNarrativeInventoryComponent::GetSpaceForItem(TSubclassOf<class UNarrativeItem> ItemClass, FText& NoSpaceReason) const 
{
	if (!IsValid(ItemClass))
	{
		return 0;
	}

	//Figure out how many of a given item we can add 
	if (const UNarrativeItem* ItemCDO = GetDefault<UNarrativeItem>(ItemClass))
	{
		const int32 MaxStackSize = ItemCDO->bStackable ? ItemCDO->MaxStackSize : 1;
		int32 ExistingStackSpace = 0;

		// Existing stacks can still accept items when every inventory slot is in
		// use. Sum all matching stack room before deciding capacity is exhausted.
		for (const UNarrativeItem* ExistingItem : FindItemsOfClass(ItemClass))
		{
			if (IsValid(ExistingItem))
			{
				ExistingStackSpace += FMath::Max(ExistingItem->GetStackSpace(), 0);
			}
		}

		const int32 EmptySlots = FMath::Max(GetCapacity() - Items.Num(), 0);
		const int32 CapacitySpace = ExistingStackSpace + (EmptySlots * MaxStackSize);
		if (CapacitySpace <= 0)
		{
			NoSpaceReason = LOCTEXT("NoSpaceReason_CapacitySpace", "You don't have any inventory slots left for this item.");
			return 0;
		}

		const int32 WeightSpace = FMath::IsNearlyZero(ItemCDO->Weight)
			? INT_MAX
			: FMath::Max(
				FMath::FloorToInt((WeightCapacity - GetCurrentWeight()) / ItemCDO->Weight),
				0);

		if (WeightSpace < CapacitySpace)
		{
			if (WeightSpace <= 0)
			{
				NoSpaceReason = LOCTEXT("NoSpaceReason_WeightFull", "You're carrying too much weight.");
			}

			return WeightSpace;
		}

		return CapacitySpace;
	}

	return 0;
}

int32 UNarrativeInventoryComponent::GetCurrency() const
{
	return Currency;
}

void UNarrativeInventoryComponent::AddCurrency(const int32 Amount)
{
	if (GetOwnerRole() >= ROLE_Authority && Amount != 0)
	{
		SetCurrency(GetCurrency() + Amount);
	}
}

void UNarrativeInventoryComponent::SetCurrency(const int32 Amount)
{
	if (GetOwnerRole() >= ROLE_Authority && Amount >= 0)
	{
		int32 OldCurrency = Currency;
		Currency = Amount;
		OnRep_Currency(OldCurrency);
	}
}

float UNarrativeInventoryComponent::GetCurrentWeight() const
{
	float Weight = 0.f;

	for (auto& Item : Items)
	{
		if (Item)
		{
			Weight += Item->GetStackWeight();
		}
	}

	return Weight;
}

bool UNarrativeInventoryComponent::IsVendor() const
{
	return bIsVendor;
}

void UNarrativeInventoryComponent::SetWeightCapacity(const float NewWeightCapacity)
{
	WeightCapacity = NewWeightCapacity;
	OnInventoryUpdated.Broadcast();
}

void UNarrativeInventoryComponent::SetCapacity(const int32 NewCapacity)
{
	Capacity = NewCapacity;
	OnInventoryUpdated.Broadcast();
}

void UNarrativeInventoryComponent::SetInventoryFriendlyName(const FText& Name)
{
	InventoryFriendlyName = Name;
}

void UNarrativeInventoryComponent::SetIsVendor(const bool bNewIsVendor)
{
	bIsVendor = bNewIsVendor;
}

APawn* UNarrativeInventoryComponent::GetOwningPawn() const
{
	if (OwnerPC)
	{
		return OwnerPC->GetPawn();
	}
	
	if (APlayerState* OwningPS = Cast<APlayerState>(GetOwner()))
	{
		return OwningPS->GetPawn();
	}

	if (APawn* OwningPawn = Cast<APawn>(GetOwner()))
	{
		return OwningPawn;
	}

	if (APlayerController* OwningController = Cast<APlayerController>(GetOwner()))
	{
		return OwningController->GetPawn();
	}

	return nullptr;
}

APlayerController* UNarrativeInventoryComponent::GetOwningController() const
{
	//We cache this on beginplay as to not re-find it every time 
	if (OwnerPC)
	{
		return OwnerPC;
	}

	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	APlayerState* OwningPS = Cast<APlayerState>(GetOwner());

	if (OwningController)
	{
		return OwningController;
	}

	if (!OwningController && OwningPawn)
	{
		return Cast<APlayerController>(OwningPawn->GetController());
	}

	if (OwningPS)
	{
		return OwningPS->GetPlayerController();
	}

	return nullptr;
}

void UNarrativeInventoryComponent::ClientRefreshInventory_Implementation()
{
	OnInventoryUpdated.Broadcast();
}

void UNarrativeInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPC = GetOwningController();
}

void UNarrativeInventoryComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (auto& Item : TickItems)
	{
		if (IsValid(Item))
		{
			Item->TickItem(DeltaTime);
		}
	}
}

void UNarrativeInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//DOREPLIFETIME(UNarrativeInventoryComponent, Items);
	DOREPLIFETIME(UNarrativeInventoryComponent, Currency);
	DOREPLIFETIME(UNarrativeInventoryComponent, ReplicatedItems);

	//If your game requires players to know if other players are looting you can remove 
	DOREPLIFETIME_CONDITION(UNarrativeInventoryComponent, LootSource, COND_OwnerOnly);

	//If your game requires players to know if other players are looting you can uncomment this
	//DOREPLIFETIME_CONDITION(UNarrativeInventoryComponent, LootSource, COND_None);
}

bool UNarrativeInventoryComponent::ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	//Check if the array of items needs to replicate
	if (Channel->KeyNeedsToReplicate(0, ReplicatedItemsKey))
	{
		for (auto& Item : Items)
		{
			if (Channel->KeyNeedsToReplicate(Item->GetUniqueID(), Item->RepKey))
			{
				bWroteSomething |= Channel->ReplicateSubobject(Item, *Bunch, *RepFlags);
			}
		}
	}

	return bWroteSomething;
}

void UNarrativeInventoryComponent::OnRegister()
{
	Super::OnRegister();
	
	ReplicatedItems.OwningInventory = this; 
}

UNarrativeItem* UNarrativeInventoryComponent::AddItem(TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity)
{
	if (Quantity > 0 && GetOwner() && GetOwner()->HasAuthority())
	{
		//Construct the item, initialize its values, and call all the relevant functions for replication etc
		//UNarrativeItem* NewItem = NewObject<UNarrativeItem>(GetOwner(), Item->GetClass());
		TStrongObjectPtr<UNarrativeInventoryComponent> InventoryLifetime(this);
		TStrongObjectPtr<UNarrativeItem> ItemLifetime(NewObject<UNarrativeItem>(GetOwner(), ItemClass));
		UNarrativeItem* NewItem = ItemLifetime.Get();
		NewItem->World = GetWorld();
		NewItem->OwningInventory = this;
		++NewItem->InventoryMembershipRevision;

		NewItem->SetQuantity(Quantity);

		Items.Add(NewItem);
		
		// Experimental fast array replication
		FNarrativeItemEntry& Item = ReplicatedItems.Items.Add_GetRef(NewItem);
		ReplicatedItems.MarkItemDirty(Item);
		
		//only assign GUID if we're a new item, otherwise serialize will restore this for loaded items. 
		if (!bIsLoading)
		{
			NewItem->ItemGUID = FGuid::NewGuid();
			ItemGUIDMap.Add(NewItem->ItemGUID, NewItem);
		}

		NewItem->AddedToInventory(this, bIsLoading);
		if (IsValid(NewItem)) { NewItem->MarkDirtyForReplication(); }

		//Clients get this via an OnRep, server needs to manually call 
		if (IsValid(this) && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed())
		{ OnInventoryUpdated.Broadcast(); }
		
		return NewItem;
	}

	return nullptr;
}

void UNarrativeInventoryComponent::OnRep_Currency(const int32 OldCurrency)
{
	OnCurrencyChanged.Broadcast(OldCurrency, Currency);
}

void UNarrativeInventoryComponent::OnRep_LootSource(class UNarrativeInventoryComponent* OldLootSource)
{
	if (LootSource)
	{
		OnBeginLooting.Broadcast(LootSource);
	}
	else
	{
		OnEndLooting.Broadcast();
	}
}

FItemAddResult UNarrativeInventoryComponent::TryAddItem_Internal(TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity /*= 1*/)
{
	if (Quantity <= 0 || !IsValid(GetOwner()) || !GetOwner()->HasAuthority()
		|| GetOwner()->IsActorBeingDestroyed() || !IsValid(ItemClass))
	{
		return FItemAddResult::AddedNone(Quantity, LOCTEXT("ErrorMessage", ""));
	}
	TStrongObjectPtr<UNarrativeInventoryComponent> InventoryLifetime(this);
	TStrongObjectPtr<AActor> OwnerLifetime(GetOwner());
	const uint64 LoadRevision = GetCinematicLoadRevision();
	const auto OwnsGrant = [this, &OwnerLifetime, LoadRevision]()
	{
		return IsValid(this) && IsValid(OwnerLifetime.Get()) && GetOwner() == OwnerLifetime.Get()
			&& !OwnerLifetime->IsActorBeingDestroyed() && OwnerLifetime->HasAuthority()
			&& GetCinematicLoadRevision() == LoadRevision;
	};
	const UNarrativeItem* Defaults = GetDefault<UNarrativeItem>(ItemClass);
	if (!IsValid(Defaults) || Defaults->GetMaxStackSize() <= 0)
	{
		return FItemAddResult::AddedNone(Quantity, LOCTEXT("ErrorMessage", ""));
	}
	const int32 MaxStackSize = Defaults->GetMaxStackSize();
	FItemAddResult Result;
	Result.ItemClass = ItemClass;
	Result.AmountToGive = Quantity;
	Result.AmountGiven = 0;
	TArray<TStrongObjectPtr<UNarrativeItem>> GrantedLifetimes;
	const int32 Admitted = FMath::Clamp(GetSpaceForItem(ItemClass, Result.ErrorText), 0, Quantity);
	if (!OwnsGrant() || Admitted == 0) { return Result; }

	// Weak snapshots survive removal/GC during a prior stack's synchronous delegates.
	TArray<TWeakObjectPtr<UNarrativeItem>> ExistingStacks;
	for (UNarrativeItem* Stack : FindItemsOfClass(ItemClass)) { ExistingStacks.Add(Stack); }
	for (const TWeakObjectPtr<UNarrativeItem>& WeakStack : ExistingStacks)
	{
		if (!OwnsGrant() || Result.AmountGiven == Admitted) { return Result; }
		if (!WeakStack.IsValid()) { continue; }
		TStrongObjectPtr<UNarrativeItem> StackLifetime(WeakStack.Get());
		UNarrativeItem* Stack = StackLifetime.Get();
		const uint64 Membership = Stack->GetInventoryMembershipRevision();
		const uint64 QuantityRevision = Stack->GetQuantityRevision();
		const int32 CapacityRemaining = FMath::Max(GetSpaceForItem(ItemClass, Result.ErrorText), 0);
		if (!OwnsGrant()) { return Result; }
		if (!IsValid(Stack) || Stack->OwningInventory != this || !Items.Contains(Stack)
			|| Stack->GetInventoryMembershipRevision() != Membership
			|| Stack->GetQuantityRevision() != QuantityRevision) { continue; }
		const int32 OldQuantity = Stack->GetQuantity();
		const int32 Applied = FMath::Min3(Admitted - Result.AmountGiven,
			FMath::Max(Stack->GetMaxStackSize() - OldQuantity, 0), CapacityRemaining);
		if (Applied <= 0) { continue; }
		// Count this committed write before notifications can spend it, refill it,
		// remove the stack or initiate another pickup. Net deltas are not receipts.
		Result.AmountGiven += Applied;
		GrantedLifetimes.Emplace(Stack);
		Result.Stacks.Add(Stack);
		Stack->SetQuantity(OldQuantity + Applied);
	}

	while (OwnsGrant() && Result.AmountGiven < Admitted)
	{
		const int32 CapacityRemaining = FMath::Max(GetSpaceForItem(ItemClass, Result.ErrorText), 0);
		if (!OwnsGrant()) { break; }
		const int32 Applied = FMath::Min3(Admitted - Result.AmountGiven, MaxStackSize, CapacityRemaining);
		if (Applied <= 0) { break; }
		// AddItem is the native membership primitive; a non-null return proves the
		// new stack was committed even if its AddedToInventory callback consumed it.
		UNarrativeItem* NewItem = AddItem(ItemClass, Applied);
		if (!NewItem) { break; }
		Result.AmountGiven += Applied;
		if (IsValid(NewItem)) { GrantedLifetimes.Emplace(NewItem); Result.Stacks.Add(NewItem); }
	}
	return Result;
}


void UNarrativeInventoryComponent::GiveDefaultItems()
{
	if (!bGaveDefaultItems && GetOwnerRole() >= ROLE_Authority)
	{
		TArray<FItemAddResult> Results;

		/**TODO. This will cause a pretty big hitch because we're using LoadSyncronous on the items. We instead are going to want 
		to modify TryAddFromLootTable to instead async load all the item soft refs and callback when they are ready to be added. */
		for (auto& DefaultItemTable : DefaultItemTables)
		{
			TryAddFromLootTable(DefaultItemTable, Results);
		}

		//This is marked SaveGame, ensuring inventories are granted their items only once ever, even across multiple sessions. 
		bGaveDefaultItems = true;
	}
}

void UNarrativeInventoryComponent::AddTickItem(UNarrativeItem* Item)
{
	if (IsValid(Item))
	{
		TickItems.Add(Item);

		SetComponentTickEnabled(true);
	}
}
void UNarrativeInventoryComponent::RemoveTickItem(UNarrativeItem* Item)
{
	if (IsValid(Item))
	{
		TickItems.Remove(Item);
	}

	if (TickItems.Num() <= 0)
	{
		SetComponentTickEnabled(false);
	}
}

void UNarrativeInventoryComponent::ServerRemoveItem_Implementation(class UNarrativeItem* Item)
{
	if (Item)
	{
		RemoveItem(Item);
	}
}

void UNarrativeInventoryComponent::ServerConsumeItem_Implementation(class UNarrativeItem* Item, const int32 Quantity)
{
	if (Item)
	{
		ConsumeItem(Item, Quantity);
	}
}

void UNarrativeInventoryComponent::ServerUseItem_Implementation(class UNarrativeItem* Item, class UNarrativeItem* OtherItem/*=nullptr*/)
{
	UseItem(Item, OtherItem);
}

void UNarrativeInventoryComponent::ServerStopLooting_Implementation()
{
	StopLooting();
}

void UNarrativeInventoryComponent::StopLooting()
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerStopLooting();
	}

	SetLootSource(nullptr);
}

bool UNarrativeInventoryComponent::RequestLootItem(class UNarrativeItem* ItemToLoot, FText& ErrorText, const int32 Quantity /*= 1*/)
{
	if (LootSource && ItemToLoot && ItemToLoot->CanBeRemoved())
	{
		if (LootSource && LootSource->AllowLootItem(this, ItemToLoot->GetClass(), Quantity, ErrorText))
		{
			if (GetOwnerRole() < ROLE_Authority)
			{
				ServerRequestLootItem(ItemToLoot, Quantity);
				return true;
			}

 			LootSource->PerformLootItem(this, ItemToLoot->GetClass(), Quantity);
			return true;
		}

	}
	return false;
}

void UNarrativeInventoryComponent::ServerRequestLootItem_Implementation(class UNarrativeItem* ItemToLoot, const int32 Quantity)
{
	FText DummyText;
	RequestLootItem(ItemToLoot, DummyText, Quantity);
}

bool UNarrativeInventoryComponent::RequestStoreItem(class UNarrativeItem* ItemToStore, FText& ErrorText, const int32 Quantity /*= 1*/)
{
	if (ItemToStore && ItemToStore->CanBeRemoved())
	{
		if (LootSource && LootSource->AllowStoreItem(this, ItemToStore->GetClass(), Quantity, ErrorText))
		{
			if (GetOwnerRole() < ROLE_Authority)
			{
				ServerRequestStoreItem(ItemToStore, Quantity);
				return true;
			}

			LootSource->PerformStoreItem(this, ItemToStore->GetClass(), Quantity);
			return true;
		}
	}
	return false;
}

void UNarrativeInventoryComponent::ServerRequestStoreItem_Implementation(class UNarrativeItem* ItemToLoot, const int32 Quantity)
{
	FText DummyText;
	RequestStoreItem(ItemToLoot, DummyText, Quantity);
}

int32 UNarrativeInventoryComponent::GetBuyPrice_Implementation(TSubclassOf<class UNarrativeItem> Item, int32 Quantity /*= 1*/) const
{
	if (IsValid(Item))
	{
		if (const UNarrativeItem* ItemCDO = GetDefault<UNarrativeItem>(Item))
		{
			return FMath::CeilToInt((ItemCDO->BaseValue * BuyItemPct) * Quantity);
		}
	}
	return 0;
}

int32 UNarrativeInventoryComponent::GetSellPrice_Implementation(TSubclassOf<class UNarrativeItem> Item, int32 Quantity /*= 1*/) const
{
	if (IsValid(Item))
	{
		if (const UNarrativeItem* ItemCDO = GetDefault<UNarrativeItem>(Item))
		{
			return FMath::CeilToInt((ItemCDO->BaseValue * SellItemPct) * Quantity);
		}
	}

	return INT_MAX;
}

void UNarrativeInventoryComponent::SetLootSource(class UNarrativeInventoryComponent* NewLootSource)
{
	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (LootSource != NewLootSource)
		{
			UNarrativeInventoryComponent* OldSource = LootSource;
			LootSource = NewLootSource;
			OnRep_LootSource(OldSource);
		}
	}
}

void FNarrativeItemEntry::PreReplicatedRemove(const struct FNarrativeItemArray& InArraySerializer)
{
	//Ask client to do any remove logic  
	if (Item)
	{
		//Maintain the items array locally for legacy
		if (Item->OwningInventory)
		{
			Item->OwningInventory->Items.RemoveSingle(Item);
		}
		
		Item->RemovedFromInventory(Item->OwningInventory);
	}
}

void FNarrativeItemEntry::PostReplicatedAdd(const struct FNarrativeItemArray& InArraySerializer)
{

	//Ask client to do any remove logic  
	if (Item)
	{
		// Item Added
		UE_LOG(LogTemp, Log, TEXT("Item: %s added"), *Item->DisplayName.ToString());
	
		//Any items without a world set have been added
		if (!Item->World)
		{
			check(InArraySerializer.OwningInventory);
			
			Item->World = InArraySerializer.OwningInventory->GetWorld();
			Item->OwningInventory = InArraySerializer.OwningInventory;

			//New active items won't have had their OnRep called, we need to call it
			if (Item->bActive)
			{
				Item->OnRep_bActive(false);
			}

			//Maintain the items array locally for legacy 
			Item->OwningInventory->Items.Add(Item);
		}
	
		Item->AddedToInventory(Item->OwningInventory, false);
	}
}

void FNarrativeItemEntry::PostReplicatedChange(const struct FNarrativeItemArray& InArraySerializer)
{

}

FString FNarrativeItemEntry::GetDebugString()
{
	return Item ? Item->DisplayName.ToString() : "";
}

void FNarrativeItemArray::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	if (OwningInventory)
	{
		OwningInventory->OnInventoryUpdated.Broadcast();
	}
}

void FNarrativeItemArray::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	if (OwningInventory)
	{
		OwningInventory->OnInventoryUpdated.Broadcast();
	}
}

void FNarrativeItemArray::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	if (OwningInventory)
	{
		OwningInventory->OnInventoryUpdated.Broadcast();
	}
}

UItemCollection::UItemCollection(const FObjectInitializer& ObjectInitializer)
{

}

bool UNarrativeInventoryComponent::IsLoading() const
{
	return bIsLoading;
}


#undef LOCTEXT_NAMESPACE
