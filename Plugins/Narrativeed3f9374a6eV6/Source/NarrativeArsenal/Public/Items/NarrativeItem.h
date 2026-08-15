// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InventoryComponent.h"
#include <GameplayTagContainer.h>
#include "Engine/StaticMesh.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UObject/SoftObjectPtr.h"
#include "NarrativeItem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemModified);

//A stat that can be shown in the item preview window
USTRUCT(BlueprintType)
struct FNarrativeItemStat
{
	GENERATED_BODY()

	FNarrativeItemStat(const FText& InStatDisplayName, const FString& InStringVariable, const FText& InStatTooltip) : StatDisplayName(InStatDisplayName), StringVariable(InStringVariable), StatTooltip(InStatTooltip)
	{

	}

	FNarrativeItemStat()
	{
		StatDisplayName = NSLOCTEXT("NarrativeItemStat", "StatDisplayName", "Stat Display Name");
		StringVariable = FString();
	};

	//The stats display name.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Stat")
	FText StatDisplayName;

	//The backing string variable for the stat - implemented using GetStringVariable function that can be overriden in Blueprints! 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Stat")
	FString StringVariable;

	//Tooltip the stat will show when hovered 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item Stat")
	FText StatTooltip;
};

//A saved fragment - which consists of the index and the data that needed serialized
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FSavedItemFragment
{

	GENERATED_USTRUCT_BODY()

	FSavedItemFragment(){};

	UPROPERTY(SaveGame)
	TSubclassOf<UNarrativeItemFragment> Fragment;

	UPROPERTY(SaveGame)
	TArray<uint8> Data;
};

/**
 * Allows data to be added to items for users wishing for a more composition based approach. 
 * 
 * A great example of where you might need composition is Ammo. You might want wieldable ammo like arrows,
 * but also non-wieldable ammo like firearm ammo that just sits in your inventory. Maybe you have a crazy weapon 
 * item that can shoot health potions at people to heal them. However Potion Items, Wieldable Items, and so on 
 * don't share the same base class, so they cant all have ammo data on them. With composition, you CAN have this,
 * by adding Ammo fragments to all items that require the ability to be used as ammo, even if the items are all unrelated. 

 * 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class NARRATIVEARSENAL_API UNarrativeItemFragment : public UObject
{
	GENERATED_BODY()

public: 

	UNarrativeItemFragment(const FObjectInitializer& ObjectInitializer);

	/** Grab all the ways an item can be used if this fragment is present.  */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Item")
	TArray<class UNarrativeItemUseAction*> GetItemUseActions() const;
	virtual TArray<class UNarrativeItemUseAction*> GetItemUseActions_Implementation() const;
};

/**
 * Defines a way an item can be used. An item usually has at least one way it can be used. 
 * Called only on clients - if use needs to send logic to server use an RPC. 
 */
UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced)
class NARRATIVEARSENAL_API UNarrativeItemUseAction : public UObject
{
	GENERATED_BODY()

public: 

	UNarrativeItemUseAction(const FObjectInitializer& ObjectInitializer);

	//Return whether the use action is active and usable right now. Returning false should hide this in the UI. 
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Usage")
	bool IsEnabled();
	virtual bool IsEnabled_Implementation();

	//Execute the item use logic. 
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Usage")
	bool OnUse(class UNarrativeItem* Item, class UNarrativeItem* OtherItem);
	virtual bool OnUse_Implementation(class UNarrativeItem* Item, class UNarrativeItem* OtherItem);

	//Does this use action require another item? IE applying poison to a weapon, putting attachment on a weapon, etc.  
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Usage")
	bool IsMultiItemUse(class UNarrativeItem* Item);
	virtual bool IsMultiItemUse_Implementation(class UNarrativeItem* Item);

	//If MultiItemUse, eturn all items that the item can be used on
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Inventory",  meta=(DeterminesOutputType="ItemClass", DynamicOutputParam="OutItems"))
	bool GetItemsUsableWith(UNarrativeItem* Item, TArray<UNarrativeItem*>& OutItems) const;
	virtual bool GetItemsUsableWith_Implementation(UNarrativeItem* Item, TArray<UNarrativeItem*>& OutItems) const;

	//Return an FText explaining what the use action does. 
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Usage")
	FText GetActionDisplayName(class UNarrativeItem* Item);
	virtual FText GetActionDisplayName_Implementation(class UNarrativeItem* Item);

};

//Used when the item is dropped and we need to create a pickup from it 
USTRUCT(BlueprintType)
struct FPickupMeshData
{
	GENERATED_BODY()

	FPickupMeshData(){};
	
	/**The mesh to display for this items pickup*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General")
	TSoftObjectPtr<class UStaticMesh> PickupMesh;

	/**The materials to display for this items pickup*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General")
	TArray<TSoftObjectPtr<class UMaterialInterface>> PickupMeshMaterials;

	/**An extra transform to offset the pickup once dropped. This is because many items won't look nice if we drop the item at its default origin, it will probably need shifted to a nicer looking spot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General")
	FTransform PickupOffset; 

};

/**
 * Item is the base class for an item that can be added to the inventory
 */
UCLASS(Blueprintable)
class NARRATIVEARSENAL_API UNarrativeItem : public UObject
{
	GENERATED_BODY()

protected:

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsSupportedForNetworking() const override;
	virtual class UWorld* GetWorld() const override;
	virtual void PostLoad() override;

	//Required for UNarrativeItems to be able to have RPCs - routes the RPC through our owning Inventory Component. 
	virtual int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	virtual bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	bool CanEditChange(const FProperty* InProperty) const;
	
#endif
	
protected:
	
	//Inspects owning inventory to check if this item instance is on server or client 
	bool HasAuthority() const;

public:

	UNarrativeItem();

	virtual void Serialize(FArchive& Ar) override;

	UPROPERTY(Transient)
	class UWorld* World;

	/**The mesh to display for this items pickup*/
	UPROPERTY(BlueprintReadWrite,Category = "Item | General", meta = (DeprecatedProperty, DeprecationMessage="PickupMesh is deprecated. Use PickupMeshData instead. "))
	TSoftObjectPtr<UStaticMesh> PickupMesh;

	/**The thumbnail for this item*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General")
	TSoftObjectPtr<class UTexture2D> Thumbnail;

	/**The use sound for the item*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General")
	TObjectPtr<class USoundBase> UseSound;

	/**The display name for this item in the inventory*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General")
	FText DisplayName;

	/**An optional description for the item*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General", meta = (MultiLine = true))
	FText Description;

	/**The weight of the item*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General", meta = (ClampMin = 0.0, Units = "Kilograms"))
	float Weight;

	/** This is the value of the item, used by vendors. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General")
	int32 BaseValue;

	/** A value denoting how "good" the item is. Completely optional, but can be useful when AI need to select the best weapon, or an auto-eat needs to find the best food, etc. 
	If left as zero, item value will be used as fallback to find the highest scored item if required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item | General")
	float BaseScore;

	/**Any gameplay tags we wish to give this item*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General")
	FGameplayTagContainer ItemTags;

	/** The inventory item button we should spawn for this item if it requires one. If none we'll just use the default item button. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | UI")
	TSubclassOf<class UNarrativeInventoryItemButton> ItemWidgetOverride;
	
	/**If true, item will have a use action automatically added that calls this items Use function when selected. 
	If your item needs many use options, override GetItemUseActions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | Usage")
	bool bAddDefaultUseOption;

	/**If true, 1 of this item will be consumed every time the item is used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | Usage")
	bool bConsumeOnUse;
	
	/**If true, using this item will open a prompt asking which other item you'd like to use it on. All items passing CanUseItemWith() will be shown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | Usage")
	bool bUsedWithOtherItem;

	/**The text for using the item. (Equip, Eat, etc)*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | Usage")
	FText UseActionText;

	/**The amount of time you have to wait between uses of the item, 0 means no recharge delay at all*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | Usage", meta = (ClampMin=0, Units = "Seconds"))
	float UseRechargeDuration;

	/**Some items might need to be activatable. Check this box if you want the item to be able to activate. 
	Activating will mean different things for different item types, and you can define this using the Activate and Deactivate() functions. 
	
	Activated items will also show a check on the UI*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | Activation")
	bool bCanActivate;

	/**If checked, using this item will cause it to activate/deactivate*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | Activation")
	bool bToggleActiveOnUse;

	/**Return true if this item is active */
	UPROPERTY(ReplicatedUsing = OnRep_bActive, BlueprintReadOnly, Category= "Item | Activation", meta = (EditCondition = bCanActivate, EditConditionHides))
	bool bActive;

	/**Whether or not this item is busy */
	UPROPERTY(BlueprintReadWrite, Category = "Item | General")
	bool bIsBusy;

	/**Whether or not this item has been favourited*/
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Item | General")
	bool bFavourite;

	/**Whether or not this item can be stacked*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General")
	bool bStackable;

	/**The maximum size that a stack of items can be*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General", meta = (ClampMin = 2, EditCondition = bStackable, EditConditionHides))
	int32 MaxStackSize;

	/**Whether the item requires TickItem() to be called. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General", meta = (AdvancedDisplay))
	bool bWantsTickByDefault;

	/** AI that acquire this item will have these activities added to themselves whilst they have the item.  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | AI")
	TArray<TSubclassOf<class UNPCActivity>> ActivitiesToGrant;

	/**The stats to show in the Item Preview UI - make sure to implement GetStringVariable function and return a value matching each stat you add.
	
	Items have 6 stats supported out of the box, Display Name, Weight, Stack Weight, Quantity, Recharge Duration, and Max Stack Size - if you want to add more simply
	override the GetStringVariable function and return a valid value for your desired variable
	*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | UI")
	TArray<FNarrativeItemStat> Stats;

	/**The inventory that owns this item*/
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	class UNarrativeInventoryComponent* OwningInventory;

	/**Used to efficiently replicate inventory items*/
	UPROPERTY()
	int32 RepKey;
	
	/**The items GUID. Primarily used so we can save item references to disk by caching their GUID and looking them up after our items load back.
	Only currently set on authority, clients dont use this. */
	UPROPERTY(SaveGame)
	FGuid ItemGUID;

	UPROPERTY(BlueprintAssignable)
	FOnItemModified OnItemModified;

	UFUNCTION()
	void OnRep_bActive(const bool bOldActive);

	UFUNCTION(BlueprintCallable, Category = "Item | Activation")
	void SetActive(const bool bNewActive, const bool bForce=false);

	/**Sets whether the item is considered "Busy". By default, this will show a loading icon and will prevent the item from being used. 
	You'd generally use this when an item needs to load some assets, like a clothing mesh, weapon item, etc and you need the player to visually see that in the UI. */
	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetBusy(const bool bNewBusy);

	UFUNCTION()
	void OnRep_Quantity(const int32 OldQuantity);

	void SetQuantity(const int32 NewQuantity);

	UFUNCTION(BlueprintPure, Category = "Item")
	FORCEINLINE bool IsActive() const {return bActive;};

	UFUNCTION(BlueprintPure, Category = "Item")
	FORCEINLINE int32 GetQuantity() const { return Quantity; };

	//If this item has bUsedWithOther item, this function allows us to define which items we'll allow this item to be used on. 
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Item | Usage")
	bool CanUseItemWith(class UNarrativeItem* TestItem) const;
	virtual bool CanUseItemWith_Implementation(class UNarrativeItem* TestItem) const;

	//Return whether this item should show a tick in the inventory screen, indicating is it currently active. 
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Item | Usage")
	bool ShowActiveInUI() const;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetLastUseTime(const float NewLastUseTime);

	UFUNCTION(BlueprintPure, Category = "Item")
	FORCEINLINE float GetLastUseTime() const { return LastUseTime; }

	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Item")
	float GetItemScore() const;

	UFUNCTION(BlueprintCallable, Category = "Item")
	FORCEINLINE float GetStackWeight() const { return Quantity * Weight; };

	UFUNCTION(BlueprintPure, Category = "Item")
	FORCEINLINE bool IsStackFull() const { return Quantity >= MaxStackSize; }

	UFUNCTION(BlueprintPure, Category = "Item")
	FORCEINLINE int32 GetMaxStackSize() const{return bStackable ? MaxStackSize : 1;}

	//Return the amount of space the item stack has left 
	UFUNCTION(BlueprintPure, Category = "Item")
	FORCEINLINE int32 GetStackSpace() const{return GetMaxStackSize() - GetQuantity();}

	UFUNCTION(BlueprintPure, Category = "Item")
	AController* GetOwningController() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	APawn* GetOwningPawn() const;

	UFUNCTION(BlueprintPure, Category = "Item")
	ANarrativeCharacter* GetOwningNarrativeCharacter() const;

	/** Grab all the ways this item can be used.  */
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Item")
	TArray<class UNarrativeItemUseAction*> GetItemUseActions() const;
	virtual TArray<class UNarrativeItemUseAction*> GetItemUseActions_Implementation() const;

	/** Try and use the item. Called on both server and client. */
	UFUNCTION(Server, Reliable)
	virtual void ServerTryUse(UNarrativeItem* OtherItem = nullptr);

	/** Try and use the item. Called on both server and client. Returns true on clients if we were able to use item, doesn't guarantee server allows use however.   */
	UFUNCTION(BlueprintCallable, Category = "Item")
	virtual bool TryUse(UNarrativeItem* OtherItem = nullptr);

	/** Called when used to define what item does when used.
	@param OtherItem the item we've dragged this item on to, for example for attachments this is the weapon we're putting the attachment on. Most items wont need this.  */
	virtual void Use(UNarrativeItem* OtherItem=nullptr);

	//BP handler for item being used. 
	UFUNCTION(BlueprintImplementableEvent)
	void OnUse(UNarrativeItem* OtherItem);

	//This function is not called by default - the item must have tick enabled
	virtual void TickItem(const float DeltaTime);

	virtual void AddedToInventory(class UNarrativeInventoryComponent* Inventory, const bool bFromLoad);
	virtual void RemovedFromInventory(class UNarrativeInventoryComponent* Inventory);

	/**Mark the object as needing replication. We must call this internally after modifying any replicated properties*/
	UFUNCTION(BlueprintCallable, Category = "Item")
	void MarkDirtyForReplication();

	/**Enable or disable ticking for the item*/
	virtual void EnableItemTick(const bool bEnable);

	/**Return the description - allows BPs to override this in case you want automatically dynamically generated descriptions based on the items properties. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
	FText GetRawDescription();
	virtual FText GetRawDescription_Implementation();

	/**
	* Return the description, with any string variables parsed out
	*/
	UFUNCTION(BlueprintPure, Category = "Item")
	virtual FText GetParsedDescription();

	/**
	* Allows you to use variables in your items description, ie Hello {Item Name}.
	*
	* Simply override this function, then check the value of variable name and return whatever value you like!
	*
	* ie if(VariableName == "PlayerName") {return OwningPawn->GetUsername();}; - then Hello {PlayerName}! will automatically become Hello xXNoobPwner420Xx! etc 
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
	FString GetStringVariable(const FString& VariableName);
	virtual FString GetStringVariable_Implementation(const FString& VariableName);

	/**
	* If this item is activatable, this will be called when the item is activated
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Item")
	void Activated();
	virtual void Activated_Implementation();

	/**
	* If this item is activatable, this will be called when the item is deactivated
	*/
	UFUNCTION(BlueprintNativeEvent, Category = "Item")
	void Deactivated();
	virtual void Deactivated_Implementation();

	/**
	* Allows you to define whether or not the item can be dropped, nice for if you want to implement special quest items that cannot be dropped for example 
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
	bool CanBeRemoved() const;
	virtual bool CanBeRemoved_Implementation() const;

	/**
	* Allows you to define whether the item can be used or not.
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
	bool CanUse() const;
	virtual bool CanUse_Implementation() const;

	/**
	* Define whether the item should automatically use itself when added to our inventory.
	*
	* This currently only works in standalone TODO we can probably re-enable it for networked since new net changes 
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
	bool ShouldUseOnAdd() const;
	virtual bool ShouldUseOnAdd_Implementation() const;

	/**
	* Allows you to define whether the item should be shown in the inventory, you may want to hide items from time to time
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
	bool ShouldShowInInventory() const;
	virtual bool ShouldShowInInventory_Implementation() const;

	/**
	* Grab a fragment of the given type. Return whether we found one. 
	*/
	UFUNCTION(BlueprintPure, Category = "Fragments", meta = (DeterminesOutputType=FragmentClass))
	UNarrativeItemFragment* GetFragment(TSubclassOf<UNarrativeItemFragment> FragmentClass, bool& OutSucceeded);

	/**
	* Grab all fragments of the given class. Return whether we found one. 
	*/
	UFUNCTION(BlueprintPure, Category = "Fragments", meta = (DeterminesOutputType=FragmentClass))
	TArray<UNarrativeItemFragment*> GetFragments(TSubclassOf<UNarrativeItemFragment> FragmentClass, bool& OutSucceeded);

	/** If requiring multiple use actions, this array can be modified in order to add such functionality.  */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Item | General")
	//TArray<TObjectPtr<class UNarrativeItemUseAction>> UseActions;

protected:


	/** Though we don't use them much in the base tool, we've added support for fragments so users can use these if requiring more composition based approach.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Item | General")
	TArray<TObjectPtr<class UNarrativeItemFragment>> Fragments;

	/** Fragments that need saved to disk are saved via this struct */
	UPROPERTY(SaveGame)
	TArray<FSavedItemFragment> SavedFragments;



	friend class UNarrativeInventoryComponent;

	//Called once inventory component reloads all its items in
	virtual void PostInventoryLoaded();

	/**The amount of the item*/
	UPROPERTY(ReplicatedUsing = OnRep_Quantity, EditAnywhere, Category = "Item | General", meta = (UIMin = 1, EditCondition = bStackable, EditConditionHides))
	int32 Quantity;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Item")
	float LastUseTime;

	/**The mesh to display for this items pickup */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Item | General")
	FPickupMeshData PickupMeshData;

public:

	/**Get the mesh to display for this items pickup*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item | General")
	FPickupMeshData GetPickupMeshData(int32 QuantityToGet) const;
	virtual FPickupMeshData GetPickupMeshData_Implementation(int32 QuantityToGet) const;
};
