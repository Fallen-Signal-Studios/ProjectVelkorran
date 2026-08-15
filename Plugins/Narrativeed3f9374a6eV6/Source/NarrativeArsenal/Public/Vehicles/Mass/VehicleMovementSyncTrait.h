// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassAgentTraits.h"
#include "VehicleMovementSyncTrait.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UVehicleMovementSyncTrait : public UMassAgentSyncTrait
{
	GENERATED_BODY()

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
