// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "EquippableItem.h"
#include "GAS/NarrativeCombatAbility.h"
#include "GAS/AttackComboAnimSet.h"
#include "WeaponItem.generated.h"

//Lets us defined the attachments a given weapon supports
USTRUCT(BlueprintType)
struct FWeaponAttachmentSlotConfig
{
	GENERATED_BODY()

	FWeaponAttachmentSlotConfig()
	{
		SocketName = FName();
	};

	//The bone or socket we want the attachment to equip to 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot Config")
	FName SocketName;

};

//Usually one of these would be defined per slot the weapon is in. HipL/HipR attach in different places etc. 
USTRUCT(BlueprintType)
struct FWeaponAttachmentConfig
{
	GENERATED_BODY()

	FWeaponAttachmentConfig()
	{
		SocketName = FName();
		Offset = FTransform::Identity;
	};

	//The bone or socket we want the attachment to equip to 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot Config")
	FName SocketName;
	
	//Any relative offset we want to apply 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Slot Config")
	FTransform Offset;
};

//How should this weapon equip to our character?
UENUM(BlueprintType)
enum class EWeaponHandRule : uint8
{
	//Item takes up both hands, for example a Rifle or a 2 Handed sword.
	WHR_Both UMETA(DisplayName="Two Handed"),
	//Item can only be used in the main hand, for example a 1H sword
	WHR_Mainhand UMETA(DisplayName = "Main Hand"),
	//Item can only be used in the offhand, for example a shield.
	WHR_Offhand UMETA(DisplayName = "Off Hand"),
	//Item can be used in either hand, for example dual wieldable weapons like a dagger or pistol
	WHR_Either UMETA(DisplayName = "Dual Wieldable")
};

/**
 * Optional policy for resolving a weapon's ordinary primary-fire damage.
 *
 * Fixed preserves Narrative's existing behavior. Distance Based is deterministic:
 * it deals the authored maximum at close range and linearly reaches the authored
 * minimum at long range. Hit-zone, ability, difficulty, and mitigation modifiers
 * are still applied later by the damage execution.
 */
UENUM(BlueprintType)
enum class EWeaponDamageVariationMode : uint8
{
	Fixed UMETA(DisplayName = "Fixed"),
	DistanceBased UMETA(DisplayName = "Distance Based")
};

//Used to track the state of our weapons magazine, if the weapon uses ammo. 
USTRUCT(BlueprintType)
struct FWeaponClipState
{
	GENERATED_BODY()

	FWeaponClipState()
	{	
		AmmoInClip = 0;
		ClientAmmoInClip = 0;
		AmmoItemGUID = FGuid();
	};

	/** The amount of ammo loaded into the clip of the weapon. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Item - Weapon | Ammo")
	int32 AmmoInClip;

	/** Client uses this to predict ammo ahead, and set back to auth when servers value onreps back in.
	 * This ensures that clients on poor connections dont have a laggy ammo counter. 
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, NotReplicated, Category = "Item - Weapon | Ammo")
	int32 ClientAmmoInClip;
	
	/** The ammo item we're consuming as ammo is used up from the clip. Stored using its GUID so we can SaveGame this.
	This will only be set to a valid item if we are using NON-EQUIPPABLE AMMO. Equippable ammo is just pulled right from our ammo slot.*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Item - Weapon | Ammo")
	TObjectPtr<UNarrativeItem> AmmoItemSource;

	//The ammo items GUID, used so we can SaveGame the ammo item and restore it. 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, NotReplicated, Category = "Item - Weapon | Ammo")
	FGuid AmmoItemGUID;

};

/**
 * Base class for an equippable weapon. Weapons don't generally have any attack logic in them, instead they grant abilities which have the logic. 
 */
UCLASS()
class NARRATIVEARSENAL_API UWeaponItem : public UEquippableItem
{
	GENERATED_BODY()

public:
	TSoftClassPtr<class AWeaponVisual> GetWeaponVisualClass() const { return WeaponVisualClass; }

	friend class UEquipmentComponent;
	friend class UNarrativeCombatAbility;
	friend class ANarrativeCharacter; 
	friend class ANarrativeCharacterVisual;
	friend class ANarrativePlayerCharacter; 
	friend class UWeaponAttachmentItem;
	friend class AWeaponVisual;
	friend class UAmmoItem;

	UWeaponItem();


protected:
	/** Clips and reserve quantities are one transaction across inventory notifications. */
	bool bAmmoCommitPending = false;

	virtual void AddedToInventory(class UNarrativeInventoryComponent* Inventory, const bool bFromLoad) override; 
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostLoad() override; 

	//Wield our weapon to the given slot. 
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void WieldInSlot(UPARAM(meta = (Categories = "Narrative.Equipment.WieldSlot"))FGameplayTag DesiredSlot);

	virtual void HandleEquip_Implementation() override;
	virtual void HandleUnequip_Implementation(const FGameplayTag& OldSlot) override;

	//Entry points for trying to add a weapon attachment to the weapon
	virtual bool TryAddAttachment(class UWeaponAttachmentItem* Attachment);
	virtual void TryRemoveAttachment(class UWeaponAttachmentItem* Attachment);

	//Actually adds the attachment - cannot fail unlike TryAddAttachment versions
	virtual void AddAttachment(class UWeaponAttachmentItem* Attachment);
	virtual void RemoveAttachment(class UWeaponAttachmentItem* Attachment);

	virtual void AddAttachmentVisual(class UWeaponAttachmentItem* Attachment);
	virtual void RemoveAttachmentVisual(class UWeaponAttachmentItem* Attachment);

	/** Test if we're allowed to attach a given attachment to this weapon */
	virtual bool WeaponAllowsAttachment(const class UWeaponAttachmentItem* Attachment) const;

	/** Check if we can dual wield this weapon with the other test weapon. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Weapon")
	bool CanDualWieldWith(class UWeaponItem* Other);
	virtual bool CanDualWieldWith_Implementation(class UWeaponItem* Other);

	/** Called when our owner actually starts holding this weapon */
	UFUNCTION(BlueprintNativeEvent, Category = "Equippable")
	void HandleWield();
	virtual void HandleWield_Implementation();

	/** Called when our owner stops holding this weapon */
	UFUNCTION(BlueprintNativeEvent, Category = "Equippable")
	void HandleUnWield();
	virtual void HandleUnWield_Implementation();

	//Add/remove the armor and attack bonus ratings to GAS 
	virtual void ModifyEquipmentEffectSpec(struct FGameplayEffectSpec* Spec) override;
	virtual FString GetStringVariable_Implementation(const FString& VariableName) override;

	//Add/remove the armor and attack bonus ratings to GAS 
	virtual void GiveWeaponAbilities();
	virtual void RemoveWeaponAbilities();

	/**Weapon visual actor to spawn. Will attach to player using Holster/Wield Attachment configs below. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Weapon Visuals")
	TSoftClassPtr<class AWeaponVisual> WeaponVisualClass;

	/**Defines how the weapon should be attached in different slots*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Weapon Visuals", meta = (ForceInlineRow, Categories = "Narrative.Equipment.Slot.Weapon"))
	TMap<FGameplayTag, FWeaponAttachmentConfig> HolsterAttachmentConfigs; 

	/**Defines how the weapon should be attached in different hands*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Weapon Visuals", meta = (ForceInlineRow, Categories = "Narrative.Equipment.WieldSlot"))
	TMap<FGameplayTag, FWeaponAttachmentConfig> WieldAttachmentConfigs; 

	/**If using a crosshair container it will populate itself with this crosshair when weapon is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Weapon Visuals")
	TSoftClassPtr<class UCrosshairWidget> CrosshairWidget;

	/** Defines which hand the weapon goes in when wielded.  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon")
	EWeaponHandRule WeaponHand;

	/** If either weapon opts in, dual wielding requires the same exact item class. Existing hand and equip rules still apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon")
	bool bRequireSameClassForDualWield = false;

	/* When the weapon is wielded on its own, we'll grant this set of abilities. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Item - Weapon")
	TArray<TSubclassOf<class UNarrativeGameplayAbility>> WeaponAbilities;

	/* When the weapon is wielded in the mainhand, we'll grant these abilities instead of the WeaponAbilities.  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Item - Weapon", meta = (EditConditionHides, EditCondition="WeaponHand==EWeaponHandRule::WHR_Either||WeaponHand==EWeaponHandRule::WHR_Mainhand"))
	TArray<TSubclassOf<class UNarrativeGameplayAbility>> MainhandWeaponAbilities;

	/* When the weapon is wielded in the offhand, we'll grant these abilities instead of the WeaponAbilities  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Item - Weapon", meta = (EditConditionHides, EditCondition="WeaponHand==EWeaponHandRule::WHR_Either||WeaponHand==EWeaponHandRule::WHR_Offhand"))
	TArray<TSubclassOf<class UNarrativeGameplayAbility>> OffhandWeaponAbilities;

	/** Handles for any abilities this weapon granted us.  */
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> WeaponAbilityHandles;

	/** Allows us to define how our wielder should rotate with this item equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Player Movement")
	ECapsuleRotationSetting OwnerCapsuleRotationSetting;
	
	/** Allows us to define whether the pawn should follow the camera rotation with this weapon equipped. Bots will not apply this as they need their AIFocus to work. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon")
	bool bPawnFollowsControlRotation;

	/** If bPawnFollowsControlRotation, do we want to snap to rotation or interp to it using CMCs rotation rate.  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon")
	bool bPawnWantsSmoothedRotation;
	
	/** Allows us to define whether the pawn orient their rotation to velocity with this weapon equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon")
	bool bPawnOrientsRotationToMovement;

	/** base damage this weapon should do. It is up the combat ability whether it wants/needs this value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Attack Settings")
	float AttackDamage;

	/**
	 * Optional deterministic variation for ordinary ranged primary fire. Fixed is
	 * the backwards-compatible default. Damage effects which explicitly author a
	 * SetByCaller.Damage magnitude, including Echo abilities, do not use this policy.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Attack Settings | Damage Variation")
	EWeaponDamageVariationMode DamageVariationMode;

	/** Damage dealt at or beyond DamageVariationFarDistance, before hit-zone modifiers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Attack Settings | Damage Variation", meta = (EditCondition = "DamageVariationMode == EWeaponDamageVariationMode::DistanceBased", EditConditionHides, ClampMin = "0.0"))
	float MinimumAttackDamage;

	/** Damage dealt at or inside DamageVariationNearDistance, before hit-zone modifiers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Attack Settings | Damage Variation", meta = (EditCondition = "DamageVariationMode == EWeaponDamageVariationMode::DistanceBased", EditConditionHides, ClampMin = "0.0"))
	float MaximumAttackDamage;

	/** Distance in centimetres at or inside which MaximumAttackDamage is dealt. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Attack Settings | Damage Variation", meta = (EditCondition = "DamageVariationMode == EWeaponDamageVariationMode::DistanceBased", EditConditionHides, ClampMin = "0.0", Units = "cm"))
	float DamageVariationNearDistance;

	/** Distance in centimetres at or beyond which MinimumAttackDamage is dealt. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Attack Settings | Damage Variation", meta = (EditCondition = "DamageVariationMode == EWeaponDamageVariationMode::DistanceBased", EditConditionHides, ClampMin = "0.0", Units = "cm"))
	float DamageVariationFarDistance;

	/** How much should base damage be multiplied for a heavy attack. It is up the combat ability whether it wants/needs this value, some weapons may not have heavy attacks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Attack Settings")
	float HeavyAttackDamageMultiplier;

	/** Whether this weapon can be manually reloaded via GA_Reload, or whether it cannot be manually reloaded.  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Attack Settings")
	bool bAllowManualReload;

	/** Defines the attachment configuration for this weapon */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Attachments", meta = (Categories = "Narrative.Equipment.Weapon.AttachSlot"))
	TMap<FGameplayTag, FWeaponAttachmentSlotConfig> WeaponAttachmentConfiguration;

	/** Ammo item class for this weapon. Combat Ability will deny activation if we don't have the required ammo. 
	empty class means weapon can attack without ammo. This is in WeaponItem base class as any weapon should be able to support ammo, not just firearms. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Ammo")
	TSubclassOf<class UNarrativeItem> RequiredAmmo;

	//If true, bots won't actually consume ammo when shooting. They will still need ammo however - it just wont be consumed.  
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Ammo")
	bool bBotsConsumeAmmo;

	//Bots will need to be within this range to try attack with this weapon. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Ammo")
	float BotAttackRange;

	/** The clip size of the weapon, if the weapon uses one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon | Ammo")
	int32 ClipSize;

	/** Stores information about our weapons clip. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_WeaponClipState, SaveGame, Category = "Item - Weapon | Ammo")
	FWeaponClipState WeaponClipState;

	/** The hand this weapon is wielded in, if any.  */
	UPROPERTY(BlueprintReadOnly, Category = "Item - Weapon")
	FGameplayTag WieldedSlot;

	/** The attachment items currently on this weapon. */
	UPROPERTY(BlueprintReadOnly, Category = "Item - Weapon | Attachments")
	TMap<FGameplayTag, class UWeaponAttachmentItem*> WeaponAttachments;

	/** The last time this weapon attacked. Requires OnAttack() to be called by owning ability, currently used for weapon spread but could be useful elsewhere. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
	float LastAttackTime;

	//Tell our weapon to use some ammo, return true if this succeeded. 
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon")
	bool ConsumeAmmo(const int32 Amount=1);
	virtual bool ConsumeAmmo_Implementation(const int32 Amount = 1);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon")
	float GetWeaponSpread() const;

	UFUNCTION()
	virtual void OnRep_WieldedSlot(const FGameplayTag& OldWieldedSlot);

	UFUNCTION()
	virtual void OnRep_WeaponClipState(const FWeaponClipState& OldClipState);
	
public:

	//Grab the attachment config for the given slot.
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FWeaponAttachmentConfig GetWeaponHolsterAttachConfig(UPARAM(meta = (Categories = "Narrative.Equipment.Slot.Weapon"))FGameplayTag DesiredSlot) const;

	//Grab the attachment config for the given slot.
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FWeaponAttachmentConfig GetWeaponWieldAttachConfig(UPARAM(meta = (Categories = "Narrative.Equipment.WieldSlot"))FGameplayTag DesiredSlot) const;

	//Return the display name of the weapon. 
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Weapon")
	FText GetWeaponDisplayName(const bool bShowAttachments, const bool bShowAmmo) const;
	virtual FText GetWeaponDisplayName_Implementation(const bool bShowAttachments, const bool bShowAmmo) const;

	/** Return this weapon's fixed authored damage before any optional variation. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Damage")
	float GetAttackDamage() const { return FMath::Max(AttackDamage, 0.f); }

	/** Read the authored single-weapon kit without drawing it or granting abilities. */
	const TArray<TSubclassOf<UNarrativeGameplayAbility>>& GetWeaponAbilities() const { return WeaponAbilities; }

	/** True when this weapon has opted into a non-fixed primary-fire damage policy. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Damage")
	bool HasDamageVariation() const { return DamageVariationMode != EWeaponDamageVariationMode::Fixed; }

	/**
	 * Resolve this weapon's pre-hit-zone primary-fire damage for a trace distance.
	 * This function is deterministic and returns AttackDamage while variation is disabled.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon|Damage")
	float ResolveAttackDamageForDistance(const float Distance) const;

	//Update the ammo in our clip. Doesn't play FX or anything, if you need that use GA_Reload ability. 
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ammo")
	bool Reload();
	virtual bool Reload_Implementation();

	//Return the amount of ammo in our weapons clip. This will return the client predicted value if we are local. 
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ammo")
	int32 GetAmmoInClip() const;
	virtual int32 GetAmmoInClip_Implementation() const;

	//Return the amount of ammo in our weapons clip. This will use the server value, so shouldn't be used for UI. 
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ammo")
	int32 GetAuthAmmoInClip() const;
	virtual int32 GetAuthAmmoInClip_Implementation() const;
	
	//Return the amount of spare ammo that our clip isnt using  
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ammo")
	int32 GetSpareAmmo()const;
	virtual int32 GetSpareAmmo_Implementation()const;

	//Return the size of our weapons clip.  
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Ammo")
	int32 GetClipSize()const;
	virtual int32 GetClipSize_Implementation()const;

	/** Configuration, not inventory availability: an exhausted firearm still has a magazine. */
	bool UsesMagazine() const { return RequiredAmmo && GetClipSize() > 0; }

	//Get the currently used ammo - either the one our mag is using or if the weapon requires equippable ammo, the one in our ammo slot. 
	UFUNCTION(BlueprintCallable, Category = "Ammo")
	virtual UNarrativeItem* GetAmmoSource() const;

	//Set our ammo source. 
	virtual void InitAmmoSource();

	//Get the attachment at the given slot if there is one 
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual class UWeaponAttachmentItem* GetAttachment(UPARAM(meta = (Categories = "Narrative.Equipment.Weapon.AttachSlot"))const FGameplayTag& AttachmentSlot) const;

	//Return the combo animations for a given attack type - by default this won't return any combo anims, weapons that need combos should override this. 
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual TArray<UNarrativeAnimSet*> GetComboAnims(const bool bHeavyAttack) const;

	//Return whether the weapon is holstered or not 
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsHolstered() const;
	
	//Return whether the weapon is wielded or not 
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsWielded() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	virtual bool WantsOrientRotationToMovement() const { return bPawnOrientsRotationToMovement;};

	UFUNCTION(BlueprintPure, Category = "Weapon")
	virtual bool WantsUseControllerRotationYaw(bool& bUseYawInterp) const { return bPawnFollowsControlRotation;};

	//GA_Fire will start reloading the gun if this returns true, usually once we have an empty clip. 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ammo")
	bool RequiresAutoReload() const;
	virtual bool RequiresAutoReload_Implementation() const;

	//Return whether or not we have ammo to fire, default is to check we have required ammo item. 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Weapon")
	bool HasAmmo() const;

	//Usually called by the combat GA whenever we attack. Note that GA's don't HAVE to call this, so make sure GA does if using this. 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Weapon")
	void OnAttack();

	/** World time of the last committed weapon attack, for cosmetic shot presentation. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetLastAttackTime() const { return LastAttackTime; }

	//Return whether the weapon is allowed to attack. 
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	bool CanAttack() const;
	virtual bool CanAttack_Implementation() const;

	//Return the trace data for the weapon. We're moving away from this and instead pushing people to store TraceData on the ability instead of coupling this to weapon. 
	UFUNCTION(BlueprintPure, Category = "Weapon")
	virtual FCombatTraceData GetTraceData() const;

	//Used by bots to determine whether they are close enough to perform an attack with this weapon 
	UFUNCTION(BlueprintPure, Category = "Weapon")
	virtual float GetAttackRange() const;

	FName GetWeaponVisualAttachBone() const;
};
