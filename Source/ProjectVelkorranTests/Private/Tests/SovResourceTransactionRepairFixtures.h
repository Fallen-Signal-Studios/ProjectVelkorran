// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Items/NarrativeItem.h"
#include "Items/WeaponItem.h"
#include "Items/InventoryComponent.h"
#include "Tests/SovExertionRuntimeTestFixtures.h"
#include "Combat/Pickups/SovAmmoCombatSustainPickup.h"
#include "Combat/Pickups/SovEchoCombatSustainPickup.h"
#include "SovResourceTransactionRepairFixtures.generated.h"

/** Native item configuration only; production inventory code performs every debit. */
UCLASS(Transient, NotBlueprintable)
class USovResourceRepairAmmo : public UNarrativeItem
{
	GENERATED_BODY()
public:
	USovResourceRepairAmmo() { bStackable = true; MaxStackSize = 100; Weight = 0.f; }
	bool bAllowRemoval = true;
	mutable bool bRewriteOnPermission = false;
	mutable int32 PermissionCalls = 0;
	int32 RefillOnPermissionCall = 0;
	bool bDestroyOwnerOnRemoval = false;
	virtual void RemovedFromInventory(UNarrativeInventoryComponent* Inventory) override
	{
		Super::RemovedFromInventory(Inventory);
		if (bDestroyOwnerOnRemoval && Inventory && Inventory->GetOwner()) { Inventory->GetOwner()->Destroy(); }
	}
	virtual bool CanBeRemoved_Implementation() const override
	{
		++PermissionCalls;
		if (bRewriteOnPermission)
		{
			bRewriteOnPermission = false;
			const_cast<USovResourceRepairAmmo*>(this)->SetQuantity(GetQuantity());
		}
		if (RefillOnPermissionCall > 0 && PermissionCalls == RefillOnPermissionCall)
		{ const_cast<USovResourceRepairAmmo*>(this)->SetQuantity(5); }
		return bAllowRemoval;
	}
};

UCLASS(Transient, NotBlueprintable)
class USovResourceRepairWeapon : public UWeaponItem
{
	GENERATED_BODY()
public:
	USovResourceRepairWeapon()
	{ RequiredAmmo = USovResourceRepairAmmo::StaticClass(); ClipSize = 1; bBotsConsumeAmmo = true; Weight = 0.f; }
	using UWeaponItem::ConsumeAmmo;
	void SetLoaded(int32 Value) { WeaponClipState.AmmoInClip = Value; MarkDirtyForReplication(); }
	int32 RawLoaded() const { return WeaponClipState.AmmoInClip; }
	void SetAmmoSourceForTest(UNarrativeItem* Item)
	{ WeaponClipState.AmmoItemSource = Item; WeaponClipState.AmmoItemGUID = Item ? Item->ItemGUID : FGuid(); MarkDirtyForReplication(); }
	mutable bool bRewriteReserveOnSpareRead = false;
	virtual int32 GetSpareAmmo_Implementation() const override
	{
		const int32 Spare = Super::GetSpareAmmo_Implementation();
		if (bRewriteReserveOnSpareRead)
		{
			bRewriteReserveOnSpareRead = false;
			if (UNarrativeItem* Ammo = GetAmmoSource()) { Ammo->SetQuantity(Ammo->GetQuantity()); }
		}
		return Spare;
	}
};

UCLASS(Transient, NotBlueprintable)
class ASovResourceRepairPlayer : public ASovExertionRuntimeTestCharacter
{
	GENERATED_BODY()
public:
	ASovResourceRepairPlayer(const FObjectInitializer& Initializer) : Super(Initializer)
	{ InventoryComponent = CreateDefaultSubobject<UNarrativeInventoryComponent>(TEXT("ResourceRepairInventory")); }
	// No authored player definition is needed for the production pickup admission path.
	virtual void PossessedBy(AController* NewController) override { APawn::PossessedBy(NewController); }
};

UCLASS(Transient, NotBlueprintable)
class ASovResourceRepairEchoPickup : public ASovEchoCombatSustainPickup
{
	GENERATED_BODY()
public:
	void Touch(AActor* Player) { HandlePickupOverlap(nullptr, Player, nullptr, 0, false, FHitResult()); }
};

UCLASS(Transient, NotBlueprintable)
class ASovResourceRepairAmmoPickup : public ASovAmmoCombatSustainPickup
{
	GENERATED_BODY()
public:
	void Touch(AActor* Player) { HandlePickupOverlap(nullptr, Player, nullptr, 0, false, FHitResult()); }
};

UCLASS(Transient, NotBlueprintable)
class USovResourceRepairProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<USovResourceRepairWeapon> Weapon;
	UPROPERTY() TObjectPtr<UNarrativeInventoryComponent> Inventory;
	UPROPERTY() TObjectPtr<UNarrativeItem> Ammo;
	UPROPERTY() TObjectPtr<ASovResourceRepairAmmoPickup> AmmoPickup;
	UPROPERTY() TObjectPtr<ASovResourceRepairPlayer> Player;
	bool bArmed = true;
	bool bNestedConsumeAccepted = false;
	bool bNestedReloadAccepted = false;
	bool bAddInsteadOfConsume = false;
	bool bRefillAfterDebit = false;
	int32 Quantity = 1;
	int32 InventoryNotifications = 0;
	int32 RemovalNotifications = 0;
	UFUNCTION(CallInEditor) void ObserveInventoryUpdate() { ++InventoryNotifications; }
	UFUNCTION(CallInEditor) void ObserveRemoval(UNarrativeItem* Item, int32 Amount) { ++RemovalNotifications; }
	UFUNCTION(CallInEditor) void DuringAmmoMutation()
	{
		if (!bArmed) { return; }
		bArmed = false;
		if (Weapon)
		{
			bNestedConsumeAccepted = Weapon->ConsumeAmmo(1);
			bNestedReloadAccepted = Weapon->Reload();
		}
		if (bRefillAfterDebit && Ammo) { Ammo->SetQuantity(5); }
		if (AmmoPickup && Player) { AmmoPickup->Touch(Player); }
		if (Inventory && Ammo)
		{
			if (bAddInsteadOfConsume) { Inventory->TryAddItemFromClass(Ammo->GetClass(), Quantity, false); }
			else if (!bRefillAfterDebit) { Inventory->ConsumeItem(Ammo, Quantity); }
		}
	}
};
