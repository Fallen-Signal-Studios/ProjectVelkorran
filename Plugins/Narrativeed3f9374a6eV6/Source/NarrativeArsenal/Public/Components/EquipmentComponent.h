// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/EquippableItem.h"
#include <GameplayTagContainer.h>
#include "EquipmentComponent.generated.h"

/**Called on server when an item is added to this inventory*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemEquipped, const FGameplayTag, Slot, class UEquippableItem*, Equippable);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemUnequipped, const FGameplayTag, Slot, class UEquippableItem*, Equippable);

/**

Add this to your pawn class, call Initialize on beginplay, and your player will be able to equip items - its that easy! 

Tracks what items are equipped, remembers what default clothing items the player should wear if an item isn't equipped, 
and generally just manages the players equipped items.
*/
UCLASS( ClassGroup=(Narrative), DisplayName = "Narrative Equipment", meta=(BlueprintSpawnableComponent) )
class NARRATIVEARSENAL_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UEquippableItem;
	friend class UWeaponItem;
	friend class UEquippableItem_Clothing;
	friend class ANarrativeCharacter;
	friend class FGameplayDebuggerCategory_NarrativeCharacter;

public:	
	// Sets default values for this component's properties
	UEquipmentComponent();

	//TODO remove all this as character visual now handles this 

	/**Initialize the equipment component, by telling it which meshes link to which slot.
	
	@param ClothingMeshes The map which maps each clothing slot to the skeletal mesh component the clothing will equip to 
	@param LeaderPoseComponent the component all of the equipped items will be told to follow upon equipping. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	virtual void Initialize(TMap<FGameplayTag, USkeletalMeshComponent*> ClothingMeshes, class USkeletalMeshComponent* LeaderPoseComponent);

	//Return the item equipped at the given slot
	UFUNCTION(BlueprintPure, Category = "Equipment")
	class UEquippableItem* GetEquippedItemAtSlot(UPARAM(meta = (Categories = "Narrative.Equipment.Slot"))const FGameplayTag Slot); 

	//Return the items equipped that match the slot subtag
	UFUNCTION(BlueprintPure, Category = "Equipment")
	TArray<class UEquippableItem*> GetItemsWithSlot(UPARAM(meta = (Categories = "Narrative.Equipment.Slot"))const FGameplayTag Slot);

	//Return all wielded weapons. 
	UFUNCTION(BlueprintPure, Category = "Equipment")
	TArray<class UWeaponItem*> GetWieldedWeapons();

	//Return wether we're dual wielding or not 
	UFUNCTION(BlueprintPure, Category = "Equipment")
	virtual bool IsDualWielding() const;

	//Return the items that match the given equippable class
	UFUNCTION(BlueprintCallable, Category="Equipment",  meta=(DeterminesOutputType="EquippableClass", DynamicOutputParam="OutEquippables"))
	void GetEquippedItemsOfClass( TSubclassOf<UEquippableItem> EquippableClass, TArray<UEquippableItem*>& OutEquippables);

	//Return the weapon equipped at the given slot
	UFUNCTION(BlueprintPure, Category = "Equipment")
	class UWeaponItem* GetEquippedWeaponAtSlot(UPARAM(meta = (Categories = "Narrative.Equipment.Slot.Weapon"))const FGameplayTag Slot); 

	//Return the weapon wielded at the given slot
	UFUNCTION(BlueprintPure, Category = "Equipment")
	class UWeaponItem* GetWieldedWeaponAtSlot(UPARAM(meta = (Categories = "Narrative.Equipment.WieldSlot"))const FGameplayTag Slot); 

	//Return the groom for the given slot
	UFUNCTION(BlueprintPure, Category = "Equipment")
	class UGroomComponent* GetGroomComponentAtSlot( UPARAM(meta = (Categories = "Narrative.Equipment.Slot.Groom")) const FGameplayTag Slot);

	//Return how much all of our equipped items weigh
	UFUNCTION(BlueprintPure, Category = "Equipment")
	virtual float GetEquippedItemsWeight() const;

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnItemEquipped OnItemEquipped;

	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnItemUnequipped OnItemUnequipped;

	//Given some slots, return the first free one. Returns empty tag is none found. 
	FGameplayTag GetFirstFreeSlot(const FGameplayTagContainer& SlotsToCheck);

protected:

	//Mark the weapon as wielded/unwielded. 
	virtual void WieldWeapon(class UWeaponItem* Weapon, const FGameplayTag& WieldSlot);
	virtual void UnwieldWeapon(const FGameplayTag& WieldSlot);

	//Mark the item as equipped/unequipped. Slot is supplied as a parameter as weapons can be equipped to various slots and need to specify. 
	virtual void EquipItem(class UEquippableItem* Equippable, const FGameplayTag& Slot);
	virtual void UnequipItem(const FGameplayTag& Slot);

	/**When we put a new item on, we need to tell it to follow the leader pose component, so we store that here. */
	UPROPERTY()
	class USkeletalMeshComponent* LeaderPoseComponent;

	/**The grooms we'll be changing if a player equips an item*/
	UPROPERTY()
	TMap<FGameplayTag, class UGroomComponent*> GroomComponents;

	/**All of the items that are currently equipped are stored in here*/
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TMap<FGameplayTag, UEquippableItem*> EquippedItems;

	/**All of the weapons that are currently wielded are stored in here*/
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TMap<FGameplayTag, UWeaponItem*> WieldedWeapons;

	/**All of the weapon holster slots available. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Equipment", meta = (ForceInlineRow, Categories = "Narrative.Equipment.Slot.Weapon"))
	FGameplayTagContainer HolsterSlots;

	/**All of the weapon wield slots available. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Equipment", meta = (ForceInlineRow, Categories = "Narrative.Equipment.WieldSlot"))
	FGameplayTagContainer WieldSlots;

public:

	FORCEINLINE class USkeletalMeshComponent* GetLeaderPoseComponent() const {return LeaderPoseComponent;};

};
