// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SovNarrativeTypes.generated.h"

UENUM(BlueprintType)
enum class ESovEvidenceStage : uint8 { Unknown, Observed, Questioned, Corroborated, Authenticated, Distributed };
UENUM(BlueprintType)
enum class ESovRecordPublicity : uint8 { Private, Shared, Institutional, Public };
UENUM(BlueprintType)
enum class ESovConsequenceCanonClass : uint8 { Optional, Variable, FixedPresentation };
UENUM(BlueprintType)
enum class ESovConsequencePersistence : uint8 { Encounter, Mission, Act, Campaign, SequelExportCandidate };
UENUM(BlueprintType)
enum class ESovConsequencePayloadType : uint8 { Count, Reference, Tag };
UENUM(BlueprintType)
enum class ESovRelationshipMemoryType : uint8 { TrustGiven, TrustWithheld, Protection, PublicExposure, ContradictionAcknowledged, BoundaryCrossed };
UENUM(BlueprintType)
enum class ESovKnowledgeMethod : uint8 { Witnessed, Learned, Inferred, Told };

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovConsequencePayload
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FName Key;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) ESovConsequencePayloadType Type = ESovConsequencePayloadType::Reference;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) int32 Count = 0;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FName Reference;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FGameplayTag Tag;
	bool IsValid() const;
	bool operator==(const FSovConsequencePayload& Other) const;
};
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovConsequenceDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FName ConsequenceId;
	/** Empty records the actual active protagonist; a named companion/system must be explicitly authored. */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FName InstigatorId;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) TArray<FName> SubjectIds;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FGameplayTag ChoiceTag;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FGameplayTag OutcomeTag;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) TArray<FName> WitnessIds;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) ESovRecordPublicity Publicity = ESovRecordPublicity::Private;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) TArray<FSovConsequencePayload> Payload;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) ESovConsequenceCanonClass CanonClass = ESovConsequenceCanonClass::Variable;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) ESovConsequencePersistence Persistence = ESovConsequencePersistence::Campaign;
	/** Archival records are a deliberate content choice; otherwise declare at least one downstream consumer ID. */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) bool bArchivalOnly = false;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) TArray<FName> ConsumerIds;
	bool Validate(FString& OutError) const;
	bool operator==(const FSovConsequenceDefinition& Other) const;
};
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovConsequenceRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) FSovConsequenceDefinition Definition;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName MissionId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName BeatId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName ResolvedInstigatorId;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 Sequence = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly) double PlaySeconds = 0.0;
};
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovRelationshipMemoryDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FName MemoryId;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FName HolderId;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FName SubjectId;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) FName ConsequenceId;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) ESovRelationshipMemoryType Type = ESovRelationshipMemoryType::TrustGiven;
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadOnly) ESovKnowledgeMethod LearnedThrough = ESovKnowledgeMethod::Witnessed;
	bool Validate(FString& OutError) const;
	bool operator==(const FSovRelationshipMemoryDefinition& Other) const;
};
