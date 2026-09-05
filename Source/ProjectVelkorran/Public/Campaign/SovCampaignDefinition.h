// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Campaign/SovCampaignProtagonistProfile.h"
#include "Resonance/SovResonanceTypes.h"
#include "Campaign/SovNarrativeTypes.h"
#include "SovCampaignDefinition.generated.h"

class ASovPlayerCharacterBase;
class UPlayerDefinition;
class UNarrativeDataTask;
class USovEvidenceDefinition;

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
	/** Empty uses the current authored lead; set for a protagonist-specific beat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag RequiredProtagonist;
	/** Native handoff-only beat. Generic CompleteBeat cannot produce a protagonist transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag HandoffToProtagonist;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RequiredHandoffAnchorId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText ObjectiveText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> PrerequisiteBeats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer RequiredKnowledge;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSovCampaignStateWrite> RequiredState;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer GrantedKnowledge;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSovCampaignStateWrite> StateWrites;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSovConsequenceDefinition> Consequences;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSovRelationshipMemoryDefinition> RelationshipMemories;
	/** Guaranteed observation at this mandatory critical-path beat; never dependent on finding an optional world source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<USovEvidenceDefinition>> CriticalEvidence;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOptional = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCanonGate = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CinematicId;
	/** Native playback receipt and typed postconditions are required; arbitrary CompleteBeat cannot skip presentation work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresCinematicProof = false;
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TArray<FSovCampaignProtagonistProfile> AlternateProtagonists;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") bool bCompletesCampaign = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TSoftClassPtr<ASovPlayerCharacterBase> PawnClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TSoftObjectPtr<UPlayerDefinition> PlayerDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TSoftObjectPtr<UWorld> Map;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") FName EntryPlayerStartTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TArray<FName> AllowedSuccessorMissions;
	/** Concrete prior facts consumed by this mission, verified before entry and by the shipping manifest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TArray<FName> RequiredPriorConsequenceIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TArray<FSovCampaignBeatDefinition> Beats;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="0",ClampMax="100")) float EntryEchoReserve = 25.f;
	/** Explicit convergence opt-in. Only authored M12/M13 definitions may enable it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") bool bAllowJointResonance = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") TArray<ESovResonanceType> AllowedResonanceTypes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") TArray<FName> ResonancePrerequisiteBeats;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") TArray<FName> AllowedCompanionIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") TArray<FSovCampaignCompanionProfile> ProtagonistCompanions;
	const FSovCampaignCompanionProfile* FindCompanionProfile(FGameplayTag Identity) const;

	UFUNCTION(BlueprintPure, Category="Campaign") bool ValidateDefinition(FString& OutError) const;
	const FSovCampaignBeatDefinition* FindBeat(FName BeatId) const;
	bool SupportsProtagonist(FGameplayTag Lead) const;
	TSoftClassPtr<ASovPlayerCharacterBase> ResolvePawnClass(FGameplayTag Lead) const;
	TSoftObjectPtr<UPlayerDefinition> ResolvePlayerDefinition(FGameplayTag Lead) const;
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
