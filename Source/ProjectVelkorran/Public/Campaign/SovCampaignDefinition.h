// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SovCampaignDefinition.generated.h"

class ASovPlayerCharacterBase;
class UPlayerDefinition;
class UNarrativeDataTask;

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCampaignStateWrite
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag Key;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag Value;
	/** Once set, a protected fact cannot be changed by a later local choice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCanonProtected = false;
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCampaignBeatDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BeatId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText ObjectiveText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> PrerequisiteBeats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer RequiredKnowledge;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSovCampaignStateWrite> RequiredState;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer GrantedKnowledge;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSovCampaignStateWrite> StateWrites;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOptional = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCanonGate = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CinematicId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInteractiveChoice = false;
	/** Requires a native companion arrival receipt; generic CompleteBeat cannot grant this beat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresCoActionProof = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RequiredCompanionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RequiredCoActionAnchorId;
	/** Optional bridge into existing Narrative quest graphs after native state commits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UNarrativeDataTask> CompletionTask;
};

/** Authored mission content over Narrative's existing quest/save framework. */
UCLASS(BlueprintType, Blueprintable)
class PROJECTVELKORRAN_API USovCampaignDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") int32 SchemaVersion = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") FName MissionId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") FGameplayTag Protagonist;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TSoftClassPtr<ASovPlayerCharacterBase> PawnClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TSoftObjectPtr<UPlayerDefinition> PlayerDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TSoftObjectPtr<UWorld> Map;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") FName EntryPlayerStartTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TArray<FName> AllowedSuccessorMissions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TArray<FSovCampaignBeatDefinition> Beats;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="0",ClampMax="100")) float EntryEchoReserve = 25.f;

	UFUNCTION(BlueprintPure, Category="Campaign") bool ValidateDefinition(FString& OutError) const;
	const FSovCampaignBeatDefinition* FindBeat(FName BeatId) const;
};

/** Native opening schema; create an asset child and assign actual definition/map/content. */
UCLASS(BlueprintType, Blueprintable)
class PROJECTVELKORRAN_API USovMantleMissionDefinition : public USovCampaignDefinition
{
	GENERATED_BODY()
public:
	USovMantleMissionDefinition();
};

UCLASS(BlueprintType, Blueprintable)
class PROJECTVELKORRAN_API USovOneDegreeMissionDefinition : public USovCampaignDefinition
{
	GENERATED_BODY()
public:
	USovOneDegreeMissionDefinition();
};
