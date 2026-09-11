// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Tests/SovCampaignMassRoundTripFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Campaign/SovEncounterTypes.h"
#include "SovEncounterInitialEntryRuntimeFixtures.generated.h"

/** Asset/readiness prerequisites stay deterministic, but movement must use the
 * real ACharacter actor/component BeginPlay and collision-overlap lifecycle. */
UCLASS(Transient, NotBlueprintable)
class ASovInitialEntryOverlapPawn : public ASovHandoffRuntimeTestPawn
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override { ACharacter::BeginPlay(); }
};

/** Supplies only delayed NPC visual/snapshot readiness; entry, overlap and capture remain native. */
UCLASS(Transient, NotBlueprintable)
class ASovInitialEntryDelayedNPC : public ASovCampaignMassRoundTripNPC
{
    GENERATED_BODY()
public:
    void SetSnapshotReadyForTest(bool bReady) { bEncounterSnapshotReady = bReady; }
};

UCLASS(Transient, NotBlueprintable)
class USovInitialEntryObserver : public UObject
{
    GENERATED_BODY()
public:
    int32 Activations = 0;
    UFUNCTION() void Changed(ESovEncounterState Previous, ESovEncounterState Current)
    { if (Current == ESovEncounterState::Active) { ++Activations; } }
};
