// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Tests/SovCoordinationRuntimeTestFixtures.h"
#include "SovCampaignMassRoundTripFixtures.generated.h"

/** Real campaign NPC/ASC/record path, with deterministic asset initialization instead of external campaign assets. */
UCLASS(Transient, NotBlueprintable)
class ASovCampaignMassRoundTripNPC : public ASovCoordinationTestNPC
{
	GENERATED_BODY()
public:
	ASovCampaignMassRoundTripNPC(const FObjectInitializer& Initializer);
	virtual void SetNPCDefinition(UNPCDefinition* Definition) override;
protected:
	virtual void SpawnDefaultController() override {}
};
