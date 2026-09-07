// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Campaign/SovCampaignProtagonistProfile.h"
#include "Resonance/SovResonanceTypes.h"
#include "Campaign/SovNarrativeTypes.h"
#include "Campaign/SovObjectiveTypes.h"
#include "Campaign/SovEncounterTypes.h"
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
	/** An authored cut between separate approaches; it never creates a protagonist companion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsolatedPerspectiveCut = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText ObjectiveText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESovObjectiveType ObjectiveType = ESovObjectiveType::General;
	/** Failure is opt-in, optional-only, and must explain its rule before the objective is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName FailureReasonId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText FailureRuleText;
	/** Exactly one optional outcome in a declared group can complete. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ChoiceGroupId;
	/** Mandatory reconvergence waits for an accepted outcome, never for every optional alternative. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> RequiredChoiceGroups;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> PrerequisiteBeats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer RequiredKnowledge;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSovCampaignStateWrite> RequiredState;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer GrantedKnowledge;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSovCampaignStateWrite> StateWrites;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSovConsequenceDefinition> Consequences;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FSovRelationshipMemoryDefinition> RelationshipMemories;
	/** Guaranteed observation at this mandatory critical-path beat; never dependent on finding an optional world source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<USovEvidenceDefinition>> CriticalEvidence;
	/** Same-scene canonical observers, proven by the controlled pawn and registered protagonist companion. Empty preserves lead-only observation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> CriticalEvidenceObserverIds;
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
	/** Native encounter victory receipt. Empty for ordinary beats; generic completion cannot satisfy this ID. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RequiredEncounterId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESovEncounterProofType RequiredEncounterProof = ESovEncounterProofType::RequiredDefeats;
	/** Exact authored receiver IDs, proven by native receiver components during this encounter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSet<FName> RequiredReceiverIds;
	/** Minimum distinct protected participants in the encounter receipt; faction backgrounds remain authored content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 MinimumProtectedParticipants = 0;
	/** Optional bridge into existing Narrative quest graphs after native state commits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UNarrativeDataTask> CompletionTask;
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCampaignChoiceGroup
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName GroupId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ReconciliationBeatId;
	/** Explicit explanation of how every legal local outcome rejoins the fixed campaign spine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText ReconciliationNote;
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") TArray<FSovCampaignChoiceGroup> ChoiceGroups;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="0",ClampMax="100")) float EntryEchoReserve = 25.f;
	/** Explicit convergence opt-in. Only authored M12/M13 definitions may enable it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") bool bAllowJointResonance = false;
	/** Optional mandatory handoff that first establishes the protagonist partnership. Empty starts together. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") FName CompanionActivationBeat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") TArray<ESovResonanceType> AllowedResonanceTypes;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") TArray<FName> ResonancePrerequisiteBeats;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") TArray<FName> AllowedCompanionIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign|Companions") TArray<FSovCampaignCompanionProfile> ProtagonistCompanions;
	const FSovCampaignCompanionProfile* FindCompanionProfile(FGameplayTag Identity) const;

	UFUNCTION(BlueprintPure, Category="Campaign") virtual bool ValidateDefinition(FString& OutError) const;
	const FSovCampaignBeatDefinition* FindBeat(FName BeatId) const;
	const FSovCampaignChoiceGroup* FindChoiceGroup(FName GroupId) const;
	bool ValidateObjectives(FString& OutError) const;
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
