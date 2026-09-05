// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "Campaign/SovNarrativeTypes.h"
#include "SovCampaignNarrativeAdapters.generated.h"
class UTreePerk;
class UNarrativeDataTask;
class USovCampaignDefinition;

UENUM(BlueprintType)
enum class ESovCampaignQuery : uint8 { BeatComplete, MissionComplete, Knowledge, EvidenceStage, EvidenceKnownBy, ConsequenceKnown, ConsequencePayload, RelationshipMemory, CompanionAvailable, TechniqueOwned, NarrativeTaskCompleted };

/** Immutable native queries for existing Narrative dialogue nodes; no parallel dialogue graph or morality score. */
UCLASS(EditInlineNew, BlueprintType)
class PROJECTVELKORRAN_API USovCampaignNarrativeCondition : public UNarrativeCondition
{
	GENERATED_BODY()
public:
	USovCampaignNarrativeCondition();
	UPROPERTY(EditAnywhere, Category="Campaign") ESovCampaignQuery Query = ESovCampaignQuery::BeatComplete;
	UPROPERTY(EditAnywhere, Category="Campaign") FName MissionId;
	UPROPERTY(EditAnywhere, Category="Campaign") FName RecordId;
	/** Empty uses the actual active protagonist's narrative identity. */
	UPROPERTY(EditAnywhere, Category="Campaign") FName ObserverId;
	UPROPERTY(EditAnywhere, Category="Campaign") FName SubjectId;
	UPROPERTY(EditAnywhere, Category="Campaign") FGameplayTagContainer RequiredKnowledge;
	UPROPERTY(EditAnywhere, Category="Campaign") ESovEvidenceStage MinimumEvidenceStage = ESovEvidenceStage::Observed;
	UPROPERTY(EditAnywhere, Category="Campaign") ESovRelationshipMemoryType MemoryType = ESovRelationshipMemoryType::TrustGiven;
	UPROPERTY(EditAnywhere, Category="Campaign") FSovConsequencePayload RequiredPayload;
	UPROPERTY(EditAnywhere, Category="Campaign") TSubclassOf<UTreePerk> RequiredTechnique;
	/** Existing Narrative completion history, including authored conversation-end tasks. */
	UPROPERTY(EditAnywhere, Category="Campaign") TObjectPtr<UNarrativeDataTask> RequiredNarrativeTask;
	bool ValidateConfiguration(const USovCampaignDefinition* Mission, FString& OutError) const;
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};

UENUM(BlueprintType)
enum class ESovCampaignNarrativeAction : uint8 { CompleteBeat, AcquireEvidence };

/** Narrative End event adapter; native commit remains authority and repeated execution is idempotent. */
UCLASS(EditInlineNew, BlueprintType)
class PROJECTVELKORRAN_API USovCampaignNarrativeEvent : public UNarrativeEvent
{
	GENERATED_BODY()
public:
	USovCampaignNarrativeEvent(const FObjectInitializer& ObjectInitializer);
	UPROPERTY(EditAnywhere, Category="Campaign") ESovCampaignNarrativeAction Action = ESovCampaignNarrativeAction::CompleteBeat;
	UPROPERTY(EditAnywhere, Category="Campaign") FName BeatId;
	/** Exact unique authored placed-source identity. A raw evidence ID cannot manufacture acquisition. */
	UPROPERTY(EditAnywhere, Category="Campaign") FGuid EvidenceSourceId;
	bool ValidateConfiguration(const USovCampaignDefinition* Mission, FString& OutError) const;
	virtual void ExecuteEvent_Implementation(APawn* Target, APlayerController* Controller, UTalesComponent* NarrativeComponent) override;
	virtual FString GetGraphDisplayText_Implementation() override;
};
