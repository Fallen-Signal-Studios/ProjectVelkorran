// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassAgentTraits.h"
#include "MassEntityTraitBase.h"
#include "VisualLogger/VisualLogger.h"
#include "AgentAccelerationSyncTrait.generated.h"

/**
 * Alternative to UMassAgentMovementSyncTrait which directly sets velocities on the CMC. This trait will set acceleration values instead.
 */
UCLASS()
class NARRATIVEARSENAL_API UAgentAccelerationSyncTrait : public UMassAgentSyncTrait
{
	GENERATED_BODY()
protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
