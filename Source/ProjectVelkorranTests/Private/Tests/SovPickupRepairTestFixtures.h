// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Tests/SovExertionRuntimeTestFixtures.h"
#include "Combat/Pickups/SovEchoCombatSustainPickup.h"
#include "SovPickupRepairTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class ASovPickupRepairTestCharacter : public ASovExertionRuntimeTestCharacter
{
    GENERATED_BODY()
public:
    ASovPickupRepairTestCharacter(const FObjectInitializer& Initializer) : Super(Initializer) {}
    virtual void PossessedBy(AController* NewController) override { APawn::PossessedBy(NewController); }
};

UCLASS(Transient, NotBlueprintable)
class ASovPickupRepairTestPickup : public ASovEchoCombatSustainPickup
{
    GENERATED_BODY()
public:
    void Touch(AActor* Player) { HandlePickupOverlap(nullptr, Player, nullptr, 0, false, FHitResult()); }
};
