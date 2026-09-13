// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Character/CharacterAppearance.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "GameplayEffect.h"
#include "Items/NarrativeItem.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "SovProtagonistPartitionTestFixtures.generated.h"
class ASovPlayerState;

/**
 * Supplies only what streamed content would: the shared ASC binding, a one-time attribute
 * baseline and visual readiness. It makes no Technique, faction, inventory or snapshot
 * decision; the production controller owns those.
 */
UCLASS(Abstract, Transient, NotBlueprintable)
class ASovPartitionTestPawn : public ASovPlayerCharacterBase
{
	GENERATED_BODY()
public:
	ASovPartitionTestPawn(const FObjectInitializer& ObjectInitializer);
	virtual void PossessedBy(AController* NewController) override { APawn::PossessedBy(NewController); }
	/** Visual readiness is staged below; there is no appearance content to present. */
	virtual void SpawnCharacterVisual_Implementation(UCharacterAppearance* DefaultAppearance) override {}
	bool StageContentReadiness(ASovPlayerState* State);
};

UCLASS(Transient, NotBlueprintable)
class ASovPartitionTestTarrik : public ASovPartitionTestPawn
{
	GENERATED_BODY()
public:
	ASovPartitionTestTarrik(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
	virtual FGameplayTag GetProtagonistIdentityTag() const override;
};

UCLASS(Transient, NotBlueprintable)
class ASovPartitionTestSelene : public ASovPartitionTestPawn
{
	GENERATED_BODY()
public:
	ASovPartitionTestSelene(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
	virtual FGameplayTag GetProtagonistIdentityTag() const override;
};

/**
 * A campaign controller for worlds that register a real local player. The fixture viewport has no
 * overlay to host a gameplay HUD, so none is created; every campaign, save and readiness path is unchanged.
 */
UCLASS(Transient, NotBlueprintable)
class ASovPartitionTestController : public ASovHandoffRuntimeTestController
{
	GENERATED_BODY()
public:
	ASovPartitionTestController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { GameplayHUDClass = nullptr; }
};

/** Signature equipment stand-ins: each class is only ever granted to one protagonist. */
UCLASS()
class USovPartitionTarrikKit : public UNarrativeItem
{
	GENERATED_BODY()
public:
	USovPartitionTarrikKit() { bStackable = true; MaxStackSize = 100; Weight = 1.f; }
};

UCLASS()
class USovPartitionSeleneKit : public UNarrativeItem
{
	GENERATED_BODY()
public:
	USovPartitionSeleneKit() { bStackable = true; MaxStackSize = 100; Weight = 1.f; }
};

/** Infinite, modifier-free combat effect: transient state that must not survive a handoff. */
UCLASS(Transient, NotBlueprintable)
class USovPartitionTransientCombatEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	USovPartitionTransientCombatEffect() { DurationPolicy = EGameplayEffectDurationType::Infinite; }
};
