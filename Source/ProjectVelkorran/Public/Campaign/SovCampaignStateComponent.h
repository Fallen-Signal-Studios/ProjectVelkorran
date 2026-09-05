// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovEvidenceDefinition.h"
#include "NarrativeSavableComponent.h"
#include "SovCampaignStateComponent.generated.h"

UENUM(BlueprintType)
enum class ESovCampaignResult : uint8
{
	Applied, AlreadyApplied, NotAuthority, Busy, Invalid, PrerequisiteMissing,
	KnowledgeMissing, ProtectedStateConflict, SkipUnavailable
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCampaignMissionRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FName> CompletedBeats;
	UPROPERTY(SaveGame, BlueprintReadOnly) bool bSucceeded = false;
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCampaignKnowledgeRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) FGameplayTagContainer Knowledge;
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCampaignJournalEntry
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) FGuid EventId;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 Sequence = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName MissionId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName BeatId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGameplayTag Protagonist;
	UPROPERTY(SaveGame, BlueprintReadOnly) bool bPresentationSkipped = false;
	UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FSovConsequenceRecord> Consequences;
	UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FSovRelationshipMemoryDefinition> RelationshipMemories;
	/** Empty for ordinary beats; a co-action receipt persists with the committed fact. */
	UPROPERTY(SaveGame, BlueprintReadOnly) FGuid HandoffRequestId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGameplayTag HandoffToProtagonist;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName HandoffAnchorId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGuid CinematicSessionId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGuid CoActionRequestId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName CoActionCompanionId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName CoActionAnchorId;
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovEvidenceAcquisition
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) FName EvidenceId;
	UPROPERTY(SaveGame, BlueprintReadOnly) TObjectPtr<USovEvidenceDefinition> Definition;
	UPROPERTY(SaveGame, BlueprintReadOnly) ESovEvidenceStage Stage = ESovEvidenceStage::Observed;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGuid CriticalBeatEventId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName SourceLocationId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName CustodianId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName SupportingEvidenceId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName CopyDestination;
	UPROPERTY(SaveGame, BlueprintReadOnly) ESovRecordPublicity Publicity = ESovRecordPublicity::Private;
	UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FName> WitnessIds;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGuid SourceId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGameplayTag Protagonist;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName MissionId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName AcquisitionBeat;
	/** Exact native grant and journal position captured at acquisition, used for restore validation. */
	UPROPERTY(SaveGame, BlueprintReadOnly) FGameplayTagContainer GrantedKnowledge;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 AfterJournalSequence = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovCampaignBeatCommitted, const FSovCampaignJournalEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovCampaignMissionChanged, FName, MissionId, bool, bSucceeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovCampaignStateRestored, bool, bValid);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovEvidenceRecorded, const FSovEvidenceAcquisition&, Evidence);

/** Durable, validated campaign boundary. Narrative still owns quests, dialogue and disk storage. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCampaignStateComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()
public:
	USovCampaignStateComponent();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign")
	ESovCampaignResult BeginMission(USovCampaignDefinition* Definition);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign")
	ESovCampaignResult CompleteBeat(FName BeatId, bool bSkipPresentation = false);
	/** Call from successful full Sequence playback only. A skipped/aborted play does not grant permission. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign|Cinematics")
	bool RecordCinematicViewed(FName BeatId);
	UFUNCTION(BlueprintPure, Category="Campaign|Cinematics") bool CanSkipCinematic(FName BeatId) const;
	UFUNCTION(BlueprintPure, Category="Campaign") bool IsBeatComplete(FName MissionId, FName BeatId) const;
	UFUNCTION(BlueprintPure, Category="Campaign") bool IsMissionComplete(FName MissionId) const;
	UFUNCTION(BlueprintPure, Category="Campaign") bool CanEnterMission(const USovCampaignDefinition* Definition) const;
	UFUNCTION(BlueprintPure, Category="Campaign") bool HasKnowledge(FGameplayTag Protagonist, const FGameplayTagContainer& Required) const;
	UFUNCTION(BlueprintPure, Category="Campaign") FGameplayTag GetStateValue(FGameplayTag Key) const;
	UFUNCTION(BlueprintPure, Category="Campaign") USovCampaignDefinition* GetActiveMission() const { return ActiveMission; }
	bool IsMutationInProgress() const { return bMutating; }
	UFUNCTION(BlueprintPure, Category="Campaign") bool IsStateValid() const { return bStateValid; }
	UFUNCTION(BlueprintPure, Category="Campaign") const TArray<FSovCampaignJournalEntry>& GetJournal() const { return Journal; }
	UFUNCTION(BlueprintPure, Category="Campaign|Evidence") bool KnowsEvidence(FName EvidenceId, FGameplayTag Protagonist) const;
	UFUNCTION(BlueprintPure, Category="Campaign|Evidence") ESovEvidenceStage GetEvidenceStage(FName EvidenceId, FGameplayTag Protagonist) const;
	UFUNCTION(BlueprintPure, Category="Campaign|Evidence") bool HasEvidenceCopy(FName EvidenceId, FName DestinationId) const;
	UFUNCTION(BlueprintPure, Category="Campaign|Evidence") bool ObserverKnowsEvidence(FName EvidenceId, FName ObserverId) const;
	UFUNCTION(BlueprintPure, Category="Campaign|Consequence") bool FindConsequence(FName ConsequenceId, FName ObserverId, FSovConsequenceRecord& OutRecord) const;
	UFUNCTION(BlueprintPure, Category="Campaign|Consequence") bool HasRelationshipMemory(FName HolderId, FName SubjectId, ESovRelationshipMemoryType Type, FName ConsequenceId = NAME_None) const;
	UFUNCTION(BlueprintPure, Category="Campaign|Evidence") const TArray<FSovEvidenceAcquisition>& GetEvidence() const { return Evidence; }
	/** Source component is the only acquisition entry; a raw ID is never sufficient proof. */
	ESovCampaignResult AcquireEvidence(class USovEvidenceSourceComponent* Source);

	UFUNCTION(BlueprintPure, Category="Campaign") FGameplayTag GetActiveProtagonist() const;
	static bool ValidateSerializedSave(const TArray<uint8>& Bytes, FString& OutError);
	static bool GetSerializedActiveProtagonist(const TArray<uint8>& Bytes, FGameplayTag& OutProtagonist, FString& OutError);
	virtual void Serialize(FArchive& Ar) override;
	virtual ENarrativeRestorePhase GetSaveRestorePhase() const override { return ENarrativeRestorePhase::World; }
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	UPROPERTY(BlueprintAssignable, Category="Campaign") FSovCampaignBeatCommitted OnBeatCommitted;
	UPROPERTY(BlueprintAssignable, Category="Campaign") FSovCampaignMissionChanged OnMissionChanged;
	UPROPERTY(BlueprintAssignable, Category="Campaign") FSovCampaignStateRestored OnCampaignStateRestored;
	UPROPERTY(BlueprintAssignable, Category="Campaign|Evidence") FSovEvidenceRecorded OnEvidenceRecorded;
private:
	friend struct FSovCampaignStateTestAccess;
	friend struct FSovNarrativeStateTestAccess;
	friend struct FSovCoActionTestAccess;
	friend class ASovCoActionAnchor;
	friend class ASovPlayerController;
	friend class USovCampaignCinematicComponent;
	ESovCampaignResult CompleteCinematic(class USovCampaignCinematicComponent* Source, bool bSkipped);
	ESovCampaignResult CompleteAuthoredHandoff(FName BeatId, const FGuid& RequestId);
	ESovCampaignResult CompleteCoAction(class ASovCoActionAnchor* Source);
	ESovCampaignResult CompleteBeatInternal(FName BeatId, bool bSkipPresentation, class ASovCoActionAnchor* CoActionSource, const FGuid& HandoffRequestId = FGuid(), class USovCampaignCinematicComponent* CinematicSource = nullptr);
	bool ValidateSavedState() const;
	bool HasAuthorityOwner() const;
	bool DoesCurrentPawnMatch(FGameplayTag Protagonist) const;
	bool StateWritesValid(const TArray<FSovCampaignStateWrite>& Writes) const;
	static FName NarrativeIdentity(FGameplayTag Protagonist);
	static bool ValidateEvidenceStep(const FSovEvidenceAcquisition& Candidate, const TArray<FSovEvidenceAcquisition>& Prior);
	static ESovEvidenceStage EvidenceStageIn(const TArray<FSovEvidenceAcquisition>& History, FName EvidenceId, FGameplayTag Hero);
	static bool EvidenceKnownTo(const TArray<FSovEvidenceAcquisition>& History, FName EvidenceId, FName Observer);
	UPROPERTY(SaveGame) int32 SavedSchemaVersion = 1;
	UPROPERTY(SaveGame) TObjectPtr<USovCampaignDefinition> ActiveMission;
	UPROPERTY(SaveGame) FGameplayTag ActiveProtagonist;
	UPROPERTY(SaveGame) TMap<FName, FSovCampaignMissionRecord> Missions;
	UPROPERTY(SaveGame) TMap<FName, TObjectPtr<USovCampaignDefinition>> MissionDefinitions;
	UPROPERTY(SaveGame) TMap<FGameplayTag, FSovCampaignKnowledgeRecord> CharacterKnowledge;
	UPROPERTY(SaveGame) TMap<FGameplayTag, FGameplayTag> StateValues;
	UPROPERTY(SaveGame) FGameplayTagContainer ProtectedStateKeys;
	UPROPERTY(SaveGame) TArray<FName> ViewedCinematics;
	UPROPERTY(SaveGame) TArray<FSovCampaignJournalEntry> Journal;
	UPROPERTY(SaveGame) TArray<FSovEvidenceAcquisition> Evidence;
	bool bStateValid = true;
	bool bMutating = false;
};
