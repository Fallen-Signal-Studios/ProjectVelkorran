// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campaign/SovEncounterTypes.h"
#include "Components/SovCommandLinkComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "NarrativeSavableActor.h"
#include "Campaign/SovCampaignMassTypes.h"
#include "AI/Mass/Peds/NarrativeMassParticipantBridge.h"
#include "MassEntityConfigAsset.h"
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
	/** Explicit campaign eligibility, never inferred from a pedestrian definition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAllowMassRepresentation = false;
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
	UPROPERTY(SaveGame) bool bAllowMassRepresentation = false;
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

USTRUCT()
struct FSovEncounterMassRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FSovEncounterNPCRecord NPC;
	UPROPERTY(SaveGame) FSovCampaignMassState Transfer;
	UPROPERTY(SaveGame) ESovCampaignRepresentationTier Tier = ESovCampaignRepresentationTier::Mass;
	UPROPERTY(SaveGame) TArray<FVector> Route;
	UPROPERTY(SaveGame) int32 NextRoutePoint = 0;
	UPROPERTY(SaveGame) float RouteSpeed = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovEncounterStateChanged, ESovEncounterState, Previous, ESovEncounterState, Current);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovEncounterRestoreFailed, const FString&, Reason);

/**
 * Authoritative encounter entry checkpoint, using Narrative actor/component records.
 * Checkpoints are captured before combat. Loading an active attempt exposes Failed
 * and requires RetryEncounter; it never claims arbitrary midfight physics rollback.
 */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovEncounterDirector : public AActor, public INarrativeSavableActor, public INarrativeMassParticipantOwner
{
	GENERATED_BODY()
public:
	ASovEncounterDirector();
	/** Stable and globally unique (e.g. M01.Courtyard); never rename after shipping saves. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Encounter") FName EncounterId;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Replicated, Category = "Encounter") TArray<FSovEncounterParticipant> Participants;
	/** Registered non-victory NPCs whose death fails this attempt. Kept as actors throughout combat. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Encounter|Protection") TSet<FName> ProtectedParticipantIds;
	/** Current native confirmed-defeat and survivor result; scripted empty completion is not campaign proof. */
	UFUNCTION(BlueprintPure, Category="Encounter") bool HasConfirmedVictory() const;
	uint64 GetLifecycleGeneration() const { return RestoreGeneration; }
	/** Retained by the director even if its objective actor streams out during publication. */
	bool IsCampaignReceiptPending() const { return State == ESovEncounterState::Succeeded && bAwaitingCampaignReceipt; }
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "0")) float CompletionEchoReserve = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter", meta = (ClampMin = "1")) float RestoreTimeoutSeconds = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Encounter") bool bCompleteWhenRequiredParticipantsDefeated = true;
	/** Disable for authored duels and canonical failure segments. Environmental fatal hits never allow rescue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter|Recovery") bool bAllowCompanionRescue = true;

	UFUNCTION(BlueprintPure, Category = "Encounter") ESovEncounterState GetEncounterState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "Encounter") bool HasEncounterPlayer(const AActor* Actor) const;
	ASovPlayerCharacterBase* GetEncounterPlayer() const { return EncounterPlayer; }
	UFUNCTION(BlueprintPure, Category="Encounter") class USovEncounterCoordinationComponent* GetCoordinationComponent() const { return Coordination; }
	/** Save admission may ignore only these registered, frozen entry participants. */
	bool IsEntryCheckpointQuiescentForSave(const ASovPlayerCharacterBase* Player) const;
	UFUNCTION(BlueprintPure, Category = "Encounter") FGuid GetAttemptId() const { return AttemptId; }
	UFUNCTION(BlueprintPure, Category = "Encounter") ASovNPCCharacterBase* GetParticipant(FName ParticipantId) const;
	UFUNCTION(BlueprintPure, Category = "Encounter") FName FindParticipantId(const AActor* Actor) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Encounter") bool RegisterParticipant(FName ParticipantId, ASovNPCCharacterBase* Character, bool bRequiredForVictory = true);
	/** Only explicit director-owned boundaries admit conversion; ordinary distance LOD cannot convert combatants. */
	UPROPERTY(EditAnywhere, Category="Encounter|Mass") TSet<FName> RepresentationBoundaries = { TEXT("Encounter.ControlledBoundary") };
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Encounter|Mass") bool SetParticipantRepresentation(FName ParticipantId, ESovCampaignRepresentationTier Tier, FName Boundary, FString& Error);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Encounter|Mass") bool SetParticipantMassRoute(FName ParticipantId, const TArray<FVector>& Points, float Speed, FString& Error);
	UFUNCTION(BlueprintPure, Category="Encounter|Mass") bool IsParticipantMassRepresented(FName ParticipantId) const;
	UFUNCTION(BlueprintPure, Category="Encounter|Mass") bool IsMassPromotionPending() const { return !MassPromotions.IsEmpty(); }
	virtual bool AcceptMassRepresentation(FMassEntityManager& Manager, FMassEntityHandle Entity, AActor& Actor, const FNarrativeMassParticipantFragment& Identity) override;
	virtual bool IsMassRepresentationCurrent(FMassEntityManager& Manager, FMassEntityHandle Entity, const AActor& Actor, const FNarrativeMassParticipantFragment& Identity) const override;
	virtual void ReleaseMassRepresentation(FMassEntityManager& Manager, FMassEntityHandle Entity, AActor& Actor, const FNarrativeMassParticipantFragment& Identity) override;
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
	virtual ENarrativeRestorePhase GetSaveRestorePhase() const override { return ENarrativeRestorePhase::Encounters; }

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
	UPROPERTY(SaveGame) TSet<FName> EntryProtectedParticipantIds;
	UPROPERTY(SaveGame) bool bAwaitingCampaignReceipt = false;
	UPROPERTY(SaveGame) TSet<FName> ClaimedCompletionRewards;
	UPROPERTY(SaveGame) TSet<FName> DefeatedParticipants;
	UPROPERTY(SaveGame) TSet<FName> ClaimedAttemptRewards;
	UPROPERTY(SaveGame) TArray<FGuid> AttemptActorGuids;

private:
	friend struct FSovCampaignMassTestAccess;
	friend struct FSovEncounterCallbackTestAccess;
	friend struct FSovCoordinationTestAccess;
	friend class ASovCampaignEncounterObjective;
	UPROPERTY(VisibleAnywhere, Category="Encounter") TObjectPtr<class USovEncounterCoordinationComponent> Coordination;
	bool bPlayerAndControllerRestored = false;
	void SetState(ESovEncounterState NewState);
	ASovPlayerCharacterBase* ResolvePlayer() const;
	bool ValidateEntry(FString& Error) const;
	bool CaptureNPC(const FSovEncounterParticipant& Participant, FSovEncounterNPCRecord& OutRecord, FString& Error) const;
	void FinishRestore();
	void AbortRestore(const FString& Error);
	void CleanupAttemptActors();
	bool CleanupAttemptActors(TFunctionRef<bool()> CanContinue);
	void HandleActorSpawned(AActor* Actor);
	void SuspendActor(AActor* Actor);
	void ReleaseSuspensions();
	bool ReleaseSuspensions(TFunctionRef<bool()> CanContinue);
	bool ReleaseActorSuspension(AActor* Actor, TFunctionRef<bool()> CanContinue);
	void RemoveTimedEffects(UAbilitySystemComponent* ASC);
	bool RemoveTimedEffects(UAbilitySystemComponent* ASC, TFunctionRef<bool()> CanContinue);
	UFUNCTION() void HandleDeath(AActor* KilledActor, UNarrativeAbilitySystemComponent* ASC, bool bIsDead);
	void EvaluateCompletionConditions();
	bool AreProtectedParticipantsAlive() const;
	bool HasConfirmedRequiredDefeats() const;
	bool ValidateProtectionConfiguration(FString& Error) const;
	void BindDeaths();
	void UnbindDeaths();
	UPROPERTY(Transient) TObjectPtr<ASovPlayerCharacterBase> EncounterPlayer;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> AttemptActors;
	UPROPERTY(Transient) TArray<TObjectPtr<UAbilitySystemComponent>> SuspendedASCs;
	UPROPERTY(Transient) TArray<TObjectPtr<UBrainComponent>> PausedBrains;
	UPROPERTY(Transient) TArray<TObjectPtr<UNarrativeAbilitySystemComponent>> BoundDeathASCs;
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, TWeakObjectPtr<AActor>> SuspendedAvatars;
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> OwnedBusySuspensions;
	TSet<TWeakObjectPtr<UAbilitySystemComponent>> OwnedProtectionSuspensions;
	TMap<TWeakObjectPtr<UBrainComponent>, TWeakObjectPtr<APawn>> PausedBrainPawns;
	TMap<TWeakObjectPtr<class ANarrativeNPCController>, TWeakObjectPtr<APawn>> SuspendedThreatControllers;
	uint64 RestoreGeneration = 0;
	TWeakObjectPtr<AController> RestoreController;
	TWeakObjectPtr<class ASovPlayerState> RestorePlayerState;
	TWeakObjectPtr<UAbilitySystemComponent> RestorePlayerASC;
	TSet<FName> RestoredParticipants;
	FDelegateHandle ActorSpawnedHandle;
	float RestoreStartedAt = 0.f;
	bool bMutationInProgress = false;
	bool bInvalidEncounterIdentity = false;
	/** Current C/D state shares this director's checkpoint record/serialization authority. Never saved on a proxy. */
	UPROPERTY(SaveGame) TArray<FSovEncounterMassRecord> MassParticipants;
	UPROPERTY(Transient) FMassEntityConfig MassConfig;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<ASovNPCCharacterBase>> MassPromotions;
	TMap<FName, FMassEntityHandle> MassEntities;
	TMap<FName, TWeakObjectPtr<AActor>> MassProxies;
	TMap<FName, uint64> MassEpochs;
	TMap<FName, double> MassPromotionStarts;
	uint64 NextMassEpoch = 0;
	bool CreateMassEntity(FSovEncounterMassRecord& Record, FString& Error);
	void DestroyMassEntity(FName Id);
	void ClearMassRepresentations(bool bDiscardRecords);
	void CaptureMassTransforms();
	void RefreshMassProcessingState();
	void TickMassPromotions();
	bool CaptureMassTransfer(ASovNPCCharacterBase* NPC, FSovCampaignMassState& Transfer, FString& Error) const;
	bool RestoreMassTransfer(ASovNPCCharacterBase* NPC, const FSovEncounterMassRecord& Record, FString& Error);
};
