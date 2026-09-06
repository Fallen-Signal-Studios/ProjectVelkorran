// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Items/MeleeWeaponItem.h"
#include "Items/RangedWeaponItem.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Weapons/WeaponVisual.h"
#include "SovWeaponPairingTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class USovWeaponPairingTestMelee : public UMeleeWeaponItem
{
	GENERATED_BODY()
public:
	using UWeaponItem::CanDualWieldWith;
	using UWeaponItem::WeaponHand;
	using UEquippableItem::CurrentSlot;
};

UCLASS(Transient, NotBlueprintable)
class USovWeaponPairingTestRanged : public URangedWeaponItem
{
	GENERATED_BODY()
public:
	using UWeaponItem::CanDualWieldWith;
	using UWeaponItem::WeaponHand;
	using UWeaponItem::bRequireSameClassForDualWield;
	using UEquippableItem::CurrentSlot;
	bool CanPairUsingBase(UWeaponItem* Other) { return UWeaponItem::CanDualWieldWith_Implementation(Other); }
};

/** Supplies content-free equipped visuals; all pairing decisions use production weapon code. */
UCLASS(Transient, NotBlueprintable)
class ASovWeaponPairingTestCharacter : public ANarrativeCharacter
{
	GENERATED_BODY()
public:
	ASovWeaponPairingTestCharacter(const FObjectInitializer& Initializer) : Super(Initializer) {}
	void SetTestVisual(ANarrativeCharacterVisual* Visual) { CharVisual = Visual; }
};

UCLASS(Transient, NotBlueprintable)
class ASovWeaponPairingTestVisual : public ANarrativeCharacterVisual
{
	GENERATED_BODY()
public:
	void SetTestWeapon(FGameplayTag Slot, AWeaponVisual* Visual)
	{
		if (Visual) { SpawnedWeaponVisuals.Add(Slot, Visual); }
		else { SpawnedWeaponVisuals.Remove(Slot); }
	}
};
