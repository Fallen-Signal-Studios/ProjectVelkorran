// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Combat/Pickups/SovAmmoCombatSustainPickup.h"

#include "Characters/SovPlayerCharacterBase.h"
#include "Items/AmmoItem.h"
#include "Items/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/StrongObjectPtr.h"

void ASovAmmoCombatSustainPickup::InitializeAmmo(
	const TSubclassOf<UAmmoItem> InAmmoItemClass,
	const int32 InQuantity)
{
	if (!HasAuthority() || bGrantInProgress || IsClaimed())
	{
		return;
	}

	AmmoItemClass = InAmmoItemClass;
	AmmoQuantity = FMath::Max(InQuantity, 0);
}

bool ASovAmmoCombatSustainPickup::TryGrantTo(
	ASovPlayerCharacterBase* CollectingPlayer)
{
	if (!IsValid(CollectingPlayer) || !IsValid(AmmoItemClass) || AmmoQuantity <= 0)
	{
		return false;
	}

	UNarrativeInventoryComponent* Inventory =
		CollectingPlayer->GetInventoryComponent();
	if (!IsValid(Inventory))
	{
		return false;
	}
	TStrongObjectPtr<UNarrativeInventoryComponent> InventoryLifetime(Inventory);

	const UAmmoItem* AmmoDefaults = GetDefault<UAmmoItem>(AmmoItemClass);
	if (!IsValid(AmmoDefaults))
	{
		return false;
	}

	// Narrative weapons bind to one ammo-item stack. Treat that asset's maximum
	// stack size as the total carried cap so pickups never create surplus stacks
	// that the weapon and HUD would ignore.
	const int32 CarriedQuantity = Inventory->GetTotalQuantityOfItem(
		AmmoItemClass,
		false);
	const int32 AvailableReserveSpace = FMath::Max(
		AmmoDefaults->GetMaxStackSize() - CarriedQuantity,
		0);
	const int32 RequestedQuantity = FMath::Min(
		AmmoQuantity,
		AvailableReserveSpace);
	if (RequestedQuantity <= 0)
	{
		return false;
	}

	const FItemAddResult AddResult = Inventory->TryAddItemFromClass(
		AmmoItemClass,
		RequestedQuantity,
		false);

	// A partially accepted pack keeps its unclaimed remainder in the world.
	// Returning false leaves the overlap claim open; the player can step back
	// over it after making reserve space.
	const int32 AcceptedQuantity = FMath::Clamp(
		AddResult.AmountGiven,
		0,
		RequestedQuantity);
	AmmoQuantity -= AcceptedQuantity;
	if (AcceptedQuantity > 0 && AmmoQuantity > 0 && !IsActorBeingDestroyed())
	{
		ForceNetUpdate();
	}
	return AmmoQuantity == 0;
}

void ASovAmmoCombatSustainPickup::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASovAmmoCombatSustainPickup, AmmoItemClass);
	DOREPLIFETIME(ASovAmmoCombatSustainPickup, AmmoQuantity);
}
