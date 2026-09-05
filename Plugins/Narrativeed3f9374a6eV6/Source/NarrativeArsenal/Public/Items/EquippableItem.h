// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "NarrativeItem.h"
#include "GameplayTagContainer.h"
#include <GameplayEffect.h>
#include "CharacterCreator/CharacterCreatorAttributes.h"
#include "EquippableItem.generated.h"

UCLASS()
class NARRATIVEARSENAL_API UUseAction_Equip : public UNarrativeItemUseAction
{
	GENERATED_BODY()

public: 

	UUseAction_Equip(const FObjectInitializer& ObjectInitializer);

	//Execute the item use logic. 
	virtual bool OnUse_Implementation(class UNarrativeItem* Item, class UNarrativeItem* OtherItem) override;
	virtual FText GetActionDisplayName_Implementation(class UNarrativeItem* Item) override;

	FGameplayTag EquipToSlot;

};

/**
 * The base class for an equippable item the player can put on. Networking is built right in.
 */
UCLASS()
class NARRATIVEARSENAL_API UEquippableItem : public UNarrativeItem
{
	GENERATED_BODY()
	
protected:

	friend class UEquipmentComponent;
	friend class ANarrativeCharacter;
	friend class ANarrativeCharacterVisual;
	friend class UUseAction_Equip;
	friend class UNarrativeInventoryComponent;

	UEquippableItem();

	virtual void PostLoad() override; 
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	/**Allows you to override what equipping the item does. By default this sets the players mesh to the Equippable Mesh, but perhaps
	you want a weapon actor to spawn in, or have an equippable aura effect that follows the player. by overriding HandleEquip you can 
	do whatever custom logic you want. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equippable")
	void HandleEquip();
	virtual void HandleEquip_Implementation();

	/**Allows you to override what happens when the item unequips. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equippable")
	void HandleUnequip(const FGameplayTag& OldSlot);
	virtual void HandleUnequip_Implementation(const FGameplayTag& OldSlot);

	UFUNCTION(Server, Reliable)
	virtual void ServerEquipItem(UPARAM(meta = (Categories = "Narrative.Equipment.Slot"))FGameplayTag DesiredSlot);

	//Equip this item to the given slot. Return true if succeeded. 
	UFUNCTION(BlueprintCallable, Category = "Equippable")
	virtual bool EquipItem(UPARAM(meta = (Categories = "Narrative.Equipment.Slot"))FGameplayTag DesiredSlot);

	//Unequip this item from the given slot. Return true if succeeded. 
	UFUNCTION(BlueprintCallable, Category = "Equippable")
	virtual void UnequipItem();

	virtual void Use(UNarrativeItem* OtherItem/* =nullptr */) override; 
	virtual void AddedToInventory(class UNarrativeInventoryComponent* Inventory, const bool bFromLoad) override; 
	virtual void RemovedFromInventory(class UNarrativeInventoryComponent* Inventory) override;
	virtual bool ShowActiveInUI_Implementation() const override;
	virtual void PostInventoryLoaded() override; 
	virtual TArray<class UNarrativeItemUseAction*> GetItemUseActions_Implementation() const;

	//Add/remove the armor and attack bonus ratings to GAS 
	virtual void ApplyEquipmentAttributes();
	virtual void RemoveEquipmentAttributes();

	//Overridable in case children want to do anything with the spec - weapon items for example need to modify the spec to add damage. 
	virtual void ModifyEquipmentEffectSpec(struct FGameplayEffectSpec* Spec);

	virtual bool ShouldUseOnAdd_Implementation() const override;

	virtual FString GetStringVariable_Implementation(const FString& VariableName) override;

	/** Handles for any abilities this equippable granted us.  */
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;

	/** The slot this item is currently equipped to.  */
	UPROPERTY( BlueprintReadOnly, ReplicatedUsing=OnRep_CurrentSlot, SaveGame, Category = "Item - Weapon")
	FGameplayTag CurrentSlot;

	/**The slot this item equips to - deprecated and EquippableSlots should be used instead. */
	UPROPERTY()
	FGameplayTag EquippableSlot;

	/**The slots this item can be equipped to, usually only 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Equippable", meta = (Categories = "Narrative.Equipment.Slot"))
	FGameplayTagContainer EquippableSlots;

	// Gameplay effect to apply when the item is equipped, removed when item is taken off. For weapons this is applied when wielded. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Equippable")
	TSubclassOf<UGameplayEffect> EquipmentEffect;

	//The handle to the equipment gameplay effect that applies the equipments attributes to our player. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Equippable")
	struct FActiveGameplayEffectHandle EquipmentGEHandle;

	//TODO maybe move these to GameplayEffectContainers similar to epics RPG sample instead of hardcoded vars. 

	//
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Equippable", meta = (Categories = "SetByCaller"))
	TMap<FGameplayTag, float> EquipmentEffectValues;


	//RATINGS - we're phasing these out in favour of EquipmentEffectValues as defined above - much more flexible. 

	/**The amount we'll increase the wearers attack rating by when this item is equipped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Equippable")
	float AttackRating;

	/**The amount we'll increase the wearers armor rating by when this item is equipped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Equippable")
	float ArmorRating;

	/**The amount we'll increase the wearers stealth rating by when item is equipped  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Equippable")
	float StealthRating;

	/*Equipping this item will grant these abilities to the user; we'll remove them when the item is unequipped */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Item - Equippable")
	TArray<TSubclassOf<class UNarrativeGameplayAbility>> EquipmentAbilities;


protected:

	UFUNCTION()
	virtual void OnRep_CurrentSlot(const FGameplayTag& OldCurrentSlot);

	class UEquipmentComponent* GetEquipmentComponent() const;

public:
	FGameplayTag GetEquippedSlot() const { return CurrentSlot; }
	uint64 GetEquipmentRevision() const { return EquipmentRevision; }
	bool AllowsEquipmentSlot(FGameplayTag Slot) const { return Slot.IsValid() && EquippableSlots.HasTagExact(Slot); }

	UFUNCTION(BlueprintPure, Category = "Item - Equippable")
	bool IsEquipped() const; 

private:
	uint64 EquipmentRevision = 0;

};

/**
 * Defines a clothing item. Will use the skeletal mesh set in your EquipmentComponent and set it to the clothing mesh you select.
 */
UCLASS()
class NARRATIVEARSENAL_API UEquippableItem_Clothing : public UEquippableItem
{
	GENERATED_BODY()

protected:

	friend class ANarrativeCharacterVisual;

	UEquippableItem_Clothing();

	#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	#endif 

	virtual void HandleEquip_Implementation() override;
	virtual void HandleUnequip_Implementation(const FGameplayTag& OldSlot) override;

	UFUNCTION()
	virtual void ApplyClothingMesh();

	/**The mesh to apply to the player */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Equippable | Clothing Mesh")
	FCharacterCreatorAttribute_Mesh ClothingMeshData;

	
	/** Add any form specific meshes to this map, and narrative will select the correct one. 
	Useful for selecting different meshes for different character types. ie female, male, orc, etc. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Equippable | Clothing Mesh", meta = (ForceInlineRow, Categories = "Narrative.CharacterCreator.Forms"))
	TMap<FGameplayTag, FCharacterCreatorAttribute_Mesh> FormSpecificMeshData;
};
