// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassCommonFragments.h"
#include "MassEntityTraitBase.h"
#include "MassRadiusTrait.generated.h"

/**
 * Adds a radius fragment to the entity config. This is mainly a fix for assorted fragments not being recorded correctly on agent components.
 */
UCLASS()
class NARRATIVEARSENAL_API UMassRadiusTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()
	
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	
	UPROPERTY(EditAnywhere, Category="Mass")
	FAgentRadiusFragment RadiusFragment;
};
