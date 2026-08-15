// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassCrowdVisualizationTrait.h"
#include "NarrativePedFragments.h"
#include "NarrativeVisualizationTrait.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeVisualizationTrait : public UMassCrowdVisualizationTrait
{
	GENERATED_BODY()

	UNarrativeVisualizationTrait();

	virtual void SanitizeParams(FMassRepresentationParameters& InOutParams, const bool bStaticMeshDeterminedInvalid = false) const override;
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

#if WITH_EDITOR
	virtual bool ValidateParams() const override;
#endif

	UPROPERTY(EditAnywhere, Category = "NarrativeVisualizationTrait")
	FNarrativePedProperties PedProperties;
};
