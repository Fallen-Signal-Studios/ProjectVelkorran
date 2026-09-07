// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Campaign/SovEncounterTypes.h"
#include "SovCampaignEncounterObjective.generated.h"

class ASovEncounterDirector;
class ASovCampaignRelayReceiver;
class ASovPlayerCharacterBase;
class ASovPlayerController;
class UNarrativeAbilitySystemComponent;
class USovCampaignDefinition;
class USovCampaignStateComponent;
class UBoxComponent;

/** Binds one native encounter to one typed campaign objective. Narrative's director checkpoint
 * and campaign journal remain the only save owners. Loaded success never manufactures a receipt. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovCampaignEncounterObjective : public AActor
{
    GENERATED_BODY()
public:
    ASovCampaignEncounterObjective();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") TObjectPtr<UBoxComponent> StartVolume;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") TObjectPtr<ASovEncounterDirector> EncounterDirector;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") FName MissionId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") FName CompletionBeat;
    /** Exact physical actors matching the beat's RequiredReceiverIds. Both E2 receivers must be reachable on foot. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign|Receivers") TArray<TObjectPtr<ASovCampaignRelayReceiver>> RequiredReceivers;
    /** Re-entering the volume after a failed attempt requests the existing director's entry retry. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") bool bStartOnPlayerOverlap = false;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Campaign") FString LastError;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign") bool StartEncounter(ASovPlayerCharacterBase* Player, FString& Error);
    /** Save admission holds an uncommitted victory, including a retired authority context. */
    UFUNCTION(BlueprintPure, Category="Campaign") bool IsResultPending() const;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    friend class USovCampaignStateComponent;
    friend class ASovCampaignRelayReceiver;
    struct FAttemptContext
    {
        TWeakObjectPtr<ASovPlayerCharacterBase> Player;
        TWeakObjectPtr<ASovPlayerController> Controller;
        TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC;
        TWeakObjectPtr<USovCampaignStateComponent> State;
        TWeakObjectPtr<USovCampaignDefinition> Mission;
        TWeakObjectPtr<ASovEncounterDirector> Director;
        FGuid AttemptId;
        FName MissionId, BeatId, EncounterId;
        TSet<FName> ProtectedIds;
        TMap<FName, TWeakObjectPtr<ASovCampaignRelayReceiver>> Receivers;
        TSet<FName> DisabledReceiverIds;
        int32 ReadyEpoch = 0;
        uint64 ActorInfoEpoch = 0, TransitionEpoch = 0, DirectorGeneration = 0;
    };
    bool ValidateContext(ASovPlayerCharacterBase* Player, FString& Error) const;
    bool OwnsAttempt(const FAttemptContext& Context, ESovEncounterState ExpectedState) const;
    bool HasCommitReceipt(const USovCampaignStateComponent* State, FName BeatId) const;
    void AcknowledgeCommitReceipt(const USovCampaignStateComponent* State);
    FGuid GetReceiptAttemptId() const { return Attempt.AttemptId; }
    TSet<FName> GetDisabledReceiverReceiptIds() const { return Attempt.DisabledReceiverIds; }
    FGuid GetReceiverAttemptId() const { return Attempt.AttemptId; }
    bool ValidateReceiverConfiguration(const TSet<FName>& RequiredIds, FString& Error, bool bCheckFrozen) const;
    bool HasRequiredReceiverProof() const;
    bool HasReceiverDisabled(const ASovCampaignRelayReceiver* Receiver) const;
    bool CanDisableReceiver(const ASovCampaignRelayReceiver* Receiver, const ASovPlayerCharacterBase* Player, FString& Error) const;
    bool AcceptReceiverDisable(ASovCampaignRelayReceiver* Receiver, const FGuid& AttemptId);
    void QueueVictoryIfReady();
    void BindDirector();
    void RetireAttempt();
    void CommitVictory(FAttemptContext Context);
    UFUNCTION() void HandleEncounterState(ESovEncounterState Previous, ESovEncounterState Current);
    UFUNCTION() void HandleCampaignRestored(bool bValid);
    UFUNCTION() void HandleMissionChanged(FName ChangedMissionId, bool bSucceeded);
    UFUNCTION() void HandleStartOverlap(UPrimitiveComponent* Component, AActor* Actor, UPrimitiveComponent* OtherComponent,
        int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
    TWeakObjectPtr<ASovEncounterDirector> BoundDirector;
    TWeakObjectPtr<USovCampaignStateComponent> BoundCampaign;
    FAttemptContext Attempt;
    FTimerHandle ResultTimer;
    bool bPending = false, bExecuting = false, bStarting = false, bEnding = false;
};
