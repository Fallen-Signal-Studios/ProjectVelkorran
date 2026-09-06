// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Items/InventoryComponent.h"
#include "Items/NarrativeItem.h"
#include "Items/EquippableItem.h"
#include "Items/WeaponItem.h"
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "NarrativeGameplayTags.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Weapons/WeaponVisual.h"
#include "SovCinematicInventoryRuntimeTestFixtures.generated.h"

UCLASS()
class USovCinematicTestStack : public UNarrativeItem
{
    GENERATED_BODY()
public:
    USovCinematicTestStack()
    { bStackable = true; MaxStackSize = 100; Weight = 1.f; bAllowCampaignCinematicGrant = true; bAllowCampaignCinematicRemoval = true; }
    mutable bool bRewriteDuringPermission = false;
    virtual bool CanBeRemoved_Implementation() const override
    {
        if (bRewriteDuringPermission)
        { bRewriteDuringPermission = false; const_cast<USovCinematicTestStack*>(this)->SetQuantity(GetQuantity()); }
        return true;
    }
};
UCLASS()
class USovCinematicTestResourceWeapon : public UWeaponItem
{
    GENERATED_BODY()
public:
    USovCinematicTestResourceWeapon()
    { bAllowCampaignCinematicGrant = true; bAllowCampaignCinematicRemoval = true; Weight = 1.f; }
    void SetTestClip(int32 Ammo) { WeaponClipState.AmmoInClip = Ammo; MarkDirtyForReplication(); }
    int32 GetTestClip() const { return WeaponClipState.AmmoInClip; }
};
UCLASS()
class USovCinematicTestEquipment : public UEquippableItem
{
    GENERATED_BODY()
public:
    USovCinematicTestEquipment()
    { bAllowCampaignCinematicGrant = true; bAllowCampaignCinematicRemoval = true; bAllowCampaignCinematicEquipment = true; EquipmentEffect = nullptr; Weight = 1.f;
        EquippableSlots.AddTag(FNarrativeGameplayTags::Get().Equipment_Slot_Ammo); }
    void SetTestSlot(FGameplayTag Slot) { EquippableSlots.Reset(); EquippableSlots.AddTag(Slot); }
};
UCLASS()
class USovCinematicTestEquipmentReplacement : public USovCinematicTestEquipment
{
    GENERATED_BODY()
};
UCLASS()
class USovCinematicInventoryProbe : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UNarrativeInventoryComponent> Inventory;
    UPROPERTY() TObjectPtr<UNarrativeItem> Item;
    UPROPERTY() TObjectPtr<UEquippableItem> OtherEquipment;
    UPROPERTY() TObjectPtr<USovCampaignCinematicComponent> Managed;
    bool bFireOnce = true;
    bool bBusyRewrite = false;
    int32 AddedCount = 0;
    int32 RemovedCount = 0;
    UFUNCTION() void Added(const FItemAddResult& Result)
    {
        ++AddedCount;
        if (bFireOnce && bBusyRewrite && !Result.Stacks.IsEmpty())
        { bFireOnce = false; Result.Stacks[0]->SetBusy(true); Result.Stacks[0]->SetBusy(false); }
    }
    UFUNCTION() void Removed(UNarrativeItem* RemovedItem, int32 Amount) { ++RemovedCount; }
    UFUNCTION() void RetireDuringAdd(const FItemAddResult& Result)
    { if (bFireOnce && Managed) { bFireOnce = false; Managed->UnregisterComponent(); } }
    UFUNCTION() void ReplaceDuringUnequip(FGameplayTag Slot, UEquippableItem* Previous)
    { if (bFireOnce && Inventory && OtherEquipment) { bFireOnce = false; Inventory->SetCinematicEquipment(OtherEquipment, Slot); } }
};

UCLASS()
class ASovCinematicPresentationTestCharacter : public ANarrativeCharacter
{
    GENERATED_BODY()
public:
    ASovCinematicPresentationTestCharacter(const FObjectInitializer& Initializer) : Super(Initializer) {}
    void SetTestVisual(ANarrativeCharacterVisual* Visual) { CharVisual = Visual; }
};
UCLASS()
class ASovCinematicPresentationTestVisual : public ANarrativeCharacterVisual
{
    GENERATED_BODY()
public:
    void Configure(ANarrativeCharacter* Character, USkeletalMeshComponent* Mesh)
    { OwnerCharacter = Character; MeshComponents.Add(FNarrativeGameplayTags::Get().Equipment_Slot_Character_Mesh, Mesh); }
};
UCLASS()
class USovCinematicPresentationTestWeaponItem : public UWeaponItem
{
    GENERATED_BODY()
public:
    void Configure(FGameplayTag Slot)
    { FWeaponAttachmentConfig Config; Config.SocketName = TEXT("blade_root"); Config.Offset = FTransform::Identity; HolsterAttachmentConfigs.Add(Slot, Config); }
};
UCLASS()
class ASovCinematicPresentationTestWeapon : public AWeaponVisual
{
    GENERATED_BODY()
public:
    ASovCinematicPresentationTestWeapon(const FObjectInitializer& Initializer) : Super(Initializer) {}
    void PrimeCachedAttachment(UWeaponItem* Item, ANarrativeCharacter* Character, ANarrativeCharacterVisual* Visual, FGameplayTag Slot)
    {
        WeaponOwner = Item; CharacterOwner = Character; VisualOwner = Visual;
        AttachState.WeaponOwner = Item; AttachState.CharOwner = Character; AttachState.VisualOwner = Visual;
        AttachState.EquippedSlot = Slot; AttachState.WieldedSlot = FGameplayTag();
        AppliedWieldSlot = FGameplayTag(); bHasAppliedAttachment = true; bAttachedSuccesfully = true;
    }
};
