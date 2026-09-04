// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campaign/SovEncounterTypes.h"
#include "Components/SovCommandLinkComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "NarrativeSavableActor.h"
#include "SovEncounterDirector.generated.h"

class ASovNPCCharacterBase;
class ASovPlayerCharacterBase;
class ASovPlayerState;
class UAbilitySystemComponent;
class UBrainComponent;

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovEncounterParticipant
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ParticipantId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<ASovNPCCharacterBase> Character = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiredForVictory = true;
};

USTRUCT()
struct FSovEncounterLinkRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FName ComponentName;
	UPROPERTY(SaveGame) FSovCommandLinkSnapshot State;
	UPROPERTY(SaveGame) FName SourceParticipantId;
	UPROPERTY(SaveGame) TArray<FName> LinkedParticipantIds;
};

USTRUCT()
struct FSovEncounterNPCRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FName ParticipantId;
	UPROPERTY(SaveGame) bool bRequiredForVictory = true;
	UPROPERTY(SaveGame) FNarrativeActorRecord ActorRecord;
	UPROPERTY(SaveGame) TSoftObjectPtr<UNPCDefinition> Definition;
	UPROPERTY(SaveGame) FNPCSpawnInfo SpawnInfo;
	/** Narrative's FNPCSpawnParams fields lack SaveGame flags; retain authored overrides explicitly. */
	UPROPERTY(SaveGame) TArray<uint8> SpawnInfoData;
	UPROPERTY(SaveGame) FSovCombatResourceSnapshot Resources;
	UPROPERTY(SaveGame) FSovWeakPointStateSnapshot WeakPoints;
	UPROPERTY(SaveGame) bool bHasWeakPoints = false;
	UPROPERTY(SaveGame) int32 SeveredRegionMask = 0;
	UPROPERTY(SaveGame) FGameplayTagContainer WieldEquipSlots;
	UPROPERTY(SaveGame) FGameplayTagContainer WieldSlots;
	UPROPERTY(SaveGame) TArray<FSovEncounterLinkRecord> Links;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovEncounterStateChanged, ESovEncounterState, Previous, ESovEncounterState, Current);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovEncounterRestoreFailed, const FString&, Reason);

/**
 * Authoritative encounter entry checkpoint, using Narrative actor/component records.
 * Checkpoints are captured before combat. Loading an active attempt exposes Failed
 * and requires RetryEncounter; it never claims arbitrary midfight physics rollback.
 */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovEncounterDirector : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()
public:
	ASovEncounterDirector();
	/** Stable and globally unique (e.g. M01.Courtyard); never rename after shipping saves. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter") FName EncounterId;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Replicated, Category = "Encounter") TArray<FSovEncounterParticipant> Participants;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "0")) float CompletionEchoReserve = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "1")) float RestoreTimeoutSeconds = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter") bool bCompleteWhenRequiredParticipantsDefeated = true;

	UFUNCTION(BlueprintPure, Category = "Encounter") ESovEncounterState GetEncounterState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "Encounter") FGuid GetAttemptId() const { return AttemptId; }
	UFUNCTION(BlueprintPure, Category = "Encounter") ASovNPCCharacterBase* GetParticipant(FName ParticipantId) const;
	UFUNCTION(BlueprintPure, Category = "Encounter") FName FindParticipantId(const AActor* Actor) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Encounter") bool RegisterParticipant(FName ParticipantId, ASovNPCCharacterBase* Character, bool bRequiredForVictory = true);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Encounter") bool CaptureEntryCheckpoint(ASovPlayerCharacterBase* Player, FString& Error);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Encounter") bool BeginEncounter();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Encounter") bool CompleteEncounter();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Encounter") bool FailEncounter();
	/** Starts asynchronous NPC reinitialization; State becomes Active only after every record succeeds. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Encounter") bool RetryEncounter(FString& Error);
	/** Server reward handlers must claim a stable key before granting a completion reward. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Encounter") bool ClaimCompletionReward(FName RewardId);
	/** Native validated reward sources only; consumed once across all gates in this attempt. */
	bool ClaimAttemptReward(FName RewardId);
	/** Explicit attribution for delayed custom spawns whose Owner was unavailable at SpawnActor time. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Encounter") bool RegisterAttemptActor(AActor* SpawnedActor);
	UPROPERTY(BlueprintAssignable, Category = "Encounter") FSovEncounterStateChanged OnEncounterStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Encounter") FSovEncounterRestoreFailed OnEncounterRestoreFailed;
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& SavedGUID) override;
	virtual bool ShouldRespawn_Implementation() const override { return false; }
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION() void OnRep_State(ESovEncounterState Previous);
	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_State) ESovEncounterState State = ESovEncounterState::Inactive;
	UPROPERTY(SaveGame, Replicated) FGuid AttemptId;
	UPROPERTY(SaveGame) int32 SnapshotSchemaVersion = 1;
	UPROPERTY(SaveGame) bool bHasEntryCheckpoint = false;
	UPROPERTY(SaveGame) FSovProtagonistSnapshot EntryPlayer;
	UPROPERTY(SaveGame) FNarrativeActorRecord EntryController;
	UPROPERTY(SaveGame) TArray<FSovEncounterNPCRecord> EntryParticipants;
	UPROPERTY(SaveGame) TSet<FName> ClaimedCompletionRewards;
	UPROPERTY(SaveGame) TSet<FName> DefeatedParticipants;
	UPROPERTY(SaveGame) TSet<FName> ClaimedAttemptRewards;
	UPROPERTY(SaveGame) TArray<FGuid> AttemptActorGuids;

private:
	void SetState(ESovEncounterState NewState);
	ASovPlayerCharacterBase* ResolvePlayer() const;
	bool ValidateEntry(FString& Error) const;
	bool CaptureNPC(const FSovEncounterParticipant& Participant, FSovEncounterNPCRecord& OutRecord, FString& Error) const;
	void FinishRestore();
	void AbortRestore(const FString& Error);
	void CleanupAttemptActors();
	void HandleActorSpawned(AActor* Actor);
	void SuspendActor(AActor* Actor);
	void ReleaseSuspensions();
	void RemoveTimedEffects(UAbilitySystemComponent* ASC);
	UFUNCTION() void HandleDeath(AActor* KilledActor, UNarrativeAbilitySystemComponent* ASC, bool bIsDead);
	void BindDeaths();
	void UnbindDeaths();
	UPROPERTY(Transient) TObjectPtr<ASovPlayerCharacterBase> EncounterPlayer;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> AttemptActors;
	UPROPERTY(Transient) TArray<TObjectPtr<UAbilitySystemComponent>> SuspendedASCs;
	UPROPERTY(Transient) TArray<TObjectPtr<UBrainComponent>> PausedBrains;
	UPROPERTY(Transient) TArray<TObjectPtr<UNarrativeAbilitySystemComponent>> BoundDeathASCs;
	TSet<FName> RestoredParticipants;
	FDelegateHandle ActorSpawnedHandle;
	float RestoreStartedAt = 0.f;
	bool bMutationInProgress = false;
	bool bInvalidEncounterIdentity = false;
};
