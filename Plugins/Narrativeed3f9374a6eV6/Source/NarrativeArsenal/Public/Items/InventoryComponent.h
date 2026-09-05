// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeSavableComponent.h"
#include <Engine/DataTable.h>
#include <Engine/DataAsset.h>

#include "Net/Serialization/FastArraySerializer.h"
#include "Items/NarrativeCinematicTransaction.h"
#include "GameplayTagContainer.h"
#include "InventoryComponent.generated.h"

class UNarrativeItem;

USTRUCT()
struct FNarrativeItemEntry : public FFastArraySerializerItem
{
	GENERATED_USTRUCT_BODY()
	
	FNarrativeItemEntry() = default;
	FNarrativeItemEntry(UNarrativeItem* InItem): Item(InItem) {}
	
	UPROPERTY()
	UNarrativeItem* Item = nullptr;
	
	void PreReplicatedRemove(const struct FNarrativeItemArray& InArraySerializer);
	void PostReplicatedAdd(const struct FNarrativeItemArray& InArraySerializer);
	void PostReplicatedChange(const struct FNarrativeItemArray& InArraySerializer);

	// Optional: debug string used with LogNetFastTArray logging
	FString GetDebugString();
	
	bool operator==(UNarrativeItem* OtherItem) const
	{
		return OtherItem == Item;
	}
	bool operator==(const FNarrativeItemEntry OtherItem) const
	{
		return OtherItem.Item == this->Item;
	}
};

USTRUCT()
struct FNarrativeItemArray : public FFastArraySerializer
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	TArray<FNarrativeItemEntry>	Items;

	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	
	bool NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FNarrativeItemEntry, FNarrativeItemArray>( Items, DeltaParms, *this );
	}
	
public:
	
	UPROPERTY(NotReplicated)
	TObjectPtr<UNarrativeInventoryComponent> OwningInventory;
};

template<>
struct TStructOpsTypeTraits< FNarrativeItemArray > : public TStructOpsTypeTraitsBase2< FNarrativeItemArray >
{
	enum 
	{
		WithNetDeltaSerializer = true,
   };
};


//Represents the result of adding an item to the inventory.
USTRUCT(BlueprintType)
struct FItemAddResult
{

	GENERATED_BODY()

public:

	FItemAddResult() {};
	FItemAddResult(int32 InItemQuantity) : AmountToGive(InItemQuantity), AmountGiven(0) {};
	FItemAddResult(int32 InItemQuantity, int32 InQuantityAdded) : AmountToGive(InItemQuantity), AmountGiven(InQuantityAdded) {};

	//The item class we were attempting to add 
	UPROPERTY(BlueprintReadOnly, Category = "Item Add Result")
	TSubclassOf<class UNarrativeItem> ItemClass = TSubclassOf<class UNarrativeItem>();

	//Any new stacks that were created
	UPROPERTY(BlueprintReadOnly, Category = "Item Add Result")
	TArray<class UNarrativeItem*> Stacks = {};

	//The amount of the item that we tried to add
	UPROPERTY(BlueprintReadOnly, Category = "Item Add Result")
	int32 AmountToGive = 0;

	//The amount of the item that was actually added in the end. Maybe we tried adding 10 items, but only 8 could be added because of capacity/weight
	UPROPERTY(BlueprintReadOnly, Category = "Item Add Result")
	int32 AmountGiven = 0;

	//If something went wrong, like we didnt have enough capacity or carrying too much weight this contains the reason why
	UPROPERTY(BlueprintReadOnly, Category = "Item Add Result")
	FText ErrorText = FText::GetEmpty();

	//Helpers
	static FItemAddResult AddedNone(const int32 InItemQuantity, const FText& ErrorText)
	{
		FItemAddResult AddedNoneResult(InItemQuantity);
		
		AddedNoneResult.ErrorText = ErrorText;

		return AddedNoneResult;
	}

	static FItemAddResult AddedSome(TArray<class UNarrativeItem*> InItems, const int32 InItemQuantity, const int32 ActualAmountGiven, const FText& ErrorText)
	{
		FItemAddResult AddedSomeResult(InItemQuantity, ActualAmountGiven);

		AddedSomeResult.Stacks = InItems;
		AddedSomeResult.ErrorText = ErrorText;

		return AddedSomeResult;
	}

	static FItemAddResult AddedAll(TArray<class UNarrativeItem*> InItems, const int32 InItemQuantity)
	{
		FItemAddResult AddAllResult(InItemQuantity, InItemQuantity);

		AddAllResult.Stacks = InItems;

		return AddAllResult;
	}

};


//Called when the inventory is changed and the UI needs an update. 
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

/**Called on server when an item is added to this inventory*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemAdded, const FItemAddResult&, AddResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemRemoved, class UNarrativeItem*, Item, const int32, Amount);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeginLooting, class UNarrativeInventoryComponent*, LootSource);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEndLooting);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemUsed, class UNarrativeItem*, Item);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCurrencyChanged, const int32, OldCurrency, const int32, NewCurrency);

//Just a nice way of defining an item along with some amount of the item 
USTRUCT(BlueprintType)
struct FItemWithQuantity
{
	GENERATED_BODY()

public:

	FItemWithQuantity()
	{
		Item = nullptr;
		Quantity = 1;
	};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Item")
	TSoftClassPtr<class UNarrativeItem> Item;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Item")
	int32 Quantity;
};

/**
 * Data asset with a collection of items. Useful for grouping things together like armor sets, weapon sets, etc. 
 */
UCLASS()
class NARRATIVEARSENAL_API UItemCollection : public UDataAsset
{
public:

	GENERATED_BODY()

	UItemCollection(const FObjectInitializer& ObjectInitializer);

	//The items to grant if this row is given to the inventory
	UPROPERTY(EditAnywhere, Category = "Loot Table Row")
	TArray<FItemWithQuantity> Items;

};

//Defines a roll of a loot table 
USTRUCT(BlueprintType)
struct FLootTableRoll
{
	GENERATED_BODY()
public:

	FLootTableRoll()
	{
		NumRolls = 1;
		Chance = 1.f;
	}

	
	//The items to grant - these will always be granted 
	UPROPERTY(EditAnywhere, Category = "Loot Table Row")
	TArray<FItemWithQuantity> ItemsToGrant;

	//Item collections to grant - these will always be granted
	UPROPERTY(EditAnywhere, Category = "Loot Table Row")
	TArray<TObjectPtr<UItemCollection>> ItemCollectionsToGrant;

	//The items to grant if this row is given to the inventory 
	UPROPERTY(EditAnywhere, Category = "Loot Table Row", meta = (RequiredAssetDataTags = "RowStructure=/Script/NarrativeArsenal.LootTableRow"))
	TObjectPtr<UDataTable> TableToRoll;

	//The number of times we should roll the table 
	UPROPERTY(EditAnywhere, Category = "Loot Table Row", meta = (ClampMin = 0, ClampMax = 100))
	int32 NumRolls;

	//The chance of each roll succeeding 
	UPROPERTY(EditAnywhere, Category = "Loot Table Row", meta = (ClampMin=0.0, ClampMax=1.0))
	float Chance;

	bool CanRoll() const
	{
		return NumRolls > 0 && Chance > 0.f;
	}

};

//Defines a row in a loot table
USTRUCT(BlueprintType)
struct FLootTableRow : public FTableRowBase
{
	GENERATED_BODY()
public:

	FLootTableRow()
	{
		Chance = 1.f;
	}

	//The items to grant if this row is given to the inventory
	UPROPERTY(EditAnywhere, Category = "Loot Table Row")
	TArray<FItemWithQuantity> ItemsToGrant;

	//Item collections to grant
	UPROPERTY(EditAnywhere, Category = "Loot Table Row")
	TArray<TObjectPtr<UItemCollection>> ItemCollectionsToGrant;

	//Subloot tables that this loot table will roll 
	UPROPERTY(EditAnywhere, Category = "Loot Table Row")
	TArray<struct FLootTableRoll> SubTablesToRoll;

	//The chance this row will actually be given if selected 
	UPROPERTY(EditAnywhere, Category = "Loot Table Row", meta = (ClampMin=0.0, ClampMax=1.0))
	float Chance;
};

//Used by the save interface to save our data 
USTRUCT()
struct FNarrativeSavedItem
{
GENERATED_BODY()

public:

	FNarrativeSavedItem()
	{
		Quantity = 0;
		bActive = false;
	};

	FNarrativeSavedItem(class UNarrativeItem* Item) ;

	UPROPERTY(SaveGame)
	TSubclassOf<class UNarrativeItem> ItemClass;

	//Any savegame variables on the item get serialized and stored in here 
	UPROPERTY(SaveGame)
	TArray<uint8> ByteData;

	//TODO quantity and bActive are fields on UNarrativeItem, we can probably just mark those as SaveGame rather than storing in here 
	UPROPERTY(SaveGame)
	int32 Quantity;

	UPROPERTY(SaveGame)
	bool bActive;

};

/**An inventory component manages a collection of Narrative Items. For players, this should be added to the player state.
For lootable items like chests, simply add the inventory component to the chest actor. */
UCLASS( ClassGroup=(Narrative), DisplayName = "Narrative Inventory", meta=(BlueprintSpawnableComponent) )
class NARRATIVEARSENAL_API UNarrativeInventoryComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()

	friend class UNarrativeItem;
	friend struct FNarrativeItemEntry; 

public:
	// Sets default values for this component's properties
	UNarrativeInventoryComponent(const FObjectInitializer& ObjectInitializer);

	//Save system
	virtual void PrepareForSave_Implementation();
	virtual void Load_Implementation();

	bool IsLoading() const;

protected:

	bool bIsLoading;

	UPROPERTY(SaveGame)
	TArray<FNarrativeSavedItem> SavedItems;

	UPROPERTY(SaveGame)
	int32 SavedCurrency;

public:

	/**Use the given item. Return true if the item was successfully used. Please note Using is currently predicted by clients. */
	UFUNCTION(BlueprintCallable, Category = "Inventory", meta = (DeprecatedFunction, DeprecationMessage="UseItem is deprecated - Call TryUse on the item directly instead."))
	virtual bool UseItem(class UNarrativeItem* Item, class UNarrativeItem* OtherItem=nullptr);

	/**Add an item to our inventory using a loot table instead of an item class. Current loads the granted items syncrounously and may cause hitching depending on how heavy the item is. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void TryAddFromLootTable(FLootTableRoll LootTable, TArray<FItemAddResult>& OutItemAddResults);

	/**Add an item to the inventory using the item class
	@param ItemClass the item to add
	@param Quantity the amount of the item to add
	@param bCheckAutoUse if true, we'll check if the item wants to be auto-used on add. Clothing items do this for example to auto-equip. 
	@return the amount of the item that was added to the inventory */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	virtual FItemAddResult TryAddItemFromClass(TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity = 1, const bool bCheckAutoUse=true);

	/** Take some quantity away from the item, and remove it from the inventory when quantity reaches zero.
	Useful for things like eating food, using ammo, etc.*/
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual int32 ConsumeItem(class UNarrativeItem* Item, const int32 Quantity = 1);

	/** See how much of an item we have - will iterate all stacks of the item. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual int32 GetTotalQuantityOfItem(TSubclassOf<UNarrativeItem> ItemClass, const bool bCheckVisibility = false) const;

	/** See how much of an item we have, class must match exactly.  */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual int32 GetTotalQuantityOfItemExact(TSoftClassPtr<UNarrativeItem> ItemClass, const bool bCheckVisibility = false) const;

	/** Take some quantity away from any stacks of this item we have, and remove if reaches zero.*/
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual int32 ConsumeItemsOfClass(TSubclassOf<UNarrativeItem> ItemClass, const int32 Quantity = 1);

	/** Remove the item from the inventory. We do actually allow clients to call this unlike AddItem, however the server
	will still validate that the item is allowed to be removed before removing the item, so still cheat safe */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual bool RemoveItem(class UNarrativeItem* Item);

	/**Return true if we have a given amount of an item. bCheckVisibility will only look for items that are visible in the inventory. 
	Works for child classes, for example you can pass (UWeaponItem, 10) to see if the player has 10 weapons. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual bool HasItem(TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity = 1, const bool bCheckVisibility = false) const;

	/** Version of HasItem that only checks for items matching the type of ItemClass, and won't allow child classes. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual bool HasItemExact(TSoftClassPtr<class UNarrativeItem> ItemClass, const int32 Quantity = 1, const bool bCheckVisibility = false) const;

	/**Return true if the taker can loot the item from this inventory */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual bool AllowLootItem(class UNarrativeInventoryComponent* Taker, TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity, FText& ErrorText) const;

	/**Return true if the storer can store the given item in this inventory */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual bool AllowStoreItem(class UNarrativeInventoryComponent* Storer, TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity, FText& ErrorText) const;

	/** Remove the item from us, and give it to the taker. Return how much of the item was able to be looted */
	virtual FItemAddResult PerformLootItem(class UNarrativeInventoryComponent* Taker, TSubclassOf <class UNarrativeItem> ItemClass, const int32 Quantity = 1);

	/** Remove the item from the storer, and give it to us. Return how much of the item was able to be stored*/
	virtual FItemAddResult PerformStoreItem(class UNarrativeInventoryComponent* Storer, TSubclassOf <class UNarrativeItem> ItemClass, const int32 Quantity = 1);

	/** Look an item up via its GUID, mostly used by code that needs to save an item ref to disk and restore it on load via GUID.  */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UNarrativeItem* FindItemByGUID(const FGuid& ItemGUID) const;

	/** Native bounded cinematic postconditions. Callers must retain journals and retire them after receipt commit. */
	bool PrepareCinematicItemChange(const FNarrativeCinematicItemMutation& Mutation, FNarrativeCinematicItemChange& Change, FString& OutError);
	bool ApplyCinematicItemChange(FNarrativeCinematicItemChange& Change, FString& OutError);
	bool ValidateCinematicItemChange(const FNarrativeCinematicItemChange& Change, bool bApplied, FString& OutError) const;
	bool RollbackCinematicItemChange(FNarrativeCinematicItemChange& Change);
	bool SetCinematicEquipment(class UEquippableItem* Item, FGameplayTag Slot);
	uint64 GetCinematicLoadRevision() const { return CinematicLoadRevision; }

	/**Return the first item with the same class as ItemClass, including child classes. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UNarrativeItem* FindItemOfClass(TSubclassOf<class UNarrativeItem> ItemClass, const bool bCheckVisibility = false) const;

	/**Return the first item with the same class as ItemClass.*/
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UNarrativeItem* FindItemByClassExact(TSoftClassPtr<class UNarrativeItem> ItemClass, const bool bCheckVisibility = false) const;
	
	/**Get all inventory items that matching the item class. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<UNarrativeItem*> FindItemsByClass(TSoftClassPtr<class UNarrativeItem> ItemClass, const bool bCheckVisibility = false) const;

	/**Get all inventory items that are children of the supplied ItemClass. Requires class to be loaded. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<UNarrativeItem*> FindItemsOfClass(TSubclassOf<class UNarrativeItem> ItemClass, const bool bCheckVisibility = false) const;

	//Return the items that match the given class
	UFUNCTION(BlueprintCallable, Category="Inventory",  meta=(DeterminesOutputType="ItemClass", DynamicOutputParam="OutItems"))
	bool GetItemsOfClass(TSubclassOf<UNarrativeItem> ItemClass, TArray<UNarrativeItem*>& OutItems, const bool bCheckVisibility = false) const;

	//Return all items that the supplied item can be used on 
	UFUNCTION(BlueprintCallable, Category="Inventory",  meta=(DeterminesOutputType="ItemClass", DynamicOutputParam="OutItems"))
	bool GetItemsUsableWith(UNarrativeItem* Item, TArray<UNarrativeItem*>& OutItems, const bool bCheckVisibility = false) const;

	/**Return the amount of a given item that we have space for based on its weight, max stack size, etc.
	@param NoSpaceReason If we have no space for the item, this is the reason why */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	virtual int32 GetSpaceForItem(TSubclassOf<class UNarrativeItem> ItemClass, FText& NoSpaceReason) const;

	/** Return the amount of currency we have */
	UFUNCTION(BlueprintPure, Category = "Currency")
	int32 GetCurrency() const;

	/** Add some currency to the players inventory. Can accept negative values. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Currency")
	virtual void AddCurrency(const int32 Amount);

	/** set the amount of currency the player has . */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Currency")
	virtual void SetCurrency(const int32 Amount);

	//Get the current weight of the inventory. To get the amount of items in the inventory, just do GetItems().Num()
	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetCurrentWeight() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsVendor() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetWeightCapacity(const float NewWeightCapacity);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetCapacity(const int32 NewCapacity);
	
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetInventoryFriendlyName(const FText& Name);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetIsVendor(const bool bNewIsVendor);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetBuyPercentage(const float NewBuyPercentage) { BuyItemPct = NewBuyPercentage; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetSellPercentage(const float NewSellPercentage) { SellItemPct = NewSellPercentage; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FORCEINLINE float GetWeightCapacity() const { return WeightCapacity; };

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FORCEINLINE int32 GetCapacity() const { return Capacity; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	FORCEINLINE TArray<class UNarrativeItem*> GetItems() const { return Items; }

	UFUNCTION(BlueprintPure, Category = "Narrative")
	virtual APawn* GetOwningPawn() const;

	UFUNCTION(BlueprintPure, Category = "Narrative")
	virtual APlayerController* GetOwningController() const;

	UFUNCTION(Client, Reliable)
	void ClientRefreshInventory();

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryUpdated OnInventoryUpdated;

	//Delegate for an item being added. Currently only called server side.
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemAdded OnItemAdded;

	//Delegate for an item being removed. Called on server and client 
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemRemoved OnItemRemoved;
		
	//Delegate for an item being Used. Currently only called server side.
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnItemUsed OnItemUsed;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnBeginLooting OnBeginLooting;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnEndLooting OnEndLooting;

	//Notify when our currency balance changes 
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnCurrencyChanged OnCurrencyChanged;

protected:

	//The name of this inventory - used by the UI that ships with Narrative Inventory. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	FText InventoryFriendlyName;

	//The maximum weight the inventory can hold. For players, backpacks and other items increase this limit
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (Units = "Kilograms"))
	float WeightCapacity;

	//The maximum number of items the inventory can hold. For players, backpacks and other items increase this limit
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = 0, ClampMax = 1000))
	int32 Capacity;

	/**The inventory we're currently looting from*/
	UPROPERTY(ReplicatedUsing = OnRep_LootSource, VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory")
	class UNarrativeInventoryComponent* LootSource;

	/**Item that want to tick are stored in here so we can tick them. */
	UPROPERTY()
	TSet<TObjectPtr<UNarrativeItem>> TickItems;

	/**The items currently in our inventory. Not replicated as this is legacy however we want UI code and backwards compatibility keeping this intact. */
	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	TArray<class UNarrativeItem*> Items;
	
	UPROPERTY(Replicated)
	FNarrativeItemArray ReplicatedItems;
	
	/**The items currently in our inventory, mapped using their GUID. This is nice in cases where you need to SaveGame some item ptr,
	 * which cannot be done - however by saving the GUID you can simply re-look the item up later - weapon attachments do this to remember their owner. 
	 */
	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	TMap<FGuid, class UNarrativeItem*> ItemGUIDMap;

	/**The amount of currency our player has*/
	UPROPERTY(ReplicatedUsing = OnRep_Currency, EditDefaultsOnly, Category = "Inventory")
	int32 Currency;

	/**Items that should be put in our inventory on BeginPlay. We savegame this as to only add the items once.  */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TArray<FItemWithQuantity> DefaultItems;

	/** Loot table we'll use to generate this inventories items. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<FLootTableRoll> DefaultItemTables;

	/** Nice way to ensure we don't give default items multiple times.  */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Inventory")
	bool bGaveDefaultItems;

	/** If true, this inventory will be treated as a vendor, meaning taking/giving items will check you/the vendor has enough currency, and will 
	give you/the vendor the currency when the taking/giving occurs also. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	bool bIsVendor;

	//The percentage of the items value we'll buy items for if we're a vendor. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vendor Settings")
	float BuyItemPct;

	//The percentage of the items value we'll sell items for 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vendor Settings")
	float SellItemPct;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void OnRegister() override; 

	//Internal, non-BP exposed add item function. Don't call this directly, use TryAddItem(), or TryAddItemFromClass() instead.
	virtual FItemAddResult TryAddItem_Internal(TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity = 1);

public:

	/** Instead of calling this automatically on beginplay we let designers call this when they like.  */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void GiveDefaultItems();

	/**Sets our loot source. Only the server can call this for obvious reasons! */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	virtual void SetLootSource(class UNarrativeInventoryComponent* NewLootSource);

protected:

	//Register the item as wanting a tick - we'll tick it until it wants removed. 
	virtual void AddTickItem(UNarrativeItem* Item);
	virtual void RemoveTickItem(UNarrativeItem* Item);

	UFUNCTION(Server, Reliable)
	virtual void ServerRemoveItem(class UNarrativeItem* Item);

	UFUNCTION(Server, Reliable)
	virtual void ServerConsumeItem(class UNarrativeItem* Item, const int32 Quantity);

	UFUNCTION(Server, Reliable)
	virtual void ServerUseItem(class UNarrativeItem* Item, class UNarrativeItem* OtherItem = nullptr);

	UFUNCTION(Server, Reliable)
	virtual void ServerStopLooting();

	/**Stop looting. Clients will RPC to the server to end looting. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void StopLooting();

	/** Loot an item. Clients RPC to tell server they want to do this; server will validate to prevent cheating
		If we're client return true/false if we notified server to loot. Server returns true if loot actually happened. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual bool RequestLootItem(class UNarrativeItem* ItemToLoot, FText& ErrorText, const int32 Quantity = 1);

	UFUNCTION(Server, Reliable)
	virtual void ServerRequestLootItem(class UNarrativeItem* ItemToLoot, const int32 Quantity);

	/** Store an item. Clients RPC to tell server they want to do this; server will validate to prevent cheating
	If we're client return true/false if we notified server to store. Server returns true if store actually happened. 
	*/
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual bool RequestStoreItem(class UNarrativeItem* ItemToStore, FText& ErrorText, const int32 Quantity = 1);

	UFUNCTION(Server, Reliable)
	virtual void ServerRequestStoreItem(class UNarrativeItem* ItemToLoot, const int32 Quantity);

	//We cache the OwningController, we won't cache pawn as this might change
	UPROPERTY()
	class APlayerController* OwnerPC;

	/**
	* Return the price we'll buy the given item for
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
	int32 GetBuyPrice(TSubclassOf<class UNarrativeItem> Item, int32 Quantity = 1) const;
	virtual int32 GetBuyPrice_Implementation(TSubclassOf<class UNarrativeItem> Item, int32 Quantity = 1) const;

	/**
	* Return the price we'll sell the given item for
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
	int32 GetSellPrice(TSubclassOf<class UNarrativeItem> Item, int32 Quantity = 1) const;
	virtual int32 GetSellPrice_Implementation(TSubclassOf<class UNarrativeItem> Item, int32 Quantity = 1) const;

private:
	uint64 CinematicLoadRevision = 0;
	bool OwnsCinematicItem(const UNarrativeItem* Item) const;
	bool InsertCinematicItem(UNarrativeItem* Item, bool bRestoreExisting = false);
	bool RemoveOwnedItemInternal(UNarrativeItem* Item);
	
	/**Don't call Items.Add() directly, use this function instead, as it handles replication and ownership*/
	UNarrativeItem* AddItem(TSubclassOf<class UNarrativeItem> ItemClass, const int32 Quantity);
	
	UFUNCTION()
	void OnRep_Currency(const int32 OldCurrency);

	UFUNCTION()
	void OnRep_LootSource(class UNarrativeInventoryComponent* OldLootSource);

	UPROPERTY()
	int32 ReplicatedItemsKey;

};
