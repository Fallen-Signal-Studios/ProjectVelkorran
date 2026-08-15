// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "TrafficVehicleObstacleTrait.generated.h"

/**
 * Trait to mark entity as an obstacle for traffic vehicles
 */
UCLASS()
class NARRATIVEARSENAL_API UTrafficVehicleObstacleTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
