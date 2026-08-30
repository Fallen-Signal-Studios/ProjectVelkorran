// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Pickups/SovCombatSustainPickup.h"
#include "SovAmmoCombatSustainPickup.generated.h"

class UAmmoItem;

/** Automatic pickup that adds a configured Narrative ammo item quantity. */
UCLASS(Blueprintable, meta = (DisplayName = "Ammo Combat Sustain Pickup"))
class PROJECTVELKORRAN_API ASovAmmoCombatSustainPickup : public ASovCombatSustainPickup
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Copies the authoritative payload before FinishSpawning is called. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Combat Sustain|Ammo")
	void InitializeAmmo(TSubclassOf<UAmmoItem> InAmmoItemClass, int32 InQuantity);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Combat Sustain|Ammo")
	TSubclassOf<UAmmoItem> GetAmmoItemClass() const { return AmmoItemClass; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Combat Sustain|Ammo")
	int32 GetAmmoQuantity() const { return AmmoQuantity; }

protected:
	virtual bool TryGrantTo(ASovPlayerCharacterBase* CollectingPlayer) override;

	/** Item asset/class that defines which weapon reserve this pickup refills. */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Ammo")
	TSubclassOf<UAmmoItem> AmmoItemClass;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Ammo", meta = (ClampMin = "1"))
	int32 AmmoQuantity = 5;
};
