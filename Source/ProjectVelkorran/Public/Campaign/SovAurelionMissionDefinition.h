// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Campaign/SovCampaignDefinition.h"
#include "SovAurelionMissionDefinition.generated.h"

class ULevelSequence;

/** Authored content fills these native contracts; the existing campaign journal owns every outcome. */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROJECTVELKORRAN_API USovAurelionMissionDefinition : public USovCampaignDefinition
{
	GENERATED_BODY()
public:
	/** Mandatory scene dependencies, keyed by CinematicId. These do not play scenes or manufacture receipts.
	 * The matching placed USovCampaignCinematicComponent must use the same sequence and native beat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Aurelion")
	TMap<FName, TSoftObjectPtr<ULevelSequence>> StorySequences;

	/** Source-contract check, usable before unavailable maps, characters and scenes have been authored. */
	UFUNCTION(BlueprintPure, Category="Campaign|Aurelion")
	bool ValidateAurelionContract(FString& OutError) const;
	/** Admission and commit both check this binding; listing a scene dependency cannot authorize another sequence. */
	bool MatchesStorySequence(FName BeatId, const TSoftObjectPtr<ULevelSequence>& Sequence) const;

	virtual bool ValidateDefinition(FString& OutError) const override;
};

/** Development-only corridor preparation. Grants no canon, evidence, assent or convergence progress. */
UCLASS(BlueprintType, Blueprintable)
class PROJECTVELKORRAN_API USovAurelionTarrikPreparationMissionDefinition : public USovAurelionMissionDefinition
{
	GENERATED_BODY()
public:
	USovAurelionTarrikPreparationMissionDefinition();
};

/** August TDD M12, manuscript chapter 24. Map, player/companion definitions and scenes are deliberately unassigned. */
UCLASS(BlueprintType, Blueprintable)
class PROJECTVELKORRAN_API USovAurelionFireAndFrostMissionDefinition : public USovAurelionMissionDefinition
{
	GENERATED_BODY()
public:
	USovAurelionFireAndFrostMissionDefinition();
};

/** August TDD M13, manuscript chapters 25-26. Terminal authority never implies permission to release. */
UCLASS(BlueprintType, Blueprintable)
class PROJECTVELKORRAN_API USovAurelionContraryWitnessMissionDefinition : public USovAurelionMissionDefinition
{
	GENERATED_BODY()
public:
	USovAurelionContraryWitnessMissionDefinition();
};
