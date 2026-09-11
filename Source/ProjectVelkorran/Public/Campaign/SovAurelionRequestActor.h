// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableComponent.h"
#include "SovAurelionRequestActor.generated.h"

class ASovPlayerCharacterBase;
class ASovPlayerController;
class ASovAurelionStorySequenceActor;
class ASovCampaignHandoffAnchor;
class ASovCoActionAnchor;
class ASovEncounterDirector;
class ASovCampaignEncounterObjective;
class ASovProtagonistCompanionCharacter;
class USovAurelionThermalFractureComponent;
class USovCampaignDefinition;
class UNarrativeAbilitySystemComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ESovAurelionRequest : uint8 { PlayScene, Handoff, CoAction, MoveFrostPartner, FrostSetup, HeatConfirm, TravelToMission, RetryEncounter };

UCLASS()
class PROJECTVELKORRAN_API USovAurelionRequestInteractable : public UNarrativeInteractableComponent
{
    GENERATED_BODY()
public:
    virtual bool CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error) override;
    virtual FText GetInteractableActionText_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction) const override;
protected:
    virtual bool Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction) override;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovAurelionRequestResult, bool, bAccepted, const FText&, Message);

/** Physical request surface only. The target owner must earn every cinematic, handoff, companion,
 * fracture and mission result. There is deliberately no generic CompleteBeat or state-write operation. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionRequestActor : public AActor
{
    GENERATED_BODY()
public:
    ASovAurelionRequestActor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovAurelionRequestInteractable> Interactable;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") FName RequestId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") FName MissionId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") FName BeatId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") ESovAurelionRequest Operation = ESovAurelionRequest::PlayScene;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") TObjectPtr<ASovAurelionStorySequenceActor> Story;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") TObjectPtr<ASovCampaignHandoffAnchor> HandoffAnchor;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") TObjectPtr<ASovCoActionAnchor> CoActionAnchor;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") TObjectPtr<USovAurelionThermalFractureComponent> Thermal;
    /** Retry-safe alternative to Thermal. The exact participant's current component is
     * resolved for each new input; a queued request never retargets a replacement. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") TObjectPtr<ASovEncounterDirector> ThermalDirector;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") FName ThermalParticipantId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") TObjectPtr<USovCampaignDefinition> DestinationMission;
    /** Exact existing entry owners. This request never starts a fresh or successful encounter. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") TObjectPtr<ASovCampaignEncounterObjective> RetryObjective;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion|Request") TObjectPtr<ASovEncounterDirector> RetryDirector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aurelion|Request") FText ActionText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Aurelion|Request") FText LastResult;
    UPROPERTY(BlueprintAssignable, Category="Aurelion|Request") FSovAurelionRequestResult OnRequestResult;
    UFUNCTION(BlueprintPure, Category="Aurelion|Request") bool IsRequestPending() const { return bPending || bExecuting; }
    UFUNCTION(BlueprintPure, Category="Aurelion|Request") bool CanUse(const APawn* Pawn, FText& Error) const;
    UFUNCTION(BlueprintPure, Category="Aurelion|Request") USovAurelionThermalFractureComponent* GetCurrentThermalTarget() const;
    bool RequestUse(APawn* Pawn, FText& Error);
    virtual void Tick(float DeltaSeconds) override;
protected:
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    struct FRequest
    {
        TWeakObjectPtr<ASovPlayerCharacterBase> Pawn;
        TWeakObjectPtr<ASovPlayerController> Controller;
        TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC;
        TWeakObjectPtr<USovAurelionRequestInteractable> Interactable;
        TWeakObjectPtr<USovCampaignDefinition> Mission, Destination;
        TWeakObjectPtr<ASovAurelionStorySequenceActor> Story;
        TWeakObjectPtr<ASovCampaignHandoffAnchor> Handoff;
        TWeakObjectPtr<ASovCoActionAnchor> CoAction;
        TWeakObjectPtr<USovAurelionThermalFractureComponent> Thermal;
        TWeakObjectPtr<USovAurelionThermalFractureComponent> AuthoredThermal;
        TWeakObjectPtr<ASovEncounterDirector> AuthoredThermalDirector;
        TWeakObjectPtr<ASovEncounterDirector> Director;
        TWeakObjectPtr<ASovCampaignEncounterObjective> RetryObjective;
        TWeakObjectPtr<ASovEncounterDirector> AuthoredRetryDirector;
        TWeakObjectPtr<ASovProtagonistCompanionCharacter> Companion;
        FName RequestId, MissionId, BeatId, ThermalParticipantId;
        FSoftObjectPath Sequence;
        FGuid AttemptId;
        ESovAurelionRequest Operation = ESovAurelionRequest::PlayScene;
        int32 ReadyEpoch = 0;
        uint64 ActorInfoEpoch = 0, TransitionEpoch = 0, EncounterGeneration = 0;
    };
    bool CanUseInternal(const APawn* Pawn, FText& Error, bool bExecutingRequest) const;
    bool OwnsRequest(const FRequest& Request) const;
    void ExecuteRequest(FRequest Request);
    void PublishResult(bool bAccepted, const FText& Message);
    FTimerHandle RequestTimer;
    bool bPending = false, bExecuting = false, bEnding = false;
};
