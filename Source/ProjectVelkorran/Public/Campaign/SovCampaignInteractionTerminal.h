// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableComponent.h"
#include "SovCampaignInteractionTerminal.generated.h"

class ASovPlayerCharacterBase;
class ASovPlayerController;
class USovCampaignDefinition;
class UNarrativeAbilitySystemComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class PROJECTVELKORRAN_API USovCampaignTerminalInteractable : public UNarrativeInteractableComponent
{
    GENERATED_BODY()
public:
    virtual bool CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error) override;
    virtual FText GetInteractableActionText_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction) const override;
protected:
    virtual bool Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction) override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovCampaignTerminalResult, bool, bSucceeded, const FText&, Message);

/** Physical, single-beat mission interaction through Narrative's normal focus/hold/input path.
 * Completion uses the campaign journal. Travel uses its existing durable origin/recovery owner.
 * It cannot produce cinematic, co-action, handoff, choice or protected canon proof. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovCampaignInteractionTerminal : public AActor
{
    GENERATED_BODY()
public:
    ASovCampaignInteractionTerminal();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovCampaignTerminalInteractable> Interactable;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") FName TerminalId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") FName MissionId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") FName CompletionBeat;
    /** Empty means remain in this segment. A destination must be an explicitly allowed successor. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") TObjectPtr<USovCampaignDefinition> DestinationMission;
    /** Travel always writes its own durable boundary regardless of this ordinary-terminal option. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") bool bWriteCheckpoint = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign") FText ActionText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Campaign") FText LastResult;
    UPROPERTY(BlueprintAssignable, Category="Campaign") FSovCampaignTerminalResult OnTerminalResult;
    UFUNCTION(BlueprintPure, Category="Campaign") bool IsRequestPending() const { return bPending || bExecuting; }
    bool CanUse(const APawn* Pawn, FText& Error) const;
    bool RequestUse(APawn* Pawn, FText& Error);
protected:
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    struct FRequest
    {
        TWeakObjectPtr<ASovPlayerCharacterBase> Pawn;
        TWeakObjectPtr<ASovPlayerController> Controller;
        TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC;
        TWeakObjectPtr<USovCampaignTerminalInteractable> Interactable;
        TWeakObjectPtr<USovCampaignDefinition> Mission, Destination;
        FName TerminalId, MissionId, BeatId;
        int32 ReadyEpoch = 0;
        uint64 TransitionEpoch = 0;
        bool bCheckpoint = false;
    };
    bool CanUseInternal(const APawn* Pawn, FText& Error, bool bExecutingRequest) const;
    bool OwnsRequest(const FRequest& Request) const;
    void ExecuteRequest(FRequest Request);
    void PublishResult(bool bSucceeded, const FText& Message);
    FTimerHandle RequestTimer;
    bool bPending = false;
    bool bExecuting = false;
    bool bEnding = false;
    bool bBoundarySucceeded = false;
};
