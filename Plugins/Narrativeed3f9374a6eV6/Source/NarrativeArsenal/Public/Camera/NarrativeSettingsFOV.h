// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Core/CameraNode.h"
#include "Core/CameraParameters.h"
#include "Nodes/CameraNodeTypes.h"

#include "NarrativeSettingsFOV.generated.h"


/**
 * Allows us to control the FOV via PC->GetDesiredFOV. We dont really want camera modes to handle blending FOVs as it seems fairly
 * expirimental and difficult to control blend transition times etc, so letting PC control it seemed like better option. 
 */
UCLASS(MinimalAPI, meta=(CameraNodeCategories="Common,Lens"))
class UNarrativeSettingsFOVCameraNode : public UCameraNode
{
	GENERATED_BODY()

public:

	/** The field of view, as a percentage of the FOV degrees set in the players Game User Settings. */
	//UPROPERTY(EditAnywhere, Category=Common, meta = (ClampMin=0.1, ClampMax=10.f))
	//FFloatCameraParameter FieldOfViewPct;

public:

	UNarrativeSettingsFOVCameraNode(const FObjectInitializer& ObjectInit);

protected:

	// UCameraNode interface.
	virtual FCameraNodeEvaluatorPtr OnBuildEvaluator(FCameraNodeEvaluatorBuilder& Builder) const override;
};

