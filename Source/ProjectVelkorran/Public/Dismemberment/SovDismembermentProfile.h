// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Dismemberment/SovDismembermentTypes.h"
#include "SovDismembermentProfile.generated.h"

/** Per-skeleton/form authoring asset for dismemberment mappings, rules, and gore presentation. */
UCLASS(BlueprintType)
class PROJECTVELKORRAN_API USovDismembermentProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	USovDismembermentProfile();

	/** Builds the complete standard Epic SK_Mannequin sever map. */
	static void BuildSKMannequinRegionDefinitions(
		TArray<FSovDismembermentRegionDefinition>& OutRegions);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment", meta = (TitleProperty = "Region"))
	TArray<FSovDismembermentRegionDefinition> Regions;

	/** Any matching rule permits the sever. Empty uses the component's lethal Edge fallback rule. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment", meta = (TitleProperty = "MinimumAppliedHealthDamage"))
	TArray<FSovDismembermentRule> SeverRules;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dismemberment")
	bool GetRegionDefinition(
		ESovDismembermentRegion Region,
		FSovDismembermentRegionDefinition& OutDefinition) const;

	/** Restores canonical SK_Mannequin bone fields without replacing authored cosmetics or rules. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Sovereign|Dismemberment", meta = (DisplayName = "Apply SK Mannequin Bone Map"))
	void ApplySKMannequinBoneMap();
};
