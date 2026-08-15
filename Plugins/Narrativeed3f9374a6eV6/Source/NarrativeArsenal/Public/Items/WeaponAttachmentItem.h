// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Items/NarrativeItem.h"
#include "WeaponAttachmentItem.generated.h"

/**
 * An item that can be attached to a WeaponItem in order to modify its functionality and/or visuals 
 */
UCLASS()
class NARRATIVEARSENAL_API UWeaponAttachmentItem : public UNarrativeItem
{
	GENERATED_BODY()
	
protected:

	friend class UWeaponItem;
	friend class AWeaponVisual;

	UWeaponAttachmentItem();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Serialize(FArchive& Ar) override; 
	virtual void PostInventoryLoaded() override; 
	virtual void AddedToInventory(class UNarrativeInventoryComponent* Inventory, const bool bFromLoad) override;
	
	virtual void Use(UNarrativeItem* OtherItem=nullptr) override;
	virtual bool CanUseItemWith_Implementation(class UNarrativeItem* TestItem) const override;
	virtual bool ShouldUseOnAdd_Implementation() const override;

	/** Handle any logic the attachment wants on Attach */
	UFUNCTION(BlueprintNativeEvent, Category = "Attachments")
	void HandleAttach(UWeaponItem* AttachingTo);
	virtual void HandleAttach_Implementation(UWeaponItem* AttachingTo);

	/** Handle any logic the attachment wants on Attach */
	UFUNCTION(BlueprintNativeEvent, Category = "Attachments")
	void HandleDetach(UWeaponItem* DetachingFrom);
	virtual void HandleDetach_Implementation(UWeaponItem* DetachingFrom);

	/** Called when our owner actually starts holding this weapon, or when we get attached a currently wielded weapon.
	Nice hook for modifying state if attachment needs to do that. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equippable")
	void HandleWield();
	virtual void HandleWield_Implementation();

	/** Called when our owner stops holding this weapon, or when we get detached from the currently wielded weapon.  
	Nice hook for resetting modified state if attachment needs to do that. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equippable")
	void HandleUnWield();
	virtual void HandleUnWield_Implementation();

	UFUNCTION(Server, Reliable)
	virtual void ServerSetWeaponOwner(UWeaponItem* InWeaponOwner);

	virtual void SetWeaponOwner(UWeaponItem* WeaponOwner);

	UFUNCTION()
	void OnRep_WeaponOwner(class UWeaponItem* PreviousOwner);

	/** The weapon this attachment is current attached to - null if not attached*/
	UPROPERTY(ReplicatedUsing = OnRep_WeaponOwner, BlueprintReadOnly, Category= "Attachments")
	TObjectPtr<class UWeaponItem> WeaponOwner;

	/** Our owners GUID, cached so we can look the weapon back up from a savegame.  */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category= "Attachments")
	FGuid WeaponOwnerGUID;

	//The slot this attachment should equip to on the weapon 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon Attachment", meta = (Categories = "Narrative.Equipment.Weapon.AttachSlot"))
	FGameplayTag WeaponAttachmentSlot;

	//The mesh that we should create and attach to the gun - TODO soft ref and load this 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon Attachment")
	TObjectPtr<UStaticMesh> AttachmentMesh;

	//If non negative value will act as an FOV override for ADS 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon Attachment | FOV")
	float FOVOverride;

	//If non negative value will act as an FOV override for ADS 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon Attachment | FOV")
	float WeaponRenderFOVOverride;
	
	//If non negative value will act as an FStop override when aiming  
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon Attachment | FOV")
	float WeaponAimFStopOverride;
public:

	/** Lets the attachment override the FOV if desired. */
	UFUNCTION(BlueprintNativeEvent, Category = "Attachments")
	float OverrideWeaponCameraFOV() const;

	/** Lets the attachment override the weapons render FOV if desired. */
	UFUNCTION(BlueprintNativeEvent, Category = "Attachments")
	float OverrideWeaponRenderFOV() const;

		/** Lets the attachment override the cameras FSTop when aiming if desired. */
	UFUNCTION(BlueprintNativeEvent, Category = "Attachments")
	float OverrideWeaponAimFStop() const;
};
