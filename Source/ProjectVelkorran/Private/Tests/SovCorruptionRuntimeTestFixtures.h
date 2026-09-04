// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Tests/SovCampaignRuntimeTestFixtures.h"
#include "Corruption/SovCorruptionProfile.h"
#include "SovCorruptionRuntimeTestFixtures.generated.h"

/** Uses the actual player component and Narrative ASC while excluding content initialization. */
UCLASS(Transient, NotBlueprintable)
class ASovCorruptionRuntimeTestPawn : public ASovCampaignRuntimeTestPawn
{
	GENERATED_BODY()
public:
	ASovCorruptionRuntimeTestPawn(const FObjectInitializer& ObjectInitializer);
	void InitializeCombat();
	void SetReadyForRestoreTest(bool bReady) { bCharacterReady = bReady; }
	UFUNCTION() void SaveOnBandChanged(ESovCorruptionBand Previous, ESovCorruptionBand Current);
	int32 SaveCallbacks = 0;
};
